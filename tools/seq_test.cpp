// Prove automatiche del sequencer di MuMU (senza interfaccia). Esce con 1 se una prova fallisce.
// Uso: mumu_seqtest
#include <juce_audio_basics/juce_audio_basics.h>
#include "Engine.h"

using namespace mumu;

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("  FALLITA: %s  (%s:%d)\n", msg, __FILE__, __LINE__); ++failures; } } while (0)

struct Abs { long long t; int note; float vel; int mask; };

static GlobalValues defaults()
{
    GlobalValues g;
    for (int i = 0; i < GP_COUNT; ++i) g.v[i] = globalDefs()[(size_t) i].def;
    return g;
}

// fa girare il sequencer per 'samples' campioni a blocchi e restituisce gli eventi in tempo assoluto
static std::vector<Abs> run (Sequencer& s, GlobalValues& g, long long samples, double bpm, bool host, double ppqStart = 0,
                             int block = 256, long long* clock = nullptr, double sr = 48000)
{
    std::vector<Abs> all;
    std::vector<Sequencer::Ev> ev;
    long long base = clock ? *clock : 0;
    for (long long pos = 0; pos < samples; pos += block)
    {
        const int n = (int) std::min<long long> (block, samples - pos);
        ev.clear();
        const double ppq = ppqStart + (double) pos * bpm / 60.0 / sr;
        s.process (n, g, bpm, ppq, host, ev);
        for (auto& e : ev) all.push_back ({ base + pos + e.offset, e.note, e.vel, e.mask });
    }
    if (clock) *clock = base + samples;
    return all;
}

static int countOn (const std::vector<Abs>& v) { int c = 0; for (auto& e : v) if (e.vel > 0) ++c; return c; }

// ogni note-on ha il suo note-off e nessuna nota parte due volte senza essere chiusa
static bool balanced (const std::vector<Abs>& v)
{
    int open[128] {};
    for (auto& e : v)
    {
        if (e.vel > 0) { if (open[e.note] > 0) return false; ++open[e.note]; }
        else { if (open[e.note] == 0) return false; --open[e.note]; }
    }
    for (int o : open) if (o != 0) return false;
    return true;
}

