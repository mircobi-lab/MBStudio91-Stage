#!/usr/bin/env python3
"""Genera l'icona dell'app in stile hardware MBStudio91.

Telaio nero opaco con angoli alla macOS, display incassato con il marchio
MBSTUDIO91 stampato e tre file di pad in gomma, l'ultima accesa coi colori
dei LED MBStudio.

Uso: python3 make_icon.py          (scrive icon.png accanto a questo file)
"""
import os
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
S = 4            # sovracampionamento
OUT = 1024
W = OUT * S

CHASSIS_TOP    = (0x15, 0x16, 0x17)
CHASSIS_BOTTOM = (0x0a, 0x0a, 0x0b)
RECESS         = (0x05, 0x05, 0x06)
RECESS_EDGE    = (0x26, 0x28, 0x2b)
KEY_TOP        = (0x1e, 0x1f, 0x21)
KEY            = (0x12, 0x13, 0x14)
KEY_EDGE       = (0x34, 0x36, 0x3a)
PRINT          = (0xf3, 0xf3, 0xf1)
LEDS = [(0x3d, 0x8b, 0xff), (0x3e, 0xe0, 0x5c), (0xff, 0xb0, 0x20), (0xff, 0x3b, 0x30)]


def rounded(size, radius, fill):
    im = Image.new("RGBA", size, (0, 0, 0, 0))
    ImageDraw.Draw(im).rounded_rectangle([0, 0, size[0] - 1, size[1] - 1], radius, fill=fill)
    return im


def vertical_gradient(size, top, bottom):
    im = Image.new("RGB", (1, size[1]))
    px = im.load()
    for y in range(size[1]):
        t = y / max(1, size[1] - 1)
        px[0, y] = tuple(round(a + (b - a) * t) for a, b in zip(top, bottom))
    return im.resize(size)


def paste_rounded_gradient(dst, box, radius, top, bottom):
    x, y, w, h = box
    grad = vertical_gradient((w, h), top, bottom).convert("RGBA")
    mask = rounded((w, h), radius, (255, 255, 255, 255)).split()[-1]
    dst.paste(grad, (x, y), mask)


def main():
    img = Image.new("RGBA", (W, W), (0, 0, 0, 0))

    # Telaio: quadrato con angoli arrotondati, proporzioni da icona macOS
    inset = round(0.0977 * W)
    side = W - 2 * inset
    radius = round(0.2237 * side)
    paste_rounded_gradient(img, (inset, inset, side, side), radius, CHASSIS_TOP, CHASSIS_BOTTOM)

    # Bordo inciso: chiaro in alto, spento in basso
    edge = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    ImageDraw.Draw(edge).rounded_rectangle(
        [inset, inset, inset + side - 1, inset + side - 1], radius,
        outline=KEY_EDGE + (255,), width=round(2.5 * S))
    fade = Image.new("L", (1, W))
    fp = fade.load()
    for y in range(W):
        fp[0, y] = max(0, round(150 * (1 - y / W) ** 1.6))
    edge.putalpha(Image.composite(fade.resize((W, W)), Image.new("L", (W, W), 0), edge.split()[-1]))
    img.alpha_composite(edge)

    pad_x = inset + round(0.085 * side)
    inner_w = side - 2 * round(0.085 * side)

    # Display incassato col marchio stampato
    disp_y = inset + round(0.11 * side)
    disp_h = round(0.155 * side)
    paste_rounded_gradient(img, (pad_x, disp_y, inner_w, disp_h), round(0.03 * side), RECESS, RECESS)
    ring = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    ImageDraw.Draw(ring).rounded_rectangle(
        [pad_x, disp_y, pad_x + inner_w - 1, disp_y + disp_h - 1], round(0.03 * side),
        outline=RECESS_EDGE + (255,), width=round(2 * S))
    img.alpha_composite(ring)

    mark = Image.open(os.path.join(HERE, "..", "..", "..", "..", "Assets", "brand",
                                   "mbstudio91_wordmark.png")).convert("RGBA")
    mw = round(inner_w * 0.78)
    mh = round(mw * mark.size[1] / mark.size[0])
    mark = mark.resize((mw, mh), Image.LANCZOS)
    tint = Image.new("RGBA", mark.size, PRINT + (245,))
    tint.putalpha(Image.eval(mark.split()[-1], lambda a: round(a * 0.96)))
    img.alpha_composite(tint, (pad_x + (inner_w - mw) // 2, disp_y + (disp_h - mh) // 2))

    # Pad: 4 colonne, 3 file, l'ultima accesa
    grid_top = disp_y + disp_h + round(0.075 * side)
    grid_bottom = inset + side - round(0.11 * side)
    gap = round(0.045 * side)
    pad_w = (inner_w - 3 * gap) // 4
    pad_h = (grid_bottom - grid_top - 2 * gap) // 3
    pr = round(pad_w * 0.18)

    glow = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    for row in range(3):
        for col in range(4):
            x = pad_x + col * (pad_w + gap)
            y = grid_top + row * (pad_h + gap)
            if row == 2:
                c = LEDS[col]
                glow.alpha_composite(rounded((pad_w, pad_h), pr, c + (190,)), (x, y))
            else:
                paste_rounded_gradient(img, (x, y, pad_w, pad_h), pr, KEY_TOP, KEY)
                o = Image.new("RGBA", (pad_w, pad_h), (0, 0, 0, 0))
                ImageDraw.Draw(o).rounded_rectangle([0, 0, pad_w - 1, pad_h - 1], pr,
                                                    outline=KEY_EDGE + (210,), width=round(2 * S))
                img.alpha_composite(o, (x, y))

    img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(round(0.016 * side))))
    for col in range(4):
        x = pad_x + col * (pad_w + gap)
        y = grid_top + 2 * (pad_h + gap)
        c = LEDS[col]
        paste_rounded_gradient(img, (x, y, pad_w, pad_h), pr,
                               tuple(min(255, round(v * 1.15)) for v in c), c)

    img.resize((OUT, OUT), Image.LANCZOS).save(os.path.join(HERE, "icon.png"))
    print("scritto", os.path.join(HERE, "icon.png"))


if __name__ == "__main__":
    main()
