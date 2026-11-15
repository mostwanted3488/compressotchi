#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

using APVTS = juce::AudioProcessorValueTreeState;

static juce::String fmtOneDecimal (float v) { return juce::String (v, 1); }

APVTS::ParameterLayout CompressotchiProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto norm = NormalisableRange<float> (0.0f, 1.0f, 0.0f);

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "input", 1 }, "Input", norm, 0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
            const float d = v * 36.0f - 24.0f; return String (d > 0 ? "+" : "") + String (d, 0) + " dB"; })));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", norm, 0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
            return String ((v - 0.5f) * 40.0f, 1) + " dB"; })));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "attack", 1 }, "Attack", norm, 0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
            return fmtOneDecimal (1.0f + v * 6.0f) + " (" + String (roundToInt (800.0f * std::pow (0.025f, v))) + " us)"; })));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Release", norm, 0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
            return fmtOneDecimal (1.0f + v * 6.0f) + " (" + String (roundToInt (1100.0f * std::pow (50.0f / 1100.0f, v))) + " ms)"; })));

    // même ordre que les boutons de l'interface : 20 / 12 / 8 / 4
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "ratio", 1 }, "Ratio",
        StringArray { "20:1", "12:1", "8:1", "4:1" }, 3));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "in", 1 }, "IN", true));

    return { p.begin(), p.end() };
}

CompressotchiProcessor::CompressotchiProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pInput   = apvts.getRawParameterValue ("input");
    pOutput  = apvts.getRawParameterValue ("output");
    pAttack  = apvts.getRawParameterValue ("attack");
    pRelease = apvts.getRawParameterValue ("release");
    pRatio   = apvts.getRawParameterValue ("ratio");
    pIn      = apvts.getRawParameterValue ("in");
}

bool CompressotchiProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto& out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == l.getMainInputChannelSet();
}

void CompressotchiProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    gr = 0.0f;
    outMs = 0.0f;

    inGain.reset (sr, 0.03);   inGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (*pInput * 36.0f - 24.0f));
    outGain.reset (sr, 0.03);  outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain ((*pOutput - 0.5f) * 40.0f));
    wetMix.reset (sr, 0.015);  wetMix.setCurrentAndTargetValue (*pIn > 0.5f ? 1.0f : 0.0f);

    oversampling.initProcessing ((size_t) samplesPerBlock);
    oversampling.reset();

    const int latency = (int) std::round (oversampling.getLatencyInSamples());
    setLatencySamples (latency);

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) samplesPerBlock, 2 };
    dryDelay.prepare (spec);
    dryDelay.setDelay ((float) latency);
    dryDelay.reset();

    dryBuffer.setSize (2, samplesPerBlock, false, false, true);
}

void CompressotchiProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);
    if (numCh == 0 || n == 0) return;

    if (dryBuffer.getNumSamples() < n) dryBuffer.setSize (2, n, false, false, true);

    // --- chemin sec (retardé de la latence du suréchantillonnage) ---
    for (int c = 0; c < numCh; ++c)
    {
        auto* in = buffer.getReadPointer (c);
        auto* d = dryBuffer.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            dryDelay.pushSample (c, in[i]);
            d[i] = dryDelay.popSample (c);
        }
    }

    // --- paramètres ---
    inGain.setTargetValue (juce::Decibels::decibelsToGain (pInput->load() * 36.0f - 24.0f));    // -24 .. +12 dB
    outGain.setTargetValue (juce::Decibels::decibelsToGain ((pOutput->load() - 0.5f) * 40.0f)); // -20 .. +20 dB
    wetMix.setTargetValue (pIn->load() > 0.5f ? 1.0f : 0.0f);

    static constexpr float ratios[4] = { 20.0f, 12.0f, 8.0f, 4.0f };
    const float ratio = ratios[juce::jlimit (0, 3, (int) std::round (pRatio->load()))];
    const float slope = 1.0f - 1.0f / ratio;
    const float atkT = 0.0008f * std::pow (0.025f, pAttack->load());               // 800 us -> 20 us
    const float relT = 1.1f * std::pow (50.0f / 1100.0f, pRelease->load());        // 1,1 s -> 50 ms
    const float atk = std::exp (-1.0f / ((float) sr * atkT));
    const float rel = std::exp (-1.0f / ((float) sr * relT));
    constexpr float thr = -14.0f, knee = 6.0f;

    // --- compresseur : détecteur de crête lié, calcul en dB, genou doux ---
    float grMax = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = inGain.getNextValue();
        float lvl = 0.0f;
        for (int c = 0; c < numCh; ++c)
        {
            auto* x = buffer.getWritePointer (c);
            x[i] *= g;
            lvl = juce::jmax (lvl, std::abs (x[i]));
        }
        const float o = 20.0f * std::log10 (lvl + 1.0e-9f) - thr;
        const float tg = o <= -knee * 0.5f ? 0.0f
                       : (o >= knee * 0.5f ? o * slope : slope * (o + knee * 0.5f) * (o + knee * 0.5f) / (2.0f * knee));
        const float cf = tg > gr ? atk : rel;
        gr = cf * gr + (1.0f - cf) * tg;
        if (gr < 1.0e-6f) gr = 0.0f;
        grMax = juce::jmax (grMax, gr);
        const float gn = std::pow (10.0f, -gr / 20.0f);
        for (int c = 0; c < numCh; ++c)
            buffer.getWritePointer (c)[i] *= gn;
    }

    // --- grain FET : saturation douce asymétrique (2e harmonique), suréchantillonnée x2, sans bruit ---
    {
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
        auto up = oversampling.processSamplesUp (block);
        constexpr float A = 1.2f, B = 0.15f;
        const float nb = std::tanh (B), nrm = A * (1.0f - nb * nb);
        for (size_t c = 0; c < up.getNumChannels(); ++c)
        {
            auto* x = up.getChannelPointer (c);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
                x[i] = (std::tanh (A * x[i] + B) - nb) / nrm;
        }
        oversampling.processSamplesDown (block);
    }

    // --- sortie + bypass (fondu) ---
    double ms = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float og = outGain.getNextValue();
        const float w = wetMix.getNextValue();
        for (int c = 0; c < numCh; ++c)
        {
            auto* x = buffer.getWritePointer (c);
            const float y = x[i] * og * w + dryBuffer.getReadPointer (c)[i] * (1.0f - w);
            x[i] = y;
            ms += (double) y * (double) y;
        }
    }

    // --- mesures pour l'interface ---
    ms /= (double) (n * numCh);
    const float k = std::exp (-(float) n / (0.04f * (float) sr));
    outMs = k * outMs + (1.0f - k) * (float) ms;
    outDb.store (10.0f * std::log10 (outMs + 1.0e-10f));
    if (grMax > grPeak.load()) grPeak.store (grMax);
}

juce::AudioProcessorEditor* CompressotchiProcessor::createEditor() { return new CompressotchiEditor (*this); }

void CompressotchiProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("ui", uiState, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void CompressotchiProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            uiState = vt.getProperty ("ui").toString();
            apvts.replaceState (vt);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CompressotchiProcessor(); }
