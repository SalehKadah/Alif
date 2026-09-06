#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""فحصُ مشاهدِ الرسومات — بلا مكتباتٍ خارجيّة.

لا نقارن صورةً بصورةٍ مرجعيّة: الخطوطُ تختلف بين ويندوز ولينكس وماك،
فالمقارنةُ البِتّيّةُ تفشل لأسبابٍ لا علاقةَ لها بصحّة الشيفرة. نتحقّق
بدلاً منها من أنّ الألوانَ المطلوبةَ بلغت البكسلَ بأعدادٍ معقولة — وهذا
يُثبت السلسلةَ كاملة: بناءٌ ⇒ تخطيطٌ ⇒ تصييرٌ ⇒ نافذةُ SDL.

الاستعمال: python3 tests/graphics/فحص.py <مسار alif>
"""

import os
import struct
import subprocess
import sys
import tempfile

# صدَفةُ ويندوز تُصدِر بـcp1252 افتراضاً، فتفشل عند أوّلِ محرفٍ عربيّ أو
# علامةِ صحّ. نُثبّت UTF-8 على المخرجَين قبل أيّ طباعة.
for _stream in (sys.stdout, sys.stderr):
    try:
        _stream.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass


def اقرأ_bmp(path):
    """يقرأ BMP ‏24/32 بت غيرَ مضغوط ويُعيد عدّاداً للألوان."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:2] != b"BM":
        raise ValueError("ليس ملفَّ BMP: " + path)
    offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    height = struct.unpack_from("<i", data, 22)[0]
    bpp = struct.unpack_from("<H", data, 28)[0]
    if bpp not in (24, 32):
        raise ValueError("عمقُ ألوانٍ غيرُ مدعوم: %d" % bpp)

    step = bpp // 8
    row_size = ((width * step + 3) // 4) * 4
    counts = {}
    for y in range(abs(height)):
        base = offset + y * row_size
        for x in range(width):
            i = base + x * step
            counts[(data[i + 2], data[i + 1], data[i])] = \
                counts.get((data[i + 2], data[i + 1], data[i]), 0) + 1
    return abs(width), abs(height), counts


def hex_to_rgb(h):
    h = h.lstrip("#")
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16))


# المشهد: (الملفّ، اللقطة، [(لون، أدنى عددِ بكسلات)])
المشاهد = [
    (
        "مشهد_ألوان.alif",
        "مشهد_ألوان.bmp",
        [("#0b1a2e", 5000), ("#ffd479", 40), ("#e6e6e6", 20), ("#3a4256", 500)],
    ),
    (
        "مشهد_عناصر.alif",
        "مشهد_عناصر.bmp",
        [("#0b1a2e", 5000), ("#ffd479", 40), ("#3a4256", 500)],
    ),
    (
        "مشهد_دالة_بناء.alif",
        "مشهد_دالة_بناء.bmp",
        [("#101820", 3000), ("#22cc22", 40)],
    ),
]


def main():
    if len(sys.argv) < 2:
        print("الاستعمال: فحص.py <مسار alif>")
        return 2
    alif = os.path.abspath(sys.argv[1])
    if not os.path.exists(alif):
        print("لا يوجد مُنفَّذ: " + alif)
        return 2

    here = os.path.dirname(os.path.abspath(__file__))
    فشل = 0

    with tempfile.TemporaryDirectory() as work:
        for scene, shot, expected in المشاهد:
            src = os.path.join(here, scene)
            r = subprocess.run([alif, src], cwd=work,
                               capture_output=True, timeout=120)
            if r.returncode != 0:
                print("✗ %s — رمزُ الخروج %d" % (scene, r.returncode))
                sys.stdout.write(r.stdout.decode("utf-8", "replace"))
                sys.stdout.write(r.stderr.decode("utf-8", "replace"))
                فشل += 1
                continue

            path = os.path.join(work, shot)
            if not os.path.exists(path):
                print("✗ %s — لم تُكتب اللقطة" % scene)
                فشل += 1
                continue

            w, h, counts = اقرأ_bmp(path)
            مفقود = []
            for hx, أدنى in expected:
                n = counts.get(hex_to_rgb(hx), 0)
                if n < أدنى:
                    مفقود.append("%s: %d < %d" % (hx, n, أدنى))
            if مفقود:
                print("✗ %s (%dx%d) — %s" % (scene, w, h, " · ".join(مفقود)))
                فشل += 1
            else:
                print("✓ %s (%dx%d، %d لوناً مميّزاً)" % (scene, w, h, len(counts)))

    if فشل:
        print("فشل %d من %d" % (فشل, len(المشاهد)))
        return 1
    print("نجح %d من %d" % (len(المشاهد), len(المشاهد)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
