#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

#include <atomic>
#include <functional>

// Riceve i frame del display del Push 2 da chi li manda sulla rete locale, con
// l'intestazione "PSMF" piu' la lunghezza su 4 byte usata da push-simulator.
// Un client alla volta: chi arriva dopo aspetta che il primo chiuda.
class FrameServer final : private juce::Thread
{
public:
    static constexpr int defaultPort = 48484;

    explicit FrameServer (std::function<void (juce::Image)> onFrame);
    ~FrameServer() override;

    bool isListening() const noexcept { return listening.load(); }
    bool hasClient() const noexcept { return connected.load(); }
    int getPort() const noexcept { return port; }
    int getFrameCount() const noexcept { return frames.load(); }
    juce::String getLastError() const;

private:
    void run() override;
    void serve (juce::StreamingSocket& client);
    bool readFully (juce::StreamingSocket& client, void* dest, int size);

    std::function<void (juce::Image)> callback;
    juce::StreamingSocket listener;
    int port = defaultPort;

    juce::CriticalSection lock;
    juce::String lastError;
    std::atomic<bool> listening { false }, connected { false };
    std::atomic<int> frames { 0 };
};
