// tools/offline_tester/main.cpp
#define JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED 1
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_events/juce_events.h>
#include <cmath>
#include <memory>

// === Include your DSP headers ===
#include "PitchEngine.h"
#include "Shifter.h"
#include "FormantRescue.h"
#include "AuthenticEMUZPlane.h"

// Simple args
struct Args {
    juce::File in, out;
    int key = 0;                   // 0=C .. 11=B
    juce::String scale = "Major";  // "Major"|"Minor"|"Chrom"
    float retune = 0.65f;          // 0..1
    int bias = +1;                 // -1/0/+1
    juce::String mode = "Track";   // "Track"|"Print"
    juce::String style = "Focus";  // "Air"|"Focus"|"Velvet"
    int block = 512;               // processing block size
    bool mono = true;              // analyze L only
};

static uint16_t maskForScale(const juce::String& s) {
    if (s.equalsIgnoreCase("Major"))   return 2741; // 0b101010110101
    if (s.equalsIgnoreCase("Minor"))   return 1453; // 0b010110101101
    return 0x0FFF; // Chromatic
}

static Shifter::Mode modeFor(const juce::String& m){
    return m.equalsIgnoreCase("Print") ? Shifter::PrintHQ : Shifter::TrackPSOLA;
}

static int styleIndex(const juce::String& s){
    if (s.equalsIgnoreCase("Air")) return 0;
    if (s.equalsIgnoreCase("Velvet")) return 2;
    return 1; // Focus
}

static void printUsage() {
    juce::Logger::outputDebugString(
        "Usage:\n"
        "  autotune_offline --in <in.wav> --out <out.wav> [--key 0..11] [--scale Major|Minor|Chrom]\n"
        "                   [--retune 0..1] [--bias -1|0|1] [--mode Track|Print]\n"
        "                   [--style Air|Focus|Velvet] [--block N]\n"
        "Examples:\n"
        "  autotune_offline --in vox.wav --out vox_tuned.wav --key 0 --scale Major --retune 0.7 --mode Track --style Air\n"
    );
}

static bool parseArgs(int argc, char** argv, Args& a) {
    juce::StringArray tokens;
    for (int i=0;i<argc;++i) tokens.add(argv[i]);
    for (int i=1;i<tokens.size(); ++i){
        auto t = tokens[i];
        auto next = [&]{ return (i+1<tokens.size()) ? tokens[++i] : juce::String(); };
        if (t == "--in") a.in = juce::File(next());
        else if (t == "--out") a.out = juce::File(next());
        else if (t == "--key") a.key = next().getIntValue();
        else if (t == "--scale") a.scale = next();
        else if (t == "--retune") a.retune = (float) next().getDoubleValue();
        else if (t == "--bias") a.bias = next().getIntValue();
        else if (t == "--mode") a.mode = next();
        else if (t == "--style") a.style = next();
        else if (t == "--block") a.block = next().getIntValue();
        else if (t == "--stereo") a.mono = false;
    }
    if (!a.in.existsAsFile() || a.out.getFullPathName().isEmpty()) {
        printUsage(); return false;
    }
    a.key = juce::jlimit(0, 11, a.key);
    a.retune = juce::jlimit(0.0f, 1.0f, a.retune);
    a.block = juce::jlimit(64, 4096, a.block);
    return true;
}

