/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: syntax_highlighter.cpp
 * المسار: features/graphics/core/src/syntax_highlighter.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * تنفيذُ الملوّنِ النحويِّ للغة ص — نُقِل من خلفيّةِ سطحِ المكتبِ إلى القلب
 * (منطقُ المكتبةِ في المكتبة، والخلفيّةُ جسر).
 *
 * ═══════════════════════════════════════════════════════════════════════════════
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/syntax_highlighter.h"
#include "sad_ui/generated/syntax_keywords_generated.h"

#include <algorithm>
#include <unordered_set>
#include <cstring>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// SadSyntaxHighlighter — تلوين نحوي للغة ص
// ═══════════════════════════════════════════════════════════════════════════════

bool SadSyntaxHighlighter::isIdentStart(unsigned char c, const std::string& text, size_t pos) {
    // حرف ASCII أو _ أو حرف عربي (UTF-8 يبدأ بـ 0xD8 أو 0xD9)
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') return true;
    if (c == 0xD8 || c == 0xD9) return true; // حروف عربية
    if (c >= 0xC0 && (c & 0xE0) == 0xC0) return true; // UTF-8 2-byte
    if (c >= 0xE0 && (c & 0xF0) == 0xE0) return true; // UTF-8 3-byte
    return false;
}

bool SadSyntaxHighlighter::isIdentPart(unsigned char c, const std::string& text, size_t pos) {
    if (isIdentStart(c, text, pos)) return true;
    if (c >= '0' && c <= '9') return true;
    // التشكيل العربي
    if ((c & 0xC0) == 0x80) return true; // continuation byte
    return false;
}

std::string SadSyntaxHighlighter::extractIdent(const std::string& text, size_t& pos) {
    size_t start = pos;
    while (pos < text.size()) {
        unsigned char c = static_cast<unsigned char>(text[pos]);
        if (!isIdentPart(c, text, pos)) break;
        if ((c & 0x80) == 0) { ++pos; continue; }
        // multi-byte UTF-8
        size_t len = 1;
        if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        pos += len;
    }
    return text.substr(start, pos - start);
}

SyntaxTokenType SadSyntaxHighlighter::classifyIdent(const std::string& ident) {
    // ═══════════════════════════════════════════════════════════════════
    // (AR) المفرداتُ من مصدرِ الحقيقةِ لا من جدولٍ مكتوبٍ بيد.
    //   كانت هذه الجداولُ يدويّةً فانحرفت عن language-truth/keywords.yaml
    //   بـ٣١ كلمةً محجوزةً مقيسة (أجّل، أنتج، حالة، فضاء، قالب، ماكرو،
    //   طبيعي…): كودُ ص يُعرَض في «كتلة_كود» بتلوينٍ ناقصٍ صامتًا — لا خطأَ
    //   بناءٍ ولا تشغيل، فقط كلماتٌ بلونِ النصِّ العاديّ. والعددُ المكتوبُ في
    //   التعليقِ («40») كان قد صار كاذبًا هو الآخر.
    // ═══════════════════════════════════════════════════════════════════
#define SAD_UI_SYNTAX_ENTRY(Word) Word,
    static const std::unordered_set<std::string> keywords = {
        SAD_UI_SYNTAX_KEYWORD_LIST(SAD_UI_SYNTAX_ENTRY)};
    static const std::unordered_set<std::string> logicalOps = {
        SAD_UI_SYNTAX_LOGICAL_OP_LIST(SAD_UI_SYNTAX_ENTRY)};
    static const std::unordered_set<std::string> contextKeywords = {
        SAD_UI_SYNTAX_CONTEXT_KEYWORD_LIST(SAD_UI_SYNTAX_ENTRY)};
    static const std::unordered_set<std::string> typeNames = {
        SAD_UI_SYNTAX_TYPE_NAME_LIST(SAD_UI_SYNTAX_ENTRY)};
#undef SAD_UI_SYNTAX_ENTRY
    static_assert(SAD_UI_SYNTAX_KEYWORD_LIST_COUNT > 0, "جردُ الكلماتِ المحجوزةِ فارغ");

    // (AR) القيمُ المنطقيّةُ والعدمُ والمدمجاتُ — من مصدرِ الحقيقةِ كذلك.
    //   وهذه الثلاثةُ ليست تحسينًا تجميليًّا: «صحيح/خطأ/لاشيء» تسكن تصنيفَ
    //   `reserved` في `keywords.yaml`، وفحصُ `classifyIdent` أدناه يبدأ
    //   بالمحجوزةِ — فلو وُلِّدت ضمنَها لعادت `Keyword` ولَما بلغت `Boolean`
    //   ولا `NullLiteral` أبدًا. المولّدُ يفصلُها بـ`tokenType` لا بالاسم.
#define SAD_UI_SYNTAX_ENTRY(Word) Word,
    static const std::unordered_set<std::string> booleans = {
        SAD_UI_SYNTAX_BOOLEAN_LIST(SAD_UI_SYNTAX_ENTRY)};
    static const std::unordered_set<std::string> nullLiterals = {
        SAD_UI_SYNTAX_NULL_LIST(SAD_UI_SYNTAX_ENTRY)};
    static const std::unordered_set<std::string> builtins = {
        SAD_UI_SYNTAX_BUILTIN_LIST(SAD_UI_SYNTAX_ENTRY)};
#undef SAD_UI_SYNTAX_ENTRY

    if (keywords.count(ident)) return SyntaxTokenType::Keyword;
    if (logicalOps.count(ident)) return SyntaxTokenType::Keyword;
    if (booleans.count(ident)) return SyntaxTokenType::Boolean;
    if (nullLiterals.count(ident)) return SyntaxTokenType::NullLiteral;
    if (contextKeywords.count(ident)) return SyntaxTokenType::ContextKeyword;
    if (typeNames.count(ident)) return SyntaxTokenType::TypeName;
    if (builtins.count(ident)) return SyntaxTokenType::Builtin;
    return SyntaxTokenType::Normal;
}

