#include "Push2Display.h"
#include "Push2Protocol.h"

#include <libusb.h>

Push2Display::Push2Display() : juce::Thread ("Push2 display")
{
    if (libusb_init (&context) != LIBUSB_SUCCESS)
    {
        context = nullptr;
        lastError = "libusb non si avvia";
        return;
    }
    startThread();
}

Push2Display::~Push2Display()
{
    stopThread (3000);

    if (handle != nullptr)
    {
        // Lascia il display nero invece dell'ultima immagine
        std::vector<std::uint8_t> black (push2::displayWidth * push2::displayHeight * 3, 0);
        sendFrame (push2::encodeFrame (black.data(), push2::displayWidth, push2::displayHeight, push2::displayWidth * 3));
    }

    close();
    if (context != nullptr)
        libusb_exit (context);
}

void Push2Display::setImage (const juce::Image& image)
{
    const int w = push2::displayWidth, h = push2::displayHeight;
    std::vector<std::uint8_t> rgb (static_cast<size_t> (w * h * 3), 0);

    const int iw = juce::jmin (w, image.getWidth());
    const int ih = juce::jmin (h, image.getHeight());
    for (int y = 0; y < ih; ++y)
        for (int x = 0; x < iw; ++x)
        {
            const auto c = image.getPixelAt (x, y);
            auto* p = rgb.data() + (y * w + x) * 3;
            p[0] = c.getRed(); p[1] = c.getGreen(); p[2] = c.getBlue();
        }

    auto frame = push2::encodeFrame (rgb.data(), w, h, w * 3);
    const juce::ScopedLock sl (lock);
    pending = std::move (frame);
}

juce::String Push2Display::getLastError() const
{
    const juce::ScopedLock sl (lock);
    return lastError;
}

bool Push2Display::open()
{
    handle = libusb_open_device_with_vid_pid (context, push2::vendorId, push2::productId);
    if (handle == nullptr)
    {
        state = State::searching;
        return false;
    }

    const int r = libusb_claim_interface (handle, push2::displayInterface);
    if (r != LIBUSB_SUCCESS)
    {
        {
            const juce::ScopedLock sl (lock);
            lastError = juce::String ("interfaccia display occupata: ") + libusb_error_name (r);
        }
        libusb_close (handle);
        handle = nullptr;
        state = State::busy;
        return false;
    }

    state = State::connected;
    return true;
}

void Push2Display::close()
{
    if (handle == nullptr)
        return;
    libusb_release_interface (handle, push2::displayInterface);
    libusb_close (handle);
    handle = nullptr;
}

bool Push2Display::sendFrame (const std::vector<std::uint8_t>& frame)
{
    auto send = [this] (const std::uint8_t* data, int size)
    {
        int done = 0;
        const int r = libusb_bulk_transfer (handle, push2::displayEndpoint,
                                            const_cast<unsigned char*> (data), size, &done, 1000);
        if (r != LIBUSB_SUCCESS || done != size)
        {
            const juce::ScopedLock sl (lock);
            lastError = juce::String ("invio al display fallito: ") + libusb_error_name (r);
            return false;
        }
        return true;
    };

    const auto& header = push2::frameHeader();
    if (! send (header.data(), static_cast<int> (header.size())))
        return false;

    for (size_t offset = 0; offset < frame.size(); offset += push2::transferBytes)
        if (! send (frame.data() + offset, push2::transferBytes))
            return false;

    return true;
}

void Push2Display::run()
{
    std::vector<std::uint8_t> current;

    while (! threadShouldExit())
    {
        if (handle == nullptr && ! open())
        {
            wait (1000); // riprova ogni secondo: Push scollegato o Live aperto
            continue;
        }

        {
            const juce::ScopedLock sl (lock);
            if (! pending.empty())
                current.swap (pending), pending.clear();
        }

        if (! current.empty() && ! sendFrame (current))
        {
            close();
            state = State::searching;
            wait (1000);
            continue;
        }

        wait (40);
    }
}
