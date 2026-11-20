// Render di prova offline: suona ogni preset di fabbrica (accordo, arpeggio, nota bassa, sweep di risonanza)
// e salva un WAV per preset, con picco, RMS e controllo NaN.  Uso: mumu_render <cartella_uscita> [indice]
#include <juce_audio_formats/juce_audio_formats.h>
#include "Engine.h"
#include "Presets.h"

using namespace mumu;

static void fromPreset (const Preset& p, TimbreValues (&t)[2], GlobalValues& g)
{
    for (int k = 0; k < 2; ++k) for (int i = 0; i < TP_COUNT; ++i) t[k].v[i] = timbreDefs()[(size_t) i].def;
    for (int i = 0; i < GP_COUNT; ++i) g.v[i] = globalDefs()[(size_t) i].def;
    for (auto& kv : p.values)
    {
        const juce::String id (kv.first);
        bool found = false;
        for (int k = -1; k < 2 && ! found; ++k)
        {
            const int count = k < 0 ? GP_COUNT : TP_COUNT;
            for (int i = 0; i < count; ++i)
                if (paramID (k, i) == id) { (k < 0 ? g.v[i] : t[k].v[i]) = kv.second; found = true; break; }
        }
        if (! found) std::printf ("  ID SCONOSCIUTO: %s\n", kv.first);
    }
}

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    if (auto os = std::unique_ptr<juce::OutputStream> (f.createOutputStream()))
        if (auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (b.getNumChannels()).withBitsPerSample (24)))
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

// suona una lista di eventi con valori dati e restituisce il buffer
struct TEv { double t; int note; bool on; int cc = -1; int val = 0; };
static juce::AudioBuffer<float> play (TimbreValues (&tv)[2], GlobalValues& gv, std::vector<TEv> evs, double seconds, double sr, float& peak, bool& bad)
{
    Synth s; const int block = 128; s.prepare (sr, block);
    const int total = (int) (seconds * sr);
    juce::AudioBuffer<float> out (2, total), buf (2, block);
    peak = 0; bad = false;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        juce::MidiBuffer midi;
        for (auto& e : evs)
        {
            const int at = (int) (e.t * sr) - pos;
            if (at < 0 || at >= n) continue;
            if (e.cc >= 0) midi.addEvent (juce::MidiMessage::controllerEvent (1, e.cc, e.val), at);
            else midi.addEvent (e.on ? juce::MidiMessage::noteOn (1, e.note, (juce::uint8) 100) : juce::MidiMessage::noteOff (1, e.note), at);
        }
        juce::AudioBuffer<float> b (buf.getArrayOfWritePointers(), 2, n);
        b.clear();
        s.process (b, midi, tv, gv, 0, 0, false);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float x = b.getSample (c, i);
                if (! std::isfinite (x)) bad = true;
                peak = std::max (peak, std::abs (x));
                out.setSample (c, pos + i, x);
            }
    }
    return out;
}

