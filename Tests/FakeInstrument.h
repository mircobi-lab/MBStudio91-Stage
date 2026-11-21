#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <set>

// Base minima per finti plugin usati nei test.
class FakePluginBase : public juce::AudioPluginInstance
{
public:
    explicit FakePluginBase (bool isSynth)
        : juce::AudioPluginInstance (isSynth
              ? BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true)
              : BusesProperties().withInput ("In", juce::AudioChannelSet::stereo(), true)
                                 .withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
          synth (isSynth) {}

    void fillInPluginDescription (juce::PluginDescription& d) const override
    {
        d.name = getName();
        d.pluginFormatName = "Fake";
        d.isInstrument = synth;
    }

    void prepareToPlay (double sr, int) override { lastSampleRate = sr; ++prepareCount; }
    void releaseResources() override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return synth; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& mb) override { mb.replaceAll (stateText.toRawUTF8(), stateText.getNumBytesAsUTF8()); }
    void setStateInformation (const void* d, int n) override { stateText = juce::String::fromUTF8 ((const char*) d, n); ++setStateCount; }

    juce::String stateText;
    int setStateCount = 0;

    using juce::AudioPluginInstance::processBlock;

    double lastSampleRate = 0.0;
    int prepareCount = 0;

private:
    bool synth;
};

// Strumento finto: scrive 0.5 su tutti i canali finche' almeno una nota e' tenuta.
class FakeInstrument final : public FakePluginBase
{
public:
    FakeInstrument() : FakePluginBase (true) {}
    const juce::String getName() const override { return "FakeInstrument"; }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            received.push_back (m);
            if (m.isNoteOn())                               notes.insert (m.getNoteNumber());
            else if (m.isNoteOff())                         notes.erase (m.getNoteNumber());
            else if (m.isAllNotesOff() || m.isAllSoundOff()) notes.clear();
        }
        buffer.clear();
        if (! notes.empty())
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), 0.5f, buffer.getNumSamples());
    }

    void reset() override { ++resetCount; }

    bool receivedNoteOff (int note) const
    {
        for (const auto& m : received)
            if (m.isNoteOff() && m.getNoteNumber() == note) return true;
        return false;
    }

    bool receivedController (int cc) const
    {
        for (const auto& m : received)
            if (m.isController() && m.getControllerNumber() == cc) return true;
        return false;
    }

    std::vector<juce::MidiMessage> received;
    std::set<int> notes;
    int resetCount = 0;
};

// Come il wrapper VST3 di JUCE: prepareToPlay non fa nulla se e' gia' attivo
// con la stessa frequenza e lo stesso blocco gia' impostati con setRateAndBufferSizeDetails.
class EarlyReturnInstrument final : public FakePluginBase
{
public:
    EarlyReturnInstrument() : FakePluginBase (true) {}
    const juce::String getName() const override { return "EarlyReturnInstrument"; }

    void prepareToPlay (double sr, int bs) override
    {
        if (active && juce::approximatelyEqual (getSampleRate(), sr) && getBlockSize() == bs)
            return;
        active = true;
        lastSampleRate = sr;
        ++prepareCount;
    }

    void releaseResources() override { active = false; }

    void processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&) override { b.clear(); }

    using FakePluginBase::processBlock;
    bool active = false;
};

// Strumento che "risuona": dopo il primo note-on scrive 0.5 per sempre, finche' non riceve CC120/CC123.
class RingingInstrument final : public FakePluginBase
{
public:
    RingingInstrument() : FakePluginBase (true) {}
    const juce::String getName() const override { return "RingingInstrument"; }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            if (m.isNoteOn())                                 { ringing = true; ++noteOns; }
            else if (m.isAllSoundOff() || m.isAllNotesOff())  ringing = false;
        }
        buffer.clear();
        if (ringing)
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), 0.5f, buffer.getNumSamples());
    }

    using FakePluginBase::processBlock;
    bool ringing = false;
    int noteOns = 0;
};

// Effetto finto: moltiplica il segnale per un fattore.
class FakeGainEffect final : public FakePluginBase
{
public:
    explicit FakeGainEffect (float g) : FakePluginBase (false), factor (g) {}
    const juce::String getName() const override { return "FakeGainEffect"; }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        buffer.applyGain (factor);
    }

    float factor;
};
