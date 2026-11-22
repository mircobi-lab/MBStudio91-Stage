#!/usr/bin/env python3
"""Genera l'icona di MBStudio91 Stage, in stile hardware MBStudio91.

Stessa famiglia dell'icona di push2-test: telaio nero con angoli alla macOS,
display incassato col marchio stampato. Al posto della griglia di pad ci sono
le quattro postazioni, ognuna con il suo LED acceso e la barra del volume:
e' quello che si vede aprendo Stage.

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
SLOT_TOP       = (0x1b, 0x1c, 0x1e)
SLOT           = (0x0e, 0x0f, 0x10)
SLOT_EDGE      = (0x34, 0x36, 0x3a)
PRINT          = (0xf3, 0xf3, 0xf1)

# I quattro LED delle postazioni, come sul pannello
LEDS = [(0x3d, 0x8b, 0xff), (0x3e, 0xe0, 0x5c), (0xff, 0xb0, 0x20), (0xff, 0x3b, 0x30)]
# Quanto e' alzato il volume di ogni postazione, da 0 a 1
LEVELS = [0.78, 0.62, 0.90, 0.46]


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


def outline(dst, box, radius, colour, alpha, width):
    x, y, w, h = box
    o = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ImageDraw.Draw(o).rounded_rectangle([0, 0, w - 1, h - 1], radius,
                                        outline=colour + (alpha,), width=width)
    dst.alpha_composite(o, (x, y))


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
        outline=SLOT_EDGE + (255,), width=round(2.5 * S))
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
    disp_r = round(0.03 * side)
    paste_rounded_gradient(img, (pad_x, disp_y, inner_w, disp_h), disp_r, RECESS, RECESS)
    outline(img, (pad_x, disp_y, inner_w, disp_h), disp_r, RECESS_EDGE, 255, round(2 * S))

    mark = Image.open(os.path.join(HERE, "mbstudio91-wordmark.png")).convert("RGBA")
    mw = round(inner_w * 0.78)
    mh = round(mw * mark.size[1] / mark.size[0])
    mark = mark.resize((mw, mh), Image.LANCZOS)
    tint = Image.new("RGBA", mark.size, PRINT + (245,))
    tint.putalpha(Image.eval(mark.split()[-1], lambda a: round(a * 0.96)))
    img.alpha_composite(tint, (pad_x + (inner_w - mw) // 2, disp_y + (disp_h - mh) // 2))

    # Le quattro postazioni: LED in cima, sotto la barra del volume
    top = disp_y + disp_h + round(0.075 * side)
    bottom = inset + side - round(0.11 * side)
    gap = round(0.05 * side)
    col_w = (inner_w - 3 * gap) // 4
    col_r = round(col_w * 0.3)

    led_h = round(col_w * 0.62)
    bar_top = top + led_h + round(0.035 * side)
    bar_h = bottom - bar_top
    bar_r = round(col_w * 0.3)

    glow = Image.new("RGBA", (W, W), (0, 0, 0, 0))

    for col in range(4):
        x = pad_x + col * (col_w + gap)
        c = LEDS[col]

        # Corsa della barra, incassata
        paste_rounded_gradient(img, (x, bar_top, col_w, bar_h), bar_r, SLOT, SLOT_TOP)
        outline(img, (x, bar_top, col_w, bar_h), bar_r, SLOT_EDGE, 200, round(2 * S))

        # Riempimento dal basso, col colore della postazione
        fill_h = max(round(col_w * 0.8), round(bar_h * LEVELS[col]))
        fy = bar_top + bar_h - fill_h
        glow.alpha_composite(rounded((col_w, fill_h), bar_r, c + (150,)), (x, fy))

        # LED in cima
        glow.alpha_composite(rounded((col_w, led_h), col_r, c + (200,)), (x, top))

    img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(round(0.016 * side))))

    # Sopra l'alone, le superfici nitide
    for col in range(4):
        x = pad_x + col * (col_w + gap)
        c = LEDS[col]
        bright = tuple(min(255, round(v * 1.18)) for v in c)

        fill_h = max(round(col_w * 0.8), round(bar_h * LEVELS[col]))
        fy = bar_top + bar_h - fill_h
        paste_rounded_gradient(img, (x, fy, col_w, fill_h), bar_r, bright, c)
        paste_rounded_gradient(img, (x, top, col_w, led_h), col_r, bright, c)

    img.resize((OUT, OUT), Image.LANCZOS).save(os.path.join(HERE, "icon.png"))
    print("scritto", os.path.join(HERE, "icon.png"))


if __name__ == "__main__":
    main()
