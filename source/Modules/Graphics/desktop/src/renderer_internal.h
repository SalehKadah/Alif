/**
 * ═══════════════════════════════════════════════════════════════════
 * ملف: renderer_internal.h
 * المسار: features/graphics/backends/desktop/src/renderer_internal.h
 * ═══════════════════════════════════════════════════════════════════
 *
 * هيدر داخلي مشترك لملفات renderer*.cpp
 * يحتوي على الدوال المساعدة والأنواع والمتغيرات العامة المشتركة
 *
 * ═══════════════════════════════════════════════════════════════════
 */

#pragma once

#include "sad_ui/desktop/renderer.h"
#include "sad_ui/types.h"
#include "sad_ui/prop_keys.h" // مصدر الحقيقة لمفاتيح الخصائص (لا سلاسل حرفيّة)

#ifdef SAD_UI_USE_SDL2
#include <SDL.h>
// SDL_ttf.h يأتي من renderer.h (يُحدِّد فيه TTF_Font الحقيقي)
#endif

#include <cmath>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <algorithm>
#include <string>
#include <variant>
#include <cstdint>

namespace sad {
namespace ui {
namespace desktop {

// ═══════════════════════════════════════════════════════════════════
// كشف النص العربي
// ═══════════════════════════════════════════════════════════════════

inline bool isArabicUTF8(const std::string& text) {
    for (size_t i = 0; i + 1 < text.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c >= 0xD8 && c <= 0xDB) return true;
        if (c < 0x80) continue;
        if ((c & 0xE0) == 0xC0) { i += 1; continue; }
        if ((c & 0xF0) == 0xE0) { i += 2; continue; }
        if ((c & 0xF8) == 0xF0) { i += 3; continue; }
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════
// كشف الإيموجي
// ═══════════════════════════════════════════════════════════════════

inline std::pair<uint32_t, int> decodeUTF8Codepoint(const std::string& text, size_t pos) {
    if (pos >= text.size()) return {0, 0};
    unsigned char c = static_cast<unsigned char>(text[pos]);
    if (c < 0x80) return {c, 1};
    if ((c & 0xE0) == 0xC0 && pos + 1 < text.size()) {
        uint32_t cp = (c & 0x1F) << 6 | (static_cast<unsigned char>(text[pos+1]) & 0x3F);
        return {cp, 2};
    }
    if ((c & 0xF0) == 0xE0 && pos + 2 < text.size()) {
        uint32_t cp = (c & 0x0F) << 12
            | (static_cast<unsigned char>(text[pos+1]) & 0x3F) << 6
            | (static_cast<unsigned char>(text[pos+2]) & 0x3F);
        return {cp, 3};
    }
    if ((c & 0xF8) == 0xF0 && pos + 3 < text.size()) {
        uint32_t cp = (c & 0x07) << 18
            | (static_cast<unsigned char>(text[pos+1]) & 0x3F) << 12
            | (static_cast<unsigned char>(text[pos+2]) & 0x3F) << 6
            | (static_cast<unsigned char>(text[pos+3]) & 0x3F);
        return {cp, 4};
    }
    return {0xFFFD, 1};
}

inline bool isEmojiCodepoint(uint32_t cp) {
    if (cp >= 0x2600 && cp <= 0x27BF) return true;
    if (cp >= 0x1F600 && cp <= 0x1F64F) return true;
    if (cp >= 0x1F300 && cp <= 0x1F5FF) return true;
    if (cp >= 0x1F680 && cp <= 0x1F6FF) return true;
    if (cp >= 0x1F900 && cp <= 0x1F9FF) return true;
    if (cp >= 0x1FA00 && cp <= 0x1FA6F) return true;
    if (cp >= 0x1FA70 && cp <= 0x1FAFF) return true;
    if (cp >= 0x1F1E0 && cp <= 0x1F1FF) return true;
    if (cp == 0x2702 || cp == 0x2705 || cp == 0x2708 || cp == 0x2709 ||
        cp == 0x270A || cp == 0x270B || cp == 0x270C || cp == 0x270D ||
        cp == 0x270F || cp == 0x2712 || cp == 0x2714 || cp == 0x2716 ||
        cp == 0x271D || cp == 0x2721 || cp == 0x2728 || cp == 0x2733 ||
        cp == 0x2734 || cp == 0x2744 || cp == 0x2747 || cp == 0x274C ||
        cp == 0x274E || cp == 0x2753 || cp == 0x2754 || cp == 0x2755 ||
        cp == 0x2757 || cp == 0x2763 || cp == 0x2764 || cp == 0x2795 ||
        cp == 0x2796 || cp == 0x2797 || cp == 0x27A1 || cp == 0x27B0 ||
        cp == 0x23E9 || cp == 0x23EA || cp == 0x23EB || cp == 0x23EC ||
        cp == 0x23F0 || cp == 0x23F3 || cp == 0x25AA || cp == 0x25AB ||
        cp == 0x25B6 || cp == 0x25C0 || cp == 0x25FB || cp == 0x25FC ||
        cp == 0x25FD || cp == 0x25FE) return true;
    if (cp == 0xFE0F || cp == 0x200D) return true;
    return false;
}

inline bool containsEmoji(const std::string& text) {
    for (size_t i = 0; i < text.size();) {
        auto [cp, len] = decodeUTF8Codepoint(text, i);
        if (len == 0) break;
        if (isEmojiCodepoint(cp)) return true;
        i += len;
    }
    return false;
}

struct TextSegment {
    std::string text;
    bool isEmoji;
};

inline std::vector<TextSegment> splitTextByEmoji(const std::string& text) {
    std::vector<TextSegment> segments;
    std::string current;
    bool currentIsEmoji = false;
    for (size_t i = 0; i < text.size();) {
        auto [cp, len] = decodeUTF8Codepoint(text, i);
        if (len == 0) break;
        bool emoji = isEmojiCodepoint(cp);
        if (cp == 0xFE0F || cp == 0x200D) {
            current.append(text, i, len);
            i += len;
            continue;
        }
        if (emoji != currentIsEmoji && !current.empty()) {
            segments.push_back({current, currentIsEmoji});
            current.clear();
        }
        currentIsEmoji = emoji;
        current.append(text, i, len);
        i += len;
    }
    if (!current.empty()) {
        segments.push_back({current, currentIsEmoji});
    }
    return segments;
}

// ═══════════════════════════════════════════════════════════════════
// خط الإيموجي
// ═══════════════════════════════════════════════════════════════════

#ifdef SAD_UI_HAS_SDL_TTF
extern TTF_Font* g_emojiFont_;
extern std::unordered_map<int, TTF_Font*> g_emojiFontCache_;
#endif

inline TTF_Font* getEmojiFontForSize(int size) {
#ifdef SAD_UI_HAS_SDL_TTF
    if (size < 1) size = 16;
    auto it = g_emojiFontCache_.find(size);
    if (it != g_emojiFontCache_.end()) return it->second;
    const char* emojiPaths[] = {
#ifdef _WIN32
        "C:/Windows/Fonts/seguiemj.ttf",
        "C:/Windows/Fonts/segmdl2.ttf",
#elif defined(__APPLE__)
        "/System/Library/Fonts/Apple Color Emoji.ttc",
#else
        "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf",
        "/usr/share/fonts/noto/NotoColorEmoji.ttf",
        "/usr/share/fonts/truetype/noto/NotoEmoji-Regular.ttf",
#endif
        nullptr
    };
    TTF_Font* font = nullptr;
    for (int i = 0; emojiPaths[i] != nullptr; ++i) {
        font = TTF_OpenFont(emojiPaths[i], size);
        if (font) break;
    }
    if (font) {
        TTF_SetFontDirection(font, TTF_DIRECTION_LTR);
        g_emojiFontCache_[size] = font;
    }
    return font;
#else
    (void)size;
    return nullptr;
#endif
}

// ═══════════════════════════════════════════════════════════════════
// دوال مساعدة للقراءة من خصائص IR
// ═══════════════════════════════════════════════════════════════════

inline size_t utf8CharCount(const std::string& text) {
    size_t count = 0;
    for (size_t i = 0; i < text.size(); ++count) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) { i += 1; }
        else if ((c & 0xE0) == 0xC0) { i += 2; }
        else if ((c & 0xF0) == 0xE0) { i += 3; }
        else if ((c & 0xF8) == 0xF0) { i += 4; }
        else { i += 1; }
    }
    return count;
}

// (AR) حُذِفت من هنا ثلاثُ نسخٍ ميّتةٍ من محلّلاتِ الخصائصِ
//   (`getNumericProp` · `parseColorProp` · `getBoolProp`) — بقيّةُ توحيدِ #395،
//   بصفرِ استعمالٍ في `backends/desktop/src/*.cpp` كلِّها.
//   ولم يكن بقاؤُها حياديًّا: كانت **حقيقةً ثانيةً تنجرف** — نسخةُ `parseColorProp`
//   هنا بلا حارسِ `stoul` (تُنهي البرنامجَ على لونٍ معطوب) بعدَ أن حُرِست نسخةُ
//   القلب، ونسخةُ `getBoolProp` تقارنُ **سلاسلَ خامّةً** (`"صحيح"`/`"true"`/`"1"`/
//   `"مفعل"`) بينما نسخةُ القلبِ تقرأُ ثوابتَ `propval::` المولَّدة.
//   المرجعُ الوحيدُ الآن: `features/graphics/core/src/platform_renderer.cpp`.
// (EN) Three dead duplicates of the property parsers were removed here (the tail of
//   the #395 unification): zero uses across backends/desktop/src. They were not
//   harmless — a second truth that had already drifted (no stoul guard, raw
//   boolean literals instead of the generated propval:: constants).

// ═══════════════════════════════════════════════════════════════════
// (AR) بحثٌ موحَّد عن نصّ العنصر عبر قائمة مفاتيح مرتَّبة بالأولويّة.
//   يزيل تكرار سلسلة fallback المبعثرة في فروع العناصر (كان تكرارها يُدخِل
//   انحراف مفاتيح صامتًا: زرٌّ فارغ لعدم قراءة «عنوان»، وزرٌّ عائم بلا أيقونة
//   لخطأ إملائيّ في المفتاح). يحوّل القيم العدديّة إلى نصّ كي يَعرض زر(5)
//   نصَّه. يعيد true ويملأ out عند أوّل مفتاح موجود (ولو نصُّه فارغ).
//   القاعدة القانونيّة: مرّر «عنوان» أوّلًا فهو ما يكتبه مصنع زر() (SoT).
// ═══════════════════════════════════════════════════════════════════
inline bool findTextProp(const IRNode& node,
                         std::initializer_list<const char*> keys,
                         std::string& out) {
    for (const char* key : keys) {
        const IRProperty* prop = node.findProperty(key);
        if (!prop) continue;
        if (auto* s = std::get_if<std::string>(&prop->value)) { out = *s; return true; }
        if (auto* i = std::get_if<int64_t>(&prop->value)) { out = std::to_string(*i); return true; }
        if (auto* d = std::get_if<double>(&prop->value)) {
            std::string s = std::to_string(*d);
            // (AR) إزالة الأصفار الزائدة: «12.500000»⇒«12.5»، «12.000000»⇒«12».
            if (s.find('.') != std::string::npos) {
                s.erase(s.find_last_not_of('0') + 1);
                if (!s.empty() && s.back() == '.') s.pop_back();
            }
            out = s; return true;
        }
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════
// تخزين مؤقت للنصوص
// ═══════════════════════════════════════════════════════════════════

struct TextCacheKey {
    std::string text;
    float fontSize;
    uint32_t colorRGBA;
    bool operator==(const TextCacheKey& other) const {
        return text == other.text && fontSize == other.fontSize && colorRGBA == other.colorRGBA;
    }
};

struct TextCacheKeyHash {
    size_t operator()(const TextCacheKey& k) const {
        return std::hash<std::string>()(k.text) ^
               (std::hash<float>()(k.fontSize) << 1) ^
               (std::hash<uint32_t>()(k.colorRGBA) << 2);
    }
};

#ifdef SAD_UI_USE_SDL2
#ifdef SAD_UI_HAS_SDL_TTF
struct TextCacheEntry {
    SDL_Texture* texture = nullptr;
    int width = 0;
    int height = 0;
};

extern std::unordered_map<TextCacheKey, TextCacheEntry, TextCacheKeyHash> g_textCache_;
extern std::unordered_map<int, TTF_Font*> g_fontSizeCache_;
#endif
#endif

// تحويل Color (float 0-1) إلى Uint8 (0-255)
inline Uint8 toU8(float f) {
    return static_cast<Uint8>(f * 255.0f);
}

} // namespace desktop
} // namespace ui
} // namespace sad
