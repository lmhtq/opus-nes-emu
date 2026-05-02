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
#if defined(__ARM_NEON)
#  include <arm_neon.h>
#endif

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
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

        const int W_real = in.width, H_real = in.height, S = cfg_.scale;
        // CoreML's EnumeratedShapes mechanism on this model defaults to the
        // first shape (320). Feeding a 256-wide MLMultiArray still gets
        // resolved as 320 internally, producing a 1280-wide output that gets
        // unpacked at the wrong stride and tiles 4x horizontally. To stay
        // compatible without re-exporting, always pad input to the model's
        // native width (320) when smaller, then center-crop the output back
        // to (W_real * S) so callers see the contract they expect.
        const int kModelW = 320; // matches model's first EnumeratedShape
        const int W = (W_real < kModelW) ? kModelW : W_real;
        const int H = H_real;
        const int OW = W * S, OH = H * S;
        const int OW_real = W_real * S;
        const int pad_left = (W - W_real) / 2;

        @autoreleasepool {
            MLModel* m = (__bridge MLModel*)model_;
            NSError* aerr = nil;

            // Lazily allocate (and re-allocate on shape change) the input/output
            // MLMultiArrays *and* the wrapping FeatureValue/Provider/Options
            // so we don't pay alloc + Obj-C dispatch every frame.
            if (!in_arr_ || cached_W_ != W || cached_H_ != H) {
                in_arr_ = nil; out_arr_ = nil;
                fp_ = nil; pred_opts_ = nil;
                NSArray<NSNumber*>* ishape = @[@1, @3, @(H), @(W)];
                NSArray<NSNumber*>* oshape = @[@1, @3, @(OH), @(OW)];
                in_arr_ = [[MLMultiArray alloc] initWithShape:ishape
                                                     dataType:MLMultiArrayDataTypeFloat32
                                                        error:&aerr];
                if (!in_arr_) { if (err) *err = "alloc input MLMultiArray failed"; return false; }
                out_arr_ = [[MLMultiArray alloc] initWithShape:oshape
                                                      dataType:MLMultiArrayDataTypeFloat32
                                                         error:&aerr];
                if (!out_arr_) { if (err) *err = "alloc output MLMultiArray failed"; return false; }

                NSString* iname = [NSString stringWithUTF8String:input_name_.c_str()];
                NSString* oname = [NSString stringWithUTF8String:output_name_.c_str()];
                NSDictionary* feats = @{
                    iname: [MLFeatureValue featureValueWithMultiArray:in_arr_]
                };
                fp_ = [[MLDictionaryFeatureProvider alloc] initWithDictionary:feats error:&aerr];
                if (!fp_) { if (err) *err = "build feature provider failed"; return false; }

                pred_opts_ = [[MLPredictionOptions alloc] init];
                // NOTE: do NOT use setOutputBackings here. With this model's
                // EnumeratedShapes (256 / 320 widths), pinning a backing of the
                // 256-output size made CoreML silently fall back to the 320
                // input variant and produce 1280-wide output, which then got
                // unpacked at the wrong stride (visible as ~4x horizontal
                // repetition + vertical compression). We instead read the true
                // shape from the returned MLMultiArray below.
                cached_W_ = W; cached_H_ = H;

                in_planeR_.resize((size_t)H * W);
                in_planeG_.resize((size_t)H * W);
                in_planeB_.resize((size_t)H * W);
                in_planeA_.resize((size_t)H * W);
                out_planeR_.resize((size_t)OH * OW);
                out_planeG_.resize((size_t)OH * OW);
                out_planeB_.resize((size_t)OH * OW);
                out_planeA_.assign((size_t)OH * OW, 255);
            }

            // === input fill: RGBA8 -> CHW float32 [0,1] via vImage ===
            // If padding is needed (W_real < W), edge-extend the original
            // frame horizontally into a W-wide RGBA scratch first, then run
            // the existing tightly-packed planar conversion.
            const uint8_t* src_rgba = in.rgba.data();
            size_t src_stride = (size_t)W_real * 4;
            if (W_real != W) {
                in_padded_rgba_.resize((size_t)H * W * 4);
                for (int y = 0; y < H; ++y) {
                    const uint8_t* sr = in.rgba.data() + (size_t)y * W_real * 4;
                    uint8_t* dr = in_padded_rgba_.data() + (size_t)y * W * 4;
                    // left edge-extend
                    const uint8_t* edge_l = sr;
                    for (int x = 0; x < pad_left; ++x) {
                        std::memcpy(dr + x * 4, edge_l, 4);
                    }
                    // center copy
                    std::memcpy(dr + pad_left * 4, sr, (size_t)W_real * 4);
                    // right edge-extend
                    const uint8_t* edge_r = sr + (W_real - 1) * 4;
                    for (int x = pad_left + W_real; x < W; ++x) {
                        std::memcpy(dr + x * 4, edge_r, 4);
                    }
                }
                src_rgba = in_padded_rgba_.data();
                src_stride = (size_t)W * 4;
            }

            float* p = (float*)in_arr_.dataPointer;
            const int chan_stride = H * W;
            vImage_Buffer src_buf = { (void*)src_rgba,
                                      (vImagePixelCount)H, (vImagePixelCount)W,
                                      src_stride };
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

            id<MLFeatureProvider> result =
                [m predictionFromFeatures:fp_ options:pred_opts_ error:&aerr];
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
            // One-time log: verify the shape/strides we are about to unpack.
            static bool logged = false;
            if (!logged) {
                logged = true;
                std::string ss, st;
                for (NSNumber* n in om.shape)   { ss += std::to_string([n longValue]) + " "; }
                for (NSNumber* n in om.strides) { st += std::to_string([n longValue]) + " "; }
                std::fprintf(stderr,
                    "[ai-upscale-coreml] output shape=[%s] strides=[%s] dtype=%lu\n",
                    ss.c_str(), st.c_str(), (unsigned long)om.dataType);
            }

            // Trust the *actual* dimensions/strides reported by CoreML rather
            // than W*S/H*S — the model can resolve a different EnumeratedShape
            // than we requested (see comment near pred_opts_).
            int OH_actual = OH, OW_actual = OW;
            long stride_c = (long)OH * OW; // channel stride (in elements)
            long stride_h = (long)OW;      // row stride (in elements)
            if (om.shape.count >= 4) {
                OH_actual = (int)[om.shape[2] longValue];
                OW_actual = (int)[om.shape[3] longValue];
            }
            if (om.strides.count >= 4) {
                stride_c = [om.strides[1] longValue];
                stride_h = [om.strides[2] longValue];
            }
            // Center-crop the model output back to the caller's expected
            // resolution (OW_real x OH).  pad_left==0 means no crop.
            const int crop_left_cols = pad_left * S;
            const int OW_out = OW_real;          // visible width
            const int OH_out = H_real * S;       // visible height (==OH)
            // Re-point planar working buffers.
            out_planeR_.resize((size_t)OH_out * OW_out);
            out_planeG_.resize((size_t)OH_out * OW_out);
            out_planeB_.resize((size_t)OH_out * OW_out);
            if (out_planeA_.size() != (size_t)OH_out * OW_out)
                out_planeA_.assign((size_t)OH_out * OW_out, 255);
            // Expect shape (1,3,OH,OW); pack to RGBA8 via vImage.
            // Supports both Float32 and Float16 outputs.
            out.id = in.id;
            out.width = OW_out;
            out.height = OH_out;
            const size_t need = (size_t)OW_out * OH_out * 4;
            {
                std::lock_guard<std::mutex> g(pool_mu_);
                if (!pool_.empty()) {
                    out.rgba.swap(pool_.back());
                    pool_.pop_back();
                }
            }
            if (out.rgba.size() != need) out.rgba.resize(need);
            const long ostride = stride_c;
            (void)OW_actual;

            vImage_Buffer dst_buf = { out.rgba.data(),
                                      (vImagePixelCount)OH_out, (vImagePixelCount)OW_out,
                                      (size_t)OW_out * 4 };
            vImage_Buffer pR = { out_planeR_.data(), (vImagePixelCount)OH_out, (vImagePixelCount)OW_out, (size_t)OW_out };
            vImage_Buffer pG = { out_planeG_.data(), (vImagePixelCount)OH_out, (vImagePixelCount)OW_out, (size_t)OW_out };
            vImage_Buffer pB = { out_planeB_.data(), (vImagePixelCount)OH_out, (vImagePixelCount)OW_out, (size_t)OW_out };
            vImage_Buffer pA = { out_planeA_.data(), (vImagePixelCount)OH_out, (vImagePixelCount)OW_out, (size_t)OW_out };
            bool wrote_rgba_directly = false;

            if (om.dataType == MLMultiArrayDataTypeFloat32) {
                const float* op = (const float*)om.dataPointer;
                vImage_Buffer fR_o = { (void*)(op + 0 * ostride + crop_left_cols),
                                       (vImagePixelCount)OH_out, (vImagePixelCount)OW_out, (size_t)stride_h * 4 };
                vImage_Buffer fG_o = { (void*)(op + 1 * ostride + crop_left_cols),
                                       (vImagePixelCount)OH_out, (vImagePixelCount)OW_out, (size_t)stride_h * 4 };
                vImage_Buffer fB_o = { (void*)(op + 2 * ostride + crop_left_cols),
                                       (vImagePixelCount)OH_out, (vImagePixelCount)OW_out, (size_t)stride_h * 4 };
                vImageConvert_PlanarFtoPlanar8(&fR_o, &pR, 1.0f, 0.0f, kvImageNoFlags);
                vImageConvert_PlanarFtoPlanar8(&fG_o, &pG, 1.0f, 0.0f, kvImageNoFlags);
                vImageConvert_PlanarFtoPlanar8(&fB_o, &pB, 1.0f, 0.0f, kvImageNoFlags);
            } else if (om.dataType == MLMultiArrayDataTypeFloat16) {
                const uint16_t* op = (const uint16_t*)om.dataPointer;
#if defined(__ARM_NEON)
                // Single-pass NEON kernel: 3 F16 planes -> RGBA8 (A=255).
                // Reads 3 * 2B + writes 4B per pixel — ~half the memory traffic
                // of the F16->F32->U8->interleave chain.
                const float16x8_t v255 = vdupq_n_f16((__fp16)255.0f);
                const float16x8_t vzero = vdupq_n_f16((__fp16)0.0f);
                const uint8x8_t alpha = vdup_n_u8(255);
                auto convert_row = [&](const __fp16* rp, const __fp16* gp,
                                       const __fp16* bp, uint8_t* dp, int n) {
                    int i = 0;
                    for (; i + 8 <= n; i += 8) {
                        float16x8_t fr = vld1q_f16(rp + i);
                        float16x8_t fg = vld1q_f16(gp + i);
                        float16x8_t fb = vld1q_f16(bp + i);
                        fr = vminq_f16(vmaxq_f16(vmulq_f16(fr, v255), vzero), v255);
                        fg = vminq_f16(vmaxq_f16(vmulq_f16(fg, v255), vzero), v255);
                        fb = vminq_f16(vmaxq_f16(vmulq_f16(fb, v255), vzero), v255);
                        uint16x8_t ur = vcvtq_u16_f16(fr);
                        uint16x8_t ug = vcvtq_u16_f16(fg);
                        uint16x8_t ub = vcvtq_u16_f16(fb);
                        uint8x8x4_t pix = {{ vmovn_u16(ur), vmovn_u16(ug), vmovn_u16(ub), alpha }};
                        vst4_u8(dp + i * 4, pix);
                    }
                    for (; i < n; ++i) {
                        auto cvt = [](float v){
                            v = v * 255.0f; if (v < 0) v = 0; if (v > 255) v = 255;
                            return (uint8_t)v;
                        };
                        dp[i*4+0] = cvt((float)rp[i]);
                        dp[i*4+1] = cvt((float)gp[i]);
                        dp[i*4+2] = cvt((float)bp[i]);
                        dp[i*4+3] = 255;
                    }
                };
                const __fp16* R0 = (const __fp16*)(op + 0 * ostride) + crop_left_cols;
                const __fp16* G0 = (const __fp16*)(op + 1 * ostride) + crop_left_cols;
                const __fp16* B0 = (const __fp16*)(op + 2 * ostride) + crop_left_cols;
                if (crop_left_cols == 0 && stride_h == OW_out) {
                    // fully tightly-packed AND no crop: single fast pass
                    convert_row(R0, G0, B0, out.rgba.data(),
                                OH_out * OW_out);
                } else {
                    for (int y = 0; y < OH_out; ++y) {
                        convert_row(R0 + (long)y * stride_h,
                                    G0 + (long)y * stride_h,
                                    B0 + (long)y * stride_h,
                                    out.rgba.data() + (size_t)y * OW_out * 4,
                                    OW_out);
                    }
                }
                wrote_rgba_directly = true;
#else
                f16_scratch_.resize((size_t)OH_out * OW_out);
                auto convert_plane = [&](const uint16_t* base, vImage_Buffer& outU8) {
                    vImage_Buffer fbuf16 = { (void*)(base + crop_left_cols),
                                             (vImagePixelCount)OH_out, (vImagePixelCount)OW_out, (size_t)stride_h * 2 };
                    vImage_Buffer fbuf32 = { f16_scratch_.data(),
                                             (vImagePixelCount)OH_out, (vImagePixelCount)OW_out, (size_t)OW_out * 4 };
                    vImageConvert_Planar16FtoPlanarF(&fbuf16, &fbuf32, kvImageNoFlags);
                    vImageConvert_PlanarFtoPlanar8(&fbuf32, &outU8, 1.0f, 0.0f, kvImageNoFlags);
                };
                convert_plane(op + 0 * ostride, pR);
                convert_plane(op + 1 * ostride, pG);
                convert_plane(op + 2 * ostride, pB);
#endif
            } else {
                if (err) *err = "unsupported output dataType";
                return false;
            }

            // vImageConvert_Planar8toARGB8888 just interleaves 4 planes byte-by-byte
            // in the order given. Pass (R,G,B,A) to produce RGBA8 layout.
            if (!wrote_rgba_directly) {
                vImageConvert_Planar8toARGB8888(&pR, &pG, &pB, &pA, &dst_buf, kvImageNoFlags);
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

    void recycle_output_buffer(ByteVec&& buf) override {
        if (buf.capacity() == 0) return;
        std::lock_guard<std::mutex> g(pool_mu_);
        if (pool_.size() < 4) pool_.emplace_back(std::move(buf));
    }

private:
    UpscalerConfig cfg_;
    void* model_ = nullptr; // CFRetain'd MLModel*
    std::string input_name_;
    std::string output_name_;
    // Per-shape cached objects (rebuilt on first frame and on shape change).
    int cached_W_ = 0, cached_H_ = 0;
    MLMultiArray* in_arr_ = nil;
    MLMultiArray* out_arr_ = nil;
    MLDictionaryFeatureProvider* fp_ = nil;
    MLPredictionOptions* pred_opts_ = nil;
    // vImage scratch buffers (reused frame-to-frame to avoid allocs).
    std::vector<uint8_t> in_planeR_, in_planeG_, in_planeB_, in_planeA_;
    std::vector<uint8_t> out_planeR_, out_planeG_, out_planeB_, out_planeA_;
    std::vector<float>   f16_scratch_;
    std::vector<uint8_t> in_padded_rgba_; // edge-extended input (W_real -> W)
    std::mutex pool_mu_;
    std::vector<ByteVec> pool_;
};

} // namespace

std::unique_ptr<IAiUpscaler> make_coreml_upscaler() {
    return std::make_unique<CoreMLUpscaler>();
}

} // namespace fcemu
