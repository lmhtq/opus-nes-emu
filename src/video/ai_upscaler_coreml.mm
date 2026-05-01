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
#import <Accelerate/Accelerate.h>

#include <chrono>
#include <cstdio>
#include <cstring>
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
            const int chan_stride = H * W;
            // Reusable scratch U8 planes for vImage split.
            in_planeR_.resize((size_t)chan_stride);
            in_planeG_.resize((size_t)chan_stride);
            in_planeB_.resize((size_t)chan_stride);
            in_planeA_.resize((size_t)chan_stride);

            vImage_Buffer src_buf = { (void*)in.rgba.data(),
                                      (vImagePixelCount)H, (vImagePixelCount)W,
                                      (size_t)W * 4 };
            vImage_Buffer dR = { in_planeR_.data(), (vImagePixelCount)H, (vImagePixelCount)W, (size_t)W };
            vImage_Buffer dG = { in_planeG_.data(), (vImagePixelCount)H, (vImagePixelCount)W, (size_t)W };
            vImage_Buffer dB = { in_planeB_.data(), (vImagePixelCount)H, (vImagePixelCount)W, (size_t)W };
            vImage_Buffer dA = { in_planeA_.data(), (vImagePixelCount)H, (vImagePixelCount)W, (size_t)W };
            vImageConvert_RGBA8888toPlanar8(&src_buf, &dR, &dG, &dB, &dA, kvImageNoFlags);

            vImage_Buffer fR = { p + 0 * chan_stride, (vImagePixelCount)H, (vImagePixelCount)W, (size_t)W * 4 };
            vImage_Buffer fG = { p + 1 * chan_stride, (vImagePixelCount)H, (vImagePixelCount)W, (size_t)W * 4 };
            vImage_Buffer fB = { p + 2 * chan_stride, (vImagePixelCount)H, (vImagePixelCount)W, (size_t)W * 4 };
            vImageConvert_Planar8toPlanarF(&dR, &fR, 1.0f, 0.0f, kvImageNoFlags);
            vImageConvert_Planar8toPlanarF(&dG, &fG, 1.0f, 0.0f, kvImageNoFlags);
            vImageConvert_Planar8toPlanarF(&dB, &fB, 1.0f, 0.0f, kvImageNoFlags);

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
            // Expect shape (1,3,OH,OW); pack to RGBA8 via vImage.
            // Supports both Float32 and Float16 outputs.
            out.id = in.id;
            out.width = OW;
            out.height = OH;
            out.rgba.assign((size_t)OW * OH * 4, 255);
            const int ostride = OH * OW;

            // Reusable U8 output planes
            out_planeR_.resize((size_t)ostride);
            out_planeG_.resize((size_t)ostride);
            out_planeB_.resize((size_t)ostride);
            if (out_planeA_.size() != (size_t)ostride) {
                out_planeA_.assign((size_t)ostride, 255);
            }

            vImage_Buffer dst_buf = { out.rgba.data(),
                                      (vImagePixelCount)OH, (vImagePixelCount)OW,
                                      (size_t)OW * 4 };
            vImage_Buffer pR = { out_planeR_.data(), (vImagePixelCount)OH, (vImagePixelCount)OW, (size_t)OW };
            vImage_Buffer pG = { out_planeG_.data(), (vImagePixelCount)OH, (vImagePixelCount)OW, (size_t)OW };
            vImage_Buffer pB = { out_planeB_.data(), (vImagePixelCount)OH, (vImagePixelCount)OW, (size_t)OW };
            vImage_Buffer pA = { out_planeA_.data(), (vImagePixelCount)OH, (vImagePixelCount)OW, (size_t)OW };

            if (om.dataType == MLMultiArrayDataTypeFloat32) {
                const float* op = (const float*)om.dataPointer;
                vImage_Buffer fR_o = { (void*)(op + 0 * ostride),
                                       (vImagePixelCount)OH, (vImagePixelCount)OW, (size_t)OW * 4 };
                vImage_Buffer fG_o = { (void*)(op + 1 * ostride),
                                       (vImagePixelCount)OH, (vImagePixelCount)OW, (size_t)OW * 4 };
                vImage_Buffer fB_o = { (void*)(op + 2 * ostride),
                                       (vImagePixelCount)OH, (vImagePixelCount)OW, (size_t)OW * 4 };
                vImageConvert_PlanarFtoPlanar8(&fR_o, &pR, 1.0f, 0.0f, kvImageNoFlags);
                vImageConvert_PlanarFtoPlanar8(&fG_o, &pG, 1.0f, 0.0f, kvImageNoFlags);
                vImageConvert_PlanarFtoPlanar8(&fB_o, &pB, 1.0f, 0.0f, kvImageNoFlags);
            } else if (om.dataType == MLMultiArrayDataTypeFloat16) {
                // Convert F16 -> F32 in scratch, then to U8.
                f16_scratch_.resize((size_t)ostride);
                const uint16_t* op = (const uint16_t*)om.dataPointer;
                auto convert_plane = [&](const uint16_t* base, vImage_Buffer& outU8) {
                    vImage_Buffer fbuf16 = { (void*)base,
                                             (vImagePixelCount)OH, (vImagePixelCount)OW, (size_t)OW * 2 };
                    vImage_Buffer fbuf32 = { f16_scratch_.data(),
                                             (vImagePixelCount)OH, (vImagePixelCount)OW, (size_t)OW * 4 };
                    vImageConvert_Planar16FtoPlanarF(&fbuf16, &fbuf32, kvImageNoFlags);
                    vImageConvert_PlanarFtoPlanar8(&fbuf32, &outU8, 1.0f, 0.0f, kvImageNoFlags);
                };
                convert_plane(op + 0 * ostride, pR);
                convert_plane(op + 1 * ostride, pG);
                convert_plane(op + 2 * ostride, pB);
            } else {
                if (err) *err = "unsupported output dataType";
                return false;
            }

            // vImageConvert_Planar8toARGB8888 just interleaves 4 planes byte-by-byte
            // in the order given. Pass (R,G,B,A) to produce RGBA8 layout.
            vImageConvert_Planar8toARGB8888(&pR, &pG, &pB, &pA, &dst_buf, kvImageNoFlags);
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
    // vImage scratch buffers (reused frame-to-frame to avoid allocs).
    std::vector<uint8_t> in_planeR_, in_planeG_, in_planeB_, in_planeA_;
    std::vector<uint8_t> out_planeR_, out_planeG_, out_planeB_, out_planeA_;
    std::vector<float>   f16_scratch_;
};

} // namespace

std::unique_ptr<IAiUpscaler> make_coreml_upscaler() {
    return std::make_unique<CoreMLUpscaler>();
}

} // namespace fcemu
