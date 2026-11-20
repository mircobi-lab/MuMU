#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

/*  MBStudio91 MuMU — mappa dei parametri.
    Ogni patch ha due TIMBRI (A e B) con gli stessi parametri (prefisso "A_" / "B_") e una parte GLOBALE ("G_").
    Le liste X() sotto sono l'unica fonte: enum, tabella dei parametri e lettura nel motore derivano da qui.

    X(enum, chiave, nome lungo, etichetta pannello, tipo, min, max, default, centro-scala, scelte, unità)
    tipo: R reale, I intero, C scelta, T interruttore.   centro-scala > 0: valore a metà corsa (scala logaritmica).   */

#define MUMU_TIMBRE_PARAMS(X) \
    X (VMODE,    "VMODE",   "Voice Mode",        "MODE",     C, 0, 2, 0, 0,   "POLY|MONO|UNISON", "") \
    X (UNIVOX,   "UNIVOX",  "Unison Voices",     "VOICES",   I, 2, 8, 4, 0,   "", "") \
    X (UNIDET,   "UNIDET",  "Unison Detune",     "DETUNE",   R, 0, 1, .25f, 0, "", "%") \
    X (MTRIG,    "MTRIG",   "Mono Trigger",      "TRIGGER",  C, 0, 1, 0, 0,   "LEGATO|RETRIG", "") \
    X (GLIDE,    "GLIDE",   "Glide Time",        "GLIDE",    R, 0, 5, 0, .25f, "", "s") \
    X (GLMODE,   "GLMODE",  "Glide Mode",        "TYPE",     C, 0, 2, 0, 0,   "EXP|RATE|TIME", "") \
    X (BEND,     "BEND",    "Bend Range",        "BEND",     I, 0, 12, 2, 0,  "", "st") \
    X (OCT,      "OCT",     "Timbre Octave",     "OCTAVE",   I, -2, 2, 0, 0,  "", "") \
    X (O1OCT,    "O1OCT",   "Osc 1 Octave",      "OCTAVE",   C, 0, 5, 2, 0,   "32'|16'|8'|4'|2'|1'", "") \
    X (O1WAVE,   "O1WAVE",  "Osc 1 Wave",        "WAVE",     R, 0, 1, .34f, 0, "", "wave") \
    X (O2OCT,    "O2OCT",   "Osc 2 Octave",      "OCTAVE",   C, 0, 5, 2, 0,   "32'|16'|8'|4'|2'|1'", "") \
    X (O2SEMI,   "O2SEMI",  "Osc 2 Semitone",    "SEMI",     I, -12, 12, 0, 0, "", "st") \
    X (O2FINE,   "O2FINE",  "Osc 2 Fine",        "FINE",     R, -50, 50, 4, 0, "", "ct") \
    X (O2WAVE,   "O2WAVE",  "Osc 2 Wave",        "WAVE",     R, 0, 1, .34f, 0, "", "wave") \
    X (SYNC,     "SYNC",    "Osc 2 Hard Sync",   "SYNC",     T, 0, 1, 0, 0,   "", "") \
    X (FM,       "FM",      "Osc FM 1<>2",       "FM 1<>2",  R, -1, 1, 0, 0,  "", "%") \
    X (MOWAVE,   "MOWAVE",  "Mod Osc Wave",      "WAVE",     C, 0, 4, 1, 0,   "SINE|TRI|SAW|SQUARE|S&H", "") \
    X (MOTRACK,  "MOTRACK", "Mod Osc Tracking",  "TRACK",    C, 0, 1, 1, 0,   "KEY|FREE", "") \
    X (MOPITCH,  "MOPITCH", "Mod Osc Pitch",     "PITCH",    R, -24, 24, 0, 0, "", "st") \
    X (MORATE,   "MORATE",  "Mod Osc Rate",      "RATE",     R, .05f, 1000, 5, 8, "", "Hz") \
    X (MOAPITCH, "MOAPITCH","Mod Osc > Pitch",   "> PITCH",  R, 0, 1, 0, .25f, "", "%") \
    X (MOACUT,   "MOACUT",  "Mod Osc > Cutoff",  "> CUTOFF", R, 0, 1, 0, 0,   "", "%") \
    X (MOAWAVE,  "MOAWAVE", "Mod Osc > Wave",    "> WAVE",   R, 0, 1, 0, 0,   "", "%") \
    X (MOAAMP,   "MOAAMP",  "Mod Osc > Amp",     "> AMP",    R, 0, 1, 0, 0,   "", "%") \
    X (MIX1,     "MIX1",    "Mixer Osc 1",       "OSC 1",    R, 0, 1, .8f, 0, "", "%") \
    X (MIX2,     "MIX2",    "Mixer Osc 2",       "OSC 2",    R, 0, 1, .6f, 0, "", "%") \
    X (MIXMO,    "MIXMO",   "Mixer Mod Osc",     "MOD OSC",  R, 0, 1, 0, 0,   "", "%") \
    X (MIXRING,  "MIXRING", "Mixer Ring Mod",    "RING",     R, 0, 1, 0, 0,   "", "%") \
    X (MIXNOISE, "MIXNOISE","Mixer Noise",       "NOISE",    R, 0, 1, 0, 0,   "", "%") \
    X (DRIVE,    "DRIVE",   "Mixer Drive",       "DRIVE",    R, 0, 1, .3f, 0, "", "%") \
    X (OFFSET,   "OFFSET",  "Mixer Offset",      "OFFSET",   R, 0, 1, 0, 0,   "", "%") \
    X (F1CUT,    "F1CUT",   "Filter 1 Cutoff",   "CUTOFF",   R, 20, 20000, 1800, 700, "", "Hz") \
    X (F1RES,    "F1RES",   "Filter 1 Resonance","RESO",     R, 0, 1, .15f, 0, "", "%") \
    X (F1MODE,   "F1MODE",  "Filter 1 Mode",     "MODE",     C, 0, 1, 0, 0,   "LP|HP", "") \
    X (F2CUT,    "F2CUT",   "Filter 2 Cutoff",   "CUTOFF",   R, 20, 20000, 20000, 700, "", "Hz") \
    X (F2RES,    "F2RES",   "Filter 2 Resonance","RESO",     R, 0, 1, 0, 0,   "", "%") \
    X (FROUTE,   "FROUTE",  "Filter Routing",    "ROUTING",  C, 0, 2, 0, 0,   "SERIAL|PARALLEL|STEREO", "") \
    X (FLINK,    "FLINK",   "Filter Link",       "LINK",     T, 0, 1, 0, 0,   "", "") \
    X (FSPACE,   "FSPACE",  "Filter Spacing",    "SPACING",  R, -4, 4, 0, 0,  "", "oct") \
    X (KEYTRK,   "KEYTRK",  "Filter Key Track",  "KB TRACK", R, 0, 1.5f, .5f, 0, "", "%") \
    X (FENVAMT,  "FENVAMT", "Filter Env Amount", "ENV AMT",  R, -1, 1, .35f, 0, "", "%") \
    X (FVELO,    "FVELO",   "Filter Env Velocity","VELO",    R, 0, 1, .25f, 0, "", "%") \
    X (FA,       "FA",      "Filter Env Attack", "ATTACK",   R, .001f, 20, .005f, .5f, "", "s") \
    X (FD,       "FD",      "Filter Env Decay",  "DECAY",    R, .001f, 20, .6f, .5f, "", "s") \
    X (FS,       "FS",      "Filter Env Sustain","SUSTAIN",  R, 0, 1, .4f, 0, "", "%") \
    X (FR,       "FR",      "Filter Env Release","RELEASE",  R, .001f, 20, .5f, .5f, "", "s") \
    X (FCURVE,   "FCURVE",  "Filter Env Curve",  "CURVE",    R, -1, 1, 0, 0,  "", "%") \
    X (FLOOP,    "FLOOP",   "Filter Env Loop",   "LOOP",     T, 0, 1, 0, 0,   "", "") \
    X (AA,       "AA",      "Amp Env Attack",    "ATTACK",   R, .001f, 20, .003f, .5f, "", "s") \
    X (AD,       "AD",      "Amp Env Decay",     "DECAY",    R, .001f, 20, .4f, .5f, "", "s") \
    X (AS,       "AS",      "Amp Env Sustain",   "SUSTAIN",  R, 0, 1, .85f, 0, "", "%") \
    X (AR,       "AR",      "Amp Env Release",   "RELEASE",  R, .001f, 20, .35f, .5f, "", "s") \
    X (ACURVE,   "ACURVE",  "Amp Env Curve",     "CURVE",    R, -1, 1, 0, 0,  "", "%") \
    X (ALOOP,    "ALOOP",   "Amp Env Loop",      "LOOP",     T, 0, 1, 0, 0,   "", "") \
    X (AVELO,    "AVELO",   "Amp Velocity",      "VELO",     R, 0, 1, .5f, 0, "", "%") \
    X (L1WAVE,   "L1WAVE",  "LFO 1 Wave",        "WAVE",     C, 0, 6, 1, 0,   "SINE|TRI|SAW|RAMP|SQUARE|S&H|SMOOTH", "") \
    X (L1RATE,   "L1RATE",  "LFO 1 Rate",        "RATE",     R, .01f, 1000, 3, 4, "", "Hz") \
    X (L1SYNC,   "L1SYNC",  "LFO 1 Clock",       "CLOCK",    C, 0, 11, 0, 0,  "FREE|4/1|2/1|1/1|1/2|1/4|1/4T|1/8|1/8T|1/16|1/16T|1/32", "") \
    X (L1KEY,    "L1KEY",   "LFO 1 Key Sync",    "KEY SYNC", T, 0, 1, 0, 0,   "", "") \
    X (L1UNI,    "L1UNI",   "LFO 1 Unipolar",    "UNIPOLAR", T, 0, 1, 0, 0,   "", "") \
    X (L2WAVE,   "L2WAVE",  "LFO 2 Wave",        "WAVE",     C, 0, 6, 0, 0,   "SINE|TRI|SAW|RAMP|SQUARE|S&H|SMOOTH", "") \
    X (L2RATE,   "L2RATE",  "LFO 2 Rate",        "RATE",     R, .01f, 1000, .4f, 4, "", "Hz") \
    X (L2SYNC,   "L2SYNC",  "LFO 2 Clock",       "CLOCK",    C, 0, 11, 0, 0,  "FREE|4/1|2/1|1/1|1/2|1/4|1/4T|1/8|1/8T|1/16|1/16T|1/32", "") \
    X (L2KEY,    "L2KEY",   "LFO 2 Key Sync",    "KEY SYNC", T, 0, 1, 0, 0,   "", "") \
    X (L2UNI,    "L2UNI",   "LFO 2 Unipolar",    "UNIPOLAR", T, 0, 1, 0, 0,   "", "") \
    X (PLRATE,   "PLRATE",  "Pitch LFO Rate",    "RATE",     R, .1f, 20, 5.5f, 5, "", "Hz") \
    X (PLAMT,    "PLAMT",   "Pitch LFO Amount",  "AMOUNT",   R, 0, 1, 0, 0,   "", "%") \
    X (PLWHEEL,  "PLWHEEL", "Pitch LFO Wheel",   "WHEEL",    R, 0, 1, .3f, 0, "", "%") \
    X (M1SRC, "M1SRC", "Mod 1 Source", "SOURCE", C, 0, 12, 0, 0, MUMU_SRC, "") X (M1DST, "M1DST", "Mod 1 Destination", "DEST", C, 0, 21, 0, 0, MUMU_DST, "") X (M1AMT, "M1AMT", "Mod 1 Amount", "AMOUNT", R, -1, 1, 0, 0, "", "%") \
    X (M2SRC, "M2SRC", "Mod 2 Source", "SOURCE", C, 0, 12, 0, 0, MUMU_SRC, "") X (M2DST, "M2DST", "Mod 2 Destination", "DEST", C, 0, 21, 0, 0, MUMU_DST, "") X (M2AMT, "M2AMT", "Mod 2 Amount", "AMOUNT", R, -1, 1, 0, 0, "", "%") \
    X (M3SRC, "M3SRC", "Mod 3 Source", "SOURCE", C, 0, 12, 0, 0, MUMU_SRC, "") X (M3DST, "M3DST", "Mod 3 Destination", "DEST", C, 0, 21, 0, 0, MUMU_DST, "") X (M3AMT, "M3AMT", "Mod 3 Amount", "AMOUNT", R, -1, 1, 0, 0, "", "%") \
    X (M4SRC, "M4SRC", "Mod 4 Source", "SOURCE", C, 0, 12, 0, 0, MUMU_SRC, "") X (M4DST, "M4DST", "Mod 4 Destination", "DEST", C, 0, 21, 0, 0, MUMU_DST, "") X (M4AMT, "M4AMT", "Mod 4 Amount", "AMOUNT", R, -1, 1, 0, 0, "", "%") \
    X (M5SRC, "M5SRC", "Mod 5 Source", "SOURCE", C, 0, 12, 0, 0, MUMU_SRC, "") X (M5DST, "M5DST", "Mod 5 Destination", "DEST", C, 0, 21, 0, 0, MUMU_DST, "") X (M5AMT, "M5AMT", "Mod 5 Amount", "AMOUNT", R, -1, 1, 0, 0, "", "%") \
    X (M6SRC, "M6SRC", "Mod 6 Source", "SOURCE", C, 0, 12, 0, 0, MUMU_SRC, "") X (M6DST, "M6DST", "Mod 6 Destination", "DEST", C, 0, 21, 0, 0, MUMU_DST, "") X (M6AMT, "M6AMT", "Mod 6 Amount", "AMOUNT", R, -1, 1, 0, 0, "", "%") \
    X (M7SRC, "M7SRC", "Mod 7 Source", "SOURCE", C, 0, 12, 0, 0, MUMU_SRC, "") X (M7DST, "M7DST", "Mod 7 Destination", "DEST", C, 0, 21, 0, 0, MUMU_DST, "") X (M7AMT, "M7AMT", "Mod 7 Amount", "AMOUNT", R, -1, 1, 0, 0, "", "%") \
    X (M8SRC, "M8SRC", "Mod 8 Source", "SOURCE", C, 0, 12, 0, 0, MUMU_SRC, "") X (M8DST, "M8DST", "Mod 8 Destination", "DEST", C, 0, 21, 0, 0, MUMU_DST, "") X (M8AMT, "M8AMT", "Mod 8 Amount", "AMOUNT", R, -1, 1, 0, 0, "", "%") \
    X (LEVEL,    "LEVEL",   "Timbre Level",      "LEVEL",    R, 0, 1, .8f, 0, "", "%") \
    X (PAN,      "PAN",     "Timbre Pan",        "PAN",      R, -1, 1, 0, 0,  "", "%") \
    X (SPREAD,   "SPREAD",  "Voice Spread",      "SPREAD",   R, 0, 1, .35f, 0, "", "%")

