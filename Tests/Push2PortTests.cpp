#include "push2/Push2Protocol.h"
#include "control/Push2Controller.h"

// Il Push 2 presenta due porte MIDI. Appena si accende e' in modalita' Live e
// ascolta la porta Live; la User comincia a rispondere solo dopo aver premuto
// USER. Stage quindi prende la Live, e la User solo quando gliela si chiede.
class Push2PortTests final : public juce::UnitTest
{
public:
    Push2PortTests() : juce::UnitTest ("Push2 porte", "Stage") {}

    static juce::Array<juce::MidiDeviceInfo> ports (juce::StringPairArray nameToId)
    {
        juce::Array<juce::MidiDeviceInfo> out;
        for (const auto& name : nameToId.getAllKeys())
            out.add ({ name, nameToId[name] });
        return out;
    }

    static juce::Array<juce::MidiDeviceInfo> twoPorts()
    {
        juce::StringPairArray p;
        p.set ("Ableton Push 2 Live Port", "live-1");
        p.set ("Ableton Push 2 User Port", "user-1");
        return ports (p);
    }

    void runTest() override
    {
        beginTest ("con tutte e due le porte si prende la Live");
        {
            const auto p = Push2Controller::choosePort (twoPorts(), false);
            expect (p.has_value());
            expectEquals (p->identifier, juce::String ("live-1"));
        }

        beginTest ("la User si prende solo se richiesta");
        {
            const auto p = Push2Controller::choosePort (twoPorts(), true);
            expect (p.has_value());
            expectEquals (p->identifier, juce::String ("user-1"));
        }

        beginTest ("se c'e' una porta sola si prende quella");
        {
            juce::StringPairArray onlyOne;
            onlyOne.set ("Ableton Push 2", "solo-1");
            const auto one = ports (onlyOne);
            const auto p = Push2Controller::choosePort (one, false);
            expect (p.has_value());
            expectEquals (p->identifier, juce::String ("solo-1"));

            juce::StringPairArray onlyUser;
            onlyUser.set ("Ableton Push 2 User Port", "user-1");
            const auto userOnly = ports (onlyUser);
            const auto q = Push2Controller::choosePort (userOnly, false);
            expect (q.has_value());
            expectEquals (q->identifier, juce::String ("user-1"));
        }

        beginTest ("le altre periferiche non sono il Push");
        {
            juce::StringPairArray other;
            other.set ("Launchkey 49 MK3 DAW", "lk-1");
            other.set ("Keystation 88 MK3", "ks-1");
            expect (! Push2Controller::choosePort (ports (other), false).has_value());
            expect (! Push2Controller::choosePort (juce::Array<juce::MidiDeviceInfo>(), false).has_value());
        }
    }
};

static Push2PortTests push2PortTests;

// Il comando che fa passare il Push in User mode. I byte sono quelli dell'esempio
// del manuale Ableton: [F0 00 21 1D 01 01 0A 01 F7] = set MIDI mode to User.
class Push2MidiModeTests final : public juce::UnitTest
{
public:
    Push2MidiModeTests() : juce::UnitTest ("Push2 MIDI mode", "Stage") {}

    void runTest() override
    {
        beginTest ("User mode, Live mode, Dual mode");
        {
            const std::vector<std::uint8_t> user { 0xF0, 0x00, 0x21, 0x1D, 0x01, 0x01, 0x0A, 0x01, 0xF7 };
            expect (push2::setMidiMode (push2::midiModeUser) == user);

            const std::vector<std::uint8_t> live { 0xF0, 0x00, 0x21, 0x1D, 0x01, 0x01, 0x0A, 0x00, 0xF7 };
            expect (push2::setMidiMode (push2::midiModeLive) == live);

            expectEquals ((int) push2::setMidiMode (push2::midiModeDual)[7], 2);
        }
    }
};

static Push2MidiModeTests push2MidiModeTests;
