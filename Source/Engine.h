#pragma once
#include "Params.h"
#include "Sequencer.h"
#include "Chord.h"

/*  Motore audio di MBStudio91 MuMU.
    Struttura ispirata all'architettura del Moog Muse descritta nella recensione di Sound On Sound:
    8 voci, due timbri (A/B) singoli, in layer o in split; per voce due VCO a forma d'onda continua
    (triangolo > dente di sega > quadra > impulso), sync e FM bidirezionale, oscillatore di modulazione
    (audio o LFO), ring modulator, rumore bianco, mixer saturante con offset (clip asimmetrico),
    due filtri ladder a 4 poli (F1 LP/HP, F2 LP) in serie, parallelo o stereo con collegamento,
    inviluppi ADSR digitali con curva e loop, due LFO + LFO di pitch, matrice di modulazione a 8 slot,
    glide a 3 modi, arpeggiatore e delay a diffusione stereo.                                            */
namespace mumu
{
namespace dsp
{
struct Rng
{
    uint32_t s = 0x9e3779b9u;
    float next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s * (1.f / 2147483648.f); }
    float uni() noexcept { return next() * .5f + .5f; }
};

// oscillatore a forma d'onda continua con correzione PolyBLEP
struct VCO
{
    double phase = 0;
    float tick (double inc, float wave, bool& wrapped, double& frac) noexcept;
};

// oscillatore di modulazione: SINE TRI SAW SQUARE S&H
struct ModOsc
{
    double phase = 0;
    float held = 0;
    Rng rng;
    float tick (double inc, int shape) noexcept;
};

// filtro ladder 4 poli a zero ritardo (TPT) con saturazione nel ritorno
struct Ladder
{
    float s[4] {};
    float process (float x, float g, float k, bool highPass) noexcept;
    void reset() noexcept { s[0] = s[1] = s[2] = s[3] = 0; }
};

// ADSR con curva regolabile, loop, attacco dal valore corrente
struct Envelope
{
    enum Stage { idle, attack, decay, sustain, release };
    Stage stage = idle;
    float out = 0;
    void set (float a, float d, float s, float r, float curve, bool loop, double sr) noexcept;
    void gateOn (bool fromZero) noexcept { if (fromZero) out = 0; stage = attack; }
    void gateOff() noexcept { if (stage != idle) stage = release; }
    void kill() noexcept { stage = idle; out = 0; }
    float tick() noexcept;
    bool active() const noexcept { return stage != idle; }
private:
    float aCoef = 0, aBase = 0, dCoef = 0, dBase = 0, rCoef = 0, rBase = 0, sus = 1;
    bool looping = false;
};

struct LFO
{
    double phase = 0;
    float a = 0, b = 0, held = 0;
    Rng rng;
    float advance (double inc, int shape) noexcept;   // avanza di inc e restituisce il valore (-1..1)
};

struct DelayLine
{
    std::vector<float> buf;
    int w = 0, mask = 0;
    void init (int minSize);
    void clear() { std::fill (buf.begin(), buf.end(), 0.f); }
    void write (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float read (float delay) const noexcept;   // delay in campioni (>= 1), interpolazione cubica
};

struct Allpass
{
    DelayLine line;
    float process (float x, float g, float len) noexcept
    {
        const float d = line.read (len);
        const float v = x + g * d;
        line.write (v);
        return d - g * v;
    }
};

// delay stereo con diffusione: 8 allpass di pre-diffusione + 2 linee + 8 allpass nel ritorno, modulati da 6 LFO
class DiffusionDelay
{
public:
    void prepare (double sr);
    void reset();
    void process (float* L, float* R, int n, const GlobalValues&, double bpm) noexcept;
private:
    double sr = 48000;
    Allpass pre[2][4], loop[2][3];
    DelayLine main[2];
    float lp[2] {}, fb[2] {}, timeSm = 0, mixSm = 0;
    double lfoPh[6] {};
    bool wasOn = false;
};
}

// ------------------------------------------------------------------------------------------------ voce
struct VoiceContext
{
    double sr = 48000, bpm = 120;
    float wheel = 0, aftertouch = 0, bend = 0, macro = 0, tuneSemis = 0;
};

class Voice
{
public:
    bool active = false, gate = false, sustained = false;
    int timbre = 0, note = 60, index = 0;
    float vel = 1, rnd = 0, detune = 0, panPos = 0;
    uint64_t age = 0;

