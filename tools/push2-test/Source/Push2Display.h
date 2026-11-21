#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

#include <atomic>
#include <vector>

struct libusb_context;
struct libusb_device_handle;

// Manda al display del Push 2 l'ultima immagine ricevuta, circa 25 volte al secondo.
// Il Push spegne il display se non riceve frame per 2 secondi, quindi il thread
// rimanda lo stesso frame anche quando non cambia.
class Push2Display final : private juce::Thread
{
public:
    enum class State { searching, busy, connected };

    Push2Display();
    ~Push2Display() override;

    void setImage (const juce::Image& image); // 960x160, qualsiasi formato
    State getState() const noexcept { return state.load(); }
    juce::String getLastError() const;

private:
    void run() override;
    bool open();
    void close();
    bool sendFrame (const std::vector<std::uint8_t>& frame);

    libusb_context* context = nullptr;
    libusb_device_handle* handle = nullptr;

    juce::CriticalSection lock;
    std::vector<std::uint8_t> pending; // frame gia' codificato
    juce::String lastError;
    std::atomic<State> state { State::searching };
};
