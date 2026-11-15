#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
    juce::String paramsJson (CompressotchiProcessor& p)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("input",   (double) p.apvts.getRawParameterValue ("input")->load());
        o->setProperty ("output",  (double) p.apvts.getRawParameterValue ("output")->load());
        o->setProperty ("attack",  (double) p.apvts.getRawParameterValue ("attack")->load());
        o->setProperty ("release", (double) p.apvts.getRawParameterValue ("release")->load());
        o->setProperty ("ratio",   (int) std::round (p.apvts.getRawParameterValue ("ratio")->load()));
        o->setProperty ("byp",     p.apvts.getRawParameterValue ("in")->load() < 0.5f);
        return juce::JSON::toString (juce::var (o), true);
    }
}

CompressotchiEditor::CompressotchiEditor (CompressotchiProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    using Opt = juce::WebBrowserComponent::Options;

    auto options = Opt {}
#if JUCE_WINDOWS
        .withBackend (Opt::Backend::webview2)
        .withWinWebView2Options (Opt::WinWebView2 {}
            .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("COMPRESSOTCHI_webview")))
#endif
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withInitialisationData ("ui", juce::var (proc.uiState))
        .withInitialisationData ("params", juce::var (paramsJson (proc)))
        .withResourceProvider ([] (const juce::String& url) -> std::optional<juce::WebBrowserComponent::Resource>
        {
            if (url == "/" || url == "/index.html")
            {
                const auto* b = reinterpret_cast<const std::byte*> (BinaryData::index_html);
                return juce::WebBrowserComponent::Resource { std::vector<std::byte> (b, b + BinaryData::index_htmlSize),
                                                             juce::String ("text/html") };
            }
            return std::nullopt;
        })
        .withEventListener ("param", [this] (const juce::var& v)
        {
            if (v.getDynamicObject() == nullptr) return;
            auto get = [&] (const char* k, float& out) { auto x = v.getProperty (k, juce::var()); if (! x.isVoid()) { out = (float) (double) x; return true; } return false; };
            float f = 0.0f;
            if (get ("input", f))   setParam ("input", f);
            if (get ("output", f))  setParam ("output", f);
            if (get ("attack", f))  setParam ("attack", f);
            if (get ("release", f)) setParam ("release", f);
            if (get ("ratio", f))   setParam ("ratio", juce::jlimit (0.0f, 1.0f, std::round (f) / 3.0f));
            if (get ("on", f))      setParam ("in", f > 0.5f ? 1.0f : 0.0f);
        })
        .withEventListener ("ui", [this] (const juce::var& v)
        {
            proc.uiState = v.getProperty ("json", juce::var()).toString();
        });

    web = std::make_unique<juce::WebBrowserComponent> (options);
    addAndMakeVisible (*web);

    setResizable (true, true);
    setResizeLimits (420, 380, 1800, 1500);
    setSize (980, 820);

    web->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    startTimerHz (30);
}

CompressotchiEditor::~CompressotchiEditor()
{
    stopTimer();
}

void CompressotchiEditor::resized()
{
    if (web != nullptr) web->setBounds (getLocalBounds());
}

void CompressotchiEditor::setParam (const juce::String& id, float normalised)
{
    if (auto* p = proc.apvts.getParameter (id))
        if (std::abs (p->getValue() - normalised) > 1.0e-4f)
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
}

void CompressotchiEditor::timerCallback()
{
    if (web == nullptr) return;

    auto* o = new juce::DynamicObject();
    o->setProperty ("gr", (double) proc.grPeak.exchange (0.0f));
    o->setProperty ("db", (double) proc.outDb.load());

    juce::Array<juce::var> p;
    p.add ((double) proc.apvts.getRawParameterValue ("input")->load());
    p.add ((double) proc.apvts.getRawParameterValue ("output")->load());
    p.add ((double) proc.apvts.getRawParameterValue ("attack")->load());
    p.add ((double) proc.apvts.getRawParameterValue ("release")->load());
    p.add ((double) std::round (proc.apvts.getRawParameterValue ("ratio")->load()));
    p.add ((double) proc.apvts.getRawParameterValue ("in")->load());
    o->setProperty ("p", p);

    web->emitEventIfBrowserIsVisible ("meter", juce::var (o));
}
