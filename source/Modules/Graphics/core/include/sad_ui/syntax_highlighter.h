/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: syntax_highlighter.h
 * المسار: features/graphics/core/include/sad_ui/syntax_highlighter.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * تلوينٌ نحويٌّ خفيفٌ للغة ص — منطقٌ خالصٌ لا يمسُّ منصّةً ولا سطحَ رسم.
 *
 * (AR) كان يسكن خلفيّةَ سطحِ المكتب (`backends/desktop/src/text_editing.h`)
 *      فحُبِست عقدةُ «كتلة_كود» هناك ولم تُرسَم في أيِّ مسارٍ آخر. مكانُه
 *      المكتبةُ لا الخلفيّة: الخلفيّاتُ جسرٌ إلى العتاد، والمنطقُ في القلب.
 *
 * ═══════════════════════════════════════════════════════════════════════════════
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_SYNTAX_HIGHLIGHTER_H
#define SAD_UI_SYNTAX_HIGHLIGHTER_H

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace sad
{
    namespace ui
    {

        /**
         * @brief نوع الرمز في التلوين النحوي
         */
        enum class SyntaxTokenType : uint8_t
        {
            Normal,         ///< نص عادي
            Keyword,        ///< كلمة محجوزة (دالة، صنف، إذا...)
            ContextKeyword, ///< كلمة سياقية (غير_متزامن، لامدا...)
            TypeName,       ///< اسم نوع مدمج (رقم، نص، عشري...)
            String,         ///< نص حرفي "..."
            Number,         ///< رقم حرفي
            Comment,        ///< تعليق # أو #* *#
            Operator,       ///< عامل (+ - * / = == ...)
            Bracket,        ///< قوس ( ) [ ]
            Builtin,        ///< دالة مدمجة (اطبع، اطبع_سطر...)
            Boolean,        ///< صحيح / خطأ
            NullLiteral     ///< لاشيء
        };

        /**
         * @brief رمز ملوّن
         */
        struct SyntaxToken
        {
            size_t start;         ///< بداية الرمز (byte offset)
            size_t length;        ///< طول الرمز (bytes)
            SyntaxTokenType type; ///< نوع الرمز
        };

        /**
         * @brief ملوّن نحوي خفيف للغة ص
         *
         * يعمل على سطر واحد أو نص كامل ويُنتج قائمة رموز ملوّنة.
         */
        class SadSyntaxHighlighter
        {
        public:
            /// تحليل النص وإنتاج رموز ملوّنة
            std::vector<SyntaxToken> tokenize(const std::string &text) const;

            /// الحصول على لون لنوع رمز (RGBA 0-1)
            struct TokenColor
            {
                float r, g, b, a;
            };
            static TokenColor getColor(SyntaxTokenType type, bool darkTheme = true);

        private:
            /// هل الحرف بداية مُعرّف عربي أو لاتيني؟
            static bool isIdentStart(unsigned char c, const std::string &text, size_t pos);

            /// هل الحرف جزء من مُعرّف؟
            static bool isIdentPart(unsigned char c, const std::string &text, size_t pos);

            /// استخراج مُعرّف كامل بدءًا من موقع
            static std::string extractIdent(const std::string &text, size_t &pos);

            /// تصنيف مُعرّف
            static SyntaxTokenType classifyIdent(const std::string &ident);
        };

    } // namespace ui
} // namespace sad

#endif // SAD_UI_SYNTAX_HIGHLIGHTER_H
