#pragma once
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

/** Interface : la page web COMPRESSOTCHI (œuf Tamagotchi) dans une WebView, pilotée par les paramètres du plugin. */
class CompressotchiEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CompressotchiEditor (CompressotchiProcessor&);
    ~CompressotchiEditor() override;

    void resized() override;

private:
    void timerCallback() override;
    void setParam (const juce::String& id, float normalised);

    CompressotchiProcessor& proc;
    std::unique_ptr<juce::WebBrowserComponent> web;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompressotchiEditor)
};
