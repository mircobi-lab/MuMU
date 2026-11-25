#pragma once
#include "Params.h"

/*  Sequencer a 64 passi di MuMU, sul modello di quello del Moog Muse:
    - 8 sequenze indipendenti dalla patch (cambiare preset non le tocca), salvate col progetto;
    - ogni passo ha fino a 6 note (accordo), velocity, gate, probabilità, legatura al passo dopo
      e il timbro che suona (quello del KEY MODE, solo A, solo B, A+B);
    - lunghezza da 1 a 64, 4 pagine da 16 passi, swing, valore del passo come l'arpeggiatore;
    - REC con il sequencer in moto: la nota o l'accordo suonato sostituisce il passo acceso,
      tenendo premuto si lega sui passi successivi; REC da fermo: registrazione passo passo;
    - KEY TRANSPOSE: in riproduzione il tasto suonato traspone la sequenza rispetto a C3.
    I dati dei passi sono atomici: l'interfaccia li scrive, il thread audio li legge senza lock. */
namespace mumu
{
struct SeqStep
{
    static constexpr int maxNotes = 6;
    std::atomic<int8_t> note[maxNotes];
    std::atomic<uint8_t> vel { 100 }, gate { 60 }, prob { 100 }, timbre { 0 }, tie { 0 };

    SeqStep() { clear(); }
    void clear() noexcept;
    int count() const noexcept;
    bool has (int n) const noexcept;
    bool add (int n) noexcept;                    // false se il passo è pieno
    void copyFrom (const SeqStep&) noexcept;
    void transpose (int semis) noexcept;
};

struct SeqData
{
    static constexpr int numSlots = 8, numSteps = 64;
    SeqStep step[numSlots][numSteps];
    std::atomic<int> length[numSlots];
    // catena: fino a 16 sequenze suonate una dopo l'altra, ognuna per tutta la sua lunghezza
    static constexpr int maxChain = 16;
    std::atomic<int> chain[maxChain] {};
    std::atomic<int> chainLen { 0 };
    bool chainAppend (int slot) noexcept;

    SeqData();
    void clearSlot (int slot) noexcept;
    void loadDemo() noexcept;                     // pattern di partenza nella sequenza 1
    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree&);
};

class Sequencer
{
public:
    SeqData data;
    std::atomic<int> playStep { -1 };             // passo che sta suonando (per i LED), -1 = fermo
    std::atomic<int> recCursor { 0 };             // passo su cui scrive la registrazione passo passo
    std::atomic<int> transposeShown { 0 };
    std::atomic<int> editRevision { 0 };          // cresce quando la registrazione cambia un passo
    std::atomic<int> chainPos { -1 };             // voce della catena che suona, -1 = catena spenta
    std::atomic<int> playSlot { 0 };              // sequenza che suona (con la catena può non essere SEQUENCE)

    struct Ev { int offset, note; float vel; int mask; };     // vel 0 = note-off; mask 0 = segue KEY MODE, 1 A, 2 B, 3 A+B

    void prepare (double sampleRate);
    void reset();
    // tasti suonati mentre il sequencer è attivo. Restituisce true se la nota va consumata (non suonata)
    bool keyOn (int note, float vel, const GlobalValues&);
    bool keyOff (int note, const GlobalValues&);
    void process (int n, const GlobalValues&, double bpm, double ppqStart, bool hostPlaying, std::vector<Ev>& out);
    void stopAll (std::vector<Ev>& out, int offset);

private:
    double sr = 48000, freeBase = 0, freeBpm = 0, lastPpqEnd = -1;
    long long freeSamples = 0;
    long long lastK = std::numeric_limits<long long>::min();
    bool wasRunning = false, wasHost = false;
    int transpose = 0, curStep = -1, curSlot = 0;
    int chainIdx = 0;
    long long chainStartK = std::numeric_limits<long long>::min();
    bool chainOn = false;
    std::vector<int> keysDown, transposeKeys;
    // registrazione
    std::vector<std::pair<int, float>> recChord;  // note tenute durante la registrazione
    int recLiveStep = -1;
    bool recStepFresh = true;
    uint32_t rnd = 0x5eed1234u;

    struct Sounding { int note, mask; double offPpq; bool tieWait; };
    std::vector<Sounding> sounding;

    double stepBeats (const GlobalValues&) const;
    double stepTime (long long k, double beats, float swing) const;
    void trigger (long long k, int offset, double ppqAt, double beats, const GlobalValues&, std::vector<Ev>&);
    void recordLiveAdvance (int newStep);
};
}
