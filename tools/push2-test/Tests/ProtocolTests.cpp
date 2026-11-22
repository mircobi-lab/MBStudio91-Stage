// Prove del protocollo Push 2 senza dispositivo collegato.
#include "Push2Protocol.h"

#include <cstdio>
#include <cstdlib>

static int failures = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (! (cond)) { std::printf ("FALLITO: %s (riga %d)\n", #cond, __LINE__); ++failures; } \
    } while (false)

int main()
{
    using namespace push2;

    // Pad: angoli dal manuale e da Push2-map.json
    CHECK (padNote (7, 0) == 36);   // in basso a sinistra
    CHECK (padNote (7, 7) == 43);   // in basso a destra
    CHECK (padNote (0, 0) == 92);   // in alto a sinistra
    CHECK (padNote (0, 7) == 99);   // in alto a destra
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c)
        {
            auto pos = padPosition (padNote (r, c));
            CHECK (pos && pos->first == r && pos->second == c);
        }
    CHECK (! padPosition (35));
    CHECK (! padPosition (100));

    // Rotativi: valore relativo in complemento a due, 1..63 a destra, 127..64 a sinistra
    CHECK (encoderDelta (1) == 1);
    CHECK (encoderDelta (63) == 63);
    CHECK (encoderDelta (127) == -1);
    CHECK (encoderDelta (64) == -64);
    CHECK (encoderDelta (0) == 0);
    CHECK (trackEncoderCC == 71 && trackEncoderTouchNote == 0);
    // le note del tocco stanno fuori dall'intervallo dei pad, non si confondono
    for (int n = trackEncoderTouchNote; n < trackEncoderTouchNote + numTrackEncoders; ++n)
        CHECK (! padPosition (n));

    // Identity Reply letta dal Push 2 di Mirco (Mac mini, 21/11/2026)
    const std::uint8_t reply[] { 0xF0, 0x7E, 0x01, 0x06, 0x02, 0x00, 0x21, 0x1D, 0x67, 0x32, 0x02, 0x00,
                                 0x01, 0x00, 0x47, 0x00, 0x50, 0x4D, 0x5C, 0x08, 0x00, 0x01, 0xF7 };
    auto id = parseIdentityReply (reply, sizeof (reply));
    CHECK (id && id->major == 1 && id->minor == 0 && id->build == 71);
    CHECK (id && id->serial == 18294480u);
    CHECK (id && id->boardRevision == 1);
    // Come lo consegna JUCE (senza F0 e F7)
    auto id2 = parseIdentityReply (reply + 1, sizeof (reply) - 2);
    CHECK (id2 && id2->build == 71);

    // Esempio del manuale: firmware 1.0.47, seriale 17295091
    const std::uint8_t manual[] { 0xF0, 0x7E, 0x01, 0x06, 0x02, 0x00, 0x21, 0x1D, 0x67, 0x32, 0x02, 0x00,
                                  0x01, 0x00, 0x2F, 0x00, 0x73, 0x4D, 0x1F, 0x08, 0x00, 0x01, 0xF7 };
    auto id3 = parseIdentityReply (manual, sizeof (manual));
    CHECK (id3 && id3->build == 47 && id3->serial == 17295091u);

    // Palette: esempio del manuale, voce 125 = blu 255, bianco 126
    const Bytes expected { 0xF0, 0x00, 0x21, 0x1D, 0x01, 0x01, 0x03, 0x7D,
                           0x00, 0x00, 0x00, 0x00, 0x7F, 0x01, 0x7E, 0x00, 0xF7 };
    CHECK (setPaletteEntry (125, 0, 0, 255, 126) == expected);
    CHECK (reapplyPalette() == (Bytes { 0xF0, 0x00, 0x21, 0x1D, 0x01, 0x01, 0x05, 0xF7 }));
    CHECK (setDisplayBrightness (255) == (Bytes { 0xF0, 0x00, 0x21, 0x1D, 0x01, 0x01, 0x08, 0x7F, 0x01, 0xF7 }));
    CHECK (setLedBrightness (64) == (Bytes { 0xF0, 0x00, 0x21, 0x1D, 0x01, 0x01, 0x06, 0x40, 0xF7 }));

    // Pixel: blu nei 5 bit alti, rosso nei 5 bassi
    CHECK (packPixel (255, 0, 0) == 0x001F);
    CHECK (packPixel (0, 255, 0) == 0x07E0);
    CHECK (packPixel (0, 0, 255) == 0xF800);

    // Frame: intestazione, dimensione, XOR
    CHECK (frameHeader()[0] == 0xFF && frameHeader()[1] == 0xCC && frameHeader()[2] == 0xAA
           && frameHeader()[3] == 0x88 && frameHeader()[15] == 0x00);
    std::vector<std::uint8_t> img (displayWidth * displayHeight * 3, 0);
    img[0] = 255; // primo pixel rosso
    auto frame = encodeFrame (img.data(), displayWidth, displayHeight, displayWidth * 3);
    CHECK (frame.size() == 327680u);
    CHECK (frame[0] == (0x1F ^ 0xE7) && frame[1] == (0x00 ^ 0xF3));
    CHECK (frame[2] == 0xE7 && frame[3] == 0xFF); // pixel nero = solo il pattern
    CHECK (frame[lineBytes] == 0xE7);             // inizio riga 2, il pattern riparte

    if (failures == 0)
        std::printf ("Tutte le prove passate\n");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