static int runTests (const juce::File& dir)
{
    const double sr = 48000;
    Preset clean { "clean", { { "A_F1CUT", 20000.f }, { "A_FENVAMT", 0.f }, { "A_KEYTRK", 0.f }, { "A_MIX2", 0.f }, { "A_DRIVE", 0.f },
                              { "A_AA", .001f }, { "A_AS", 1.f }, { "A_AR", .01f }, { "A_SPREAD", 0.f }, { "G_DLON", 0.f }, { "G_LOWCUT", 0.f } } };
    float peak; bool bad;
    // 1) aliasing: C7 tenuto, forme d'onda diverse, filtro aperto
    for (float w : { 0.f, .34f, .66f, .9f })
    {
        TimbreValues tv[2]; GlobalValues gv; fromPreset (clean, tv, gv);
        tv[0].v[O1WAVE] = w; tv[0].v[F1RES] = 0;
        auto b = play (tv, gv, { { .0, 96, true }, { 1.0, 96, false } }, 1.1, sr, peak, bad);
        writeWav (dir.getChildFile ("test_alias_w" + juce::String ((int) (w * 100)) + ".wav"), b, sr);
        std::printf ("alias w=%.2f peak %.3f%s\n", w, peak, bad ? " NAN" : "");
    }
    // 2) autooscillazione: risonanza massima, nessuna sorgente, key track 100%
    {
        TimbreValues tv[2]; GlobalValues gv; fromPreset (clean, tv, gv);
        tv[0].v[MIX1] = 0; tv[0].v[F1RES] = 1; tv[0].v[F1CUT] = 261.63f; tv[0].v[KEYTRK] = 1;
        auto b = play (tv, gv, { { .0, 48, true }, { 1.0, 48, false }, { 1.1, 60, true }, { 2.1, 60, false }, { 2.2, 72, true }, { 3.2, 72, false } }, 3.4, sr, peak, bad);
        writeWav (dir.getChildFile ("test_selfosc.wav"), b, sr);
        std::printf ("selfosc peak %.3f%s\n", peak, bad ? " NAN" : "");
    }
    // 3) sweep di cutoff con risonanza massima e drive alto (stabilità)
    {
        TimbreValues tv[2]; GlobalValues gv; fromPreset (clean, tv, gv);
        tv[0].v[F1RES] = 1; tv[0].v[F1CUT] = 20; tv[0].v[FENVAMT] = 1; tv[0].v[FA] = 3; tv[0].v[FS] = 1; tv[0].v[DRIVE] = 1;
        tv[0].v[F2RES] = 1; tv[0].v[F2CUT] = 20000; tv[0].v[FROUTE] = 0;
        auto b = play (tv, gv, { { .0, 36, true }, { 3.5, 36, false } }, 4.0, sr, peak, bad);
        writeWav (dir.getChildFile ("test_sweep.wav"), b, sr);
        std::printf ("sweep peak %.3f%s\n", peak, bad ? " NAN" : "");
    }
    // 4) furto di voci: 16 note veloci con release lungo (8 voci)
    {
        TimbreValues tv[2]; GlobalValues gv; fromPreset (clean, tv, gv);
        tv[0].v[AR] = 2; tv[0].v[F1CUT] = 3000; tv[0].v[O1WAVE] = .34f;
        std::vector<TEv> ev;
        for (int k = 0; k < 16; ++k) { ev.push_back ({ k * .08, 48 + (k * 5) % 24, true }); ev.push_back ({ k * .08 + .05, 48 + (k * 5) % 24, false }); }
        auto b = play (tv, gv, ev, 3.0, sr, peak, bad);
        writeWav (dir.getChildFile ("test_steal.wav"), b, sr);
        std::printf ("steal peak %.3f%s\n", peak, bad ? " NAN" : "");
    }
    // 5) mono legato con glide e pedale sustain
    {
        TimbreValues tv[2]; GlobalValues gv; fromPreset (clean, tv, gv);
        tv[0].v[VMODE] = 1; tv[0].v[GLIDE] = .2f; tv[0].v[F1CUT] = 2000;
        auto b = play (tv, gv, { { .0, 48, true }, { .5, 60, true }, { 1.0, 60, false }, { 1.4, 48, false },
                                 { 1.6, 0, true, 64, 127 }, { 1.7, 55, true }, { 1.9, 55, false }, { 2.6, 0, true, 64, 0 } }, 3.2, sr, peak, bad);
        writeWav (dir.getChildFile ("test_mono.wav"), b, sr);
        std::printf ("mono peak %.3f%s\n", peak, bad ? " NAN" : "");
    }
    return 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::ScopedNoDenormals nd;
    const juce::File outDir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory());
    outDir.createDirectory();
    if (argc > 2 && juce::String (argv[2]) == "test") return runTests (outDir);
    const int only = argc > 2 ? std::atoi (argv[2]) : -1;
    const double sr = 48000; const int block = 256; const double seconds = 9.0;
    const auto& list = factoryPresets();
    for (int idx = 0; idx < (int) list.size(); ++idx)
    {
        if (only >= 0 && idx != only) continue;
        Synth synth; synth.prepare (sr, block);
        TimbreValues tv[2]; GlobalValues gv;
        fromPreset (list[(size_t) idx], tv, gv);
        const int total = (int) (seconds * sr);
        juce::AudioBuffer<float> out (2, total), buf (2, block);
        struct Ev { double t; int note; bool on; };
        std::vector<Ev> evs;
        for (int n : { 48, 60, 64, 67 }) { evs.push_back ({ .05, n, true }); evs.push_back ({ 2.5, n, false }); }
        const int arp[] = { 60, 64, 67, 72, 76, 72, 67, 64 };
        for (int k = 0; k < 8; ++k) { evs.push_back ({ 3.0 + k * .22, arp[k], true }); evs.push_back ({ 3.0 + k * .22 + .18, arp[k], false }); }
        evs.push_back ({ 5.0, 36, true }); evs.push_back ({ 5.8, 36, false });
        evs.push_back ({ 6.3, 84, true }); evs.push_back ({ 7.3, 84, false });
        double sumSq = 0; float peak = 0; bool bad = false; double maxMs = 0, sumMs = 0; int blocks = 0;
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = std::min (block, total - pos);
            juce::MidiBuffer midi;
            for (auto& e : evs)
            {
                const int at = (int) (e.t * sr) - pos;
                if (at >= 0 && at < n) midi.addEvent (e.on ? juce::MidiMessage::noteOn (1, e.note, (juce::uint8) 100) : juce::MidiMessage::noteOff (1, e.note), at);
            }
            juce::AudioBuffer<float> b (buf.getArrayOfWritePointers(), 2, n);
            b.clear();
            const auto t0 = juce::Time::getHighResolutionTicks();
            synth.process (b, midi, tv, gv, 0, 0, false);
            const double ms = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) * 1000.0;
            if (pos > 0) maxMs = std::max (maxMs, ms);   // il primo blocco include l'allocazione iniziale
            sumMs += ms; ++blocks;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                {
                    const float x = b.getSample (c, i);
                    if (! std::isfinite (x)) bad = true;
                    peak = std::max (peak, std::abs (x)); sumSq += (double) x * x;
                    out.setSample (c, pos + i, x);
                }
        }
        const double rms = std::sqrt (sumSq / (2.0 * total));
        const juce::String name (list[(size_t) idx].name);
        std::printf ("%02d %-20s peak %.3f  rms %.4f (%.1f dBFS)  blocco medio %.3f ms, max %.2f ms (budget %.2f)%s\n", idx + 1, name.toRawUTF8(), peak, rms,
                     20 * std::log10 (rms + 1e-12), sumMs / std::max (1, blocks), maxMs, block / sr * 1000.0, bad ? "  NAN!" : "");
        auto file = outDir.getChildFile (juce::String (idx + 1).paddedLeft ('0', 2) + "_" + name.replaceCharacter (' ', '_').replaceCharacter ('+', 'n') + ".wav");
        file.deleteFile();
        juce::WavAudioFormat wav;
        if (auto os = std::unique_ptr<juce::OutputStream> (file.createOutputStream()))
        {
            auto writer = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (24));
            if (writer) writer->writeFromAudioSampleBuffer (out, 0, total);
        }
    }
    return 0;
}