int main()
{
    const double sr = 48000, bpm = 120;
    const long long stepLen = 6000;   // 1/16 a 120 BPM e 48 kHz

    std::printf ("1. sequenza dimostrativa, tempi e gate\n");
    {
        Sequencer s; s.prepare (sr);
        auto g = defaults();
        g.v[SEQON] = 1;
        long long clk = 0;
        auto ev = run (s, g, stepLen * 16, bpm, false, 0, 256, &clk);
        g.v[SEQON] = 0;
        auto tail = run (s, g, 512, bpm, false, 0, 256, &clk);
        ev.insert (ev.end(), tail.begin(), tail.end());
        CHECK (! ev.empty() && ev[0].t == 0 && ev[0].note == 36 && ev[0].vel > .9f, "il primo passo suona C1 al campione 0");
        bool found3 = false;
        for (auto& e : ev) if (e.vel > 0 && e.note == 36 && e.t == 3 * stepLen) found3 = true;
        CHECK (found3, "il passo 4 parte esattamente a 3 passi");
        long long off0 = -1;
        for (auto& e : ev) if (e.vel == 0 && e.note == 36) { off0 = e.t; break; }
        CHECK (std::llabs (off0 - (long long) (stepLen * .70)) <= 1, "gate del passo 1 al 70%");
        CHECK (balanced (ev), "nessuna nota appesa dopo lo stop");
        // passo 15 (accordo legato) -> 16: 63 e 67 continuano, 39 è nuova
        int on63 = 0;
        for (auto& e : ev) if (e.vel > 0 && e.note == 63) ++on63;
        CHECK (on63 == 1, "la legatura non ribatte le note ripetute");
        bool reTrig = false; long long off67 = -1;
        for (auto& e : ev) { if (e.vel > 0 && e.t == 15 * stepLen) reTrig = true; if (e.vel == 0 && e.note == 67) off67 = e.t; }
        CHECK (! reTrig, "il passo 16 continua l'accordo legato senza ribattere");
        CHECK (std::llabs (off67 - (long long) (stepLen * 15.95)) <= 1, "l'accordo legato si chiude col gate del passo 16");
    }

    std::printf ("1b. stesso risultato con blocchi di qualsiasi misura\n");
    {
        std::vector<long long> ref;
        for (int block : { 256, 1, 37, 511, 4096 })
        {
            Sequencer s; s.prepare (sr);
            auto g = defaults();
            g.v[SEQON] = 1; g.v[SEQSWING] = .6f;
            auto ev = run (s, g, stepLen * 40, bpm, false, 0, block);
            std::vector<long long> ons;
            for (auto& e : ev) if (e.vel > 0) ons.push_back (e.t * 1000 + e.note);
            if (ref.empty()) ref = ons;
            CHECK (ons == ref, "gli eventi non dipendono dalla misura del blocco");
        }
    }

    std::printf ("2. swing\n");
    {
        Sequencer s; s.prepare (sr);
        auto g = defaults();
        g.v[SEQON] = 1; g.v[SEQSWING] = .66f;
        s.data.clearSlot (0);
        for (int i = 0; i < 4; ++i) s.data.step[0][i].add (60);
        s.data.length[0] = 4;
        auto ev = run (s, g, stepLen * 4, bpm, false);
        std::vector<long long> ons;
        for (auto& e : ev) if (e.vel > 0) ons.push_back (e.t);
        CHECK (ons.size() == 4, "quattro passi");
        if (ons.size() == 4)
        {
            CHECK (std::llabs (ons[1] - (long long) (stepLen * 1.32)) <= 1, "il passo pari arriva al 66%");
            CHECK (ons[2] == stepLen * 2, "il passo dispari resta in griglia");
        }
    }

    std::printf ("3. lunghezza, probabilità e timbro\n");
    {
        Sequencer s; s.prepare (sr);
        auto g = defaults();
        g.v[SEQON] = 1;
        s.data.clearSlot (0);
        s.data.length[0] = 3;
        s.data.step[0][0].add (60);
        s.data.step[0][1].add (62); s.data.step[0][1].prob = 0;
        s.data.step[0][2].add (64); s.data.step[0][2].timbre = 2;
        auto ev = run (s, g, stepLen * 9, bpm, false);
        int n60 = 0, n62 = 0, n64 = 0; bool maskB = true;
        for (auto& e : ev) if (e.vel > 0) { n60 += e.note == 60; n62 += e.note == 62; if (e.note == 64) { ++n64; maskB = maskB && e.mask == 2; } }
        CHECK (n60 == 3 && n64 == 3, "lunghezza 3: ogni passo suona 3 volte in 9");
        CHECK (n62 == 0, "probabilità 0: il passo non suona mai");
        CHECK (maskB, "il passo con timbro B va solo al timbro B");
    }

    std::printf ("4. sincronia con la DAW e salto di posizione\n");
    {
        Sequencer s; s.prepare (sr);
        auto g = defaults();
        g.v[SEQON] = 1;
        s.data.clearSlot (0);
        for (int i = 0; i < 16; ++i) s.data.step[0][i].add (40 + i);
        // partenza a metà del passo 3 (ppq 0,625): il primo passo che suona è il 4 (nota 43)
        auto ev = run (s, g, stepLen * 2, bpm, true, .625);
        CHECK (! ev.empty() && ev[0].vel > 0 && ev[0].note == 43 && ev[0].t == stepLen / 2, "in DAW riparte dal passo giusto, sulla griglia");
        // salto indietro a ppq 0 (loop): note chiuse e ripartenza dal passo 1
        std::vector<Sequencer::Ev> e2;
        s.process (256, g, bpm, 0.0, true, e2);
        bool closed = false, restart = false;
        for (auto& e : e2) { if (e.vel == 0) closed = true; if (e.vel > 0 && e.note == 40 && e.offset == 0) restart = true; }
        CHECK (closed && restart, "al salto chiude le note e riparte dal passo 1");
    }

    std::printf ("5. trasposizione dalla tastiera\n");
    {
        Sequencer s; s.prepare (sr);
        auto g = defaults();
        g.v[SEQON] = 1; g.v[SEQKEY] = 1;
        s.data.clearSlot (0);
        s.data.length[0] = 2;
        s.data.step[0][0].add (48); s.data.step[0][1].add (50);
        long long clk = 0;
        run (s, g, stepLen, bpm, false, 0, 256, &clk);
        CHECK (s.keyOn (67, .8f, g), "in TRANSPOSE il tasto non suona");
        auto ev = run (s, g, stepLen * 2, bpm, false, 0, 256, &clk);
        bool t55 = false;
        for (auto& e : ev) if (e.vel > 0 && e.note == 55) t55 = true;
        CHECK (t55, "G3 traspone di +7: C2 diventa G2");
        CHECK (s.keyOff (67, g), "anche il rilascio è consumato");
    }

    std::printf ("6. registrazione passo passo\n");
    {
        Sequencer s; s.prepare (sr);
        auto g = defaults();
        g.v[SEQREC] = 1;
        s.data.clearSlot (0);
        s.recCursor = 0;
        CHECK (! s.keyOn (60, 1.f, g), "la nota registrata si sente");
        s.keyOn (64, .5f, g);
        s.keyOff (60, g);
        CHECK (s.recCursor.load() == 0, "il cursore aspetta che si lascino tutti i tasti");
        s.keyOff (64, g);
        CHECK (s.recCursor.load() == 1, "poi va al passo dopo");
        CHECK (s.data.step[0][0].has (60) && s.data.step[0][0].has (64) && s.data.step[0][0].vel.load() == 127, "accordo e velocity registrati");
        s.keyOn (67, .5f, g); s.keyOff (67, g);
        CHECK (s.data.step[0][1].has (67) && s.recCursor.load() == 2, "secondo passo");
    }

    std::printf ("7. registrazione dal vivo con legatura\n");
    {
        Sequencer s; s.prepare (sr);
        auto g = defaults();
        g.v[SEQON] = 1; g.v[SEQREC] = 1;
        s.data.clearSlot (0);
        s.data.length[0] = 8;
        long long clk = 0;
        run (s, g, stepLen / 2, bpm, false, 0, 256, &clk);        // siamo nel passo 1
        s.keyOn (62, .7f, g);
        auto ev = run (s, g, stepLen * 2, bpm, false, 0, 256, &clk);   // tenuta per i passi 2 e 3
        s.keyOff (62, g);
        CHECK (s.data.step[0][0].has (62) && s.data.step[0][0].tie.load() == 1, "passo 1 scritto e legato");
        CHECK (s.data.step[0][1].has (62) && s.data.step[0][1].tie.load() == 1, "passo 2 legato");
        CHECK (s.data.step[0][2].has (62) && s.data.step[0][2].tie.load() == 0, "passo 3 chiude la legatura");
        CHECK (countOn (ev) == 0, "mentre la si tiene, la nota non viene ribattuta dal sequencer");
        g.v[SEQREC] = 0;
        auto ev2 = run (s, g, stepLen * 8, bpm, false, 0, 256, &clk);
        int on62 = 0;
        for (auto& e : ev2) if (e.vel > 0 && e.note == 62) ++on62;
        CHECK (on62 == 1, "in riproduzione i tre passi legati sono una nota sola");
    }

    std::printf ("8. salvataggio e lettura\n");
    {
        Sequencer a, b;
        a.data.step[3][5].add (70); a.data.step[3][5].prob = 33; a.data.step[3][5].tie = 1; a.data.length[3] = 37;
        b.data.fromValueTree (a.data.toValueTree());
        CHECK (b.data.step[3][5].has (70) && b.data.step[3][5].prob.load() == 33 && b.data.step[3][5].tie.load() == 1 && b.data.length[3].load() == 37,
               "i dati tornano uguali");
        CHECK (b.data.step[0][0].has (36), "la sequenza 1 resta quella salvata");
    }

    std::printf ("9. sintetizzatore completo: suono pulito e nessuna voce appesa\n");
    {
        Synth syn; syn.prepare (sr, 256);
        TimbreValues tv[2];
        for (int k = 0; k < 2; ++k) for (int i = 0; i < TP_COUNT; ++i) tv[k].v[i] = timbreDefs()[(size_t) i].def;
        auto g = defaults();
        g.v[SEQON] = 1;
        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer midi;
        float peak = 0; bool bad = false;
        for (int b = 0; b < (int) (sr * 4 / 256); ++b)
        {
            buf.clear();
            syn.process (buf, midi, tv, g, 0, 0, false);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 256; ++i) { const float x = buf.getSample (c, i); bad = bad || ! std::isfinite (x); peak = std::max (peak, std::abs (x)); }
        }
        CHECK (! bad, "nessun NaN");
        CHECK (peak > .01f && peak < 1.5f, "suona a un livello sensato");
        g.v[SEQON] = 0;
        for (int b = 0; b < (int) (sr * 3 / 256); ++b) { buf.clear(); syn.process (buf, midi, tv, g, 0, 0, false); }
        CHECK (syn.activeVoices() == 0, "dopo lo stop le voci si spengono");
        std::printf ("   picco %.3f\n", peak);
    }

    std::printf (failures == 0 ? "TUTTE LE PROVE SUPERATE\n" : "%d PROVE FALLITE\n", failures);
    return failures == 0 ? 0 : 1;
}