std::vector<SyntaxToken> SadSyntaxHighlighter::tokenize(const std::string& text) const {
    std::vector<SyntaxToken> tokens;
    size_t i = 0;

    while (i < text.size()) {
        unsigned char c = static_cast<unsigned char>(text[i]);

        // تخطي المسافات
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            ++i;
            continue;
        }

        // تعليق سطري #
        if (c == '#') {
            size_t start = i;
            // تعليق كتلة #* ... *#
            if (i + 1 < text.size() && text[i + 1] == '*') {
                i += 2;
                while (i + 1 < text.size()) {
                    if (text[i] == '*' && text[i + 1] == '#') {
                        i += 2;
                        break;
                    }
                    ++i;
                }
                if (i >= text.size()) i = text.size();
                tokens.push_back({start, i - start, SyntaxTokenType::Comment});
                continue;
            }
            // تعليق سطري
            while (i < text.size() && text[i] != '\n') ++i;
            tokens.push_back({start, i - start, SyntaxTokenType::Comment});
            continue;
        }

        // نصوص حرفية "..." أو '...'
        if (c == '"' || c == '\'') {
            size_t start = i;
            char quote = static_cast<char>(c);
            ++i;
            while (i < text.size() && text[i] != quote) {
                if (text[i] == '\\' && i + 1 < text.size()) ++i; // تخطي escaped
                ++i;
            }
            if (i < text.size()) ++i; // تخطي قوس الإغلاق
            tokens.push_back({start, i - start, SyntaxTokenType::String});
            continue;
        }

        // أرقام
        if (c >= '0' && c <= '9') {
            size_t start = i;
            while (i < text.size()) {
                unsigned char d = static_cast<unsigned char>(text[i]);
                if ((d >= '0' && d <= '9') || d == '.' || d == '_') ++i;
                else break;
            }
            tokens.push_back({start, i - start, SyntaxTokenType::Number});
            continue;
        }

        // مُعرّفات (عربية أو لاتينية)
        if (isIdentStart(c, text, i)) {
            size_t start = i;
            std::string ident = extractIdent(text, i);
            SyntaxTokenType type = classifyIdent(ident);
            tokens.push_back({start, i - start, type});
            continue;
        }

        // أقواس
        if (c == '(' || c == ')' || c == '[' || c == ']') {
            tokens.push_back({i, 1, SyntaxTokenType::Bracket});
            ++i;
            continue;
        }

        // عوامل
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
            c == '=' || c == '!' || c == '<' || c == '>' || c == '&' ||
            c == '|' || c == '^' || c == '~' || c == '?' || c == ':') {
            size_t start = i;
            ++i;
            // عوامل مزدوجة (==, !=, <=, >=, &&, ||, **, ...)
            if (i < text.size()) {
                char next = text[i];
                if ((c == '=' && next == '=') || (c == '!' && next == '=') ||
                    (c == '<' && next == '=') || (c == '>' && next == '=') ||
                    (c == '&' && next == '&') || (c == '|' && next == '|') ||
                    (c == '*' && next == '*') || (c == '+' && next == '=') ||
                    (c == '-' && next == '=') || (c == '*' && next == '=') ||
                    (c == '/' && next == '=')) {
                    ++i;
                }
            }
            tokens.push_back({start, i - start, SyntaxTokenType::Operator});
            continue;
        }

        // الفاصلة العربية ، أو الفاصلة اللاتينية
        if (c == ',' || (c == 0xD8 && i + 1 < text.size() && 
            static_cast<unsigned char>(text[i + 1]) == 0x8C)) {
            // الفاصلة العربية ، = 0xD8 0x8C
            size_t start = i;
            if (c == 0xD8) i += 2; else ++i;
            tokens.push_back({start, i - start, SyntaxTokenType::Operator});
            continue;
        }

        // أي شيء آخر — تخطي
        ++i;
    }

    return tokens;
}

