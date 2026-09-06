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


# المشهد: (الملفّ، اللقطة، [(لون، أدنى، أقصى)]) — والأقصى اختياريّ.
#
# الحدُّ الأعلى ليس ترفاً: بحدٍّ أدنى وحدَه يمرّ المشهدُ لو ابتلع لونٌ واحدٌ
# الشاشةَ كلَّها — وهو بالضبط ما يحدث حين ينهار التخطيطُ فتُرسَم خلفيّةٌ
# على كلّ شيء. الحدّان معاً يقولان: اللونُ حاضرٌ، وبقدره.
#
# لكنّه **لألوانِ المساحاتِ لا لألوانِ النصّ**. مساحةُ الخلفيّةِ تتبع
# التخطيطَ وهو واحدٌ في المنصّات، أمّا بكسلاتُ الحرفِ فتتبع الخطَّ المتاح:
# قِسنا لونَ العنوانِ ٧٤٧ بكسلاً على ويندوز و٢٥٠٦ على ماك — ثلاثةَ أضعافٍ
# بلا فرقِ شيفرةٍ واحد. فحدٌّ أعلى على النصّ بوّابةٌ تُخفق بلا انحدار.
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
        "مشهد_سطح.alif",
        "مشهد_سطح.bmp",
        [("#0b1a2e", 50000), ("#1e88e5", 2000), ("#bdbdbd", 2000), ("#1e1e23", 500)],
    ),
    (
        "مشهد_دالة_بناء.alif",
        "مشهد_دالة_بناء.bmp",
        [("#101820", 3000), ("#22cc22", 40)],
    ),
    # المشهدُ المركَّب: جدولٌ وتقويمٌ وألسنة. الحدودُ من قياسٍ فعليّ
    # (ويندوز، إصدار) بهامشِ ±٤٠٪ لاختلافِ الخطوط بين المنصّات.
    (
        "مشهد_مركب.alif",
        "مشهد_مركب.bmp",
        [
            ("#2b3346", 12000, 30000),   # رؤوسُ الجدول ورؤوسُ أيّامِ التقويم
            ("#1b2437", 38000, 90000),   # صفوفٌ زوجيّةٌ وخلايا التقويم
            ("#141c2c", 27000, 66000),   # صفوفٌ فرديّةٌ وأرضيّةُ التقويم
            ("#1e88e5", 3800, 9800),     # اللسانُ المختارُ واليومُ المحدَّد
            ("#3a4256", 6600, 16000),    # الألسنةُ غيرُ المختارة
            ("#ffd479", 400),            # العنوانُ وخطُّ إبرازِ اللسان — بلا حدٍّ أعلى
            ("#0b1a2e", 100000, 200000), # السطح
        ],
    ),
]


def فحص_التفاعل(alif, here, work):
    """يُطلق ثلاثَ مئةِ نقرةٍ حقيقيّةٍ ويتحقّق ممّا لا تراه اللقطات.

    اللقطاتُ تُثبت أنّ الشجرةَ تبلغ البكسل. ولا تُثبت شيئاً عن المسار
    الذي يبدأ من نقرةٍ: نداءُ المعالِج، وإعادةُ البناء، والمطابقة،
    والترقيع، وتنظيفُ سجلّ المعالِجات. وهناك كان الاستعمالُ بعد التحرير،
    وهناك كان السجلُّ ينمو بلا حدّ.
    """
    src = os.path.join(here, "قياس_قصير.alif")
    r = subprocess.run([alif, src], cwd=work, capture_output=True, timeout=300)
    out = r.stdout.decode("utf-8", "replace")
    if r.returncode != 0:
        print("✗ قياس_قصير.alif — رمزُ الخروج %d" % r.returncode)
        sys.stdout.write(out)
        sys.stdout.write(r.stderr.decode("utf-8", "replace"))
        return 1

    قيم = {}
    for line in out.splitlines():
        if "=" in line:
            k, _, v = line.partition("=")
            قيم[k.strip()] = v.strip()

    أخطاء = []
    if قيم.get("نقرات") != "300":
        أخطاء.append("نُودي المعالِجُ %s مرّةً لا ٣٠٠" % قيم.get("نقرات"))
    # المعالِجُ واحدٌ في هذه الشجرة. أيُّ عددٍ أكبرَ يعني أنّ `pruneHandlers`
    # لم يُسقط أجيالَ المعالِجات السابقة — وهو التسريبُ نفسُه الذي أُصلح.
    if قيم.get("معالجات") != "1":
        أخطاء.append("بقي %s معالِجاً لا واحد" % قيم.get("معالجات"))
    # الوسيطُ لا الأقصى: الأقصى عيّنةٌ واحدةٌ يحكمها جدولُ نظامِ التشغيل.
    # قِسناه فبلغ ٥٧٫٨ م.ث في بناءِ التصحيحِ بمُطهِّرِ العناوين ووسيطُه
    # ١٦٫٩ — فحدٌّ على الأقصى بوّابةٌ تُخفق بلا انحدار. والحدُّ فضفاضٌ
    # عمداً (‏٢٠٠ م.ث): الغرضُ إمساكُ انهيارٍ فادحٍ لا معايرةُ عدّاء.
    try:
        وسيط = float(قيم.get("وسيط_العمل", "999"))
        if وسيط > 200.0:
            أخطاء.append("وسيطُ زمنِ العمل %.2f م.ث > 200" % وسيط)
    except ValueError:
        أخطاء.append("تعذّرت قراءةُ زمنِ العمل")

    if أخطاء:
        print("✗ قياس_قصير.alif — %s" % " · ".join(أخطاء))
        sys.stdout.write(out)
        return 1
    print("✓ قياس_قصير.alif (٣٠٠ نقرة، وسيطُ العمل %s م.ث، معالِجٌ باقٍ %s)"
          % (قيم.get("وسيط_العمل"), قيم.get("معالجات")))
    return 0