    void start (int timbre, int note, float vel, const TimbreValues&, const VoiceContext&,
                double globalLfoPhase0, double globalLfoPhase1, float glideFromPitch);
    void moveTo (int note, float vel, const TimbreValues&, bool retriggerEnv);   // legato mono/unisono
    void release() { gate = false; fenv.gateOff(); aenv.gateOff(); }
    void kill() { active = false; gate = false; fenv.kill(); aenv.kill(); }
    void updateEnvelopes (const TimbreValues&, double sr);
    void render (float* L, float* R, int n, const TimbreValues&, const VoiceContext&) noexcept;
    float ampLevel() const { return aenv.out; }
    float currentPitch() const { return pitchCur; }

private:
    dsp::VCO o1, o2;
    dsp::ModOsc mo;
    dsp::LFO lfo[2];
    dsp::Envelope fenv, aenv;
    dsp::Ladder f1, f2;
    double sr = 48000;
    float lfoRateMul[2] { 1.f, 1.f };
    dsp::Rng noise;
    double plPhase = 0;
    float pitchCur = 60, pitchTarget = 60, glideStep = 0;
    float prevO1 = 0, prevO2 = 0, moLast = 0, dcX = 0, dcY = 0;
    float lfoVal[2] {};
    void setGlideTarget (float target, const TimbreValues&);
};

// ------------------------------------------------------------------------------------------------ arpeggiatore
class Arp
{
public:
    void reset();
    void noteOn (int note, float vel, bool latch);
    void noteOff (int note, bool latch);
    void latchReleased();
    bool hasNotes() const { return ! held.empty(); }
    // produce eventi noteOn/noteOff (nella forma: offset, nota, velocity; vel 0 = off)
    struct Ev { int offset, note; float vel; };
    void process (int n, const GlobalValues&, double sr, double bpm, double ppqStart, bool hostPlaying, std::vector<Ev>& out);
    void stopAll (std::vector<Ev>& out, int offset);
private:
    std::vector<std::pair<int, float>> held;   // in ordine di pressione
    std::vector<int> down;                     // tasti fisicamente premuti
    std::vector<int> seq;
    int step = -1, dir = 1, playing = -1;
    double samplesToNext = 0, samplesToOff = -1, lastStepIndex = -1;
    float lastVel = .8f;
    uint32_t rnd = 12345;
    void buildSequence (const GlobalValues&);
    int nextNote (const GlobalValues&);
};

// ------------------------------------------------------------------------------------------------ sintetizzatore
class Synth
{
public:
    static constexpr int numVoices = 8;

    void prepare (double sampleRate, int maxBlock);
    void process (juce::AudioBuffer<float>&, const juce::MidiBuffer&, const TimbreValues (&t)[2], const GlobalValues&,
                  double hostBpm, double ppq, bool hostPlaying);
    int activeVoices() const;
    float getOutputLevel() const { return outLevel.load(); }
    void allNotesOff();
    void panic();        // MIDI PANIC: voci spente subito, arpeggiatore, sequencer, pedale e controller azzerati, delay svuotato
    Sequencer seq;       // sequencer a 64 passi (dati letti e scritti anche dall'interfaccia)
    ChordMemory chord;   // memoria degli accordi
    std::array<std::array<int8_t, ChordMemory::maxNotes + 1>, 128> chordPlayed {};   // note suonate per ogni tasto (per i note-off)

private:
    double sr = 48000;
    Voice voices[numVoices];
    VoiceContext ctx;
    Arp arp;
    dsp::DiffusionDelay delay;
    std::vector<float> busL, busR;
    std::vector<Arp::Ev> arpEvents;
    std::vector<Sequencer::Ev> seqEvents;

    struct TimbreState { std::vector<std::pair<int, float>> stack; int mode = -1; float lastPitch = -1; };
    TimbreState ts[2];
    uint8_t noteMask[128] {};
    bool sustainPedal = false, arpWasOn = false;
    uint64_t counter = 0;
    double lfoGlobal[2][2] {};
    float hpPrevL = 0, hpPrevR = 0, hpL = 0, hpR = 0, gainSm = 0;
    std::atomic<float> outLevel { 0 };
    bool latchWasOn = false;

    void handleMidi (const juce::MidiMessage&, const TimbreValues (&t)[2], const GlobalValues&, int forceMask = 0);
    void noteOn (int note, float vel, const TimbreValues (&t)[2], const GlobalValues&, int forceMask = 0);
    void noteOff (int note, const TimbreValues (&t)[2]);
    void sustainOff();
    void timbreNoteOn (int tm, int note, float vel, const TimbreValues&);
    void timbreNoteOff (int tm, int note, const TimbreValues&);
    Voice* findVoice (uint32_t excludeMask);
    void renderVoices (int start, int n, const TimbreValues (&t)[2]);
    double lfoRateHz (const TimbreValues&, int which) const;
};
}
