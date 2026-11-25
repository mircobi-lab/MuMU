#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <array>
#include <vector>

/*  Memoria degli accordi: con CHORD acceso ogni tasto suona l'accordo memorizzato, trasposto su quel tasto
    (la nota più bassa dell'accordo cade sul tasto premuto). LEARN: si tengono premute le note dell'accordo,
    quando si lasciano tutti i tasti l'accordo è memorizzato e CHORD si accende.
    Gli intervalli sono atomici: l'interfaccia li legge, il thread audio li scrive in LEARN. */
namespace mumu
{
struct ChordMemory
{
    static constexpr int maxNotes = 8;
    std::array<std::atomic<int8_t>, maxNotes> interval {};      // semitoni sopra la nota più bassa; il primo è 0
    std::atomic<int> count { 0 };
    std::atomic<bool> learn { false }, learnedNow { false };    // learnedNow: l'interfaccia accende CHORD

    // chiamata dal thread audio per ogni tasto quando LEARN è attivo
    void learnNote (int note, bool on)
    {
        if (on) { if (std::find (held.begin(), held.end(), note) == held.end()) { held.push_back (note); gathered.push_back (note); } return; }
        held.erase (std::remove (held.begin(), held.end(), note), held.end());
        if (! held.empty() || gathered.empty()) return;
        std::sort (gathered.begin(), gathered.end());
        gathered.erase (std::unique (gathered.begin(), gathered.end()), gathered.end());
        const int n = std::min ((int) gathered.size(), maxNotes);
        for (int i = 0; i < n; ++i) interval[(size_t) i].store ((int8_t) juce::jlimit (0, 127, gathered[(size_t) i] - gathered[0]));
        count.store (n);
        gathered.clear();
        learn.store (false);
        learnedNow.store (true);
    }
    void resetLearn() { held.clear(); gathered.clear(); }

    // note da suonare per un tasto (il tasto stesso se la memoria è vuota)
    int expand (int root, int* out) const
    {
        const int n = count.load();
        if (n < 2) { out[0] = root; return 1; }
        int k = 0;
        for (int i = 0; i < n; ++i)
        {
            const int v = root + interval[(size_t) i].load();
            if (v <= 127) out[k++] = v;
        }
        return k;
    }

    juce::String toString() const
    {
        juce::StringArray a;
        for (int i = 0; i < count.load(); ++i) a.add (juce::String ((int) interval[(size_t) i].load()));
        return a.joinIntoString (" ");
    }
    void fromString (const juce::String& s)
    {
        auto a = juce::StringArray::fromTokens (s, " ", "");
        a.removeEmptyStrings();
        const int n = std::min (a.size(), maxNotes);
        for (int i = 0; i < n; ++i) interval[(size_t) i].store ((int8_t) juce::jlimit (0, 127, a[i].getIntValue()));
        count.store (n);
    }
    juce::String describe() const
    {
        if (count.load() < 2) return "NESSUN ACCORDO";
        juce::String t;
        for (int i = 0; i < count.load(); ++i) t << (i ? " " : "") << juce::MidiMessage::getMidiNoteName (60 + interval[(size_t) i].load(), true, false, 3);
        return t;   // es. "C E G" sul Do
    }

private:
    std::vector<int> held, gathered;   // solo thread audio
};
}
