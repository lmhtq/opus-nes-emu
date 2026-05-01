// REQ-116 — Apple CoreML / ANE AI 超分后端 (macOS Apple Silicon)
//
// 直接通过 CoreML C++ via Obj-C runtime 调用 ANE 推理。
// M2 base 实测 (animevideov3 x4, 256x240->1024x960, FP16):
//   ANE: ~9.5 ms (105 fps)  vs  ncnn-vulkan: ~285 ms (3.5 fps)  →  ~30x
//
// 模型文件查找：
//   1) <model_dir>/<model>-x<scale>.mlmodelc
//   2) <model_dir>/<model>.mlmodelc
//   推荐使用预编译 .mlmodelc 目录（xcrun coremlcompiler compile），
//   也允许 .mlpackage / .mlmodel（首次加载会触发编译，慢得多）。
//
// 输入/输出张量约定（与 convert.py 对齐）：
//   input:  MultiArray Float32 NCHW = (1, 3, H, W), 范围 [0, 1]
//   output: MultiArray Float32 NCHW = (1, 3, scale*H, scale*W)
#include "fcemu/ai_upscaler.h"

#import <Foundation/Foundation.h>
#import <CoreML/CoreML.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace fcemu {

namespace {

namespace fs = std::filesystem;

NSURL* resolve_model_url(const std::string& dir, const std::string& name, int scale,
                         std::string* err) {
    auto try_one = [&](const std::string& base, const char* ext) -> NSURL* {
        fs::path p = fs::path(dir) / (base + ext);
        if (!fs::exists(p)) return nil;
        NSString* ns = [NSString stringWithUTF8String:p.string().c_str()];
        return [NSURL fileURLWithPath:ns];
    };
    NSURL* u = nil;
    if (scale > 0 && (u = try_one(name + "-x" + std::to_string(scale), ".mlmodelc"))) return u;
    if ((u = try_one(name, ".mlmodelc"))) return u;
    if (scale > 0 && (u = try_one(name + "-x" + std::to_string(scale), ".mlpackage"))) return u;
    if ((u = try_one(name, ".mlpackage"))) return u;
    if (scale > 0 && (u = try_one(name + "-x" + std::to_string(scale), ".mlmodel"))) return u;
    if ((u = try_one(name, ".mlmodel"))) return u;
    if (err) *err = "model not found under '" + dir + "': " + name +
                    "[-x" + std::to_string(scale) + "].{mlmodelc,mlpackage,mlmodel}";
    return nil;
}

class CoreMLUpscaler : public IAiUpscaler {
public:
    ~CoreMLUpscaler() override {
        if (model_) {
            CFBridgingRelease(model_); // balances __bridge_retained
            model_ = nullptr;
        }
    }

    bool init(const UpscalerConfig& cfg, std::string* err) override {
        cfg_ = cfg;
        std::string dir = cfg.model_dir;
        if (dir.empty()) {
            const char* env = std::getenv("FCEMU_AIUP_MODEL_DIR");
            dir = env ? env : "models";
        }
        @autoreleasepool {
            NSURL* url = resolve_model_url(dir, cfg.model_name, cfg.scale, err);
            if (!url) return false;

            // .mlpackage / .mlmodel 需要 compileModelAtURL（首次很慢，写日志提醒）。
            NSString* ext = [[url pathExtension] lowercaseString];
            NSURL* compiled_url = url;
            if (![ext isEqualToString:@"mlmodelc"]) {
                std::fprintf(stderr,
                    "[ai-upscale-coreml] compiling %s on first run "
                    "(may take 10-30s); to skip, ship a .mlmodelc directory.\n",
                    [[url path] UTF8String]);
                NSError* cerr = nil;
                NSURL* tmp = [MLModel compileModelAtURL:url error:&cerr];
                if (!tmp) {
                    if (err) *err = std::string("CoreML compile failed: ") +
                                    (cerr ? [[cerr localizedDescription] UTF8String] : "?");
                    return false;
                }
                compiled_url = tmp;
            }

            MLModelConfiguration* mc = [[MLModelConfiguration alloc] init];
            mc.computeUnits = MLComputeUnitsAll; // ANE-preferred

            NSError* lerr = nil;
            MLModel* m = [MLModel modelWithContentsOfURL:compiled_url
                                           configuration:mc
                                                   error:&lerr];
            if (!m) {
                if (err) *err = std::string("CoreML load failed: ") +
                                (lerr ? [[lerr localizedDescription] UTF8String] : "?");
                return false;
            }
            // CFRetain to keep alive across ARC autorelease boundaries.
            model_ = (__bridge_retained void*)m;

            // discover input/output names
            MLModelDescription* desc = m.modelDescription;
            input_name_ = [[[desc.inputDescriptionsByName allKeys] firstObject] UTF8String];
            output_name_ = [[[desc.outputDescriptionsByName allKeys] firstObject] UTF8String];
        }
        return true;
    }

    UpscalerCaps caps() const override {
        UpscalerCaps c;
        c.backend_name = "coreml";
        c.model_name = cfg_.model_name;
        c.fixed_scale = cfg_.scale;
        c.supports_batch = false;
        c.is_ai = true;
        return c;
    }