#define MUMU_SRC "OFF|LFO 1|LFO 2|MOD OSC|FILT ENV|AMP ENV|VELOCITY|KEY|MOD WHEEL|AFTERTOUCH|PITCH BEND|RANDOM|MACRO"
#define MUMU_DST "OFF|PITCH|OSC 1 PITCH|OSC 2 PITCH|OSC 1 WAVE|OSC 2 WAVE|OSC 1 LEVEL|OSC 2 LEVEL|MOD OSC LEVEL|NOISE LEVEL|FM|DRIVE|F1 CUTOFF|F2 CUTOFF|F1 RESO|F2 RESO|AMP|PAN|LFO 1 RATE|LFO 2 RATE|MOD OSC FREQ|VIBRATO"

#define MUMU_GLOBAL_PARAMS(X) \
    X (KEYMODE,  "KEYMODE", "Key Mode",          "KEY MODE", C, 0, 3, 0, 0,   "TIMBRE A|TIMBRE B|LAYER|SPLIT", "") \
    X (SPLIT,    "SPLIT",   "Split Point",       "SPLIT",    I, 24, 96, 60, 0, "", "note") \
    X (ARPON,    "ARPON",   "Arp On",            "ARP",      T, 0, 1, 0, 0,   "", "") \
    X (ARPMODE,  "ARPMODE", "Arp Mode",          "MODE",     C, 0, 4, 0, 0,   "UP|DOWN|UP-DOWN|RANDOM|ORDER", "") \
    X (ARPOCT,   "ARPOCT",  "Arp Octaves",       "OCTAVES",  I, 1, 4, 1, 0,   "", "") \
    X (ARPRATE,  "ARPRATE", "Arp Rate",          "RATE",     C, 0, 5, 3, 0,   "1/4|1/8|1/8T|1/16|1/16T|1/32", "") \
    X (ARPGATE,  "ARPGATE", "Arp Gate",          "GATE",     R, .05f, 1, .6f, 0, "", "%") \
    X (ARPLATCH, "ARPLATCH","Arp Latch",         "LATCH",    T, 0, 1, 0, 0,   "", "") \
    X (BPM,      "BPM",     "Internal Tempo",    "TEMPO",    R, 40, 240, 120, 0, "", "bpm") \
    X (DLON,     "DLON",    "Delay On",          "DELAY",    T, 0, 1, 1, 0,   "", "") \
    X (DLTIME,   "DLTIME",  "Delay Time",        "TIME",     R, 1, 2000, 380, 300, "", "ms") \
    X (DLSYNC,   "DLSYNC",  "Delay Clock",       "CLOCK",    C, 0, 8, 0, 0,   "FREE|1/2|1/4|1/4D|1/4T|1/8|1/8D|1/8T|1/16", "") \
    X (DLFB,     "DLFB",    "Delay Feedback",    "FEEDBACK", R, 0, .95f, .35f, 0, "", "%") \
    X (DLMIX,    "DLMIX",   "Delay Mix",         "MIX",      R, 0, 1, .22f, 0, "", "%") \
    X (DLDIFF,   "DLDIFF",  "Delay Diffusion",   "DIFFUSION",R, 0, 1, .45f, 0, "", "%") \
    X (DLCHAR,   "DLCHAR",  "Delay Character",   "CHARACTER",R, -1, 1, -.3f, 0, "", "%") \
    X (DLPING,   "DLPING",  "Delay Ping-Pong",   "PING-PONG",T, 0, 1, 0, 0,   "", "") \
    X (DLTONE,   "DLTONE",  "Delay Tone",        "TONE",     R, 0, 1, .6f, 0, "", "%") \
    X (MACRO,    "MACRO",   "Macro",             "MACRO",    R, 0, 1, 0, 0,   "", "%") \
    X (TUNE,     "TUNE",    "Master Tune",       "TUNE",     R, -100, 100, 0, 0, "", "ct") \
    X (LOWCUT,   "LOWCUT",  "Low Cut",           "LOW CUT",  T, 0, 1, 1, 0,   "", "") \
    X (MASTER,   "MASTER",  "Master Volume",     "VOLUME",   R, -60, 6, -9, 0, "", "dB")

