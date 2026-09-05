// ============================================================================
// (AR) تطبيع نصّ عربيّ لمكتبة الرسومات SadUI — تجريد التشكيل من المفردات النصّيّة.
// (EN) Arabic text normalization for the SadUI graphics library.
// ----------------------------------------------------------------------------
// (AR) المفردات النصّيّة التي يمرّرها مطوّر ص كوسائطَ (لا كأسماء طرق) — مثل
//   أنواع الحركة `.حرك("ظهور")` والمنحنيات `.منحنى("مرن")` وأسماء الأحداث بعد
//   البادئة — تصل المكتبةَ **بتشكيلها كما كُتب** (بخلاف المعرّفات التي يجرّد
//   المعجمُ تشكيلها). لتوحيد المطابقة (فيستوي `ترنّح` و`ترنح`) نجرّد التشكيل
//   هنا قبل المقارنة، فتحمل مصادرُ الحقيقة اسمًا قانونيًّا واحدًا بلا تشكيل.
//
//   ⚠️ يجب أن يطابق هذا المجرِّدُ سلوكَ المعجم المشترك تمامًا
//   (shared/lexer/src/lexer_core.cpp): يتخطّى بايت 0xD9 المتبوعَ ببايتٍ في
//   المدى [0x8B, 0x9F] — أي علامات التشكيل U+064B–U+065F (فتحة/ضمّة/كسرة/شدّة/
//   سكون/تنوين …). يبقى 0xD9 0x8A (ي = U+064A) وسائرُ الحروف كما هي.
//
// (EN) String vocabularies a Sad dev passes as ARGUMENTS (not method names) —
//   animation types, easing curves, event-name suffixes — reach the library
//   WITH their diacritics (unlike identifiers, which the lexer strips). To make
//   matching diacritic-insensitive (so `ترنّح` == `ترنح`) we strip diacritics
//   here before comparison, letting each SoT hold a single diacritic-free
//   canonical name. MUST mirror the shared lexer exactly: skip byte 0xD9
//   followed by a byte in [0x8B, 0x9F] (U+064B–U+065F).
// ============================================================================

#ifndef SAD_UI_TEXT_NORMALIZE_H
#define SAD_UI_TEXT_NORMALIZE_H

#include <string>

namespace sad
{
    namespace ui
    {
        /**
         * @brief (AR) يجرّد علامات التشكيل العربيّة (U+064B–U+065F) من سلسلة UTF-8.
         * @brief (EN) Strip Arabic diacritics (U+064B–U+065F) from a UTF-8 string.
         *
         * (AR) مطابقٌ حرفًا بحرف لتخطّي المعجم المشترك في lexer_core.cpp: أيّ
         *   بايت 0xD9 يتبعه بايتٌ في [0x8B, 0x9F] يُحذَف كاملًا (بايتان).
         */
        inline std::string stripArabicDiacritics(const std::string &in)
        {
            std::string out;
            out.reserve(in.size());
            for (size_t i = 0; i < in.size(); ++i)
            {
                unsigned char c = static_cast<unsigned char>(in[i]);
                if (c == 0xD9 && i + 1 < in.size())
                {
                    unsigned char next = static_cast<unsigned char>(in[i + 1]);
                    if (next >= 0x8B && next <= 0x9F)
                    {
                        ++i; // (AR) تخطِّ البايت الثاني للتشكيل — نحذف البايتين
                        continue;
                    }
                }
                out += in[i];
            }
            return out;
        }
    } // namespace ui
} // namespace sad

#endif // SAD_UI_TEXT_NORMALIZE_H
