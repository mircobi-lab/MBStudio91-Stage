// Prove del protocollo Push 2 senza dispositivo collegato.
#include "Push2Protocol.h"

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <vector>

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

    // Colonna delle scene: 43 in alto, 36 in basso, come nella figura di Ableton
    CHECK (sceneButtonCC (0) == 43);
    CHECK (sceneButtonCC (7) == 36);

    // Esempi del manuale
    CHECK (setAftertouchMode (true) == (Bytes { 0xF0, 0x00, 0x21, 0x1D, 0x01, 0x01, 0x1E, 0x01, 0xF7 }));
    CHECK (setAftertouchMode (false) == (Bytes { 0xF0, 0x00, 0x21, 0x1D, 0x01, 0x01, 0x1E, 0x00, 0xF7 }));
    CHECK (setTouchStripConfig (0x68) == (Bytes { 0xF0, 0x00, 0x21, 0x1D, 0x01, 0x01, 0x17, 0x68, 0xF7 }));
    CHECK (stripPitchBend == 0x68 && stripModWheel == 0x0C);
    // nessun comando sysex deve contenere byte con il bit alto, a parte F0 e F7
    for (auto b : setTouchStripConfig (0xFF))
        CHECK (b == 0xF0 || b == 0xF7 || b < 0x80);

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

    // Giro completo: codifica e ridecodifica devono riportare gli stessi colori,
    // a meno della perdita del 16 bit (al massimo 7 per canale)
    {
        std::vector<std::uint8_t> src ((std::size_t) displayWidth * displayHeight * 3);
        for (std::size_t i = 0; i < src.size(); ++i)
            src[i] = (std::uint8_t) ((i * 37 + (i / 97)) & 0xFF);

        const auto enc = encodeFrame (src.data(), displayWidth, displayHeight, displayWidth * 3);
        const auto dec = decodeFrame (enc.data(), (int) enc.size());
        CHECK (dec.size() == src.size());

        int worst = 0;
        for (std::size_t i = 0; i < src.size() && i < dec.size(); ++i)
            worst = std::max (worst, std::abs ((int) src[i] - (int) dec[i]));
        CHECK (worst <= 7);

        CHECK (decodeFrame (enc.data(), 10).empty()); // frame corto: niente
        CHECK (decodeFrame (nullptr, 0).empty());
    }

    if (failures == 0)
        std::printf ("Tutte le prove passate\n");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