namespace mumu
{
#define MUMU_ENUM(e, ...) e,
enum TP { MUMU_TIMBRE_PARAMS (MUMU_ENUM) TP_COUNT };
enum GP { MUMU_GLOBAL_PARAMS (MUMU_ENUM) GP_COUNT };
#undef MUMU_ENUM

enum class PType { R, I, C, T };

struct ParamDef
{
    const char* key;
    const char* name;
    const char* label;
    PType type;
    float min, max, def, centre;
    const char* choices;
    const char* unit;
    juce::StringArray choiceList() const { return juce::StringArray::fromTokens (choices, "|", ""); }
    bool bipolar() const { return type == PType::R && min < 0 && max > 0; }
};

const std::vector<ParamDef>& timbreDefs();     // indicizzati con TP
const std::vector<ParamDef>& globalDefs();     // indicizzati con GP

juce::String paramID (int timbre, int index);  // timbre 0 = A, 1 = B, -1 = globale
juce::String formatValue (const ParamDef&, float value);
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// valori letti una volta per blocco audio
struct TimbreValues
{
    float v[TP_COUNT] {};
    float operator[] (TP p) const noexcept { return v[p]; }
    int i (TP p) const noexcept { return (int) std::lround (v[p]); }
    bool b (TP p) const noexcept { return v[p] > .5f; }
};
struct GlobalValues
{
    float v[GP_COUNT] {};
    float operator[] (GP p) const noexcept { return v[p]; }
    int i (GP p) const noexcept { return (int) std::lround (v[p]); }
    bool b (GP p) const noexcept { return v[p] > .5f; }
};

class ParamReader
{
public:
    void attach (juce::AudioProcessorValueTreeState&);
    void read (TimbreValues (&t)[2], GlobalValues& g) const;
private:
    std::atomic<float>* tp[2][TP_COUNT] {};
    std::atomic<float>* gp[GP_COUNT] {};
};
}