SadSyntaxHighlighter::TokenColor SadSyntaxHighlighter::getColor(SyntaxTokenType type, bool darkTheme) {
    if (darkTheme) {
        switch (type) {
            case SyntaxTokenType::Keyword:        return {0.78f, 0.47f, 0.99f, 1.0f};  // بنفسجي فاتح
            case SyntaxTokenType::ContextKeyword:  return {0.78f, 0.47f, 0.85f, 1.0f};  // بنفسجي
            case SyntaxTokenType::TypeName:        return {0.31f, 0.82f, 0.82f, 1.0f};  // سماوي
            case SyntaxTokenType::String:          return {0.81f, 0.54f, 0.31f, 1.0f};  // برتقالي
            case SyntaxTokenType::Number:          return {0.71f, 0.84f, 0.43f, 1.0f};  // أخضر فاتح
            case SyntaxTokenType::Comment:         return {0.45f, 0.51f, 0.42f, 1.0f};  // رمادي أخضر
            case SyntaxTokenType::Operator:        return {0.86f, 0.86f, 0.86f, 1.0f};  // رمادي فاتح
            case SyntaxTokenType::Bracket:         return {1.00f, 0.84f, 0.40f, 1.0f};  // ذهبي
            case SyntaxTokenType::Builtin:         return {0.38f, 0.68f, 0.94f, 1.0f};  // أزرق
            case SyntaxTokenType::Boolean:         return {0.34f, 0.71f, 0.91f, 1.0f};  // أزرق فاتح
            case SyntaxTokenType::NullLiteral:     return {0.34f, 0.71f, 0.91f, 1.0f};  // أزرق فاتح
            default:                               return {0.86f, 0.86f, 0.86f, 1.0f};  // أبيض
        }
    } else {
        switch (type) {
            case SyntaxTokenType::Keyword:        return {0.51f, 0.11f, 0.68f, 1.0f};  // بنفسجي
            case SyntaxTokenType::ContextKeyword:  return {0.51f, 0.11f, 0.55f, 1.0f};
            case SyntaxTokenType::TypeName:        return {0.00f, 0.50f, 0.50f, 1.0f};  // سماوي غامق
            case SyntaxTokenType::String:          return {0.64f, 0.08f, 0.08f, 1.0f};  // أحمر قاتم
            case SyntaxTokenType::Number:          return {0.02f, 0.45f, 0.02f, 1.0f};  // أخضر
            case SyntaxTokenType::Comment:         return {0.42f, 0.48f, 0.39f, 1.0f};  // رمادي أخضر
            case SyntaxTokenType::Operator:        return {0.20f, 0.20f, 0.20f, 1.0f};  // أسود تقريباً
            case SyntaxTokenType::Bracket:         return {0.30f, 0.30f, 0.00f, 1.0f};  // بني غامق
            case SyntaxTokenType::Builtin:         return {0.00f, 0.30f, 0.60f, 1.0f};  // أزرق
            case SyntaxTokenType::Boolean:         return {0.00f, 0.00f, 0.80f, 1.0f};  // أزرق
            case SyntaxTokenType::NullLiteral:     return {0.00f, 0.00f, 0.80f, 1.0f};
            default:                               return {0.10f, 0.10f, 0.10f, 1.0f};  // أسود
        }
    }
}
} // namespace ui
} // namespace sad
