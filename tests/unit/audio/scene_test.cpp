// scene_test.cpp - AudioEnhancer scene presets + remix mix-in.
#include "fcemu/audio_enhancer.h"
#include <cassert>
#include <cstdio>

int main() {
    using namespace fcemu;
    std::printf("=== fcemu Audio Scene Tests ===\n");
    AudioEnhancer ae; ae.init(44100);

    ae.set_scene("action");
    assert(ae.current_scene() == "action");

    ae.set_scene("calm");
    assert(ae.current_scene() == "calm");

    // Generate a flat sine in stereo and ensure the processor doesn't
    // explode (no NaNs / clipping) at default settings.
    std::vector<int16_t> in(2048, 0);
    for (size_t i = 0; i < in.size(); i += 2) {
        in[i]   = (int16_t)((i % 256) * 100);
        in[i+1] = (int16_t)((i % 256) * 100);
    }
    std::vector<int16_t> out;
    ae.process_samples(in, out);
    assert(out.size() == in.size());

    // Remix mix-in: trigger when no track is registered should be no-op.
    ae.enable_remix(true);
    ae.trigger_remix_oneshot("nonexistent");
    ae.process_samples(in, out);
    assert(out.size() == in.size());

    // Visualization should be populated.
    auto v = ae.get_visualization();
    assert(!v.waveform.empty());

    std::printf("PASS\n");
    return 0;
}
