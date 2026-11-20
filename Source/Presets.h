#pragma once
#include "Params.h"

namespace mumu
{
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;   // id completo ("A_F1CUT", "B_VMODE", "G_DLMIX") e valore reale
};

const std::vector<Preset>& factoryPresets();
// riporta tutti i parametri al valore iniziale e applica il preset
void applyPreset (juce::AudioProcessorValueTreeState&, const Preset&);
}
