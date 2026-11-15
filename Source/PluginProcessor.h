#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

/** COMPRESSOTCHI — compresseur FET façon 1176 (INPUT / OUTPUT / ATTACK / RELEASE, RATIO 4-8-12-20, bouton IN). */
class CompressotchiProcessor : public juce::AudioProcessor
{
public:
    CompressotchiProcessor();
    ~CompressotchiProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "COMPRESSOTCHI"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // état purement visuel de l'interface (thème, personnage, décor, meter), sauvegardé avec le projet
    juce::String uiState;

    // mesures lues par l'éditeur (~30 Hz)
    std::atomic<float> grPeak { 0.0f };   // réduction de gain maximale (dB, positif)
    std::atomic<float> outDb  { -100.0f };// niveau RMS de sortie (dB)

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float>* pInput = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pAttack = nullptr;
    std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pRatio = nullptr;
    std::atomic<float>* pIn = nullptr;

    double sr = 44100.0;
    float gr = 0.0f;   // état du détecteur (dB), un seul détecteur lié pour tous les canaux
    float outMs = 0.0f;

    juce::SmoothedValue<float> inGain, outGain, wetMix;
    juce::dsp::Oversampling<float> oversampling { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 512 };
    juce::AudioBuffer<float> dryBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompressotchiProcessor)
};