def فحص_الذاكرة(alif, here, work):
    """عشرةُ آلافِ شجرةٍ تُبنى وتُهمَل — كم يبقى؟

    هذا الفحصُ هو ما فرّق بين تسريبٍ في وحدةِ الرسومات وتسريبٍ في ألفَ
    نفسِها. الدورةُ التفاعليّةُ الكاملةُ كانت تنمو ٤٫٧ ك.ب لكلّ نقرة،
    فبدا الظنُّ أنّ الشجرةَ لا تُحرَّر. والقياسُ بالطبقاتِ قال غيرَ ذلك:
    البناءُ وحدَه بنصوصٍ ثابتةٍ **صفرٌ**، والنموُّ كلُّه من تحويلِ حقلٍ
    غيرِ نصّيٍّ في `م"…"` — وهو يُقاس بلا رسومات أصلاً.
    """
    # مُطهِّرُ العناوين يحتجز المحرَّرَ في حَجْرٍ ويُحيط كلَّ تخصيصٍ بهوامش،
    # فينمو المقيمُ بلا تسريب: قِسناه ٦٨٦ م.ب في بناءِ التصحيحِ على ويندوز
    # مقابلَ صفرٍ في الإصدار. فالقياسُ هنا لا يقيس ما نريد — نتخطّاه
    # صراحةً بدل أن نرفع الحدَّ حتّى يفقد معناه في البناءِ العاديّ.
    with open(alif, "rb") as f:
        إن_مطهر = b"clang_rt.asan" in f.read()
    if إن_مطهر or os.environ.get("ASAN_OPTIONS"):
        print("• فحص_ذاكرة.alif — تُخطِّي: بناءٌ بمُطهِّرِ العناوين، والمقيمُ فيه لا يدلّ")
        return 0

    src = os.path.join(here, "فحص_ذاكرة.alif")
    r = subprocess.run([alif, src], cwd=work, capture_output=True, timeout=900)
    out = r.stdout.decode("utf-8", "replace")
    if r.returncode != 0:
        print("✗ فحص_ذاكرة.alif — رمزُ الخروج %d" % r.returncode)
        sys.stdout.write(out)
        sys.stdout.write(r.stderr.decode("utf-8", "replace"))
        return 1

    قيم = {}
    for line in out.splitlines():
        if "=" in line:
            k, _, v = line.partition("=")
            قيم[k.strip()] = v.strip()
    try:
        فرق = float(قيم.get("فرق_ك.ب", "999999"))
    except ValueError:
        print("✗ فحص_ذاكرة.alif — تعذّرت قراءةُ الفرق")
        sys.stdout.write(out)
        return 1

    # قِسناه صفراً على ويندوز. والحدُّ ٨ م.ب لا صفرٌ: مُخصِّصُ النظامِ
    # يحتفظ بكتلٍ محرَّرةٍ ولا يُعيدها، فحدٌّ صفريٌّ بوّابةٌ تُخفق بلا
    # تسريب. ولو عاد نموُّ العقدةِ الواحدةِ لبلغ الفرقُ ٦٣٠ ألفَ عقدةٍ
    # × عشراتِ البايتات — أضعافَ هذا الحدّ.
    if فرق > 8192.0:
        print("✗ فحص_ذاكرة.alif — نمَت الذاكرةُ %.0f ك.ب > 8192" % فرق)
        sys.stdout.write(out)
        return 1
    print("✓ فحص_ذاكرة.alif (١٠٠٠٠ شجرةٍ × ٦٣ عقدة، الفرق %.0f ك.ب)" % فرق)
    return 0


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
            for توقع in expected:
                hx, أدنى = توقع[0], توقع[1]
                أقصى = توقع[2] if len(توقع) > 2 else None
                n = counts.get(hex_to_rgb(hx), 0)
                if n < أدنى:
                    مفقود.append("%s: %d < %d" % (hx, n, أدنى))
                elif أقصى is not None and n > أقصى:
                    مفقود.append("%s: %d > %d" % (hx, n, أقصى))
            if مفقود:
                print("✗ %s (%dx%d) — %s" % (scene, w, h, " · ".join(مفقود)))
                فشل += 1
            else:
                print("✓ %s (%dx%d، %d لوناً مميّزاً)" % (scene, w, h, len(counts)))

        فشل += فحص_التفاعل(alif, here, work)
        فشل += فحص_الذاكرة(alif, here, work)

    الكل = len(المشاهد) + 2
    if فشل:
        print("فشل %d من %d" % (فشل, الكل))
        return 1
    print("نجح %d من %d" % (الكل, الكل))
    return 0


if __name__ == "__main__":
    sys.exit(main())