    bool upscale(const Frame& in, Frame& out, std::string* err) override {
        if (!model_) {
            if (err) *err = "not initialized";
            return false;
        }
        if (in.width <= 0 || in.height <= 0 ||
            (int)in.rgba.size() != in.width * in.height * 4) {
            if (err) *err = "invalid input frame";
            return false;
        }

        const int W = in.width, H = in.height, S = cfg_.scale;
        const int OW = W * S, OH = H * S;

        @autoreleasepool {
            MLModel* m = (__bridge MLModel*)model_;

            // input (1,3,H,W) Float32, range [0,1] — CHW layout
            NSArray<NSNumber*>* shape = @[@1, @3, @(H), @(W)];
            NSError* aerr = nil;
            MLMultiArray* arr = [[MLMultiArray alloc] initWithShape:shape
                                                          dataType:MLMultiArrayDataTypeFloat32
                                                             error:&aerr];
            if (!arr) {
                if (err) *err = "alloc input MLMultiArray failed";
                return false;
            }
            float* p = (float*)arr.dataPointer;
            // RGBA8 -> CHW float32 [0,1]
            const int chan_stride = H * W;
            const uint8_t* src = in.rgba.data();
            for (int y = 0; y < H; ++y) {
                for (int x = 0; x < W; ++x) {
                    int idx = (y * W + x) * 4;
                    int dst = y * W + x;
                    p[0 * chan_stride + dst] = src[idx + 0] / 255.0f;
                    p[1 * chan_stride + dst] = src[idx + 1] / 255.0f;
                    p[2 * chan_stride + dst] = src[idx + 2] / 255.0f;
                }
            }

            NSString* iname = [NSString stringWithUTF8String:input_name_.c_str()];
            NSDictionary<NSString*, MLFeatureValue*>* feats = @{
                iname: [MLFeatureValue featureValueWithMultiArray:arr]
            };
            MLDictionaryFeatureProvider* fp =
                [[MLDictionaryFeatureProvider alloc] initWithDictionary:feats error:&aerr];
            if (!fp) {
                if (err) *err = "build feature provider failed";
                return false;
            }

            id<MLFeatureProvider> result = [m predictionFromFeatures:fp error:&aerr];
            if (!result) {
                if (err) *err = std::string("CoreML predict failed: ") +
                                (aerr ? [[aerr localizedDescription] UTF8String] : "?");
                return false;
            }
            NSString* oname = [NSString stringWithUTF8String:output_name_.c_str()];
            MLFeatureValue* fv = [result featureValueForName:oname];
            MLMultiArray* om = fv.multiArrayValue;
            if (!om) {
                if (err) *err = "output is not multiarray";
                return false;
            }
            // Expect shape (1,3,OH,OW); coerce floats and pack to RGBA8.
            // Support both Float32 and Float16 outputs.
            out.id = in.id;
            out.width = OW;
            out.height = OH;
            out.rgba.assign((size_t)OW * OH * 4, 255);
            const int ostride = OH * OW;

            if (om.dataType == MLMultiArrayDataTypeFloat32) {
                const float* op = (const float*)om.dataPointer;
                for (int y = 0; y < OH; ++y) {
                    for (int x = 0; x < OW; ++x) {
                        int sidx = y * OW + x;
                        int didx = (y * OW + x) * 4;
                        auto cv = [&](float v) {
                            v = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
                            return (uint8_t)(v * 255.0f + 0.5f);
                        };
                        out.rgba[didx + 0] = cv(op[0 * ostride + sidx]);
                        out.rgba[didx + 1] = cv(op[1 * ostride + sidx]);
                        out.rgba[didx + 2] = cv(op[2 * ostride + sidx]);
                    }
                }
            } else if (om.dataType == MLMultiArrayDataTypeFloat16) {
                const uint16_t* op = (const uint16_t*)om.dataPointer;
                auto h2f = [](uint16_t h) {
                    uint32_t s = (h & 0x8000) << 16;
                    uint32_t e = (h >> 10) & 0x1F;
                    uint32_t m = h & 0x3FF;
                    uint32_t f;
                    if (e == 0) {
                        if (m == 0) f = s;
                        else { while (!(m & 0x400)) { m <<= 1; e -= 1; }
                            e += 1; m &= 0x3FF; f = s | ((e + 112) << 23) | (m << 13); }
                    } else if (e == 31) {
                        f = s | 0x7F800000 | (m << 13);
                    } else {
                        f = s | ((e + 112) << 23) | (m << 13);
                    }
                    float r; std::memcpy(&r, &f, 4); return r;
                };
                for (int y = 0; y < OH; ++y) {
                    for (int x = 0; x < OW; ++x) {
                        int sidx = y * OW + x;
                        int didx = (y * OW + x) * 4;
                        auto cv = [&](float v) {
                            v = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
                            return (uint8_t)(v * 255.0f + 0.5f);
                        };
                        out.rgba[didx + 0] = cv(h2f(op[0 * ostride + sidx]));
                        out.rgba[didx + 1] = cv(h2f(op[1 * ostride + sidx]));
                        out.rgba[didx + 2] = cv(h2f(op[2 * ostride + sidx]));
                    }
                }
            } else {
                if (err) *err = "unsupported output dataType";
                return false;
            }
        }
        return true;
    }

    bool upscale_batch(const std::vector<Frame>& in,
                       std::vector<Frame>& out,
                       std::string* err) override {
        out.clear();
        out.reserve(in.size());
        for (const auto& f : in) {
            Frame g;
            if (!upscale(f, g, err)) return false;
            out.push_back(std::move(g));
        }
        return true;
    }

private:
    UpscalerConfig cfg_;
    void* model_ = nullptr; // CFRetain'd MLModel*
    std::string input_name_;
    std::string output_name_;
};

} // namespace

std::unique_ptr<IAiUpscaler> make_coreml_upscaler() {
    return std::make_unique<CoreMLUpscaler>();
}

} // namespace fcemu
