#include "Sequencer.h"

namespace mumu
{
namespace
{
constexpr double seqBeats[6] = { 1, .5, 1.0 / 3.0, .25, 1.0 / 6.0, .125 };   // come l'arpeggiatore
constexpr double never = 1.0e300;
}

// ================================================================================================ passo
void SeqStep::clear() noexcept
{
    for (auto& n : note) n.store (-1, std::memory_order_relaxed);
    vel = 100; gate = 60; prob = 100; timbre = 0; tie = 0;
}

int SeqStep::count() const noexcept
{
    int c = 0;
    for (auto& n : note) if (n.load (std::memory_order_relaxed) >= 0) ++c;
    return c;
}

bool SeqStep::has (int n) const noexcept
{
    for (auto& x : note) if (x.load (std::memory_order_relaxed) == n) return true;
    return false;
}

bool SeqStep::add (int n) noexcept
{
    if (n < 0 || n > 127) return false;
    if (has (n)) return true;
    for (auto& x : note)
        if (x.load (std::memory_order_relaxed) < 0) { x.store ((int8_t) n, std::memory_order_relaxed); return true; }
    return false;
}

void SeqStep::copyFrom (const SeqStep& o) noexcept
{
    for (int i = 0; i < maxNotes; ++i) note[i].store (o.note[i].load (std::memory_order_relaxed), std::memory_order_relaxed);
    vel = o.vel.load(); gate = o.gate.load(); prob = o.prob.load(); timbre = o.timbre.load(); tie = o.tie.load();
}

void SeqStep::transpose (int semis) noexcept
{
    for (auto& x : note)
    {
        const int v = x.load (std::memory_order_relaxed);
        if (v >= 0) x.store ((int8_t) juce::jlimit (0, 127, v + semis), std::memory_order_relaxed);
    }
}

// ================================================================================================ dati
SeqData::SeqData()
{
    for (auto& l : length) l = 16;
    loadDemo();
}

void SeqData::clearSlot (int slot) noexcept
{
    slot = juce::jlimit (0, numSlots - 1, slot);
    for (auto& s : step[slot]) s.clear();
}

void SeqData::loadDemo() noexcept
{
    // sequenza 1: basso in Do minore con accordo che si lega sugli ultimi passi (pattern originale MBStudio91)
    clearSlot (0);
    length[0] = 16;
    const int bass[16] = { 36, -1, 48, 36, -1, 39, 36, 46, 36, -1, 48, 43, 41, -1, 39, -1 };
    const int vel[16]  = { 120, 0, 92, 100, 0, 104, 96, 88, 120, 0, 92, 100, 104, 0, 96, 0 };
    for (int i = 0; i < 16; ++i)
        if (bass[i] >= 0)
        {
            auto& s = step[0][i];
            s.add (bass[i]);
            s.vel = (uint8_t) vel[i];
            s.gate = (uint8_t) (i % 4 == 0 ? 70 : 45);
        }
    step[0][14].add (63); step[0][14].add (67); step[0][14].tie = 1; step[0][14].gate = 100;
    step[0][15].add (39); step[0][15].add (63); step[0][15].add (67); step[0][15].vel = 90; step[0][15].gate = 95;
    step[0][7].prob = 60;
    step[0][11].prob = 75;
}

juce::ValueTree SeqData::toValueTree() const
{
    juce::ValueTree t ("SEQDATA");
    t.setProperty ("version", 1, nullptr);
    for (int s = 0; s < numSlots; ++s)
    {
        juce::ValueTree st ("SEQ");
        st.setProperty ("slot", s, nullptr);
        st.setProperty ("length", length[s].load(), nullptr);
        juce::StringArray steps;
        for (int i = 0; i < numSteps; ++i)
        {
            const auto& p = step[s][i];
            juce::StringArray notes;
            for (auto& n : p.note) if (n.load() >= 0) notes.add (juce::String ((int) n.load()));
            steps.add (notes.joinIntoString (",") + ";" + juce::String ((int) p.vel.load()) + ";" + juce::String ((int) p.gate.load()) + ";"
                       + juce::String ((int) p.prob.load()) + ";" + juce::String ((int) p.timbre.load()) + ";" + juce::String ((int) p.tie.load()));
        }
        st.setProperty ("steps", steps.joinIntoString ("|"), nullptr);
        t.appendChild (st, nullptr);
    }
    return t;
}

void SeqData::fromValueTree (const juce::ValueTree& t)
{
    if (! t.hasType ("SEQDATA")) return;
    for (int c = 0; c < t.getNumChildren(); ++c)
    {
        const auto st = t.getChild (c);
        const int s = (int) st.getProperty ("slot", -1);
        if (s < 0 || s >= numSlots) continue;
        clearSlot (s);
        length[s] = juce::jlimit (1, numSteps, (int) st.getProperty ("length", 16));
        const auto steps = juce::StringArray::fromTokens (st.getProperty ("steps").toString(), "|", "");
        for (int i = 0; i < juce::jmin (numSteps, steps.size()); ++i)
        {
            const auto f = juce::StringArray::fromTokens (steps[i], ";", "");
            if (f.size() < 6) continue;
            auto& p = step[s][i];
            for (auto& n : juce::StringArray::fromTokens (f[0], ",", "")) if (n.isNotEmpty()) p.add (n.getIntValue());
            p.vel = (uint8_t) juce::jlimit (1, 127, f[1].getIntValue());
            p.gate = (uint8_t) juce::jlimit (5, 100, f[2].getIntValue());
            p.prob = (uint8_t) juce::jlimit (0, 100, f[3].getIntValue());
            p.timbre = (uint8_t) juce::jlimit (0, 3, f[4].getIntValue());
            p.tie = (uint8_t) (f[5].getIntValue() != 0 ? 1 : 0);
        }
    }
}

// ================================================================================================ sequencer
void Sequencer::prepare (double sampleRate)
{
    sr = sampleRate;
    reset();
}

void Sequencer::reset()
{
    sounding.clear();
    keysDown.clear(); transposeKeys.clear(); recChord.clear();
    recLiveStep = -1;
    freeBase = 0; freeSamples = 0; freeBpm = 0; lastPpqEnd = -1;
    lastK = std::numeric_limits<long long>::min();
    wasRunning = false; curStep = -1;
    playStep = -1;
}

double Sequencer::stepBeats (const GlobalValues& gv) const { return seqBeats[juce::jlimit (0, 5, gv.i (SEQRATE))]; }

double Sequencer::stepTime (long long k, double beats, float swing) const
{
    // swing: il passo dispari arriva in ritardo, (2S-1) passi; S = 0,5 dritto, 0,75 = tre a uno
    const double off = (k & 1) ? (2.0 * juce::jlimit (.5, .75, (double) swing) - 1.0) * beats : 0.0;
    return (double) k * beats + off;
}

bool Sequencer::keyOn (int note, float vel, const GlobalValues& gv)
{
    const bool running = gv.b (SEQON), rec = gv.b (SEQREC);
    keysDown.push_back (note);
    const int slot = juce::jlimit (0, SeqData::numSlots - 1, gv.i (SEQSLOT));
    if (rec)
    {
        const bool first = recChord.empty();
        recChord.push_back ({ note, vel });
        int target;
        if (running)
        {
            if (first) recLiveStep = curStep >= 0 ? curStep : 0;
            target = recLiveStep;
        }
        else target = juce::jlimit (0, SeqData::numSteps - 1, recCursor.load());
        auto& s = data.step[slot][target];
        if (first) { s.clear(); s.vel = (uint8_t) juce::jlimit (1, 127, (int) std::lround (vel * 127.f)); }
        s.add (note);
        ++editRevision;
        return false;                              // la nota registrata si sente
    }
    if (running && gv.i (SEQKEY) == 1)
    {
        transposeKeys.push_back (note);
        transpose = note - 60;
        transposeShown = transpose;
        return true;                               // in TRANSPOSE il tasto non suona, sposta la sequenza
    }
    return false;
}

bool Sequencer::keyOff (int note, const GlobalValues& gv)
{
    keysDown.erase (std::remove (keysDown.begin(), keysDown.end(), note), keysDown.end());
    const auto before = recChord.size();
    recChord.erase (std::remove_if (recChord.begin(), recChord.end(), [note] (auto& p) { return p.first == note; }), recChord.end());
    if (before > 0 && recChord.empty())
    {
        if (gv.b (SEQON)) recLiveStep = -1;
        else
        {
            const int slot = juce::jlimit (0, SeqData::numSlots - 1, gv.i (SEQSLOT));
            const int len = juce::jlimit (1, SeqData::numSteps, data.length[slot].load());
            recCursor = (recCursor.load() + 1) % len;
            ++editRevision;
        }
    }
    const auto it = std::find (transposeKeys.begin(), transposeKeys.end(), note);
    if (it != transposeKeys.end())
    {
        transposeKeys.erase (it);
        return true;                               // anche il rilascio di un tasto di trasposizione è consumato
    }
    return false;
}

void Sequencer::stopAll (std::vector<Ev>& out, int offset)
{
    for (auto& s : sounding) out.push_back ({ offset, s.note, 0.f, s.mask });
    sounding.clear();
}

void Sequencer::recordLiveAdvance (int newStep)
{
    if (recChord.empty() || recLiveStep < 0 || newStep == recLiveStep) return;
    const int slot = curSlot;
    auto& prev = data.step[slot][recLiveStep];
    auto& next = data.step[slot][newStep];
    prev.tie = 1; prev.gate = 100;
    next.clear();
    next.vel = prev.vel.load();
    for (auto& p : recChord) next.add (p.first);
    recLiveStep = newStep;
    ++editRevision;
}

void Sequencer::trigger (long long k, int offset, double ppqAt, double beats, const GlobalValues& gv, std::vector<Ev>& out)
{
    const int len = juce::jlimit (1, SeqData::numSteps, data.length[curSlot].load());
    const int s = (int) (((k % len) + len) % len);
    if (gv.b (SEQREC)) recordLiveAdvance (s);
    curStep = s;
    playStep = s;
    const auto& st = data.step[curSlot][s];

    rnd = rnd * 1664525u + 1013904223u;
    const bool plays = st.count() > 0 && (int) ((rnd >> 8) % 100u) < (int) st.prob.load();
    const int mask = (int) st.timbre.load();
    const bool tieOut = st.tie.load() != 0;
    const double offAt = tieOut ? never : ppqAt + beats * juce::jlimit (5, 100, (int) st.gate.load()) * .01;

    // note legate dal passo prima: continuano se il passo le ripete, altrimenti si chiudono qui
    std::vector<int> continued;
    for (auto it = sounding.begin(); it != sounding.end();)
    {
        const int src = it->note - transpose;
        if (it->tieWait && plays && st.has (src) && it->mask == mask)
        {
            it->tieWait = tieOut;
            it->offPpq = offAt;
            continued.push_back (src);
            ++it;
        }
        else if (it->tieWait || (plays && st.has (it->note - transpose)))
        {
            out.push_back ({ offset, it->note, 0.f, it->mask });
            it = sounding.erase (it);
        }
        else ++it;
    }
    if (! plays) return;

    const float v = (float) st.vel.load() / 127.f;
    for (auto& n : st.note)
    {
        const int src = n.load (std::memory_order_relaxed);
        if (src < 0 || std::find (continued.begin(), continued.end(), src) != continued.end()) continue;
        // in registrazione dal vivo le note tenute a mano suonano già dalla tastiera
        if (std::find_if (recChord.begin(), recChord.end(), [src] (auto& p) { return p.first == src; }) != recChord.end()) continue;
        const int nn = juce::jlimit (0, 127, src + transpose);
        out.push_back ({ offset, nn, v, mask });
        sounding.push_back ({ nn, mask, offAt, tieOut });
    }
}

void Sequencer::process (int n, const GlobalValues& gv, double bpm, double ppqStart, bool hostPlaying, std::vector<Ev>& out)
{
    const bool running = gv.b (SEQON);
    if (! gv.b (SEQREC)) { recChord.clear(); recLiveStep = -1; }
    curSlot = juce::jlimit (0, SeqData::numSlots - 1, gv.i (SEQSLOT));
    if (! running)
    {
        if (wasRunning) { stopAll (out, 0); playStep = -1; curStep = -1; }
        wasRunning = false;
        recLiveStep = -1;
        lastK = std::numeric_limits<long long>::min();
        return;
    }

    const double beats = stepBeats (gv);
    const double ppqPerSample = std::max (20.0, bpm) / 60.0 / sr;
    double p0;
    if (hostPlaying)
    {
        p0 = ppqStart;
        // salto della posizione (loop, riavvolgimento): si riparte dal passo giusto senza note appese
        if (wasRunning && wasHost && std::abs (p0 - lastPpqEnd) > ppqPerSample * 4.0)
        {
            stopAll (out, 0);
            lastK = std::numeric_limits<long long>::min();
        }
    }
    else
    {
        // tempo libero: posizione contata in campioni interi, così non si accumula errore
        if (! wasRunning || wasHost) { freeBase = 0; freeSamples = 0; freeBpm = bpm; stopAll (out, 0); lastK = std::numeric_limits<long long>::min(); }
        if (bpm != freeBpm) { freeBase += (double) freeSamples * std::max (20.0, freeBpm) / 60.0 / sr; freeSamples = 0; freeBpm = bpm; }
        p0 = freeBase + (double) freeSamples * ppqPerSample;
    }
    if (! wasRunning && hostPlaying) lastK = std::numeric_limits<long long>::min();
    const double p1 = p0 + n * ppqPerSample;
    const float swing = gv[SEQSWING];

    auto toOffset = [&] (double ppq) { return juce::jlimit (0, n - 1, (int) std::llround ((ppq - p0) / ppqPerSample)); };
    auto offsUntil = [&] (double limit)
    {
        for (auto it = sounding.begin(); it != sounding.end();)
            if (! it->tieWait && it->offPpq < limit) { out.push_back ({ toOffset (std::max (p0, it->offPpq)), it->note, 0.f, it->mask }); it = sounding.erase (it); }
            else ++it;
    };

    const long long kFirst = (long long) std::floor (p0 / beats) - 1, kLast = (long long) std::floor (p1 / beats) + 1;
    for (long long k = kFirst; k <= kLast; ++k)
    {
        if (k <= lastK) continue;
        const double t = stepTime (k, beats, swing);
        constexpr double eps = 1.0e-9;          // un passo sul confine del blocco va al blocco dopo
        if (t < p0 - eps) continue;
        if (t >= p1 - eps) break;
        offsUntil (t);
        trigger (k, toOffset (t), t, beats, gv, out);
        lastK = k;
    }
    offsUntil (p1);

    lastPpqEnd = p1;
    if (! hostPlaying) freeSamples += n;
    wasRunning = true;
    wasHost = hostPlaying;
}
}