int main (int argc, char** argv) {
    // Initialize JUCE MessageManager for console app
    juce::MessageManager::getInstance();

    Args args;
    if (!parseArgs(argc, argv, args)) return 1;

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    // --- Reader
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor(args.in));
    if (!reader) {
        juce::Logger::writeToLog("Failed to open input: " + args.in.getFullPathName());
        return 2;
    }

    const double fs  = reader->sampleRate;
    const juce::int64 len = (juce::int64) reader->lengthInSamples;
    const int    ch  = (int)   reader->numChannels;
    const int    B   = args.block;

    juce::Logger::writeToLog("Input: " + args.in.getFileName() +
                           "  fs=" + juce::String(fs) +
                           "  ch=" + juce::String(ch) +
                           "  samples=" + juce::String((int)len));

    // --- Writer
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> outStream (args.out.createOutputStream());
    if (!outStream) {
        juce::Logger::writeToLog("Failed to open output: " + args.out.getFullPathName());
        return 3;
    }

    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor(
        outStream.get(), fs, /*channels*/ 2, 24, {}, 0));
    if (!writer) {
        juce::Logger::writeToLog("Failed to create writer");
        return 4;
    }
    outStream.release(); // writer owns the stream now

    // --- Allocate I/O buffers
    juce::AudioBuffer<float> inBuf ((int)ch, B);
    juce::AudioBuffer<float> outBuf (2, B);
    juce::AudioBuffer<float> dryTap (2, B); // for wet/dry

    // Mono analysis tap
    std::vector<float> mono(B, 0.0f), processed(B, 0.0f);

    // --- Initialize engines
    PitchEngine     pitch;
    Shifter         shifter;
    FormantRescue   rescue;
    AuthenticEMUZPlane emu;

    pitch.prepare(fs, B);
    pitch.setKeyScale(args.key, maskForScale(args.scale));
    pitch.setRetune(args.retune, args.bias);

    shifter.prepare(fs, modeFor(args.mode));
    rescue.prepare(fs);
    rescue.setStyle(styleIndex(args.style));

    emu.prepare(fs);
    emu.setShapePair(AuthenticEMUZPlane::Shape::VowelAe_Bright,
                     AuthenticEMUZPlane::Shape::VowelOh_Round);
    emu.setMorphPosition(0.5f);
    emu.setIntensity(0.6f);

    // --- Process loop
    juce::int64 pos = 0;

    while (pos < len) {
        const int toRead = (int) std::min<juce::int64>(B, len - pos);
        inBuf.clear();
        reader->read(&inBuf, 0, toRead, pos, true, true);
        pos += toRead;

        // Make a dry tap for true wet/dry if you want later
        dryTap.makeCopyOf(inBuf, true);

        // --- Mono analysis (L or mono-sum)
        if (args.mono && ch > 0) {
            const float* L = inBuf.getReadPointer(0);
            for (int n=0; n<toRead; ++n) mono[(size_t)n] = L[n];
        } else if (ch == 1) {
            const float* L = inBuf.getReadPointer(0);
            for (int n=0; n<toRead; ++n) mono[(size_t)n] = L[n];
        } else {
            // simple mono sum (avoid clipping)
            const float* L = inBuf.getReadPointer(0);
            const float* R = (ch > 1) ? inBuf.getReadPointer(1) : L;
            for (int n=0; n<toRead; ++n) mono[(size_t)n] = 0.5f * (L[n] + R[n]);
        }

        // --- Analysis -> ratio stream for this block
        auto blk = pitch.analyze(mono.data(), toRead); // blk.ratio, blk.voiced, blk.sibilant

        // DSP TUTOR DEBUG: Print diagnostic info
        if (blk.ratio && toRead > 0) {
            float ratioSum = 0.0f, ratioMin = 999.0f, ratioMax = 0.0f;
            for (int i = 0; i < toRead; ++i) {
                ratioSum += blk.ratio[i];
                ratioMin = std::min(ratioMin, blk.ratio[i]);
                ratioMax = std::max(ratioMax, blk.ratio[i]);
            }
            float ratioMean = ratioSum / toRead;

            printf("[Block] f0=%.2f Hz  ratio_mean=%.4f  min=%.4f  max=%.4f  voiced=%d  sibilant=%d\n",
                   blk.f0, ratioMean, ratioMin, ratioMax, blk.voiced ? 1 : 0, blk.sibilant ? 1 : 0);
        }

        // --- Shifter (mono) using per-sample ratio AND f0
        processed.assign((size_t)toRead, 0.0f);
        shifter.processBlock(mono.data(), processed.data(), toRead, blk.ratio, blk.f0);

        // --- Copy processed mono to stereo buffer (both channels)
        outBuf.clear();
        for (int n=0; n<toRead; ++n) {
            outBuf.setSample(0, n, processed[(size_t)n]);
            outBuf.setSample(1, n, processed[(size_t)n]);
        }


        // --- Z-plane "formant rescue" mapping and processing
        rescue.processBlock(emu, blk.ratio, toRead);
        emu.process(outBuf);

        // --- Optional: sibilant/voiced gating (light)
        if (!blk.voiced || blk.sibilant) {
            outBuf.applyGain(0, 0, toRead, 0.9f);
            outBuf.applyGain(1, 0, toRead, 0.9f);
        }

        // Calculate final output RMS before WAV writer
        float finalRMS = 0.0f;
        for (int n = 0; n < toRead; ++n) {
            float L = outBuf.getSample(0, n);
            float R = outBuf.getSample(1, n);
            finalRMS += (L*L + R*R) * 0.5f;
        }
        finalRMS = std::sqrt(finalRMS / toRead);

        if (debugBlockCount % 50 == 1) {
            printf("    [FINAL DEBUG] finalRMS=%.6f  finalSample[0]=(%.6f,%.6f)\n",
                   finalRMS, outBuf.getSample(0, 0), outBuf.getSample(1, 0));
        }

        // --- Basic safety (NaN/Inf clamp)
        for (int chn=0; chn<outBuf.getNumChannels(); ++chn) {
            float* p = outBuf.getWritePointer(chn);
            for (int n=0; n<toRead; ++n) {
                if (!std::isfinite(p[n])) p[n] = 0.0f;
                p[n] = juce::jlimit(-1.0f, 1.0f, p[n]);
            }
        }

        // --- Write block
        writer->writeFromAudioSampleBuffer(outBuf, 0, toRead);
    }

    writer.reset(); // Ensure file is properly closed
    juce::Logger::writeToLog("Wrote: " + args.out.getFullPathName());

    // MessageManager cleanup handled automatically
    return 0;
}