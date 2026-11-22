#include "FrameServer.h"
#include "Push2Protocol.h"

#include <vector>

FrameServer::FrameServer (std::function<void (juce::Image)> onFrame)
    : juce::Thread ("Push2 frame server"), callback (std::move (onFrame))
{
    if (auto fromEnv = juce::SystemStats::getEnvironmentVariable ("PUSH_SIM_DISPLAY_FEED_PORT", {});
        fromEnv.isNotEmpty() && fromEnv.getIntValue() > 0)
        port = fromEnv.getIntValue();

    startThread();
}

FrameServer::~FrameServer()
{
    signalThreadShouldExit();
    listener.close();
    stopThread (2000);
}

juce::String FrameServer::getLastError() const
{
    const juce::ScopedLock sl (lock);
    return lastError;
}

bool FrameServer::readFully (juce::StreamingSocket& client, void* dest, int size)
{
    auto* p = static_cast<char*> (dest);
    int done = 0;

    while (done < size)
    {
        if (threadShouldExit())
            return false;

        const int status = client.waitUntilReady (true, 200);
        if (status < 0)
            return false;
        if (status == 0)
            continue;

        const int n = client.read (p + done, size - done, false);
        if (n <= 0)
            return false;
        done += n;
    }
    return true;
}

void FrameServer::serve (juce::StreamingSocket& client)
{
    connected = true;

    while (! threadShouldExit())
    {
        std::uint8_t header[8];
        if (! readFully (client, header, sizeof (header)))
            break;

        if (std::memcmp (header, "PSMF", 4) != 0)
        {
            const juce::ScopedLock sl (lock);
            lastError = "intestazione sconosciuta, mi aspetto PSMF";
            break;
        }

        const auto size = ((std::uint32_t) header[4] << 24) | ((std::uint32_t) header[5] << 16)
                        | ((std::uint32_t) header[6] << 8)  |  (std::uint32_t) header[7];

        if (size != (std::uint32_t) push2::frameBytes)
        {
            const juce::ScopedLock sl (lock);
            lastError = "frame di " + juce::String ((int) size) + " byte invece di "
                      + juce::String (push2::frameBytes);
            break;
        }

        std::vector<std::uint8_t> payload (size);
        if (! readFully (client, payload.data(), (int) size))
            break;

        const auto rgb = push2::decodeFrame (payload.data(), (int) payload.size());
        if (rgb.empty())
            break;

        juce::Image img (juce::Image::RGB, push2::displayWidth, push2::displayHeight, false);
        {
            const juce::Image::BitmapData data (img, juce::Image::BitmapData::writeOnly);
            for (int y = 0; y < push2::displayHeight; ++y)
            {
                const std::uint8_t* src = rgb.data() + (std::size_t) y * push2::displayWidth * 3;
                for (int x = 0; x < push2::displayWidth; ++x)
                    data.setPixelColour (x, y, juce::Colour (src[x * 3], src[x * 3 + 1], src[x * 3 + 2]));
            }
        }

        ++frames;
        {
            const juce::ScopedLock sl (lock);
            lastError.clear();
        }
        if (callback)
            callback (img);
    }

    connected = false;
}

void FrameServer::run()
{
    if (! listener.createListener (port, "127.0.0.1"))
    {
        const juce::ScopedLock sl (lock);
        lastError = "porta " + juce::String (port) + " gia' occupata";
        return;
    }

    listening = true;

    while (! threadShouldExit())
    {
        if (auto* client = listener.waitForNextConnection())
        {
            std::unique_ptr<juce::StreamingSocket> owned (client);
            serve (*owned);
        }
    }

    listening = false;
}
