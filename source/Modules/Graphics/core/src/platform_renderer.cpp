/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: platform_renderer.cpp
 * المسار: features/graphics/core/src/platform_renderer.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * المنطق المشترك لرسم عناصر الواجهة (renderNode).
 *
 * هذا الملف يحتوي على تنفيذ render() و renderNode() المشترك
 * عبر جميع المنصات (Desktop, Web, Android, iOS, macOS).
 *
 * كل منصة تُطبق فقط ~15 دالة رسم أساسية (drawFilledRect, drawText, etc.)
 * والمنطق هنا يستخدمها لرسم 40+ نوع عنصر واجهة.
 *
 * ═══════════════════════════════════════════════════════════════════════════════
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/platform_renderer.h"
#include "sad_ui/types.h"
#include "sad_ui/node.h"
#include "sad_ui/prop_keys.h"          // مفاتيح الخصائص القانونيّة (SoT) — لا سلاسلَ حرفيّةً خامّة
#include "sad_ui/syntax_highlighter.h" // تلوين «كتلة_كود» — منطقُ مكتبةٍ لا خلفيّة

#include <vector>

#include <cmath>
#include <string>
#include <algorithm>
#include <chrono>

// Platform-specific includes for PlatformFactory
#ifdef __EMSCRIPTEN__
#include "sad_ui/web/web_renderer.h"
#endif
#ifdef __ANDROID__
#include "sad_ui/android/android_renderer.h"
#endif
#ifdef __APPLE__
#include <TargetConditionals.h>
#if TARGET_OS_IOS
#include "sad_ui/ios/ios_renderer.h"
#elif defined(SAD_UI_ENABLE_MACOS_NATIVE)
// (AR) خلفية macOS الأصلية غير مكتملة (macos_renderer.mm يخالف ترويسته) — معطّلة
//      افتراضياً؛ يُستخدم عارض SDL2 لسطح المكتب بدلاً منها.
#include "sad_ui/macos/macos_renderer.h"
#endif
#endif

namespace sad
{
    namespace ui
    {

        // (AR) قِيَمُ الخصائصِ القانونيّةُ (namespace propval) تأتي **مولَّدةً** من
        //      مصدرِ الحقيقةِ ضمنَ prop_keys.h. كانت تُكتَب هنا يدويًّا ونظيرتُها
        //      تُكتَب حرفيّةً في مُرسِّمِ سطحِ المكتبِ والمولّدات، فتباعدت القوائمُ
        //      صامتةً. لا تُعِد تعريفَها هنا — أضِفها إلى language-truth/ui_props.yaml.

        // ═══════════════════════════════════════════════════════════════════════════
        // نصوصٌ ورموزٌ معروضةٌ افتراضيّةٌ (لا سلاسلَ حرفيّةً خامّةً في مواضع الرسم)
        // ═══════════════════════════════════════════════════════════════════════════
        namespace uitext
        {
            constexpr const char *PLAY_GLYPH = "\xe2\x96\xb6";                              // ▶
            constexpr const char *CALENDAR_GLYPH = "\xf0\x9f\x93\x85";                      // 📅
            constexpr const char *GLOBE_GLYPH = "\xf0\x9f\x8c\x90";                         // 🌐
            constexpr const char *ZERO_DURATION = "00:00";                                  // مؤقّت صفريّ
            constexpr const char *TAB_LABEL_PREFIX_AR = "\xd9\x82\xd8\xb3\xd9\x85 ";          // «قسم »
            constexpr const char *ITEM_AR = "\xd8\xb9\xd9\x86\xd8\xb5\xd8\xb1";             // «عنصر»
        } // namespace uitext

        // ═══════════════════════════════════════════════════════════════════════════════
        // دوال مساعدة مشتركة
        // ═══════════════════════════════════════════════════════════════════════════════

        // قراءة قيمة عددية من خاصية
        static float getNumericProp(const IRProperty *prop, float defaultVal = 0.0f)
        {
            if (!prop)
                return defaultVal;
            if (auto *v = std::get_if<double>(&prop->value))
                return static_cast<float>(*v);
            if (auto *vi = std::get_if<int64_t>(&prop->value))
                return static_cast<float>(*vi);
            return defaultVal;
        }

        // (AR) قراءةُ رقمٍ سداسيَّ عشريٍّ **بلا استثناء**: `std::stoul` ترمي
        //   `std::invalid_argument` على أوّلِ محرفٍ ليس رقمًا سداسيًّا، ولا مُلتقِطَ
        //   لها في مسارِ الرسمِ ⇒ `.خلفية("#zzzzzz")` **تُنهي البرنامجَ كلَّه** بدل
        //   الرجوعِ إلى اللونِ الافتراضيّ. والمفسّرُ لا ينهار: لا يفكُّ اللونَ أصلًا
        //   إلّا عندَ الرسم. فالفحصُ المسبقُ يجعلُ اللونَ المعطوبَ **خطأً في القيمةِ
        //   لا في حياةِ العمليّة**.
        // (EN) Parse hex WITHOUT throwing: std::stoul throws on the first non-hex
        //   char and nothing catches it on the render path, so a malformed colour
        //   terminates the whole program instead of falling back to the default.
        static bool tryParseHexDigits(const std::string &digits, unsigned int &out)
        {
            if (digits.empty() || digits.size() > 8)
                return false;
            unsigned int value = 0;
            for (char c : digits)
            {
                unsigned int digit;
                if (c >= '0' && c <= '9')
                    digit = static_cast<unsigned int>(c - '0');
                else if (c >= 'a' && c <= 'f')
                    digit = static_cast<unsigned int>(c - 'a') + 10u;
                else if (c >= 'A' && c <= 'F')
                    digit = static_cast<unsigned int>(c - 'A') + 10u;
                else
                    return false;
                value = (value << 4) | digit;
            }
            out = value;
            return true;
        }

        // تحويل نص لوني إلى Color
        static Color parseColorProp(const IRProperty *prop, const Color &defaultColor)
        {
            if (!prop)
                return defaultColor;
            auto *cs = std::get_if<std::string>(&prop->value);
            if (!cs)
                return defaultColor;
            auto nc = arabicNameToColor(*cs);
            if (nc)
                return Color::fromNamed(*nc);
            unsigned int hex = 0;
            if (cs->size() == 7 && (*cs)[0] == '#' && tryParseHexDigits(cs->substr(1), hex))
            {
                return {static_cast<float>((hex >> 16) & 0xFF) / 255.0f,
                        static_cast<float>((hex >> 8) & 0xFF) / 255.0f,
                        static_cast<float>(hex & 0xFF) / 255.0f, 1.0f};
            }
            if (cs->size() == 9 && (*cs)[0] == '#' && tryParseHexDigits(cs->substr(1), hex))
            {
                return {static_cast<float>((hex >> 24) & 0xFF) / 255.0f,
                        static_cast<float>((hex >> 16) & 0xFF) / 255.0f,
                        static_cast<float>((hex >> 8) & 0xFF) / 255.0f,
                        static_cast<float>(hex & 0xFF) / 255.0f};
            }
            return defaultColor;
        }

        // قراءة قيمة منطقية
        static bool getBoolProp(const IRProperty *prop, bool defaultVal = false)
        {
            if (!prop)
                return defaultVal;
            if (auto *b = std::get_if<bool>(&prop->value))
                return *b;
            if (auto *s = std::get_if<std::string>(&prop->value))
            {
                return (*s == propval::BOOL_TRUE_AR ||
                        *s == propval::BOOL_TRUE_EN || *s == propval::BOOL_TRUE_ONE ||
                        *s == propval::BOOL_ENABLED_AR);
            }
            if (auto *i = std::get_if<int64_t>(&prop->value))
                return *i != 0;
            return defaultVal;
        }

        // ═══════════════════════════════════════════════════════════════════════════
        // (AR) بحثٌ موحَّدٌ عن نصّ العنصر عبر قائمةِ مفاتيحَ مرتَّبةٍ بالأولويّة.
        //   يزيل تكرارَ سلسلةِ الاحتياطِ المبعثرةِ في فروعِ العناصر (كان تكرارُها
        //   يُدخِل انحرافَ مفاتيحَ صامتًا: زرٌّ فارغٌ لعدمِ قراءةِ «عنوان»). يحوّل
        //   القيمَ العدديّةَ نصًّا كي يَعرضَ زر(5) نصَّه. يعيد true عند أوّلِ مفتاحٍ
        //   موجود (ولو كان نصُّه فارغًا).
        // ═══════════════════════════════════════════════════════════════════════════
        static bool findTextProp(const IRNode &node,
                                 std::initializer_list<const char *> keys,
                                 std::string &out)
        {
            for (const char *key : keys)
            {
                const IRProperty *prop = node.findProperty(key);
                if (!prop)
                    continue;
                if (auto *s = std::get_if<std::string>(&prop->value))
                {
                    out = *s;
                    return true;
                }
                if (auto *i = std::get_if<int64_t>(&prop->value))
                {
                    out = std::to_string(*i);
                    return true;
                }
                if (auto *d = std::get_if<double>(&prop->value))
                {
                    std::string s = std::to_string(*d);
                    // (AR) إزالة الأصفار الزائدة: «12.500000»⇒«12.5»، «12.000000»⇒«12».
                    if (s.find('.') != std::string::npos)
                    {
                        s.erase(s.find_last_not_of('0') + 1);
                        if (!s.empty() && s.back() == '.')
                            s.pop_back();
                    }
                    out = s;
                    return true;
                }
            }
            return false;
        }

        // الحصول على الوقت الحالي بالمللي ثانية (بديل محمول لـ SDL_GetTicks)
        static uint32_t getCurrentTimeMs()
        {
            auto now = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch());
            return static_cast<uint32_t>(ms.count());
        }

        // ═══════════════════════════════════════════════════════════════════════════════
        // PlatformRenderer — دوال مساعدة
        // ═══════════════════════════════════════════════════════════════════════════════

        bool PlatformRenderer::isArabicText(const std::string &text) const
        {
            for (size_t i = 0; i + 1 < text.size(); ++i)
            {
                unsigned char c = static_cast<unsigned char>(text[i]);
                if (c >= 0xD8 && c <= 0xDB)
                    return true;
                if (c < 0x80)
                    continue;
                if ((c & 0xE0) == 0xC0)
                {
                    i += 1;
                    continue;
                }
                if ((c & 0xF0) == 0xE0)
                {
                    i += 2;
                    continue;
                }
                if ((c & 0xF8) == 0xF0)
                {
                    i += 3;
                    continue;
                }
            }
            return false;
        }

        size_t PlatformRenderer::utf8Length(const std::string &text) const
        {
            size_t count = 0;
            for (size_t i = 0; i < text.size(); ++count)
            {
                unsigned char c = static_cast<unsigned char>(text[i]);
                if (c < 0x80)
                    i += 1;
                else if ((c & 0xE0) == 0xC0)
                    i += 2;
                else if ((c & 0xF0) == 0xE0)
                    i += 3;
                else if ((c & 0xF8) == 0xF0)
                    i += 4;
                else
                    i += 1;
            }
            return count;
        }

        // ═══════════════════════════════════════════════════════════════════════════════
        // drawSoftShadow() — ظلٌّ ناعمٌ متعدّد الطبقات (مشترك: أوّليّاتٌ فقط)
        // ═══════════════════════════════════════════════════════════════════════════════

        void PlatformRenderer::drawSoftShadow(const LayoutRect &rect, float radius, float blur,
                                              float offsetX, float offsetY, const Color &color)
        {
            if (blur <= 0)
                return;

            int layers = static_cast<int>(blur / 2.0f);
            if (layers < 3)
                layers = 3;
            if (layers > 12)
                layers = 12;

            for (int i = layers; i >= 1; --i)
            {
                float spread = blur * static_cast<float>(i) / static_cast<float>(layers);
                float alpha = color.a * (1.0f - static_cast<float>(i) / static_cast<float>(layers + 1));
                alpha *= 0.5f;

                Color layerColor = {color.r, color.g, color.b, alpha};
                drawRoundedRect(rect.x + offsetX - spread, rect.y + offsetY - spread,
                                rect.width + spread * 2, rect.height + spread * 2,
                                layerColor, radius + spread * 0.5f);
            }
        }

        // ═══════════════════════════════════════════════════════════════════════════════
        // render() — نقطة الدخول: رسم شجرة كاملة
        // ═══════════════════════════════════════════════════════════════════════════════

        void PlatformRenderer::render(const std::shared_ptr<IRNode> &root,
                                      const std::shared_ptr<LayoutResult> &layout)
        {
            if (!root || !layout)
                return;

            // (AR) صفِّر إزاحةَ التمريرِ المتراكمةَ في مطلعِ كلِّ إطار: هي حالةٌ
            //      تُستعاد بعدَ كلِّ حاويةٍ في المسارِ الطبيعيّ، لكنّ تصفيرَها هنا
            //      يمنع تسرُّبَ قيمةٍ بائتةٍ إلى إطارٍ تالٍ مهما انتهى السابق.
            scrollTranslateX_ = 0.0f;
            scrollTranslateY_ = 0.0f;

            renderNode(*root, *layout);
        }

        // ═══════════════════════════════════════════════════════════════════════════════
        // renderNode() — المنطق المشترك لرسم كل أنواع العناصر
        // ═══════════════════════════════════════════════════════════════════════════════

        void PlatformRenderer::renderNode(const IRNode &node,
                                          const LayoutResult &layout)
        {
            const auto &origRect = layout.rect;

            // ─── 0.A تطبيق تحويلات الانيميشن (إزاحة + مقياس + شفافية + دوران) ───
            float animOpacity = 1.0f;
            float animTranslateX = 0.0f;
            float animTranslateY = 0.0f;
            float animScale = 1.0f;
            float animRotation = 0.0f;

            if (animationEngine_ && !node.getId().empty())
            {
                const std::string &nid = node.getId();
                if (animationEngine_->hasAnimation(nid, "__opacity"))
                    animOpacity = static_cast<float>(animationEngine_->getValue(nid, "__opacity", 1.0));
                if (animationEngine_->hasAnimation(nid, "__translateX"))
                    animTranslateX = static_cast<float>(animationEngine_->getValue(nid, "__translateX", 0.0));
                if (animationEngine_->hasAnimation(nid, "__translateY"))
                    animTranslateY = static_cast<float>(animationEngine_->getValue(nid, "__translateY", 0.0));
                if (animationEngine_->hasAnimation(nid, "__scale"))
                    animScale = static_cast<float>(animationEngine_->getValue(nid, "__scale", 1.0));
                if (animationEngine_->hasAnimation(nid, "__rotation"))
                    animRotation = static_cast<float>(animationEngine_->getValue(nid, "__rotation", 0.0));
            }

            // حساب المستطيل النهائي بعد تطبيق التحويلات
            // (AR) إزاحةُ التمريرِ المتراكمةُ تُطبَّق هنا لا على مستطيلِ الابنِ وحدَه،
            //      كي تَسريَ على الأحفادِ أيضًا وعلى القصِّ خارجَ منفذِ العرض.
            LayoutRect rect = origRect;
            rect.x += scrollTranslateX_;
            rect.y += scrollTranslateY_;
            rect.x += animTranslateX;
            rect.y += animTranslateY;

            // تطبيق المقياس حول مركز العنصر
            if (animScale != 1.0f && animScale > 0.0f)
            {
                float cx = rect.x + rect.width / 2.0f;
                float cy = rect.y + rect.height / 2.0f;
                float newW = rect.width * animScale;
                float newH = rect.height * animScale;
                rect.x = cx - newW / 2.0f;
                rect.y = cy - newH / 2.0f;
                rect.width = newW;
                rect.height = newH;
            }

            // إخفاء العنصر بالكامل إذا شفافيته 0
            if (animOpacity <= 0.001f)
                return;

            // 0. قص العناصر خارج منفذ العرض (Viewport Culling)
            if (viewportWidth_ > 0 && viewportHeight_ > 0)
            {
                float vpW = static_cast<float>(viewportWidth_);
                float vpH = static_cast<float>(viewportHeight_);
                if (rect.x > vpW || rect.y > vpH ||
                    (rect.x + rect.width) < 0 || (rect.y + rect.height) < 0)
                {
                    return; // العنصر خارج الشاشة بالكامل — تخطيه
                }
            }

            // 0.5 حالة التفاعل (hover / press)
            bool isHovered = (hoveredNode_ == &node);
            bool isPressed = (pressedNode_ == &node);

            // 0.6 حساب الشفافية (خاصية الشفافية + شفافية الانيميشن)
            float nodeOpacity = getNumericProp(node.findProperty(props::OPACITY), 1.0f); // شفافية
            if (nodeOpacity < 0.0f)
                nodeOpacity = 0.0f;
            if (nodeOpacity > 1.0f)
                nodeOpacity = 1.0f;
            nodeOpacity *= animOpacity; // دمج شفافية الانيميشن مع الشفافية الأصلية
            currentNodeOpacity_ = nodeOpacity;

            // 0.7 تخزين زاوية الدوران الحالية (للمنصات التي تدعمه)
            currentNodeRotation_ = animRotation;

            // 0.8 تطبيق تحويل الدوران حول مركز العنصر (إن وُجد)
            bool rotationApplied = false;
            if (currentNodeRotation_ != 0.0f)
            {
                float cx = rect.x + rect.width / 2.0f;
                float cy = rect.y + rect.height / 2.0f;
                pushRotation(cx, cy, currentNodeRotation_);
                rotationApplied = true;
            }

            // 0.9 تطبيق تحويل التكبير/التصغير (إن وُجد) — إضافة لتعديل rect السابق
            bool scaleApplied = false;
            if (animScale != 1.0f && animScale > 0.0f)
            {
                float cx = rect.x + rect.width / 2.0f;
                float cy = rect.y + rect.height / 2.0f;
                pushScale(cx, cy, animScale, animScale);
                scaleApplied = true;
            }

            // 1. رسم الظل (إن وجد) — قبل الخلفية للتطبيق الصحيح
            const auto *shadowProp = node.findProperty(props::SHADOW); // ظل
            if (shadowProp)
            {
                float elevation = getNumericProp(shadowProp, 0.0f);
                if (isHovered && !isPressed)
                    elevation += 1.5f;
                if (isPressed)
                    elevation = std::max(0.5f, elevation - 1.0f);
                if (elevation > 0)
                {
                    // ظل متعدد الطبقات (Material Design style)
                    int layers = std::min(3, static_cast<int>(elevation));
                    for (int i = 0; i < layers; ++i)
                    {
                        float expand = static_cast<float>(i + 1) * 0.8f;
                        float alpha = 0.12f / static_cast<float>(i + 1);
                        float offsetY = elevation * 0.4f * static_cast<float>(i + 1);
                        Color shadowColor = {0, 0, 0, alpha};
                        drawFilledRect(rect.x - expand, rect.y + offsetY - expand,
                                       rect.width + expand * 2, rect.height + expand * 2,
                                       shadowColor);
                    }
                }
            }

            // 2. رسم الخلفية (إن وجدت) — هذه الخطوة العامّة هي المصدر الوحيد
            //    لخلفية كلّ عقدة (بما فيها الحاويات): تدعم لون_خلفية/خلفية +
            //    الزوايا + التحويم/الضغط. الأولوية موحَّدة على لون_خلفية ثمّ خلفية.
            const auto *bgProp = node.findProperty(props::BG_COLOR); // لون_خلفية
            if (!bgProp)
                bgProp = node.findProperty(props::BG); // خلفية
            if (bgProp)
            {
                Color bgColor = parseColorProp(bgProp, {0, 0, 0, 0});
                if (bgColor.a > 0)
                {
                    // تأثير hover / press على الخلفية
                    if (isHovered && !isPressed)
                    {
                        bgColor.r = std::min(1.0f, bgColor.r + 0.05f);
                        bgColor.g = std::min(1.0f, bgColor.g + 0.05f);
                        bgColor.b = std::min(1.0f, bgColor.b + 0.05f);
                    }
                    if (isPressed)
                    {
                        bgColor.r = std::max(0.0f, bgColor.r - 0.08f);
                        bgColor.g = std::max(0.0f, bgColor.g - 0.08f);
                        bgColor.b = std::max(0.0f, bgColor.b - 0.08f);
                    }

                    const auto *radiusProp = node.findProperty(props::CORNER_RADIUS); // زوايا
                    if (!radiusProp)
                        radiusProp = node.findProperty(props::RADIUS); // نصف_قطر
                    float radius = getNumericProp(radiusProp, 0.0f);
                    if (radius > 0)
                    {
                        drawRoundedRect(rect.x, rect.y, rect.width, rect.height, bgColor, radius);
                    }
                    else
                    {
                        drawFilledRect(rect.x, rect.y, rect.width, rect.height, bgColor);
                    }
                }
            }

            // 3. رسم المحتوى حسب نوع العنصر
            switch (node.getType())
            {

            // ── النص ──
            case UINodeType::Text:
            {
                const auto *textProp = node.findProperty(props::TEXT_LATIN);
                if (!textProp)
                    textProp = node.findProperty(props::CONTENT); // محتوى
                if (!textProp)
                    textProp = node.findProperty(props::TEXT); // نص
                if (textProp)
                {
                    if (auto *text = std::get_if<std::string>(&textProp->value))
                    {
                        const auto *sizeProp = node.findProperty(props::FONT_SIZE); // حجم_خط
                        if (!sizeProp)
                            sizeProp = node.findProperty(props::FONT_SIZE_ALT); // حجم_الخط
                        if (!sizeProp)
                            sizeProp = node.findProperty(props::SIZE); // حجم
                        float fontSize = getNumericProp(sizeProp, 16.0f);

                        const auto *colorProp = node.findProperty(props::TEXT_COLOR); // لون_النص
                        if (!colorProp)
                            colorProp = node.findProperty(props::COLOR); // لون
                        Color textColor = parseColorProp(colorProp, {0, 0, 0, 1});

                        const auto *alignProp = node.findProperty(props::ALIGN); // محاذاة
                        std::string alignment;
                        if (alignProp)
                        {
                            if (auto *s = std::get_if<std::string>(&alignProp->value))
                                alignment = *s;
                        }

                        float textX = rect.x;
                        if (isArabicText(*text) || alignment == propval::ALIGN_RIGHT_AR || alignment == propval::ALIGN_RIGHT_EN)
                        {
                            auto sz = measureText(*text, fontSize);
                            textX = rect.x + rect.width - sz.first;
                            if (textX < rect.x)
                                textX = rect.x;
                        }
                        else if (alignment == propval::ALIGN_CENTER_AR || alignment == propval::ALIGN_CENTER_EN)
                        {
                            auto sz = measureText(*text, fontSize);
                            textX = rect.x + (rect.width - sz.first) / 2.0f;
                        }

                        drawText(*text, textX, rect.y, textColor, fontSize);
                    }
                }
                break;
            }

            // ── الزر ──
            case UINodeType::Button:
            {
                bool isHovered = (hoveredNode_ == &node);
                bool isPressed = (pressedNode_ == &node);

                float radius = getNumericProp(node.findProperty(props::CORNER_RADIUS), 8.0f); // زوايا
                if (!node.findProperty(props::CORNER_RADIUS))
                    radius = getNumericProp(node.findProperty(props::RADIUS), 8.0f); // نصف_قطر

                // ظل
                float elevation = getNumericProp(node.findProperty(props::SHADOW), 2.0f); // ظل
                if (isHovered && !isPressed)
                    elevation += 2.0f;
                if (isPressed)
                    elevation = std::max(0.5f, elevation - 1.0f);
                if (elevation > 0)
                {
                    drawSoftShadow(rect, radius, elevation * 2.0f,
                                   0.0f, elevation * 0.6f, {0, 0, 0, 0.2f});
                }

                // خلفية الزر
                const auto *bgColorProp = node.findProperty(props::BG_COLOR); // لون_خلفية
                if (!bgColorProp)
                    bgColorProp = node.findProperty(props::BG); // خلفية
                Color btnColor = parseColorProp(bgColorProp, Color::fromNamed(NamedColor::Primary));

                if (isHovered && !isPressed)
                {
                    btnColor.r = std::min(1.0f, btnColor.r + 0.08f);
                    btnColor.g = std::min(1.0f, btnColor.g + 0.08f);
                    btnColor.b = std::min(1.0f, btnColor.b + 0.08f);
                }
                if (isPressed)
                {
                    btnColor.r = std::max(0.0f, btnColor.r - 0.1f);
                    btnColor.g = std::max(0.0f, btnColor.g - 0.1f);
                    btnColor.b = std::max(0.0f, btnColor.b - 0.1f);
                }

                const auto *gradProp = node.findProperty(props::GRADIENT);                                             // تدرج
                const auto *gradEndProp = node.findProperty(props::GRADIENT_END); // تدرج_نهاية
                if (gradProp && gradEndProp)
                {
                    Color gradStart = parseColorProp(gradProp, btnColor);
                    Color gradEnd = parseColorProp(gradEndProp, btnColor);
                    drawLinearGradient(rect.x, rect.y, rect.width, rect.height,
                                       gradStart, gradEnd, true, radius);
                }
                else
                {
                    drawRoundedRect(rect.x, rect.y, rect.width, rect.height, btnColor, radius);
                }

                // تأثيرُ التموّج (ripple) عند الضغط
                if (isPressed)
                {
                    drawRoundedRect(rect.x, rect.y, rect.width, rect.height,
                                    {1.0f, 1.0f, 1.0f, 0.15f}, radius);
                }

                // نصُّ الزرّ — «عنوان» أوّلًا فهو ما يكتبه مصنعُ زر() (SoT)، ثمّ
                // «أيقونة» (زرّ الأيقونة)، ثمّ البدائل. findTextProp يوحّد الترتيب
                // ويحوّل العددَ نصًّا (زر(5)) ويمنع انحرافَ المفاتيح.
                std::string btnText;
                if (findTextProp(node,
                                 {props::TITLE, props::ICON, props::TEXT_LATIN,
                                  props::TEXT, props::CONTENT},
                                 btnText) &&
                    !btnText.empty())
                {
                    // لونُ نصِّ الزرّ — لون_نص/لون_النص ثمّ «لون» الصريح، وإلّا
                    // OnPrimary من الثيم (لا أبيضَ ثابتًا غيرَ مقروءٍ على الفاتح).
                    const auto *textColorProp = node.findProperty(props::TEXT_COLOR_ALT); // لون_نص
                    if (!textColorProp)
                        textColorProp = node.findProperty(props::TEXT_COLOR); // لون_النص
                    if (!textColorProp)
                        textColorProp = node.findProperty(props::COLOR); // لون
                    Color textColor = parseColorProp(textColorProp, Color::fromNamed(NamedColor::OnPrimary));
                    float btnFontSize = getNumericProp(node.findProperty(props::FONT_SIZE_ALT), 16.0f); // حجم_الخط
                    if (btnFontSize <= 1.0f)
                        btnFontSize = getNumericProp(node.findProperty(props::FONT_SIZE), 16.0f); // حجم_خط

                    auto sz = measureText(btnText, btnFontSize);
                    float textX = rect.x + (rect.width - sz.first) / 2.0f;
                    float textY = rect.y + (rect.height - sz.second) / 2.0f;
                    drawText(btnText, textX, textY, textColor, btnFontSize);
                }
                break;
            }

            // ── الفاصل ──
            case UINodeType::Divider:
            {
                Color divColor = parseColorProp(node.findProperty(props::COLOR), // لون
                                                Color::fromNamed(NamedColor::Grey));
                float thickness = getNumericProp(node.findProperty(props::THICKNESS), rect.height); // سماكة
                drawFilledRect(rect.x, rect.y, rect.width, thickness, divColor);
                break;
            }

            // ── الصورة ──
            case UINodeType::Image:
            {
                const auto *srcProp = node.findProperty(props::SOURCE); // مصدر
                if (!srcProp)
                    srcProp = node.findProperty(props::SRC_LATIN);
                if (!srcProp)
                    srcProp = node.findProperty(props::PATH); // مسار
                bool loaded = false;
                if (srcProp)
                {
                    if (auto *path = std::get_if<std::string>(&srcProp->value))
                    {
                        drawImage(*path, rect.x, rect.y, rect.width, rect.height);
                        loaded = true;
                    }
                }
                if (!loaded)
                {
                    float imgRadius = getNumericProp(node.findProperty(props::CORNER_RADIUS), 0.0f); // زوايا
                    if (imgRadius > 0)
                        drawRoundedRect(rect.x, rect.y, rect.width, rect.height,
                                        Color::fromNamed(NamedColor::LightGray), imgRadius);
                    else
                        drawFilledRect(rect.x, rect.y, rect.width, rect.height,
                                       Color::fromNamed(NamedColor::LightGray));
                    drawText("\xf0\x9f\x96\xbc", rect.x + rect.width / 2 - 8,
                             rect.y + rect.height / 2 - 8,
                             Color::fromNamed(NamedColor::Gray), 16.0f);
                }
                break;
            }

            // ── حقل الإدخال ──
            case UINodeType::TextField:
            {
                bool isFocused = (focusedNode_ == &node);
                Color bgColor = parseColorProp(node.findProperty(props::BG), // خلفية
                                               Color::fromNamed(NamedColor::White));
                Color borderColor = parseColorProp(node.findProperty(props::BORDER_COLOR), // حد_لون
                                                   Color::fromNamed(NamedColor::Gray));
                float radius = getNumericProp(node.findProperty(props::CORNER_RADIUS), 4.0f);                     // زوايا
                float fontSize = getNumericProp(node.findProperty(props::FONT_SIZE_ALT), 14.0f); // حجم_الخط

                if (isFocused)
                {
                    borderColor = Color::fromNamed(NamedColor::Primary);
                    // هالةٌ خفيفةٌ حولَ الحقلِ المُركَّزِ عليه
                    drawSoftShadow(rect, radius, 4.0f, 0.0f, 0.0f,
                                   {borderColor.r, borderColor.g, borderColor.b, 0.2f});
                }
                else if (isHovered)
                {
                    borderColor.r = std::max(0.0f, borderColor.r - 0.15f);
                    borderColor.g = std::max(0.0f, borderColor.g - 0.15f);
                    borderColor.b = std::max(0.0f, borderColor.b - 0.15f);
                }

                // حدٌّ مستديرُ الزوايا (لا drawRectOutline المربَّع) — إطارٌ ثمّ ملء
                float borderW = isFocused ? 2.0f : 1.0f;
                drawRoundedRect(rect.x, rect.y, rect.width, rect.height, borderColor, radius);
                drawRoundedRect(rect.x + borderW, rect.y + borderW,
                                rect.width - borderW * 2, rect.height - borderW * 2,
                                bgColor, std::max(0.0f, radius - borderW));

                const auto *valueProp = node.findProperty(props::VALUE); // قيمة
                if (!valueProp)
                    valueProp = node.findProperty(props::TEXT); // نص
                if (!valueProp)
                    valueProp = node.findProperty(props::VALUE_LATIN);
                std::string inputText;
                if (valueProp)
                {
                    if (auto *s = std::get_if<std::string>(&valueProp->value))
                        inputText = *s;
                }

                float textPad = 8.0f;
                float textY = rect.y + rect.height / 2 - fontSize / 2;

                if (!inputText.empty())
                {
                    Color textColor = parseColorProp(node.findProperty(props::TEXT_COLOR), {0, 0, 0, 1}); // لون_النص
                    float textX = rect.x + textPad;
                    if (isArabicText(inputText))
                    {
                        auto sz = measureText(inputText, fontSize);
                        textX = rect.x + rect.width - sz.first - textPad;
                    }

                    // إبرازُ التحديد (قبلَ النصِّ كي لا يحجبه)
                    if (isFocused &&
                        textEditInteraction_.selectionStart != textEditInteraction_.selectionEnd)
                    {
                        size_t selStart = std::min(textEditInteraction_.selectionStart,
                                                   textEditInteraction_.selectionEnd);
                        size_t selEnd = std::max(textEditInteraction_.selectionStart,
                                                 textEditInteraction_.selectionEnd);
                        if (selStart < inputText.size() && selEnd <= inputText.size())
                        {
                            auto beforeSize = measureText(inputText.substr(0, selStart), fontSize);
                            auto selSize = measureText(inputText.substr(selStart, selEnd - selStart),
                                                       fontSize);
                            // (AR) نظيرُ المؤشّر: في النصِّ العربيِّ يُقاس المُحدَّدُ
                            //   من **يمينِ** النصِّ لا من يسارِه، وإلّا وقع الإبرازُ
                            //   على مقطعٍ آخر تمامًا.
                            const float selectionX =
                                isArabicText(inputText)
                                    ? textX + measureText(inputText, fontSize).first -
                                          beforeSize.first - selSize.first
                                    : textX + beforeSize.first;
                            drawFilledRect(selectionX, rect.y + 3,
                                           selSize.first, rect.height - 6,
                                           {0.26f, 0.52f, 0.96f, 0.35f}); // أزرق شفّاف
                        }
                    }

                    drawText(inputText, textX, textY, textColor, fontSize);
                }
                else
                {
                    const auto *hintProp = node.findProperty(props::HINT); // تلميح
                    if (!hintProp)
                        hintProp = node.findProperty(props::PLACEHOLDER_LATIN);
                    if (hintProp)
                    {
                        if (auto *hint = std::get_if<std::string>(&hintProp->value))
                        {
                            Color hintColor = {0.6f, 0.6f, 0.6f, 1.0f};
                            float textX = rect.x + textPad;
                            if (isArabicText(*hint))
                            {
                                auto sz = measureText(*hint, fontSize);
                                textX = rect.x + rect.width - sz.first - textPad;
                            }
                            drawText(*hint, textX, textY, hintColor, fontSize);
                        }
                    }
                }

                // المؤشرُ الوامضُ عندَ موضعِ cursorPos لا في آخرِ النصِّ دائمًا
                if (isFocused)
                {
                    uint32_t ticks = getCurrentTimeMs();
                    if ((ticks / 500) % 2 == 0)
                    {
                        // (AR) موضعُ المؤشّرِ يتبع اتّجاهَ النصّ. في العربيّةِ يبدأ
                        //   النصُّ من **يمينِ** الشريطِ فيسير المؤشّرُ يسارًا كلّما
                        //   طال ما قبلَه؛ وفي اللاتينيّةِ العكس. جمعُ الطولِ إلى
                        //   الحافّةِ اليسرى في الحالتَين كان يثبّت المؤشّرَ عند
                        //   الطرفِ المقابلِ لموضعِ الكتابةِ في النصِّ العربيّ.
                        const bool rtlText = !inputText.empty() && isArabicText(inputText);
                        const float prefixWidth =
                            inputText.empty()
                                ? 0.0f
                                : measureText(inputText.substr(0, std::min(textEditInteraction_.cursorPos,
                                                                           inputText.size())),
                                              fontSize)
                                      .first;

                        float cursorX;
                        if (rtlText)
                        {
                            cursorX = rect.x + rect.width - textPad - prefixWidth;
                            if (cursorX < rect.x + textPad)
                                cursorX = rect.x + textPad;
                        }
                        else
                        {
                            cursorX = rect.x + textPad + prefixWidth;
                        }
                        drawFilledRect(cursorX, rect.y + 4, 2, rect.height - 8, borderColor);
                    }
                }
                break;
            }

            // ── مفتاح التبديل (Toggle) ──
            case UINodeType::Toggle:
            {
                bool isOn = getBoolProp(node.findProperty(props::ENABLED), false);                     // مفعل
                Color activeColor = parseColorProp(node.findProperty(props::ACTIVE_COLOR), // لون_نشط
                                                   Color::fromNamed(NamedColor::Primary));
                Color inactiveColor = parseColorProp(node.findProperty(props::COLOR), // لون
                                                     Color::fromNamed(NamedColor::LightGray));
                Color trackColor = isOn ? activeColor : inactiveColor;
                if (isHovered && !isPressed)
                {
                    trackColor.r = std::min(1.0f, trackColor.r + 0.06f);
                    trackColor.g = std::min(1.0f, trackColor.g + 0.06f);
                    trackColor.b = std::min(1.0f, trackColor.b + 0.06f);
                }

                drawRoundedRect(rect.x, rect.y + rect.height / 4, rect.width, rect.height / 2,
                                trackColor, rect.height / 4);

                Color knobColor = Color::fromNamed(NamedColor::White);
                float knobSize = rect.height - 4;
                float knobX = isOn ? rect.x + rect.width - knobSize - 2 : rect.x + 2;
                // ظل المقبض
                drawRoundedRect(knobX - 1, rect.y + 3, knobSize + 2, knobSize + 2,
                                {0, 0, 0, 0.15f}, (knobSize + 2) / 2);
                drawRoundedRect(knobX, rect.y + 2, knobSize, knobSize,
                                knobColor, knobSize / 2);

                // نصُّ التسمية — «عنوان» أوّلًا اتّساقًا مع بقيّة العناصر
                std::string toggleLabel;
                if (findTextProp(node, {props::TITLE, props::TEXT, props::CONTENT}, toggleLabel) &&
                    !toggleLabel.empty())
                {
                    float labelSize = getNumericProp(node.findProperty(props::FONT_SIZE_ALT), 14.0f); // حجم_الخط
                    drawText(toggleLabel, rect.x + rect.width + 8,
                             rect.y + rect.height / 2 - labelSize / 2, {0, 0, 0, 1}, labelSize);
                }
                break;
            }

            // ── الشريط المنزلق (Slider) ──
            case UINodeType::Slider:
            {
                float sliderVal = getNumericProp(node.findProperty(props::VALUE), 50.0f) / 100.0f; // قيمة
                sliderVal = std::max(0.0f, std::min(1.0f, sliderVal));
                Color trackBg = parseColorProp(node.findProperty(props::BG), // خلفية
                                               Color::fromNamed(NamedColor::LightGray));
                Color trackFill = parseColorProp(node.findProperty(props::COLOR), // لون
                                                 Color::fromNamed(NamedColor::Primary));
                float trackY = rect.y + rect.height / 2 - 2;
                drawRoundedRect(rect.x, trackY, rect.width, 4, trackBg, 2.0f);
                float filledW = rect.width * sliderVal;
                if (filledW > 0)
                    drawRoundedRect(rect.x, trackY, filledW, 4, trackFill, 2.0f);
                // مقبض
                Color thumbColor = parseColorProp(node.findProperty(props::HANDLE_COLOR), trackFill); // لون_المقبض
                float thumbX = rect.x + filledW;
                drawRoundedRect(thumbX - 9, rect.y + rect.height / 2 - 7, 18, 18,
                                {0, 0, 0, 0.15f}, 9.0f);
                drawRoundedRect(thumbX - 8, rect.y + rect.height / 2 - 8, 16, 16,
                                thumbColor, 8.0f);
                break;
            }

            // ── مربع الاختيار (Checkbox) ──
            case UINodeType::Checkbox:
            {
                bool checked = getBoolProp(node.findProperty(props::ENABLED), false); // مفعل
                Color activeColor = parseColorProp(node.findProperty(props::COLOR),         // لون
                                                   Color::fromNamed(NamedColor::Primary));
                Color borderColor = checked ? activeColor : Color::fromNamed(NamedColor::Gray);
                if (isHovered && !checked)
                {
                    borderColor.r = std::max(0.0f, borderColor.r - 0.15f);
                    borderColor.g = std::max(0.0f, borderColor.g - 0.15f);
                    borderColor.b = std::max(0.0f, borderColor.b - 0.15f);
                }
                drawRoundedRect(rect.x, rect.y, 20, 20, borderColor, 3.0f);
                if (checked)
                {
                    drawRoundedRect(rect.x + 1, rect.y + 1, 18, 18, activeColor, 2.0f);
                    drawText("\xe2\x9c\x93", rect.x + 3, rect.y + 1,
                             Color::fromNamed(NamedColor::White), 15.0f);
                }
                else
                {
                    drawRoundedRect(rect.x + 1, rect.y + 1, 18, 18,
                                    Color::fromNamed(NamedColor::White), 2.0f);
                }
                // نص
                const auto *textProp = node.findProperty(props::TEXT_LATIN);
                if (!textProp)
                    textProp = node.findProperty(props::TEXT); // نص
                if (!textProp)
                    textProp = node.findProperty(props::CONTENT); // محتوى
                if (textProp)
                {
                    if (auto *text = std::get_if<std::string>(&textProp->value))
                    {
                        Color textColor = parseColorProp(node.findProperty(props::TEXT_COLOR), {0, 0, 0, 1}); // لون_النص
                        float fontSize = getNumericProp(node.findProperty(props::FONT_SIZE_ALT), 14.0f);         // حجم_الخط
                        float textX = rect.x + 28;
                        if (isArabicText(*text))
                        {
                            auto sz = measureText(*text, fontSize);
                            textX = rect.x + rect.width - sz.first - 28;
                        }
                        drawText(*text, textX, rect.y + 2, textColor, fontSize);
                    }
                }
                break;
            }

            // ── زر الراديو ──
            case UINodeType::Radio:
            {
                bool selected = getBoolProp(node.findProperty(props::ENABLED)); // مفعل
                Color activeColor = parseColorProp(node.findProperty(props::COLOR),   // لون
                                                   Color::fromNamed(NamedColor::Primary));
                Color borderColor = selected ? activeColor : Color::fromNamed(NamedColor::Gray);
                if (isHovered && !selected)
                {
                    borderColor.r *= 0.7f;
                    borderColor.g *= 0.7f;
                    borderColor.b *= 0.7f;
                }
                drawRoundedRect(rect.x, rect.y, 20, 20, borderColor, 10.0f);
                drawRoundedRect(rect.x + 1, rect.y + 1, 18, 18,
                                Color::fromNamed(NamedColor::White), 9.0f);
                if (selected)
                {
                    drawRoundedRect(rect.x + 5, rect.y + 5, 10, 10, activeColor, 5.0f);
                }
                const auto *textProp = node.findProperty(props::TEXT_LATIN);
                if (!textProp)
                    textProp = node.findProperty(props::TEXT); // نص
                if (textProp)
                {
                    if (auto *text = std::get_if<std::string>(&textProp->value))
                    {
                        Color radioTextColor = parseColorProp(node.findProperty(props::TEXT_COLOR), // لون_النص
                                                              {0, 0, 0, 1});
                        drawText(*text, rect.x + 28, rect.y + 2, radioTextColor, 14.0f);
                    }
                }
                break;
            }

            // ── البطاقة (Card) ──
            case UINodeType::Card:
            {
                float cardRadius = getNumericProp(node.findProperty(props::CORNER_RADIUS), 12.0f); // زوايا
                float cardElevation = getNumericProp(node.findProperty(props::ELEVATION), 4.0f);   // رفع

                // تأثيرات التفاعل على الارتفاع
                if (isPressed)
                    cardElevation = std::max(1.0f, cardElevation - 2.0f);
                else if (isHovered)
                    cardElevation += 4.0f;

                // ظلٌّ ثلاثيُّ الطبقات (محاكاة ارتفاع Material)
                if (cardElevation > 0)
                {
                    drawSoftShadow(rect, cardRadius, cardElevation * 3.5f,
                                   0.0f, cardElevation * 0.5f, {0, 0, 0, 0.10f}); // محيطيّ
                    drawSoftShadow(rect, cardRadius, cardElevation * 2.0f,
                                   0.0f, cardElevation * 1.0f, {0, 0, 0, 0.18f}); // متوسّط
                    drawSoftShadow(rect, cardRadius, cardElevation * 1.0f,
                                   0.0f, cardElevation * 0.4f, {0, 0, 0, 0.12f}); // حادّ
                }

                const auto *cardColorProp = node.findProperty(props::COLOR); // لون
                if (!cardColorProp)
                    cardColorProp = node.findProperty(props::BG); // خلفية
                Color cardBg = parseColorProp(cardColorProp, Color::fromNamed(NamedColor::White));

                if (isHovered && !isPressed)
                {
                    cardBg.r = std::min(1.0f, cardBg.r + 0.03f);
                    cardBg.g = std::min(1.0f, cardBg.g + 0.03f);
                    cardBg.b = std::min(1.0f, cardBg.b + 0.03f);
                }
                if (isPressed)
                {
                    cardBg.r *= 0.95f;
                    cardBg.g *= 0.95f;
                    cardBg.b *= 0.95f;
                }
                // (AR) لا تُضرَب الشفافيّةُ هنا: الخطوةُ العامّةُ قبلَ الـswitch
                //      ضبطت currentNodeOpacity_ فتُطبّقها أوّليّاتُ كلِّ منصّة.

                // (AR) الحدُّ **قبلَ** الملء: كان يُرسَم بعدَه فيُعيد ملءَ الداخلِ
                //   بلونٍ مسطَّحٍ ⇒ بطاقةٌ لها تدرّجٌ وحدٌّ معًا تفقد تدرّجَها كلَّه.
                //   الترتيبُ الآن: إطارٌ خارجيٌّ ثمّ الملءُ (تدرّجًا أو لونًا) داخلَه.
                const auto *cardBorderProp = node.findProperty(props::BORDER_COLOR); // لون_الحد
                LayoutRect cardFill = rect;
                float cardFillRadius = cardRadius;
                if (cardBorderProp)
                {
                    Color borderColor = parseColorProp(cardBorderProp, Color::fromNamed(NamedColor::Gray));
                    drawRoundedRect(rect.x, rect.y, rect.width, rect.height, borderColor, cardRadius);
                    cardFill = {rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2};
                    cardFillRadius = std::max(0.0f, cardRadius - 1);
                }

                const auto *cardGradProp = node.findProperty(props::GRADIENT);        // تدرج
                const auto *cardGradEndProp = node.findProperty(props::GRADIENT_END); // تدرج_نهاية
                if (cardGradProp && cardGradEndProp)
                {
                    Color gradStart = parseColorProp(cardGradProp, cardBg);
                    Color gradEnd = parseColorProp(cardGradEndProp, cardBg);
                    drawLinearGradient(cardFill.x, cardFill.y, cardFill.width, cardFill.height,
                                       gradStart, gradEnd, true, cardFillRadius);
                }
                else
                {
                    drawRoundedRect(cardFill.x, cardFill.y, cardFill.width, cardFill.height,
                                    cardBg, cardFillRadius);
                }
                break;
            }

            // ── شريط التقدم ──
            case UINodeType::ProgressBar:
            {
                float barRadius = getNumericProp(node.findProperty(props::CORNER_RADIUS), rect.height / 2.0f);     // زوايا
                Color trackColor = parseColorProp(node.findProperty(props::BG), {0.9f, 0.9f, 0.9f, 1}); // خلفية
                drawRoundedRect(rect.x, rect.y, rect.width, rect.height, trackColor, barRadius);
                float progressValue = getNumericProp(node.findProperty(props::VALUE), // قيمة
                                                     getNumericProp(node.findProperty(props::VALUE_LATIN), 50.0f));
                progressValue /= 100.0f;
                progressValue = std::max(0.0f, std::min(1.0f, progressValue));
                Color fillColor = parseColorProp(node.findProperty(props::COLOR), // لون
                                                 Color::fromNamed(NamedColor::Primary));
                float fillWidth = rect.width * progressValue;
                if (fillWidth > 0)
                {
                    Color lighter = {std::min(1.0f, fillColor.r * 1.2f),
                                     std::min(1.0f, fillColor.g * 1.2f),
                                     std::min(1.0f, fillColor.b * 1.2f), fillColor.a};
                    drawLinearGradient(rect.x, rect.y, fillWidth, rect.height,
                                       lighter, fillColor, true, barRadius);
                }
                break;
            }

            // ── شريط البحث ──
            case UINodeType::SearchBar:
            {
                Color bgColor = parseColorProp(node.findProperty(props::BG), // خلفية
                                               Color::fromNamed(NamedColor::LightGray));
                float radius = getNumericProp(node.findProperty(props::CORNER_RADIUS), rect.height / 2); // زوايا
                drawRoundedRect(rect.x, rect.y, rect.width, rect.height, bgColor, radius);
                Color searchIconColor = parseColorProp(node.findProperty(props::ICON_COLOR), // لون_الأيقونة
                                                       Color::fromNamed(NamedColor::Gray));
                drawText("\xf0\x9f\x94\x8d", rect.x + 8, rect.y + rect.height / 2 - 8,
                         searchIconColor, 14.0f); // 🔍
                const auto *hintProp = node.findProperty(props::HINT); // تلميح
                if (!hintProp)
                    hintProp = node.findProperty(props::PLACEHOLDER_LATIN);
                if (hintProp)
                {
                    if (auto *hint = std::get_if<std::string>(&hintProp->value))
                        drawText(*hint, rect.x + 30, rect.y + rect.height / 2 - 7,
                                 {0.5f, 0.5f, 0.5f, 1}, 14.0f);
                }
                break;
            }

            // ── FAB ──
            case UINodeType::FAB:
            {
                Color fabColor = parseColorProp(node.findProperty(props::BG_COLOR), // لون_خلفية
                                                Color::fromNamed(NamedColor::Primary));
                if (!node.findProperty(props::BG_COLOR))
                    fabColor = parseColorProp(node.findProperty(props::COLOR), fabColor); // لون
                float radius = std::min(rect.width, rect.height) / 2;

                // ظلٌّ ناعمٌ تحتَ الزرِّ العائم
                float fabElevation = getNumericProp(node.findProperty(props::SHADOW), 6.0f); // ظل
                if (fabElevation > 0)
                {
                    drawSoftShadow(rect, radius, fabElevation * 2.0f,
                                   0.0f, fabElevation * 0.7f, {0, 0, 0, 0.3f});
                }

                drawRoundedRect(rect.x, rect.y, rect.width, rect.height, fabColor, radius);

                // نصُّ الزرِّ العائم: «أيقونة» أوّلًا (ما يكتبه مصنعُ زر_عائم،
                // بهمزةٍ على الألف) ثمّ البدائل. كان المفتاحُ يُقرأ بألفٍ مجرَّدةٍ
                // فلا تُرسَم الأيقونةُ أبدًا — findTextProp يوحّده ويصحّحه.
                std::string fabText;
                if (findTextProp(node,
                                 {props::ICON, props::TEXT_LATIN, props::TEXT,
                                  props::ICON_ALT, props::CONTENT},
                                 fabText) &&
                    !fabText.empty())
                {
                    Color textColor = parseColorProp(node.findProperty(props::TEXT_COLOR), // لون_النص
                                                     Color::fromNamed(NamedColor::OnPrimary));
                    float fontSize = getNumericProp(node.findProperty(props::FONT_SIZE_ALT), 20.0f); // حجم_الخط
                    auto sz = measureText(fabText, fontSize);
                    drawText(fabText, rect.x + (rect.width - sz.first) / 2,
                             rect.y + (rect.height - sz.second) / 2, textColor, fontSize);
                }
                break;
            }

            // ── شريط التطبيق ──
            case UINodeType::AppBar:
            {
                Color barColor = parseColorProp(node.findProperty(props::BG_COLOR), // لون_خلفية
                                                Color::fromNamed(NamedColor::Primary));
                if (!node.findProperty(props::BG_COLOR))
                    barColor = parseColorProp(node.findProperty(props::COLOR), barColor); // لون
                // ظلٌّ ناعمٌ أسفلَ الشريط
                float appBarElevation = getNumericProp(node.findProperty(props::SHADOW), 3.0f); // ظل
                if (appBarElevation > 0)
                {
                    drawSoftShadow(rect, 0.0f, appBarElevation * 2.0f,
                                   0.0f, appBarElevation, {0, 0, 0, 0.15f});
                }

                Color barLighter = {std::min(1.0f, barColor.r * 1.15f),
                                    std::min(1.0f, barColor.g * 1.15f),
                                    std::min(1.0f, barColor.b * 1.15f), barColor.a};
                drawLinearGradient(rect.x, rect.y, rect.width, rect.height,
                                   barLighter, barColor, true);
                // «عنوان» أوّلًا — الترتيبُ القانونيُّ الموحَّد عبرَ العناصر
                const auto *textProp = node.findProperty(props::TITLE); // عنوان
                if (!textProp)
                    textProp = node.findProperty(props::TEXT_LATIN);
                if (!textProp)
                    textProp = node.findProperty(props::TEXT); // نص
                if (textProp)
                {
                    if (auto *text = std::get_if<std::string>(&textProp->value))
                    {
                        Color textColor = parseColorProp(node.findProperty(props::TEXT_COLOR), // لون_النص
                                                         Color::fromNamed(NamedColor::White));
                        float fontSize = getNumericProp(node.findProperty(props::FONT_SIZE_ALT), 20.0f); // حجم_الخط
                        float textX = rect.x + 16;
                        if (isArabicText(*text))
                        {
                            auto sz = measureText(*text, fontSize);
                            textX = rect.x + rect.width - sz.first - 16;
                            if (textX < rect.x)
                                textX = rect.x; // لا تتجاوز حافّة العنصر يسارًا (منع قصّ العنوان)
                        }
                        drawText(*text, textX, rect.y + rect.height / 2 - fontSize / 2,
                                 textColor, fontSize);
                    }
                }
                break;
            }

            // ── الشارة (Badge) ──
            case UINodeType::Badge:
            {
                Color badgeColor = parseColorProp(node.findProperty(props::BG_COLOR), // لون_خلفية
                                                  Color::fromNamed(NamedColor::Error));
                if (!node.findProperty(props::BG_COLOR))
                    badgeColor = parseColorProp(node.findProperty(props::COLOR), badgeColor); // لون
                float r = std::min(rect.width, rect.height) / 2;
                drawRoundedRect(rect.x, rect.y, rect.width, rect.height, badgeColor, r);
                const auto *textProp = node.findProperty(props::TEXT_LATIN);
                if (!textProp)
                    textProp = node.findProperty(props::TEXT); // نص
                if (!textProp)
                    textProp = node.findProperty(props::CONTENT); // محتوى
                if (textProp)
                {
                    if (auto *text = std::get_if<std::string>(&textProp->value))
                    {
                        Color tC = parseColorProp(node.findProperty(props::TEXT_COLOR), Color::fromNamed(NamedColor::White));
                        float fS = getNumericProp(node.findProperty(props::FONT_SIZE_ALT), 10.0f);
                        auto sz = measureText(*text, fS);
                        drawText(*text, rect.x + (rect.width - sz.first) / 2,
                                 rect.y + (rect.height - sz.second) / 2, tC, fS);
                    }
                }
                break;
            }

            // ── حاويات وتخطيطات ──
            case UINodeType::Scaffold:
            case UINodeType::Container:
            case UINodeType::Box:
            case UINodeType::Surface:
            case UINodeType::NavigationView:
            case UINodeType::Stack:
            case UINodeType::Group:
            case UINodeType::Center:
            case UINodeType::Padding:
            case UINodeType::SizedBox:
            case UINodeType::Expanded:
            case UINodeType::Flexible:
            case UINodeType::Align:
            case UINodeType::SafeArea:
            case UINodeType::GestureDetector:
            case UINodeType::InkWell:
            case UINodeType::ListView:
            case UINodeType::FractionallySizedBox:
            case UINodeType::ConstrainedBox:
            case UINodeType::AspectRatio:
            case UINodeType::Column:
            case UINodeType::Row:
            case UINodeType::Wrap:
            case UINodeType::Grid:
            case UINodeType::LazyColumn:
            case UINodeType::LazyRow:
            case UINodeType::LazyGrid:
            case UINodeType::ScrollView:
            case UINodeType::List:
            case UINodeType::AnimatedList:
            case UINodeType::ForEach:
            case UINodeType::TabItem:
            case UINodeType::Section:
            case UINodeType::Swipeable:
            case UINodeType::CustomWidget:
            case UINodeType::Conditional:
            {
                // (AR) الخلفية تُرسَم أصلًا في خطوة الخلفية العامّة قبل الـswitch
                //      (تدعم لون_خلفية/خلفية + الزوايا + تأثير التحويم/الضغط)، فلا
                //      نكرّرها هنا. كان تكرارها يرسم الخلفية مرّتين (هدر + تراكم ألفا
                //      على خلفيّة شبه شفّافة). أزيل التكرار — يبقى القصّ فقط.
                // (EN) Background is already drawn by the general pre-switch step
                //      (handles لون_خلفية/خلفية + radius + hover/press). Re-drawing
                //      it here caused a double draw (waste + alpha accumulation on
                //      semi-transparent backgrounds). Removed; only clipping remains.
                // قص المحتوى للعناصر القابلة للتمرير
                if (isScrollableType(node.getType()))
                {
                    setClipRect(rect.x, rect.y, rect.width, rect.height);
                }
                break;
            }

            // ── SnackBar، Chip، Avatar — نصوص مع خلفية ──
            case UINodeType::SnackBar:
            {
                Color snackBg = parseColorProp(node.findProperty(props::BG), {0.2f, 0.2f, 0.2f, 0.9f});
                float snackRadius = getNumericProp(node.findProperty(props::CORNER_RADIUS), 4.0f); // زوايا
                drawRoundedRect(rect.x, rect.y, rect.width, rect.height, snackBg, snackRadius);
                const auto *textProp = node.findProperty(props::TEXT_LATIN);
                if (!textProp)
                    textProp = node.findProperty(props::TEXT);
                if (!textProp)
                    textProp = node.findProperty(props::CONTENT); // محتوى
                if (textProp)
                {
                    if (auto *text = std::get_if<std::string>(&textProp->value))
                    {
                        Color snackTextColor = parseColorProp(node.findProperty(props::TEXT_COLOR), // لون_النص
                                                              Color::fromNamed(NamedColor::White));
                        float fontSize = getNumericProp(node.findProperty(props::FONT_SIZE_ALT), 14.0f);
                        float textX = rect.x + 16;
                        if (isArabicText(*text))
                        {
                            auto sz = measureText(*text, fontSize);
                            textX = rect.x + rect.width - sz.first - 16;
                            if (textX < rect.x)
                                textX = rect.x;
                        }
                        drawText(*text, textX, rect.y + rect.height / 2 - fontSize / 2,
                                 snackTextColor, fontSize);
                    }
                }
                break;
            }

            case UINodeType::Chip:
            {
                Color chipBg = parseColorProp(node.findProperty(props::BG), Color::fromNamed(NamedColor::LightGray));
                if (!node.findProperty(props::BG))
                    chipBg = parseColorProp(node.findProperty(props::COLOR), chipBg); // لون
                float chipRadius = getNumericProp(node.findProperty(props::CORNER_RADIUS), 16.0f); // زوايا
                drawRoundedRect(rect.x, rect.y, rect.width, rect.height, chipBg, chipRadius);

                // حدُّ الرقاقة (إن وُجد) — إطارٌ خارجيٌّ ثمّ ملءٌ داخليّ
                const auto *chipBorderProp = node.findProperty(props::BORDER_COLOR); // لون_الحد
                if (chipBorderProp)
                {
                    Color borderColor = parseColorProp(chipBorderProp, Color::fromNamed(NamedColor::Gray));
                    drawRoundedRect(rect.x, rect.y, rect.width, rect.height, borderColor, chipRadius);
                    drawRoundedRect(rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2,
                                    chipBg, std::max(0.0f, chipRadius - 1));
                }

                const auto *textProp = node.findProperty(props::TEXT_LATIN);
                if (!textProp)
                    textProp = node.findProperty(props::TEXT);
                if (!textProp)
                    textProp = node.findProperty(props::CONTENT); // محتوى
                if (textProp)
                {
                    if (auto *text = std::get_if<std::string>(&textProp->value))
                    {
                        Color chipTextColor = parseColorProp(node.findProperty(props::TEXT_COLOR), {0, 0, 0, 1}); // لون_النص
                        float chipFontSize = getNumericProp(node.findProperty(props::FONT_SIZE_ALT), 13.0f);      // حجم_الخط
                        auto sz = measureText(*text, chipFontSize);
                        drawText(*text, rect.x + (rect.width - sz.first) / 2,
                                 rect.y + (rect.height - sz.second) / 2, chipTextColor, chipFontSize);
                    }
                }
                break;
            }

            case UINodeType::Avatar:
            {
                Color avatarBg = parseColorProp(node.findProperty(props::BG), Color::fromNamed(NamedColor::Primary));
                if (!node.findProperty(props::BG))
                    avatarBg = parseColorProp(node.findProperty(props::COLOR), avatarBg); // لون
                float r = std::min(rect.width, rect.height) / 2;
                drawRoundedRect(rect.x, rect.y, rect.width, rect.height, avatarBg, r);
                const auto *textProp = node.findProperty(props::TEXT_LATIN);
                if (!textProp)
                    textProp = node.findProperty(props::TEXT);
                if (!textProp)
                    textProp = node.findProperty(props::CHAR); // حرف
                if (textProp)
                {
                    if (auto *text = std::get_if<std::string>(&textProp->value))
                    {
                        Color avatarTextColor = parseColorProp(node.findProperty(props::TEXT_COLOR), // لون_النص
                                                               Color::fromNamed(NamedColor::White));
                        float avatarFontSize = getNumericProp(node.findProperty(props::FONT_SIZE_ALT), 18.0f); // حجم_الخط
                        auto sz = measureText(*text, avatarFontSize);
                        drawText(*text, rect.x + (rect.width - sz.first) / 2,
                                 rect.y + (rect.height - sz.second) / 2,
                                 avatarTextColor, avatarFontSize);
                    }
                }
                break;
            }

            case UINodeType::Icon:
            {
                Color iconColor = parseColorProp(node.findProperty(props::COLOR), {0, 0, 0, 1});
                float fontSize = getNumericProp(node.findProperty(props::SIZE), 24.0f);
                const auto *textProp = node.findProperty(props::TEXT_LATIN);
                if (!textProp)
                    textProp = node.findProperty(props::ICON_ALT); // ايقونة
                if (!textProp)
                    textProp = node.findProperty(props::SYMBOL); // رمز
                // (AR) «اسم» — وهو المفتاح الذي تكتبه دالّةُ البناء «أيقونة(اسم)»
                //   في widget_builtins.cpp (props::NAME). لم يكن مقروءًا هنا قطّ،
                //   فكانت `أيقونة("نجمة")` تُنشئ عقدةً وتُخطَّط ولا ترسم بكسلًا
                //   واحدًا: بُنيةٌ سليمة ومفتاحان لا يلتقيان. مقيسٌ على fb0 (٠ بكسل).
                if (!textProp)
                    textProp = node.findProperty(props::NAME); // اسم
                if (textProp)
                {
                    if (auto *text = std::get_if<std::string>(&textProp->value))
                        drawText(*text, rect.x, rect.y, iconColor, fontSize);
                }
                break;
            }

            // ── بقية العناصر — رسم أساسي ──
            case UINodeType::Spacer:
                break;
            case UINodeType::Tooltip:
            {
                // (AR) التلميحُ لا يظهر إلّا عندَ التحويم — هذا تعريفُه. كان
                //      المسارُ المشتركُ يرسمه دائمًا، ومُرسِّمُ سطحِ المكتبِ يقصره
                //      على التحويم؛ التوحيدُ يأخذ الدلالةَ الصحيحة.
                if (isHovered)
                {
                    const auto *tipText = node.findProperty(props::TEXT_LATIN);
                    if (!tipText)
                        tipText = node.findProperty(props::TEXT); // نص
                    if (!tipText)
                        tipText = node.findProperty(props::HINT); // تلميح
                    if (!tipText)
                        tipText = node.findProperty(props::CONTENT); // محتوى
                    if (tipText)
                    {
                        if (auto *t = std::get_if<std::string>(&tipText->value))
                        {
                            const float tipFontSize = 12.0f;
                            const float tipPadX = 8.0f;
                            const float tipPadY = 4.0f;
                            auto sz = measureText(*t, tipFontSize);
                            float tipW = sz.first + tipPadX * 2;
                            float tipH = sz.second + tipPadY * 2;
                            LayoutRect tipRect = {rect.x, rect.y - tipH - 4, tipW, tipH};
                            drawSoftShadow(tipRect, 4.0f, 4.0f, 0.0f, 2.0f, {0, 0, 0, 0.15f});
                            drawRoundedRect(tipRect.x, tipRect.y, tipRect.width, tipRect.height,
                                            {0.15f, 0.15f, 0.15f, 0.9f}, 4.0f);
                            drawText(*t, tipRect.x + tipPadX, tipRect.y + tipPadY,
                                     Color::fromNamed(NamedColor::White), tipFontSize);
                        }
                    }
                }
                break;
            }
            case UINodeType::Breadcrumb:
            {
                // رسم مسارات متصلة بـ "/"
                const auto *pathProp = node.findProperty(props::PATH); // مسار
                if (!pathProp)
                    pathProp = node.findProperty(props::TEXT_LATIN);
                if (!pathProp)
                    pathProp = node.findProperty(props::TEXT);                                             // نص
                float fs = getNumericProp(node.findProperty(props::FONT_SIZE), 13.0f); // حجم_خط
                Color textCol = parseColorProp(node.findProperty(props::COLOR), {0.4f, 0.4f, 0.4f, 1});
                Color activeCol = parseColorProp(node.findProperty(props::ACTIVE_COLOR), Color::fromNamed(NamedColor::Primary)); // لون_نشط
                if (pathProp)
                {
                    if (auto *p = std::get_if<std::string>(&pathProp->value))
                    {
                        float cx = rect.x + 8.0f;
                        float cy = rect.y + (rect.height - fs) * 0.5f;
                        // قسّم المسار على "/" أو "/"
                        std::string seg;
                        std::vector<std::string> segments;
                        for (char ch : *p)
                        {
                            if (ch == '/')
                            {
                                if (!seg.empty())
                                    segments.push_back(seg);
                                seg.clear();
                            }
                            else
                                seg += ch;
                        }
                        if (!seg.empty())
                            segments.push_back(seg);
                        if (segments.empty())
                            segments.push_back(*p);
                        for (size_t si = 0; si < segments.size(); ++si)
                        {
                            bool isLast = (si == segments.size() - 1);
                            Color col = isLast ? activeCol : textCol;
                            auto sz = drawText(segments[si], cx, cy, col, fs);
                            cx += sz.first;
                            if (!isLast)
                            {
                                auto sepSz = drawText(" / ", cx, cy, textCol, fs);
                                cx += sepSz.first;
                            }
                        }
                    }
                }
                break;
            }
            case UINodeType::Pagination:
            {
                // رسم أزرار ترقيم
                int total = (int)getNumericProp(node.findProperty(props::TOTAL), 5.0f); // المجموع
                int current = (int)getNumericProp(node.findProperty(props::CURRENT), 1.0f);       // الحالي
                total = std::max(1, std::min(total, 20));
                current = std::max(1, std::min(current, total));
                float btnW = std::min(36.0f, rect.width / (total + 2));
                float btnH = std::min(rect.height, 36.0f);
                float cx = rect.x + 4.0f;
                float cy = rect.y + (rect.height - btnH) * 0.5f;
                float fs = 13.0f;
                Color activeBg = parseColorProp(node.findProperty(props::COLOR), Color::fromNamed(NamedColor::Primary)); // لون
                Color inactiveBg = {0.92f, 0.92f, 0.92f, 1};
                // زر السابق
                drawRoundedRect(cx, cy, btnW, btnH, inactiveBg, 4.0f);
                auto szPrev = measureText("<", fs);
                drawText("<", cx + (btnW - szPrev.first) * 0.5f, cy + (btnH - szPrev.second) * 0.5f, {0.4f, 0.4f, 0.4f, 1}, fs);
                cx += btnW + 4.0f;
                // أزرار الأرقام
                for (int p = 1; p <= total; ++p)
                {
                    Color bg = (p == current) ? activeBg : inactiveBg;
                    Color tc = (p == current) ? Color::fromNamed(NamedColor::White) : Color{0, 0, 0, 1};
                    drawRoundedRect(cx, cy, btnW, btnH, bg, 4.0f);
                    std::string num = std::to_string(p);
                    auto nsz = measureText(num, fs);
                    drawText(num, cx + (btnW - nsz.first) * 0.5f, cy + (btnH - nsz.second) * 0.5f, tc, fs);
                    cx += btnW + 4.0f;
                }
                // زر التالي
                drawRoundedRect(cx, cy, btnW, btnH, inactiveBg, 4.0f);
                auto szNext = measureText(">", fs);
                drawText(">", cx + (btnW - szNext.first) * 0.5f, cy + (btnH - szNext.second) * 0.5f, {0.4f, 0.4f, 0.4f, 1}, fs);
                break;
            }
            case UINodeType::NavigationLink:
            {
                drawText("\xe2\x86\x92", rect.x + 4, rect.y + 4,
                         Color::fromNamed(NamedColor::Blue), 14.0f);
                break;
            }

            case UINodeType::DataTable:
            {
                drawFilledRect(rect.x, rect.y, rect.width, rect.height,
                               Color::fromNamed(NamedColor::White));
                drawRectOutline(rect.x, rect.y, rect.width, rect.height,
                                Color::fromNamed(NamedColor::Gray));
                float rowH = 30.0f;
                for (float y = rect.y + rowH; y < rect.y + rect.height; y += rowH)
                {
                    drawLine(rect.x, y, rect.x + rect.width, y,
                             Color::fromNamed(NamedColor::Gray), 1.0f);
                }
                break;
            }

            case UINodeType::Timeline:
            {
                drawLine(rect.x + 7, rect.y, rect.x + 7, rect.y + rect.height,
                         Color::fromNamed(NamedColor::Blue), 2.0f);
                break;
            }

            case UINodeType::Carousel:
            {
                drawFilledRect(rect.x, rect.y, rect.width, rect.height, {0.95f, 0.95f, 0.95f, 1});
                drawText("\xe2\x97\x80", rect.x + 4, rect.y + rect.height / 2 - 10,
                         Color::fromNamed(NamedColor::Gray), 20.0f);
                drawText("\xe2\x96\xb6", rect.x + rect.width - 20, rect.y + rect.height / 2 - 10,
                         Color::fromNamed(NamedColor::Gray), 20.0f);
                break;
            }

            case UINodeType::Skeleton:
            {
                drawLinearGradient(rect.x, rect.y, rect.width, rect.height,
                                   {0.82f, 0.82f, 0.82f, 1}, {0.92f, 0.92f, 0.92f, 1}, false, 4.0f);
                break;
            }
            case UINodeType::Shimmer:
            {
                drawLinearGradient(rect.x, rect.y, rect.width, rect.height,
                                   {0.88f, 0.88f, 0.88f, 1}, {0.96f, 0.96f, 0.96f, 1}, false);
                break;
            }

            case UINodeType::BottomNav:
            {
                drawFilledRect(rect.x, rect.y, rect.width, rect.height,
                               Color::fromNamed(NamedColor::White));
                drawLine(rect.x, rect.y, rect.x + rect.width, rect.y,
                         Color::fromNamed(NamedColor::Gray), 1.0f);
                break;
            }

            case UINodeType::Tabs:
            {
                drawFilledRect(rect.x, rect.y, rect.width, rect.height, {0.95f, 0.95f, 0.95f, 1});
                drawFilledRect(rect.x, rect.y + rect.height - 2, rect.width, 2,
                               Color::fromNamed(NamedColor::Blue));
                break;
            }

            case UINodeType::VideoPlayer:
            {
                drawFilledRect(rect.x, rect.y, rect.width, rect.height,
                               Color::fromNamed(NamedColor::Black));
                drawText("\xe2\x96\xb6", rect.x + rect.width / 2 - 12, rect.y + rect.height / 2 - 12,
                         Color::fromNamed(NamedColor::White), 24.0f);
                break;
            }

            case UINodeType::MapView:
            {
                drawFilledRect(rect.x, rect.y, rect.width, rect.height, {0.85f, 0.92f, 0.85f, 1});
                break;
            }

            // ── الحوار النمطيّ (Dialog) — لوح مرتفع بعنوان (كان يُرسَم في الويب فقط) ──
            case UINodeType::Dialog:
            {
                float dlgRadius = getNumericProp(node.findProperty(props::CORNER_RADIUS), 12.0f); // زوايا

                // (AR) الحوارُ طبقةٌ فوقَ الشاشةِ لا عنصرٌ في التدفّق: حجابٌ معتمٌ
                //      ثمّ لوحٌ في الوسط. متى جُهِل منفذُ العرضِ رُسِم في مستطيلِ
                //      التخطيطِ كما كان.
                // (AR) الحجابُ يغطّي منفذَ العرضِ كلَّه، أمّا **اللوحُ فيُرسَم في
                //   مستطيلِ التخطيط**: أبناءُ العقدةِ تُرسَم بمستطيلاتِ التخطيطِ
                //   في الخطوةِ التالية، فلو نُقِل اللوحُ إلى وسطِ الشاشةِ وحدَه
                //   لانفصلت الكِسوةُ عن محتواها — لوحٌ فارغٌ وأزرارٌ خارجَه.
                //   دَينٌ معلَن: توسيطُ اللوحِ الحقيقيُّ يلزمه أن يُعطيَ محرّكُ
                //   التخطيطِ العقدةَ الطافيةَ مستطيلَها، لا أن يزيحها المُرسِّم.
                LayoutRect dlgPanel = rect;
                if (viewportWidth_ > 0 && viewportHeight_ > 0)
                {
                    drawFilledRect(0, 0, static_cast<float>(viewportWidth_),
                                   static_cast<float>(viewportHeight_), {0, 0, 0, 0.5f}); // حجاب
                }

                drawSoftShadow(dlgPanel, dlgRadius, 24.0f, 0.0f, 8.0f, {0, 0, 0, 0.3f});
                Color dlgBg = parseColorProp(node.findProperty(props::BG), // خلفية
                                             Color::fromNamed(NamedColor::White));
                drawRoundedRect(dlgPanel.x, dlgPanel.y, dlgPanel.width, dlgPanel.height,
                                dlgBg, dlgRadius);
                // العنوان (إن وُجد فقط — لا نصّ افتراضيّ مباشر)
                const auto *titleProp = node.findProperty(props::TITLE); // عنوان
                if (!titleProp)
                    titleProp = node.findProperty(props::TITLE_LATIN);
                float dlgTextY = dlgPanel.y + 14;
                if (titleProp)
                {
                    if (auto *title = std::get_if<std::string>(&titleProp->value))
                    {
                        Color titleColor = parseColorProp(node.findProperty(props::TEXT_COLOR), {0, 0, 0, 1}); // لون_النص
                        float fs = getNumericProp(node.findProperty(props::FONT_SIZE_ALT), 18.0f);             // حجم_الخط
                        float tx = dlgPanel.x + 20;
                        if (isArabicText(*title))
                        {
                            auto sz = measureText(*title, fs);
                            tx = dlgPanel.x + dlgPanel.width - sz.first - 20;
                            if (tx < dlgPanel.x)
                                tx = dlgPanel.x; // مطابقة حارس AppBar — منع القصّ يسارًا
                        }
                        drawText(*title, tx, dlgTextY, titleColor, fs);
                        dlgTextY += fs + 14;
                    }
                }
                // المحتوى (إن وُجد) أسفلَ العنوان
                const auto *dlgContentProp = node.findProperty(props::CONTENT); // محتوى
                if (!dlgContentProp)
                    dlgContentProp = node.findProperty(props::CONTENT_LATIN);
                if (dlgContentProp)
                {
                    if (auto *c = std::get_if<std::string>(&dlgContentProp->value))
                    {
                        float cfs = getNumericProp(node.findProperty(props::FONT_SIZE), 14.0f); // حجم_خط
                        float cx = dlgPanel.x + 20;
                        if (isArabicText(*c))
                        {
                            auto sz = measureText(*c, cfs);
                            cx = dlgPanel.x + dlgPanel.width - sz.first - 20;
                            if (cx < dlgPanel.x)
                                cx = dlgPanel.x;
                        }
                        drawText(*c, cx, dlgTextY, {0.4f, 0.4f, 0.4f, 1.0f}, cfs);
                    }
                }
                break;
            }

            // ── منطقة النصّ (TextArea) — صندوق إدخال متعدّد الأسطر (كان في الويب فقط) ──
            case UINodeType::TextArea:
            {
                float taRadius = getNumericProp(node.findProperty(props::CORNER_RADIUS), 6.0f); // زوايا
                Color taBg = parseColorProp(node.findProperty(props::BG),           // خلفية
                                            Color::fromNamed(NamedColor::White));
                // (AR) الحدُّ يتبع التركيزَ كحقلِ النصّ. دَينٌ معلَن: مؤشّرُ الكتابةِ
                //      في منطقةِ نصٍّ متعدّدةِ الأسطرِ يحتاج موضعًا واعيًا بالسطر،
                //      ولا يملكه أيٌّ من المسارَين اليوم فلم يُنقَل.
                Color taBorder = (focusedNode_ == &node) ? Color::fromNamed(NamedColor::Primary)
                                                         : Color::fromNamed(NamedColor::Gray);
                float taBorderW = (focusedNode_ == &node) ? 2.0f : 1.0f;
                drawRoundedRect(rect.x, rect.y, rect.width, rect.height, taBorder, taRadius);
                drawRoundedRect(rect.x + taBorderW, rect.y + taBorderW,
                                rect.width - taBorderW * 2, rect.height - taBorderW * 2,
                                taBg, std::max(0.0f, taRadius - taBorderW));
                // القيمة إن وُجدت وإلا التلميح (بلون باهت)
                const auto *valProp = node.findProperty(props::VALUE); // قيمة
                if (!valProp)
                    valProp = node.findProperty(props::VALUE_LATIN);
                Color textCol = {0, 0, 0, 1};
                if (!valProp)
                {
                    valProp = node.findProperty(props::HINT); // تلميح
                    if (!valProp)
                        valProp = node.findProperty(props::PLACEHOLDER_LATIN);
                    textCol = {0.5f, 0.5f, 0.5f, 1};
                }
                if (valProp)
                {
                    if (auto *val = std::get_if<std::string>(&valProp->value))
                    {
                        float fs = getNumericProp(node.findProperty(props::FONT_SIZE_ALT), 14.0f); // حجم_الخط
                        float tx = rect.x + 10;
                        if (isArabicText(*val))
                        {
                            auto sz = measureText(*val, fs);
                            tx = rect.x + rect.width - sz.first - 10;
                            if (tx < rect.x)
                                tx = rect.x; // مطابقة حارس AppBar — منع القصّ يسارًا
                        }
                        drawText(*val, tx, rect.y + 8, textCol, fs);
                    }
                }
                break;
            }

            // ── الدرج الجانبيّ (Drawer) — لوح تنقّل بخطّ فاصل (كان في الويب فقط) ──
            case UINodeType::Drawer:
            {
                Color drawerBg = parseColorProp(node.findProperty(props::BG), // خلفية
                                                Color::fromNamed(NamedColor::White));

                // (AR) الدرجُ الجانبيُّ طبقةٌ فوقَ الشاشة: حجابٌ معتمٌ يغطّي منفذَ
                //      العرضِ كلَّه ثمّ اللوحُ نفسُه بظلٍّ. إن كان منفذُ العرضِ
                //      مجهولًا (منصّةٌ لا تُبلِّغه) يُرسَم في مستطيلِ التخطيطِ فقط.
                // (AR) الحجابُ يغطّي الشاشة، واللوحُ في مستطيلِ التخطيط (لا في
                //   يسارِ الشاشةِ صلبًا: الدرجُ في العربيّةِ يمينٌ، وموضعُه قرارُ
                //   محرّكِ التخطيطِ لا المُرسِّم). الأبناءُ يتبعون المستطيلَ نفسَه.
                if (viewportWidth_ > 0 && viewportHeight_ > 0)
                {
                    drawFilledRect(0, 0, static_cast<float>(viewportWidth_),
                                   static_cast<float>(viewportHeight_), {0, 0, 0, 0.4f}); // حجاب
                }
                drawSoftShadow(rect, 0.0f, 20.0f, 4.0f, 0.0f, {0, 0, 0, 0.3f});
                drawFilledRect(rect.x, rect.y, rect.width, rect.height, drawerBg);
                // خطّ فاصل على الحافّة الداخليّة (RTL ⇒ الحافّة اليسرى)
                drawLine(rect.x, rect.y, rect.x, rect.y + rect.height,
                         Color::fromNamed(NamedColor::Gray), 1.0f);
                break;
            }

            // ── النافذة (Window) — لوح مرتفع بحدود (م١ قشرة سطح المكتب) ──
            case UINodeType::Window:
            {
                float wr = getNumericProp(node.findProperty(props::CORNER_RADIUS), 8.0f); // زوايا
                // ظلّ الإسقاط لإيحاء التراكب
                drawFilledRect(rect.x + 3, rect.y + 8, rect.width, rect.height, {0, 0, 0, 0.22f});
                Color winBg = parseColorProp(node.findProperty(props::BG),          // خلفية
                                             Color::fromNamed(NamedColor::White));
                drawRoundedRect(rect.x, rect.y, rect.width, rect.height, winBg, wr);
                drawRectOutline(rect.x, rect.y, rect.width, rect.height,
                                Color::fromNamed(NamedColor::Gray));
                break;
            }

            // ── شريط العنوان (TitleBar) — شريط ملوَّن + عنوان RTL + أزرار النافذة ──
            case UINodeType::TitleBar:
            {
                Color barColor = parseColorProp(node.findProperty(props::BG_COLOR), // لون_خلفية
                                                Color::fromNamed(NamedColor::Primary));
                if (!node.findProperty(props::BG_COLOR))
                    barColor = parseColorProp(node.findProperty(props::COLOR), barColor); // لون
                drawFilledRect(rect.x, rect.y, rect.width, rect.height, barColor);
                // العنوان (محاذاة RTL يمينًا كـ AppBar)
                const auto *titleProp = node.findProperty(props::TITLE); // عنوان
                if (!titleProp)
                    titleProp = node.findProperty(props::TEXT_LATIN);
                if (!titleProp)
                    titleProp = node.findProperty(props::TEXT); // نص
                if (titleProp)
                {
                    if (auto *t = std::get_if<std::string>(&titleProp->value))
                    {
                        Color tc = parseColorProp(node.findProperty(props::TEXT_COLOR), // لون_النص
                                                  Color::fromNamed(NamedColor::White));
                        float fs = getNumericProp(node.findProperty(props::FONT_SIZE_ALT), 16.0f); // حجم_الخط
                        float tx = rect.x + 14;
                        if (isArabicText(*t))
                        {
                            auto sz = measureText(*t, fs);
                            tx = rect.x + rect.width - sz.first - 14;
                            if (tx < rect.x)
                                tx = rect.x; // مطابقة حارس AppBar
                        }
                        drawText(*t, tx, rect.y + rect.height / 2 - fs / 2, tc, fs);
                    }
                }
                // أزرار النافذة الثلاثة (يسار في RTL): إغلاق/تصغير/تكبير — دوائر ملوّنة
                float br = 6.0f;                          // نصف قطر الزرّ
                float cy = rect.y + rect.height / 2.0f;
                float bx = rect.x + 14.0f;                // من اليسار
                float gap = 22.0f;
                Color btnClose{0.906f, 0.298f, 0.235f, 1.0f}; // أحمر (إغلاق)
                Color btnMin{0.953f, 0.612f, 0.071f, 1.0f};   // كهرمانيّ (تصغير)
                Color btnMax{0.180f, 0.800f, 0.443f, 1.0f};   // أخضر (تكبير)
                drawRoundedRect(bx, cy - br, br * 2, br * 2, btnClose, br);
                drawRoundedRect(bx + gap, cy - br, br * 2, br * 2, btnMin, br);
                drawRoundedRect(bx + 2 * gap, cy - br, br * 2, br * 2, btnMax, br);
                break;
            }

            // ── شريط التمرير (ScrollBar) — مسار + إبهام بموضع نسبيّ للقيمة ──
            case UINodeType::ScrollBar:
            {
                Color track = parseColorProp(node.findProperty(props::BG), {0.90f, 0.90f, 0.90f, 1}); // خلفية
                float sr = rect.width / 2.0f; // شريط رأسيّ نحيل ⇒ نصف العرض
                drawRoundedRect(rect.x, rect.y, rect.width, rect.height, track, sr);
                float val = getNumericProp(node.findProperty(props::VALUE), 30.0f) / 100.0f; // قيمة
                val = std::max(0.0f, std::min(1.0f, val));
                // حدّ علويّ بارتفاع المسار: يمنع إبهامًا أطول من الشريط (⇒ thumbY<rect.y)
                float thumbH = std::min(rect.height, std::max(24.0f, rect.height * 0.3f));
                float thumbY = rect.y + (rect.height - thumbH) * val;
                Color thumb = parseColorProp(node.findProperty(props::COLOR), // لون
                                             Color::fromNamed(NamedColor::Gray));
                drawRoundedRect(rect.x, thumbY, rect.width, thumbH, thumb, sr);
                break;
            }

            // ── شريط المهامّ (Taskbar) — شريط صدَفة سفليّ داكن (م٢ قشرة سطح المكتب) ──
            case UINodeType::Taskbar:
            {
                Color barColor = parseColorProp(node.findProperty(props::BG_COLOR), // لون_خلفية
                                                {0.043f, 0.086f, 0.165f, 0.96f});   // كحليّ داكن
                if (!node.findProperty(props::BG_COLOR))
                    barColor = parseColorProp(node.findProperty(props::BG), barColor); // خلفية
                drawFilledRect(rect.x, rect.y, rect.width, rect.height, barColor);
                // خطّ إضاءة علويّ رفيع يفصل الشريط عن سطح المكتب
                drawLine(rect.x, rect.y, rect.x + rect.width, rect.y, {1, 1, 1, 0.12f}, 1.0f);
                break;
            }

            // ── قائمة ابدأ (StartMenu) — لوح منبثق مرتفع لإطلاق التطبيقات ──
            case UINodeType::StartMenu:
            {
                float smr = getNumericProp(node.findProperty(props::CORNER_RADIUS), 10.0f); // زوايا
                // ظلّ لأعلى (اللوح ينبثق من شريط المهامّ صعودًا)
                drawFilledRect(rect.x + 2, rect.y - 6, rect.width, rect.height + 6, {0, 0, 0, 0.28f});
                Color smBg = parseColorProp(node.findProperty(props::BG),           // خلفية
                                            {0.086f, 0.149f, 0.290f, 0.98f});       // لوح كحليّ
                drawRoundedRect(rect.x, rect.y, rect.width, rect.height, smBg, smr);
                break;
            }

            // ── لوحة النظام (SystemTray) — ساعة/نصّ حالة (يمين RTL) + أيقونات (أبناء) ──
            case UINodeType::SystemTray:
            {
                const auto *bgProp = node.findProperty(props::BG); // خلفية
                if (bgProp)
                {
                    Color trayBg = parseColorProp(bgProp, {1, 1, 1, 0.08f});
                    drawRoundedRect(rect.x, rect.y, rect.width, rect.height, trayBg, 6.0f);
                }
                const auto *tProp = node.findProperty(props::TEXT_LATIN);
                if (!tProp)
                    tProp = node.findProperty(props::TEXT); // نص
                if (!tProp)
                    tProp = node.findProperty(props::VALUE); // قيمة
                if (tProp)
                {
                    if (auto *t = std::get_if<std::string>(&tProp->value))
                    {
                        Color tc = parseColorProp(node.findProperty(props::COLOR), // لون
                                                  Color::fromNamed(NamedColor::White));
                        float fs = getNumericProp(node.findProperty(props::FONT_SIZE_ALT), 15.0f); // حجم_الخط
                        float tx = rect.x + 10;
                        if (isArabicText(*t))
                        {
                            auto sz = measureText(*t, fs);
                            tx = rect.x + rect.width - sz.first - 10;
                            if (tx < rect.x)
                                tx = rect.x; // مطابقة حارس AppBar
                        }
                        drawText(*t, tx, rect.y + rect.height / 2 - fs / 2, tc, fs);
                    }
                }
                break;
            }

            // ── قائمة الخيارات/السياق (Menu) — لوح منبثق مرتفع ببنود (م٣ قوائم وأوامر) ──
            case UINodeType::Menu:
            {
                float mr = getNumericProp(node.findProperty(props::CORNER_RADIUS), 8.0f); // زوايا
                drawFilledRect(rect.x + 2, rect.y + 4, rect.width, rect.height, {0, 0, 0, 0.22f}); // ظلّ
                Color menuBg = parseColorProp(node.findProperty(props::BG),         // خلفية
                                              Color::fromNamed(NamedColor::White));
                drawRoundedRect(rect.x, rect.y, rect.width, rect.height, menuBg, mr);
                drawRectOutline(rect.x, rect.y, rect.width, rect.height,
                                Color::fromNamed(NamedColor::LightGray));
                break;
            }

            // ── شريط الأدوات (Toolbar) — شريط فاتح بحدّ سفليّ (حاوية لأزرار الأوامر) ──
            case UINodeType::Toolbar:
            {
                Color tbBg = parseColorProp(node.findProperty(props::BG), {0.96f, 0.96f, 0.97f, 1}); // خلفية
                drawFilledRect(rect.x, rect.y, rect.width, rect.height, tbBg);
                drawLine(rect.x, rect.y + rect.height, rect.x + rect.width, rect.y + rect.height,
                         Color::fromNamed(NamedColor::LightGray), 1.0f); // حدّ سفليّ يفصل عن المحتوى
                break;
            }

            // ── التحكّم المقسّم (SegmentedControl) — حبّة مستديرة تحوي شرائح (أبناء) ──
            case UINodeType::SegmentedControl:
            {
                Color segBg = parseColorProp(node.findProperty(props::BG), {0.88f, 0.89f, 0.91f, 1}); // خلفية
                float r = rect.height / 2.0f; // حبّة مستديرة الحوافّ
                drawRoundedRect(rect.x, rect.y, rect.width, rect.height, segBg, r);
                break;
            }

            // ── عرض شجريّ (TreeView) — لوح تصفّح جانبيّ بخطّ إطاريّ (م٤ عروض المستكشف) ──
            case UINodeType::TreeView:
            {
                Color treeBg = parseColorProp(node.findProperty(props::BG), // خلفية
                                              Color::fromNamed(NamedColor::White));
                drawFilledRect(rect.x, rect.y, rect.width, rect.height, treeBg);
                // خطّ فاصل على الحافّة الداخليّة يفصل الشجرة عن لوح التفاصيل. في RTL
                // يجلس لوح الشجرة يمينًا والتفاصيل يساره، فالحافّة الداخليّة = rect.x.
                drawLine(rect.x, rect.y, rect.x, rect.y + rect.height,
                         Color::fromNamed(NamedColor::LightGray), 1.0f);
                break;
            }

            // ── لوح مقسّم (SplitView) — حاوية سيّد/تفصيل بمقبض فاصل (م٤ عروض المستكشف) ──
            case UINodeType::SplitView:
            {
                Color splitBg = parseColorProp(node.findProperty(props::BG), // خلفية
                                               Color::fromNamed(NamedColor::White));
                drawRoundedRect(rect.x, rect.y, rect.width, rect.height, splitBg, 6.0f);
                drawRectOutline(rect.x, rect.y, rect.width, rect.height,
                                Color::fromNamed(NamedColor::LightGray));
                // مقبضُ الفصلِ في المنتصف
                drawFilledRect(rect.x + rect.width / 2.0f - 1, rect.y, 2.0f, rect.height,
                               Color::fromNamed(NamedColor::Gray));
                break;
            }

            // ── قائمة منسدلة (Picker/Dropdown) — صندوق + سهم منسدل + قيمة/تلميح (م٥ إدخال متقدّم) ──
            case UINodeType::Picker:
            {
                float pkRad = getNumericProp(node.findProperty(props::CORNER_RADIUS), 6.0f); // زوايا
                Color pkBg = parseColorProp(node.findProperty(props::BG),                    // خلفية
                                            Color::fromNamed(NamedColor::White));
                drawRoundedRect(rect.x, rect.y, rect.width, rect.height, pkBg, pkRad);
                drawRectOutline(rect.x, rect.y, rect.width, rect.height,
                                Color::fromNamed(NamedColor::Gray));
                // سهم منسدل ▾ (خطّان) على الحافّة اليسرى — مقبض القائمة يسار في RTL
                float chvX = rect.x + 16.0f;
                float chvY = rect.y + rect.height / 2.0f;
                Color chv = Color::fromNamed(NamedColor::Gray);
                drawLine(chvX - 6, chvY - 3, chvX, chvY + 4, chv, 2.0f);
                drawLine(chvX, chvY + 4, chvX + 6, chvY - 3, chv, 2.0f);
                // القيمة إن وُجدت وإلّا التلميح بلون باهت (RTL يمينًا)
                const auto *pkVal = node.findProperty(props::VALUE); // قيمة
                if (!pkVal)
                    pkVal = node.findProperty(props::VALUE_LATIN);
                Color pkTextCol = Color::fromNamed(NamedColor::Black);
                if (!pkVal)
                {
                    pkVal = node.findProperty(props::HINT); // تلميح
                    pkTextCol = {0.5f, 0.5f, 0.5f, 1};
                }
                if (pkVal)
                {
                    if (auto *v = std::get_if<std::string>(&pkVal->value))
                    {
                        float fs = getNumericProp(node.findProperty(props::FONT_SIZE_ALT), 15.0f); // حجم_الخط
                        float tx = rect.x + 34;
                        if (isArabicText(*v))
                        {
                            auto sz = measureText(*v, fs);
                            tx = rect.x + rect.width - sz.first - 12;
                            if (tx < rect.x + 30)
                                tx = rect.x + 30; // منع القصّ فوق السهم
                        }
                        drawText(*v, tx, rect.y + rect.height / 2 - fs / 2, pkTextCol, fs);
                    }
                }
                break;
            }

            // ── منتقي تاريخ (DatePicker) — صندوق + أيقونة تقويم + قيمة (م٥ إدخال متقدّم) ──
            case UINodeType::DatePicker:
            {
                float dpRad = getNumericProp(node.findProperty(props::CORNER_RADIUS), 6.0f); // زوايا
                Color dpBg = parseColorProp(node.findProperty(props::BG),                    // خلفية
                                            Color::fromNamed(NamedColor::White));
                drawRoundedRect(rect.x, rect.y, rect.width, rect.height, dpBg, dpRad);
                drawRectOutline(rect.x, rect.y, rect.width, rect.height,
                                Color::fromNamed(NamedColor::Gray));
                // أيقونة تقويم مصغّرة (مربّع بشريط علويّ) على الحافّة اليسرى
                float calX = rect.x + 10.0f;
                float calY = rect.y + rect.height / 2.0f - 8.0f;
                Color cal = Color::fromNamed(NamedColor::Primary);
                drawRectOutline(calX, calY, 16, 16, cal);
                drawFilledRect(calX, calY, 16, 4, cal); // شريط رأس التقويم
                // القيمة (التاريخ) بمحاذاة RTL يمينًا
                const auto *dpVal = node.findProperty(props::VALUE); // قيمة
                if (!dpVal)
                    dpVal = node.findProperty(props::VALUE_LATIN);
                Color dpTextCol = Color::fromNamed(NamedColor::Black);
                if (!dpVal)
                {
                    dpVal = node.findProperty(props::HINT); // تلميح
                    dpTextCol = {0.5f, 0.5f, 0.5f, 1};
                }
                if (dpVal)
                {
                    if (auto *v = std::get_if<std::string>(&dpVal->value))
                    {
                        float fs = getNumericProp(node.findProperty(props::FONT_SIZE_ALT), 15.0f); // حجم_الخط
                        float tx = rect.x + 34;
                        if (isArabicText(*v))
                        {
                            auto sz = measureText(*v, fs);
                            tx = rect.x + rect.width - sz.first - 12;
                            if (tx < rect.x + 32)
                                tx = rect.x + 32;
                        }
                        drawText(*v, tx, rect.y + rect.height / 2 - fs / 2, dpTextCol, fs);
                    }
                }
                break;
            }

            // ── خطوات (Stepper) — [−] قيمة [+] بأزرار طرفيّة (م٥ إدخال متقدّم) ──
            case UINodeType::Stepper:
            {
                float stRad = getNumericProp(node.findProperty(props::CORNER_RADIUS), 6.0f); // زوايا
                Color stBg = parseColorProp(node.findProperty(props::BG),                    // خلفية
                                            Color::fromNamed(NamedColor::LightGray));
                drawRoundedRect(rect.x, rect.y, rect.width, rect.height, stBg, stRad);
                drawRectOutline(rect.x, rect.y, rect.width, rect.height,
                                Color::fromNamed(NamedColor::Gray));
                float btn = rect.height; // منطقة الزرّ مربّعة بارتفاع العنصر
                float cy = rect.y + rect.height / 2.0f;
                Color sign = Color::fromNamed(NamedColor::Black);
                // زرّ الطرح (يسار) — خطّ أفقيّ
                float minusCx = rect.x + btn / 2.0f;
                drawLine(minusCx - 7, cy, minusCx + 7, cy, sign, 2.0f);
                // زرّ الجمع (يمين) — خطّان متقاطعان
                float plusCx = rect.x + rect.width - btn / 2.0f;
                drawLine(plusCx - 7, cy, plusCx + 7, cy, sign, 2.0f);
                drawLine(plusCx, cy - 7, plusCx, cy + 7, sign, 2.0f);
                // خطّا فاصلان بين الأزرار والقيمة
                drawLine(rect.x + btn, rect.y + 4, rect.x + btn, rect.y + rect.height - 4,
                         Color::fromNamed(NamedColor::Gray), 1.0f);
                drawLine(rect.x + rect.width - btn, rect.y + 4, rect.x + rect.width - btn,
                         rect.y + rect.height - 4, Color::fromNamed(NamedColor::Gray), 1.0f);
                // القيمة في الوسط
                const auto *stVal = node.findProperty(props::VALUE); // قيمة
                if (!stVal)
                    stVal = node.findProperty(props::VALUE_LATIN);
                if (stVal)
                {
                    if (auto *v = std::get_if<std::string>(&stVal->value))
                    {
                        float fs = getNumericProp(node.findProperty(props::FONT_SIZE_ALT), 16.0f); // حجم_الخط
                        auto sz = measureText(*v, fs);
                        drawText(*v, rect.x + rect.width / 2 - sz.first / 2,
                                 cy - fs / 2, Color::fromNamed(NamedColor::Black), fs);
                    }
                }
                break;
            }

            // ── تقييم (RatingBar) — نجوم مربّعة ممتلئة حتّى القيمة (م٥ إدخال متقدّم) ──
            case UINodeType::RatingBar:
            {
                int maxStars = static_cast<int>(getNumericProp(node.findProperty(props::MAX), 5.0f)); // أقصى
                if (maxStars < 1)
                    maxStars = 5;
                float filled = getNumericProp(node.findProperty(props::VALUE), 0.0f); // قيمة
                float star = std::min(rect.height - 6.0f, 22.0f);
                float gap = 6.0f;
                float sy = rect.y + (rect.height - star) / 2.0f;
                Color gold{0.96f, 0.77f, 0.13f, 1};
                Color empty = Color::fromNamed(NamedColor::Gray);
                for (int i = 0; i < maxStars; ++i)
                {
                    float sx = rect.x + 4.0f + static_cast<float>(i) * (star + gap);
                    if (static_cast<float>(i) < filled)
                        drawFilledRect(sx, sy, star, star, gold);
                    else
                        drawRectOutline(sx, sy, star, star, empty);
                }
                break;
            }

            // ── تنبيه (Alert) — لوح تغذية راجعة مرتفع + شريط لون حالة + عنوان/رسالة (م٦) ──
            case UINodeType::Alert:
            {
                float alRad = getNumericProp(node.findProperty(props::CORNER_RADIUS), 10.0f); // زوايا
                // ظلّ خفيف لإيحاء الارتفاع (بنمط Dialog)
                drawFilledRect(rect.x + 2, rect.y + 5, rect.width, rect.height, {0, 0, 0, 0.16f});
                Color alBg = parseColorProp(node.findProperty(props::BG),           // خلفية
                                            Color::fromNamed(NamedColor::White));
                drawRoundedRect(rect.x, rect.y, rect.width, rect.height, alBg, alRad);
                // شريط لون الحالة على الحافّة اليمنى (RTL: البداية يمين) — لون التنبيه
                Color accent = parseColorProp(node.findProperty(props::COLOR), // لون
                                              Color::fromNamed(NamedColor::Error));
                drawFilledRect(rect.x + rect.width - 5, rect.y, 5, rect.height, accent);
                // العنوان (إن وُجد) بمحاذاة RTL يمينًا
                const auto *alTitle = node.findProperty(props::TITLE); // عنوان
                if (!alTitle)
                    alTitle = node.findProperty(props::TITLE_LATIN);
                float ty = rect.y + 16;
                if (alTitle)
                {
                    if (auto *t = std::get_if<std::string>(&alTitle->value))
                    {
                        float fs = getNumericProp(node.findProperty(props::FONT_SIZE_ALT), 18.0f); // حجم_الخط
                        float tx = rect.x + 18;
                        if (isArabicText(*t))
                        {
                            auto sz = measureText(*t, fs);
                            tx = rect.x + rect.width - sz.first - 18;
                            if (tx < rect.x)
                                tx = rect.x;
                        }
                        drawText(*t, tx, ty, accent, fs);
                        ty += fs + 10;
                    }
                }
                // الرسالة (نصّ) بلون داكن أسفل العنوان
                const auto *alMsg = node.findProperty(props::TEXT); // نص
                if (!alMsg)
                    alMsg = node.findProperty(props::TEXT_LATIN);
                if (alMsg)
                {
                    if (auto *m = std::get_if<std::string>(&alMsg->value))
                    {
                        float mfs = getNumericProp(node.findProperty(props::FONT_SIZE), 15.0f); // حجم_خط
                        float mx = rect.x + 18;
                        if (isArabicText(*m))
                        {
                            auto sz = measureText(*m, mfs);
                            mx = rect.x + rect.width - sz.first - 18;
                            if (mx < rect.x)
                                mx = rect.x;
                        }
                        drawText(*m, mx, ty, {0.25f, 0.25f, 0.25f, 1}, mfs);
                    }
                }
                break;
            }

            // ── مربّع دوّار (SpinBox) — حقل رقميّ + زرّا زيادة/نقصان رأسيّان (م٥-ب) ──
            case UINodeType::SpinBox:
            {
                float sbRad = getNumericProp(node.findProperty(props::CORNER_RADIUS), 6.0f); // زوايا
                Color sbBg = parseColorProp(node.findProperty(props::BG),                    // خلفية
                                            Color::fromNamed(NamedColor::White));
                drawRoundedRect(rect.x, rect.y, rect.width, rect.height, sbBg, sbRad);
                drawRectOutline(rect.x, rect.y, rect.width, rect.height,
                                Color::fromNamed(NamedColor::Gray));
                // منطقة الأزرار على الحافّة اليسرى (RTL: القيمة يمين، الأزرار يسار)
                float btnW = 26.0f;
                float midY = rect.y + rect.height / 2.0f;
                // خطّ فاصل رأسيّ بين حقل القيمة ومنطقة الأزرار
                drawLine(rect.x + btnW, rect.y + 3, rect.x + btnW, rect.y + rect.height - 3,
                         Color::fromNamed(NamedColor::Gray), 1.0f);
                // خطّ فاصل أفقيّ يقسم منطقة الأزرار إلى (▲ أعلى / ▼ أسفل)
                drawLine(rect.x, midY, rect.x + btnW, midY,
                         Color::fromNamed(NamedColor::Gray), 1.0f);
                Color sign = Color::fromNamed(NamedColor::Black);
                float acx = rect.x + btnW / 2.0f;
                // سهم الزيادة (▲) في النصف العلويّ — خطّان مائلان لأعلى
                float upY = rect.y + rect.height * 0.25f;
                drawLine(acx - 6, upY + 4, acx, upY - 3, sign, 2.0f);
                drawLine(acx, upY - 3, acx + 6, upY + 4, sign, 2.0f);
                // سهم النقصان (▼) في النصف السفليّ — خطّان مائلان لأسفل
                float dnY = rect.y + rect.height * 0.75f;
                drawLine(acx - 6, dnY - 4, acx, dnY + 3, sign, 2.0f);
                drawLine(acx, dnY + 3, acx + 6, dnY - 4, sign, 2.0f);
                // القيمة في حقل النصّ (يمين، محاذاة RTL)
                const auto *sbVal = node.findProperty(props::VALUE); // قيمة
                if (!sbVal)
                    sbVal = node.findProperty(props::VALUE_LATIN);
                if (sbVal)
                {
                    if (auto *v = std::get_if<std::string>(&sbVal->value))
                    {
                        float fs = getNumericProp(node.findProperty(props::FONT_SIZE_ALT), 16.0f); // حجم_الخط
                        auto sz = measureText(*v, fs);
                        // محاذاة يمنى مقصودة (عرف الحقول الرقميّة، عربيّ ولاتينيّ سواء)
                        float tx = rect.x + rect.width - sz.first - 12;
                        if (tx < rect.x + btnW + 6)
                            tx = rect.x + btnW + 6;
                        drawText(*v, tx, midY - fs / 2, Color::fromNamed(NamedColor::Black), fs);
                    }
                }
                break;
            }

            // ── صندوق تجميع (GroupBox) — إطار بعنوان علويّ يفصله عن الحدّ (م٦-ب) ──
            case UINodeType::GroupBox:
            {
                float gbRad = getNumericProp(node.findProperty(props::CORNER_RADIUS), 6.0f); // زوايا
                const auto *bgProp = node.findProperty(props::BG); // خلفية
                if (bgProp)
                    drawRoundedRect(rect.x, rect.y, rect.width, rect.height,
                                    parseColorProp(bgProp, Color::fromNamed(NamedColor::White)), gbRad);
                Color border = parseColorProp(node.findProperty(props::COLOR), // لون
                                              Color::fromNamed(NamedColor::Gray));
                drawRectOutline(rect.x, rect.y, rect.width, rect.height, border);
                // العنوان مطبوع على الحدّ العلويّ (RTL: يمينًا) مع خلفيّة تكسر الإطار
                const auto *gbTitle = node.findProperty(props::TITLE); // عنوان
                if (!gbTitle)
                    gbTitle = node.findProperty(props::TITLE_LATIN);
                if (gbTitle)
                {
                    if (auto *t = std::get_if<std::string>(&gbTitle->value))
                    {
                        float fs = getNumericProp(node.findProperty(props::FONT_SIZE_ALT), 15.0f); // حجم_الخط
                        auto sz = measureText(*t, fs);
                        // افتراض يسار؛ يُقلَب يمينًا للنصّ العربيّ (نمط TitleBar/Alert)
                        float tx = rect.x + 14;
                        if (isArabicText(*t))
                        {
                            tx = rect.x + rect.width - sz.first - 14;
                            if (tx < rect.x + 8)
                                tx = rect.x + 8;
                        }
                        // رقعة كسر الإطار تُرسَم فقط حين تُعرف خلفيّة الصندوق (تجنّب لطخة
                        // بيضاء فوق أب غير أبيض حين تُترك الخلفيّة شفّافة)
                        if (bgProp)
                        {
                            Color surface = parseColorProp(bgProp, Color::fromNamed(NamedColor::White));
                            drawFilledRect(tx - 5, rect.y - fs / 2, sz.first + 10, fs, surface);
                        }
                        Color tc = parseColorProp(node.findProperty(props::TEXT_COLOR), // لون_النص
                                                  Color::fromNamed(NamedColor::Black));
                        drawText(*t, tx, rect.y - fs / 2, tc, fs);
                    }
                }
                break;
            }

            // ── مؤشّر انشغال (Spinner) — حلقة تحميل دائريّة بنقاط متلاشية (م٦-ب) ──
            case UINodeType::Spinner:
            {
                float cx = rect.x + rect.width / 2.0f;
                float cy = rect.y + rect.height / 2.0f;
                float radius = std::min(rect.width, rect.height) / 2.0f - 4.0f;
                if (radius < 4.0f)
                    radius = 4.0f;
                // حلقة المسار الباهتة
                drawCircleOutline(cx, cy, radius, {0.80f, 0.80f, 0.80f, 1}, 3.0f);
                // اثنتا عشرة نقطة حول المحيط بتلاشٍ تدريجيّ يوحي بالدوران
                Color accent = parseColorProp(node.findProperty(props::COLOR), // لون
                                              Color::fromNamed(NamedColor::Primary));
                const int dots = 12;
                float dotR = std::max(2.0f, radius * 0.16f);
                for (int i = 0; i < dots; ++i)
                {
                    // زاوية ثابتة (لا عشوائيّة/زمن) — لقطة ساكنة تُظهر شكل المؤشّر
                    float ang = 6.2831853f * static_cast<float>(i) / static_cast<float>(dots);
                    float dx = cx + radius * std::cos(ang);
                    float dy = cy + radius * std::sin(ang);
                    float a = 0.15f + 0.85f * static_cast<float>(i) / static_cast<float>(dots - 1);
                    drawCircle(dx, dy, dotR, {accent.r, accent.g, accent.b, a});
                }
                break;
            }

            // ── شريط الحالة (StatusBar) — شريط سفليّ للنوافذ بنصّ حالة (RTL) (م٦-ب) ──
            case UINodeType::StatusBar:
            {
                Color barColor = parseColorProp(node.findProperty(props::BG_COLOR), // لون_خلفية
                                                {0.93f, 0.93f, 0.95f, 1});          // رماديّ فاتح
                if (!node.findProperty(props::BG_COLOR))
                    barColor = parseColorProp(node.findProperty(props::BG), barColor); // خلفية
                drawFilledRect(rect.x, rect.y, rect.width, rect.height, barColor);
                // خطّ حدّ علويّ رفيع يفصله عن جسم النافذة
                drawLine(rect.x, rect.y, rect.x + rect.width, rect.y,
                         Color::fromNamed(NamedColor::Gray), 1.0f);
                // نصّ الحالة (يمين RTL)
                const auto *stProp = node.findProperty(props::TEXT); // نص
                if (!stProp)
                    stProp = node.findProperty(props::TEXT_LATIN);
                if (!stProp)
                    stProp = node.findProperty(props::VALUE); // قيمة
                if (stProp)
                {
                    if (auto *t = std::get_if<std::string>(&stProp->value))
                    {
                        Color tc = parseColorProp(node.findProperty(props::COLOR), // لون
                                                  {0.20f, 0.20f, 0.20f, 1});
                        float fs = getNumericProp(node.findProperty(props::FONT_SIZE_ALT), 14.0f); // حجم_الخط
                        float tx = rect.x + 10;
                        if (isArabicText(*t))
                        {
                            auto sz = measureText(*t, fs);
                            tx = rect.x + rect.width - sz.first - 10;
                            if (tx < rect.x)
                                tx = rect.x;
                        }
                        drawText(*t, tx, rect.y + rect.height / 2 - fs / 2, tc, fs);
                    }
                }
                break;
            }

            // ═══════════════════════════════════════════════════════════════════════
            // (AR) عُقَدُ العرضِ المتقدّمة — كانت محبوسةً في شوكةِ مُرسِّمِ سطحِ
            //      المكتبِ وحدَها، فلا يراها الويبُ ولا المحمولُ ولا الوضعُ الحرّ.
            //      نُقِلت هنا كما هي دلالةً، وصُحّحت مواضعُ استُعمِل فيها الملءُ
            //      المستديرُ في موضعِ إطارٍ (كانت ترسم مستطيلًا رماديًّا فوقَ
            //      المحتوى بدلَ حدٍّ رفيع).
            // ═══════════════════════════════════════════════════════════════════════

            case UINodeType::AudioPlayer:
            {
                drawFilledRect(rect.x, rect.y, rect.width, rect.height, {0.95f, 0.95f, 0.95f, 1});
                drawRectOutline(rect.x, rect.y, rect.width, rect.height,
                                Color::fromNamed(NamedColor::Gray));
                drawText(uitext::PLAY_GLYPH, rect.x + 8, rect.y + 6,
                         Color::fromNamed(NamedColor::Blue), 18.0f);
                // شريط التقدّم
                drawFilledRect(rect.x + 40, rect.y + rect.height / 2 - 2, rect.width - 90, 4.0f,
                               Color::fromNamed(NamedColor::Gray));
                drawText(uitext::ZERO_DURATION, rect.x + rect.width - 45, rect.y + 6,
                         Color::fromNamed(NamedColor::Gray), 12.0f);
                break;
            }

            case UINodeType::BottomSheet:
            {
                Color sheetBg = parseColorProp(node.findProperty(props::BG), // خلفية
                                               Color::fromNamed(NamedColor::White));
                // (AR) كالحوارِ والدرج: حجابٌ يغطّي الشاشةَ ولوحٌ في مستطيلِ
                //   التخطيطِ كي لا تنفصلَ الكِسوةُ عن أبنائها.
                LayoutRect sheetRect = rect;
                if (viewportWidth_ > 0 && viewportHeight_ > 0)
                {
                    drawFilledRect(0, 0, static_cast<float>(viewportWidth_),
                                   static_cast<float>(viewportHeight_), {0, 0, 0, 0.35f}); // حجاب
                }

                drawSoftShadow(sheetRect, 20.0f, 16.0f, 0.0f, -4.0f, {0, 0, 0, 0.25f});
                drawRoundedRect(sheetRect.x, sheetRect.y, sheetRect.width, 24.0f, sheetBg, 20.0f);
                drawFilledRect(sheetRect.x, sheetRect.y + 12, sheetRect.width,
                               sheetRect.height - 12, sheetBg);

                // مقبضُ السحب
                const float sheetHandleW = 40.0f;
                drawRoundedRect(sheetRect.x + sheetRect.width / 2 - sheetHandleW / 2,
                                sheetRect.y + 10, sheetHandleW, 4.0f,
                                {0.7f, 0.7f, 0.7f, 1.0f}, 2.0f);
                break;
            }

            case UINodeType::TimePicker:
            case UINodeType::Calendar:
            {
                drawFilledRect(rect.x, rect.y, rect.width, rect.height,
                               Color::fromNamed(NamedColor::White));
                drawRectOutline(rect.x, rect.y, rect.width, rect.height,
                                Color::fromNamed(NamedColor::Gray));
                drawText(uitext::CALENDAR_GLYPH, rect.x + 4, rect.y + 4,
                         Color::fromNamed(NamedColor::Black), 14.0f);
                break;
            }

            case UINodeType::Canvas:
            {
                // منطقةُ رسمٍ مخصّصة — خلفيّةٌ وإطار؛ أوامرُ الرسمِ أبناءٌ تُرسَم بالتكرار
                drawFilledRect(rect.x, rect.y, rect.width, rect.height,
                               Color::fromNamed(NamedColor::White));
                drawRectOutline(rect.x, rect.y, rect.width, rect.height,
                                Color::fromNamed(NamedColor::Gray));
                break;
            }

            case UINodeType::CodeBlock:
            {
                bool codeDarkTheme = getBoolProp(node.findProperty(props::DARK), true);              // داكن
                float codeFontSize = getNumericProp(node.findProperty(props::FONT_SIZE_ALT), 13.0f); // حجم_الخط
                bool showLineNumbers = getBoolProp(node.findProperty(props::LINE_NUMBERS), true);    // أرقام_أسطر

                Color codeBg = codeDarkTheme ? Color{0.12f, 0.12f, 0.14f, 1.0f}
                                            : Color{0.96f, 0.96f, 0.96f, 1.0f};
                drawRoundedRect(rect.x, rect.y, rect.width, rect.height, codeBg, 6.0f);

                const auto *codeProp = node.findProperty(props::TEXT); // نص
                if (!codeProp)
                    codeProp = node.findProperty(props::TEXT_LATIN);
                if (!codeProp)
                    codeProp = node.findProperty(props::CODE); // كود
                if (codeProp)
                {
                    if (auto *codeText = std::get_if<std::string>(&codeProp->value))
                    {
                        const float lineHeight = codeFontSize + 4.0f;
                        const float codePadX = 8.0f;
                        const float codePadY = 8.0f;
                        float gutterWidth = 0.0f;

                        std::vector<std::string> codeLines;
                        {
                            std::string line;
                            for (char ch : *codeText)
                            {
                                if (ch == '\n')
                                {
                                    codeLines.push_back(line);
                                    line.clear();
                                }
                                else
                                {
                                    line += ch;
                                }
                            }
                            codeLines.push_back(line);
                        }

                        if (showLineNumbers)
                        {
                            int digits = 1;
                            for (int remaining = static_cast<int>(codeLines.size()); remaining >= 10;
                                 remaining /= 10)
                            {
                                ++digits;
                            }
                            gutterWidth = static_cast<float>(digits) * codeFontSize * 0.6f + 16.0f;

                            Color gutterBg = codeDarkTheme ? Color{0.10f, 0.10f, 0.12f, 1.0f}
                                                           : Color{0.92f, 0.92f, 0.92f, 1.0f};
                            drawRoundedRect(rect.x, rect.y, gutterWidth, rect.height, gutterBg, 6.0f);

                            Color lineNumColor = codeDarkTheme ? Color{0.45f, 0.45f, 0.50f, 1.0f}
                                                               : Color{0.55f, 0.55f, 0.55f, 1.0f};
                            for (size_t li = 0; li < codeLines.size(); ++li)
                            {
                                float ly = rect.y + codePadY + static_cast<float>(li) * lineHeight;
                                if (ly > rect.y + rect.height)
                                    break;
                                drawText(std::to_string(li + 1), rect.x + gutterWidth - codePadX - 4.0f,
                                         ly, lineNumColor, codeFontSize);
                            }
                        }

                        // تلوينٌ نحويٌّ — المُلوِّنُ في المكتبةِ فيعمل على كلِّ منصّة
                        SadSyntaxHighlighter highlighter;
                        const float codeStartX = rect.x + gutterWidth + codePadX;

                        for (size_t li = 0; li < codeLines.size(); ++li)
                        {
                            float ly = rect.y + codePadY + static_cast<float>(li) * lineHeight;
                            if (ly > rect.y + rect.height)
                                break;

                            auto lineTokens = highlighter.tokenize(codeLines[li]);
                            if (lineTokens.empty())
                                continue;

                            float cx = codeStartX;
                            size_t lastEnd = 0;
                            for (const auto &tok : lineTokens)
                            {
                                if (tok.start > lastEnd)
                                {
                                    auto gapSize = measureText(
                                        codeLines[li].substr(lastEnd, tok.start - lastEnd), codeFontSize);
                                    cx += gapSize.first;
                                }

                                std::string tokenText = codeLines[li].substr(tok.start, tok.length);
                                auto tokenColor = SadSyntaxHighlighter::getColor(tok.type, codeDarkTheme);
                                drawText(tokenText, cx, ly,
                                         {tokenColor.r, tokenColor.g, tokenColor.b, tokenColor.a},
                                         codeFontSize);
                                cx += measureText(tokenText, codeFontSize).first;
                                lastEnd = tok.start + tok.length;
                            }
                        }
                    }
                }
                break;
            }

            case UINodeType::Expandable:
            case UINodeType::Collapsible:
            {
                std::string expandableTitle;
                if (!findTextProp(node,
                                  {props::TITLE, props::TEXT_LATIN, props::TEXT, props::CONTENT},
                                  expandableTitle) ||
                    expandableTitle.empty())
                {
                    expandableTitle = std::string(uitext::PLAY_GLYPH) + " " + uitext::ITEM_AR;
                }
                drawText(expandableTitle, rect.x + 4, rect.y + 4,
                         Color::fromNamed(NamedColor::Black), 14.0f);
                break;
            }

            case UINodeType::ColorPicker:
            case UINodeType::ColorWheel:
            {
                // طيفٌ لونيٌّ بالتدرّج (أحمر ⇐ أخضر ⇐ أزرق ⇐ أرجوانيّ)
                const float bandWidth = rect.width / 3.0f;
                drawLinearGradient(rect.x, rect.y, bandWidth + 1, rect.height,
                                   Color::fromNamed(NamedColor::Red),
                                   Color::fromNamed(NamedColor::Green), false);
                drawLinearGradient(rect.x + bandWidth, rect.y, bandWidth + 1, rect.height,
                                   Color::fromNamed(NamedColor::Green),
                                   Color::fromNamed(NamedColor::Blue), false);
                drawLinearGradient(rect.x + 2 * bandWidth, rect.y, bandWidth, rect.height,
                                   Color::fromNamed(NamedColor::Blue),
                                   {1.0f, 0.0f, 1.0f, 1.0f}, false);
                drawRectOutline(rect.x, rect.y, rect.width, rect.height,
                                Color::fromNamed(NamedColor::Gray));
                break;
            }

            case UINodeType::ImageGallery:
            {
                drawFilledRect(rect.x, rect.y, rect.width, rect.height, {0.97f, 0.97f, 0.97f, 1});
                break;
            }

            case UINodeType::RichText:
            case UINodeType::Markdown:
            {
                const auto *richProp = node.findProperty(props::TEXT_LATIN);
                if (!richProp)
                    richProp = node.findProperty(props::TEXT); // نص
                if (!richProp)
                    richProp = node.findProperty(props::CONTENT); // محتوى
                if (richProp)
                {
                    if (auto *text = std::get_if<std::string>(&richProp->value))
                    {
                        Color richColor = parseColorProp(node.findProperty(props::COLOR), // لون
                                                         Color::fromNamed(NamedColor::Black));
                        float richFontSize = getNumericProp(node.findProperty(props::FONT_SIZE_ALT), 14.0f);
                        float richX = rect.x;
                        if (isArabicText(*text))
                        {
                            auto sz = measureText(*text, richFontSize);
                            richX = rect.x + rect.width - sz.first;
                            if (richX < rect.x)
                                richX = rect.x;
                        }
                        drawText(*text, richX, rect.y, richColor, richFontSize);
                    }
                }
                break;
            }

            case UINodeType::MegaMenu:
            {
                drawFilledRect(rect.x, rect.y, rect.width, rect.height,
                               Color::fromNamed(NamedColor::White));
                drawRectOutline(rect.x, rect.y, rect.width, rect.height,
                                Color::fromNamed(NamedColor::Gray));
                break;
            }

            case UINodeType::Sheet:
            {
                drawSoftShadow(rect, 12.0f, 16.0f, 0.0f, -4.0f, {0, 0, 0, 0.2f});
                Color popupBg = parseColorProp(node.findProperty(props::BG), // خلفية
                                               Color::fromNamed(NamedColor::White));
                drawRoundedRect(rect.x, rect.y, rect.width, rect.height, popupBg, 12.0f);
                drawRectOutline(rect.x, rect.y, rect.width, rect.height,
                                Color::fromNamed(NamedColor::Gray));
                // مقبضُ الورقة
                const float popupHandleW = 40.0f;
                drawRoundedRect(rect.x + rect.width / 2 - popupHandleW / 2, rect.y + 8,
                                popupHandleW, 4.0f, Color::fromNamed(NamedColor::Gray), 2.0f);
                break;
            }

            case UINodeType::SideNav:
            {
                drawFilledRect(rect.x, rect.y, rect.width, rect.height, {0.95f, 0.95f, 0.97f, 1});
                // خطٌّ جانبيٌّ على الحافّة
                drawFilledRect(rect.x + rect.width - 1, rect.y, 1.0f, rect.height,
                               Color::fromNamed(NamedColor::Gray));
                break;
            }

            case UINodeType::TabView:
            {
                Color tabBarBg = parseColorProp(node.findProperty(props::BG), // خلفية
                                                Color::fromNamed(NamedColor::Primary));
                const float tabBarHeight = 48.0f;
                drawFilledRect(rect.x, rect.y, rect.width, tabBarHeight, tabBarBg);

                const size_t tabCount = node.childCount();
                const int selectedTab =
                    static_cast<int>(getNumericProp(node.findProperty(props::SELECTED), 0.0f)); // محدد
                if (tabCount > 0)
                {
                    const float tabWidth = rect.width / static_cast<float>(tabCount);
                    for (size_t t = 0; t < tabCount; ++t)
                    {
                        const float tabX = rect.x + static_cast<float>(t) * tabWidth;
                        const auto &tabChild = node.getChildren()[t];
                        std::string label;
                        if (!findTextProp(*tabChild, {props::TITLE, props::TEXT, props::TITLE_LATIN},
                                          label) ||
                            label.empty())
                        {
                            label = std::string(uitext::TAB_LABEL_PREFIX_AR) + std::to_string(t + 1);
                        }

                        const bool isSelectedTab = (static_cast<int>(t) == selectedTab);
                        Color tabTextColor = isSelectedTab ? Color{1, 1, 1, 1} : Color{1, 1, 1, 0.6f};
                        auto labelSize = measureText(label, 14.0f);
                        drawText(label, tabX + (tabWidth - labelSize.first) / 2.0f, rect.y + 14,
                                 tabTextColor, 14.0f);

                        if (isSelectedTab)
                        {
                            drawFilledRect(tabX + 8, rect.y + tabBarHeight - 3, tabWidth - 16, 3.0f,
                                           {1, 1, 1, 1});
                        }
                    }
                }
                break;
            }

            case UINodeType::WebView:
            {
                drawFilledRect(rect.x, rect.y, rect.width, rect.height,
                               Color::fromNamed(NamedColor::White));
                drawRectOutline(rect.x, rect.y, rect.width, rect.height,
                                Color::fromNamed(NamedColor::Gray));
                drawText(uitext::GLOBE_GLYPH, rect.x + 4, rect.y + 4,
                         Color::fromNamed(NamedColor::Blue), 16.0f);
                break;
            }

            default:
                break;
            }

            // 3. رسم حدود التصحيح
            if (options_.debugBounds)
            {
                drawRectOutline(rect.x, rect.y, rect.width, rect.height, {1, 0, 0, 0.3f});
            }

            // 4. رسم الأبناء (تكرار) مع دعم scroll offset
            bool isScrollableContainer = isScrollableType(node.getType());
            float scrollOffY = isScrollableContainer
                                   ? getNumericProp(node.findProperty(props::OFFSET_Y), // إزاحة_ص
                                                    getNumericProp(node.findProperty(props::SCROLL_Y_LATIN), 0.0f))
                                   : 0.0f;
            float scrollOffX = isScrollableContainer
                                   ? getNumericProp(node.findProperty(props::OFFSET_X), // إزاحة_س
                                                    getNumericProp(node.findProperty(props::SCROLL_X_LATIN), 0.0f))
                                   : 0.0f;

            // (AR) إزاحةُ المنصّة (عجلةُ الفأرة في سطحِ المكتب) تُضاف إلى إزاحةِ
            //      الخاصّيّة. الإشارة: الخاصّيّةُ موجبةٌ صعودًا (تُطرَح)، وإزاحةُ
            //      المنصّةِ إزاحةُ رسمٍ موجبةٌ نزولًا (تُجمَع).
            const float platformScroll =
                isScrollableContainer ? platformScrollOffset(node) : 0.0f;

            const float prevTranslateX = scrollTranslateX_;
            const float prevTranslateY = scrollTranslateY_;
            if (isScrollableContainer)
            {
                scrollTranslateX_ -= scrollOffX;
                scrollTranslateY_ -= scrollOffY;
                scrollTranslateY_ += platformScroll;
            }

            for (size_t i = 0; i < layout.children.size() && i < node.childCount(); ++i)
            {
                renderNode(*node.getChildren()[i], *layout.children[i]);
            }

            if (isScrollableContainer)
            {
                scrollTranslateX_ = prevTranslateX;
                scrollTranslateY_ = prevTranslateY;
            }

            // 4.5 شريطُ التمرير — يُرسَم بعدَ الأبناءِ فوقَهم، ولا يُزاح معهم.
            if (isScrollableContainer)
            {
                const float contentHeight = layout.contentHeight;
                const float viewHeight = rect.height;
                if (contentHeight > viewHeight && viewHeight > 0)
                {
                    const float scrollRatio = viewHeight / contentHeight;
                    const float barHeight = std::max(20.0f, viewHeight * scrollRatio);
                    const float maxScroll = contentHeight - viewHeight;
                    const float travelled = (scrollOffY - platformScroll) / maxScroll;
                    float barY = rect.y + travelled * (viewHeight - barHeight);
                    barY = std::max(rect.y, std::min(barY, rect.y + viewHeight - barHeight));
                    drawRoundedRect(rect.x + rect.width - 6.0f, barY, 4.0f, barHeight,
                                    {0.6f, 0.6f, 0.6f, 0.4f}, 2.0f);
                }
            }

            // 5. إلغاء تحويلات التكبير والدوران
            if (scaleApplied)
            {
                popScale();
            }
            if (rotationApplied)
            {
                popRotation();
            }

            // 6. إعادة تعيين القص
            if (isScrollableType(node.getType()))
            {
                clearClipRect();
            }
        }

        // ═══════════════════════════════════════════════════════════════════════════════
        // دوال التحريكات والشفافية (مشتركة — تستخدمها جميع المنصات)
        // ═══════════════════════════════════════════════════════════════════════════════

        EasingType PlatformRenderer::curveToEasing(EasingCurve curve) const
        {
            switch (curve)
            {
            case EasingCurve::Linear:
                return EasingType::Linear;
            case EasingCurve::EaseIn:
                return EasingType::EaseIn;
            case EasingCurve::EaseOut:
                return EasingType::EaseOut;
            case EasingCurve::EaseInOut:
                return EasingType::EaseInOut;
            case EasingCurve::Spring:
                return EasingType::Spring;
            case EasingCurve::Bounce:
                return EasingType::EaseOutBounce;
            case EasingCurve::Elastic:
                return EasingType::EaseOutBack;
            default:
                return EasingType::EaseOut;
            }
        }

        void PlatformRenderer::ensureNodeId(IRNode &node)
        {
            if (node.getId().empty())
            {
                node.setId("widget_" + std::to_string(1000000 + autoIdCounter_++));
            }
            for (size_t i = 0; i < node.childCount(); ++i)
            {
                ensureNodeId(*node.getChildren()[i]);
            }
        }

        void PlatformRenderer::resetAnimationState()
        {
            initializedAnimNodes_.clear();
            autoIdCounter_ = 0;
        }

        void PlatformRenderer::initializeAnimations(
            const std::shared_ptr<IRNode> &root, uint32_t currentTimeMs)
        {
            if (!root || !animationEngine_)
                return;
            // التأكد من وجود معرّفات لجميع العقد
            ensureNodeId(*root);
            // تهيئة تحريكات العقدة الجذر والأبناء
            initNodeAnimations(*root, currentTimeMs);
        }

        void PlatformRenderer::initNodeAnimations(const IRNode &node, uint32_t startMs)
        {
            if (!animationEngine_)
                return;

            const std::string &nid = node.getId();
            if (!nid.empty() && initializedAnimNodes_.find(nid) == initializedAnimNodes_.end())
            {
                const auto &anims = node.getAnimations();
                for (const auto &irAnim : anims)
                {
                    uint32_t durationMs = static_cast<uint32_t>(irAnim.duration * 1000.0f);
                    if (durationMs == 0)
                        durationMs = 300;
                    uint32_t delayMs = static_cast<uint32_t>(irAnim.delay * 1000.0f);
                    EasingType easing = curveToEasing(irAnim.easing);
                    int repeat = (irAnim.repeatCount == 0) ? -1 : irAnim.repeatCount - 1;

                    // بناء هيكل Animation
                    Animation anim;
                    anim.targetNodeId = nid;
                    anim.durationMs = durationMs;
                    anim.delayMs = delayMs;
                    anim.easing = easing;
                    anim.repeatCount = repeat;
                    anim.autoReverse = irAnim.autoReverse;

                    // callback انتهاء التحريك
                    if (onAnimationEndCallback_)
                    {
                        std::string nodeIdCopy = nid;
                        auto cb = onAnimationEndCallback_;
                        anim.onComplete = [cb, nodeIdCopy]()
                        {
                            cb(nodeIdCopy);
                        };
                    }

                    switch (irAnim.type)
                    {
                    case AnimationType::FadeIn:
                        anim.propertyKey = "__opacity";
                        anim.fromValue = 0.0;
                        anim.toValue = 1.0;
                        animationEngine_->animate(anim, startMs);
                        break;
                    case AnimationType::FadeOut:
                        anim.propertyKey = "__opacity";
                        anim.fromValue = 1.0;
                        anim.toValue = 0.0;
                        animationEngine_->animate(anim, startMs);
                        break;
                    case AnimationType::ScaleUp:
                        anim.propertyKey = "__scale";
                        anim.fromValue = 0.0;
                        anim.toValue = 1.0;
                        animationEngine_->animate(anim, startMs);
                        break;
                    case AnimationType::ScaleDown:
                        anim.propertyKey = "__scale";
                        anim.fromValue = 1.0;
                        anim.toValue = 0.0;
                        animationEngine_->animate(anim, startMs);
                        break;
                    case AnimationType::SlideRight:
                        anim.propertyKey = "__translateX";
                        anim.fromValue = -100.0;
                        anim.toValue = 0.0;
                        animationEngine_->animate(anim, startMs);
                        break;
                    case AnimationType::SlideLeft:
                        anim.propertyKey = "__translateX";
                        anim.fromValue = 100.0;
                        anim.toValue = 0.0;
                        animationEngine_->animate(anim, startMs);
                        break;
                    case AnimationType::SlideUp:
                        anim.propertyKey = "__translateY";
                        anim.fromValue = 100.0;
                        anim.toValue = 0.0;
                        animationEngine_->animate(anim, startMs);
                        break;
                    case AnimationType::SlideDown:
                        anim.propertyKey = "__translateY";
                        anim.fromValue = -100.0;
                        anim.toValue = 0.0;
                        animationEngine_->animate(anim, startMs);
                        break;
                    case AnimationType::Rotate:
                        anim.propertyKey = "__rotation";
                        anim.fromValue = 0.0;
                        anim.toValue = 360.0;
                        animationEngine_->animate(anim, startMs);
                        break;
                    case AnimationType::Pulse:
                        anim.propertyKey = "__scale";
                        anim.fromValue = 1.0;
                        anim.toValue = 1.15;
                        anim.autoReverse = true;
                        animationEngine_->animate(anim, startMs);
                        break;
                    case AnimationType::Shake:
                        anim.propertyKey = "__translateX";
                        anim.fromValue = -8.0;
                        anim.toValue = 8.0;
                        anim.durationMs = durationMs / 2;
                        anim.easing = EasingType::Linear;
                        anim.autoReverse = true;
                        if (repeat > 0)
                            anim.repeatCount = repeat * 2;
                        animationEngine_->animate(anim, startMs);
                        break;
                    case AnimationType::Bounce:
                        anim.propertyKey = "__translateY";
                        anim.fromValue = -30.0;
                        anim.toValue = 0.0;
                        anim.easing = EasingType::EaseOutBounce;
                        animationEngine_->animate(anim, startMs);
                        break;
                    case AnimationType::CrossFade:
                        anim.propertyKey = "__opacity";
                        anim.fromValue = 0.3;
                        anim.toValue = 1.0;
                        animationEngine_->animate(anim, startMs);
                        break;
                    case AnimationType::ZoomIn:
                    {
                        // تقريب: تكبير من 0.5 مع ظهور تدريجي
                        anim.propertyKey = "__scale";
                        anim.fromValue = 0.5;
                        anim.toValue = 1.0;
                        animationEngine_->animate(anim, startMs);
                        // ظهور مصاحب
                        Animation fadeAnim = anim;
                        fadeAnim.propertyKey = "__opacity";
                        fadeAnim.fromValue = 0.0;
                        fadeAnim.toValue = 1.0;
                        animationEngine_->animate(fadeAnim, startMs);
                        break;
                    }
                    case AnimationType::ZoomOut:
                    {
                        // تبعيد: تصغير من 1.5 مع ظهور تدريجي
                        anim.propertyKey = "__scale";
                        anim.fromValue = 1.5;
                        anim.toValue = 1.0;
                        animationEngine_->animate(anim, startMs);
                        // ظهور مصاحب
                        Animation fadeAnim = anim;
                        fadeAnim.propertyKey = "__opacity";
                        fadeAnim.fromValue = 0.0;
                        fadeAnim.toValue = 1.0;
                        animationEngine_->animate(fadeAnim, startMs);
                        break;
                    }
                    case AnimationType::DropIn:
                    {
                        // سقوط: نزول من أعلى الشاشة مع ارتداد
                        anim.propertyKey = "__translateY";
                        anim.fromValue = -200.0;
                        anim.toValue = 0.0;
                        anim.easing = EasingType::EaseOutBounce;
                        animationEngine_->animate(anim, startMs);
                        // ظهور مصاحب
                        Animation fadeAnim = anim;
                        fadeAnim.propertyKey = "__opacity";
                        fadeAnim.fromValue = 0.0;
                        fadeAnim.toValue = 1.0;
                        fadeAnim.easing = EasingType::EaseOut;
                        fadeAnim.durationMs = durationMs / 2;
                        animationEngine_->animate(fadeAnim, startMs);
                        break;
                    }
                    case AnimationType::FlyOut:
                    {
                        // طيران: انزلاق للخارج (يساراً) مع اختفاء
                        anim.propertyKey = "__translateX";
                        anim.fromValue = 0.0;
                        anim.toValue = -200.0;
                        animationEngine_->animate(anim, startMs);
                        // اختفاء مصاحب
                        Animation fadeAnim = anim;
                        fadeAnim.propertyKey = "__opacity";
                        fadeAnim.fromValue = 1.0;
                        fadeAnim.toValue = 0.0;
                        animationEngine_->animate(fadeAnim, startMs);
                        break;
                    }
                    case AnimationType::Swing:
                        // تأرجح: حركة بندولية أفقية مستمرة
                        anim.propertyKey = "__translateX";
                        anim.fromValue = -15.0;
                        anim.toValue = 15.0;
                        anim.autoReverse = true;
                        anim.easing = EasingType::EaseInOut;
                        if (repeat == 0)
                            anim.repeatCount = -1; // لانهائي افتراضياً
                        animationEngine_->animate(anim, startMs);
                        break;

                        // ── الأنواع الجديدة (v4) ──────────────────────────────

                    case AnimationType::FlipX:
                    {
                        // قلب أفقي: محاكاة قلب 3D عبر تغيير scale Y من 1 → 0 → 1
                        // المرحلة الأولى: تصغير Y (الانضغاط)
                        anim.propertyKey = "__scale";
                        anim.fromValue = 1.0;
                        anim.toValue = 0.05;
                        anim.durationMs = durationMs / 2;
                        anim.autoReverse = true;
                        animationEngine_->animate(anim, startMs);
                        // ظهور مصاحب خفيف
                        Animation fadeAnim = anim;
                        fadeAnim.propertyKey = "__opacity";
                        fadeAnim.fromValue = 1.0;
                        fadeAnim.toValue = 0.7;
                        fadeAnim.autoReverse = true;
                        animationEngine_->animate(fadeAnim, startMs);
                        break;
                    }
                    case AnimationType::FlipY:
                    {
                        // قلب عمودي: محاكاة عبر دوران 180° سريع مع تغيير scale
                        anim.propertyKey = "__rotation";
                        anim.fromValue = 0.0;
                        anim.toValue = 180.0;
                        anim.autoReverse = true;
                        anim.durationMs = durationMs / 2;
                        animationEngine_->animate(anim, startMs);
                        // تصغير خفيف أثناء القلب
                        Animation scaleAnim = anim;
                        scaleAnim.propertyKey = "__scale";
                        scaleAnim.fromValue = 1.0;
                        scaleAnim.toValue = 0.8;
                        scaleAnim.autoReverse = true;
                        animationEngine_->animate(scaleAnim, startMs);
                        break;
                    }
                    case AnimationType::Wobble:
                    {
                        // ترنّح: اهتزاز دوراني سريع ذهاباً وإياباً مثل جرس
                        anim.propertyKey = "__rotation";
                        anim.fromValue = -15.0;
                        anim.toValue = 15.0;
                        anim.durationMs = durationMs / 3;
                        anim.autoReverse = true;
                        anim.easing = EasingType::EaseInOut;
                        if (repeat >= 0)
                            anim.repeatCount = (repeat + 1) * 3; // عدة اهتزازات
                        else
                            anim.repeatCount = -1;
                        animationEngine_->animate(anim, startMs);
                        break;
                    }
                    case AnimationType::BounceIn:
                    {
                        // ارتداد دخول: انزلاق من اليسار مع ارتداد مرن
                        anim.propertyKey = "__translateX";
                        anim.fromValue = -150.0;
                        anim.toValue = 0.0;
                        anim.easing = EasingType::EaseOutBounce;
                        animationEngine_->animate(anim, startMs);
                        // ظهور مصاحب
                        Animation fadeAnim = anim;
                        fadeAnim.propertyKey = "__opacity";
                        fadeAnim.fromValue = 0.0;
                        fadeAnim.toValue = 1.0;
                        fadeAnim.easing = EasingType::EaseOut;
                        fadeAnim.durationMs = durationMs / 2;
                        animationEngine_->animate(fadeAnim, startMs);
                        break;
                    }
                    case AnimationType::Blink:
                    {
                        // وميض: ظهور/اختفاء متكرر سريع
                        anim.propertyKey = "__opacity";
                        anim.fromValue = 1.0;
                        anim.toValue = 0.0;
                        anim.durationMs = durationMs / 2;
                        anim.autoReverse = true;
                        anim.easing = EasingType::Linear;
                        if (repeat >= 0)
                            anim.repeatCount = (repeat + 1) * 2;
                        else
                            anim.repeatCount = -1;
                        animationEngine_->animate(anim, startMs);
                        break;
                    }
                    case AnimationType::Stretch:
                    {
                        // تمطّي: تمدد مطاطي من 0.3 إلى 1.0 مع ارتداد نابضي
                        anim.propertyKey = "__scale";
                        anim.fromValue = 0.3;
                        anim.toValue = 1.0;
                        anim.easing = EasingType::Spring;
                        animationEngine_->animate(anim, startMs);
                        // ظهور مصاحب
                        Animation fadeAnim = anim;
                        fadeAnim.propertyKey = "__opacity";
                        fadeAnim.fromValue = 0.0;
                        fadeAnim.toValue = 1.0;
                        fadeAnim.easing = EasingType::EaseOut;
                        fadeAnim.durationMs = durationMs / 2;
                        animationEngine_->animate(fadeAnim, startMs);
                        break;
                    }
                    case AnimationType::Explode:
                    {
                        // انفجار: تكبير سريع من 1x إلى 3x مع اختفاء
                        anim.propertyKey = "__scale";
                        anim.fromValue = 1.0;
                        anim.toValue = 3.0;
                        animationEngine_->animate(anim, startMs);
                        // اختفاء مصاحب
                        Animation fadeAnim = anim;
                        fadeAnim.propertyKey = "__opacity";
                        fadeAnim.fromValue = 1.0;
                        fadeAnim.toValue = 0.0;
                        animationEngine_->animate(fadeAnim, startMs);
                        break;
                    }
                    case AnimationType::Spin3D:
                    {
                        // دورة 3D: دوران كامل 360° مع تغيّر حجم (محاكاة عمق)
                        anim.propertyKey = "__rotation";
                        anim.fromValue = 0.0;
                        anim.toValue = 360.0;
                        animationEngine_->animate(anim, startMs);
                        // نبض حجم مصاحب (يحاكي العمق 3D)
                        Animation scaleAnim = anim;
                        scaleAnim.propertyKey = "__scale";
                        scaleAnim.fromValue = 1.0;
                        scaleAnim.toValue = 0.6;
                        scaleAnim.autoReverse = true;
                        scaleAnim.durationMs = durationMs / 2;
                        animationEngine_->animate(scaleAnim, startMs);
                        break;
                    }

                    case AnimationType::Custom:
                        anim.propertyKey = irAnim.customProperty.empty() ? "__custom" : irAnim.customProperty;
                        anim.fromValue = irAnim.customFromValue;
                        anim.toValue = irAnim.customToValue;
                        animationEngine_->animate(anim, startMs);
                        break;
                    default:
                        break;
                    }
                }
                initializedAnimNodes_.insert(nid);
            }

            // تهيئة الأبناء
            for (size_t i = 0; i < node.childCount(); ++i)
            {
                initNodeAnimations(*node.getChildren()[i], startMs);
            }
        }

        // ═══════════════════════════════════════════════════════════════════════════════
        // PlatformWindow::setContentWithTransition — انتقال بصري بين الصفحات
        // ═══════════════════════════════════════════════════════════════════════════════

        void PlatformWindow::setContentWithTransition(
            std::shared_ptr<IRNode> root,
            const std::string &transitionType,
            float durationSec)
        {
            if (!root)
            {
                setContent(root);
                return;
            }

            // تحويل اسم الانتقال إلى نوع تحريك
            AnimationType animType = stringToAnimationType(transitionType);

            // إذا كان نوعاً غير معروف، نستخدم ظهور كافتراضي
            if (animType == AnimationType::Custom)
                animType = AnimationType::FadeIn;

            // إنشاء تحريك انتقال على العقدة الجذر
            IRAnimation transAnim;
            transAnim.type = animType;
            transAnim.duration = durationSec;
            transAnim.delay = 0.0f;
            transAnim.repeatCount = 1;
            transAnim.autoReverse = false;
            transAnim.playOnAppear = true;

            // منحنى التسهيل الافتراضي: تباطؤ — يبدو أنسب للانتقالات
            transAnim.easing = EasingCurve::EaseOut;

            // إضافة التحريك على العنصر الجذر
            root->addAnimation(transAnim);

            // استدعاء setContent العادية (تهيئة الانيميشن تتم هناك)
            setContent(std::move(root));
        }

        // ═══════════════════════════════════════════════════════════════════════════════
        // PlatformWindow::setContentWithExitTransition — انتقال خروج ثم دخول
        // ═══════════════════════════════════════════════════════════════════════════════

        void PlatformWindow::setContentWithExitTransition(
            std::shared_ptr<IRNode> root,
            const std::string &entryTransition,
            const std::string &exitTransition,
            float durationSec)
        {
            if (!root)
            {
                setContent(root);
                return;
            }

            uint32_t durationMs = static_cast<uint32_t>(durationSec * 1000.0f);
            uint32_t now = getCurrentTimeMs();

            // ─── (1) تحريك خروج على المحتوى الحالي ───
            // نحقن animation مباشرة في AnimationEngine على عقدة "__page_exit__" وهمية
            // ثم في renderNode ستُطبّق الشفافية/الإزاحة على الجذر الحالي
            AnimationType exitType = stringToAnimationType(exitTransition);
            if (exitType == AnimationType::Custom)
                exitType = AnimationType::FadeOut; // خروج افتراضي

            // حقن تحريك خروج: عكس تحريك الدخول
            // FadeOut: شفافية 1→0
            // SlideLeft: إزاحة 0→-300
            // SlideRight: إزاحة 0→300
            {
                Animation exitAnim;
                exitAnim.targetNodeId = "__page_root__";
                exitAnim.durationMs = durationMs;
                exitAnim.delayMs = 0;
                exitAnim.repeatCount = 0;
                exitAnim.autoReverse = false;
                exitAnim.easing = EasingType::EaseIn;

                switch (exitType)
                {
                case AnimationType::FadeOut:
                case AnimationType::FadeIn:
                    exitAnim.propertyKey = "__opacity";
                    exitAnim.fromValue = 1.0;
                    exitAnim.toValue = 0.0;
                    animationEngine_.animate(exitAnim, now);
                    break;

                case AnimationType::SlideLeft:
                    exitAnim.propertyKey = "__translateX";
                    exitAnim.fromValue = 0.0;
                    exitAnim.toValue = -300.0;
                    animationEngine_.animate(exitAnim, now);
                    // اختفاء مصاحب
                    {
                        Animation fadeExit = exitAnim;
                        fadeExit.propertyKey = "__opacity";
                        fadeExit.fromValue = 1.0;
                        fadeExit.toValue = 0.0;
                        animationEngine_.animate(fadeExit, now);
                    }
                    break;

                case AnimationType::SlideRight:
                    exitAnim.propertyKey = "__translateX";
                    exitAnim.fromValue = 0.0;
                    exitAnim.toValue = 300.0;
                    animationEngine_.animate(exitAnim, now);
                    {
                        Animation fadeExit = exitAnim;
                        fadeExit.propertyKey = "__opacity";
                        fadeExit.fromValue = 1.0;
                        fadeExit.toValue = 0.0;
                        animationEngine_.animate(fadeExit, now);
                    }
                    break;

                case AnimationType::SlideUp:
                    exitAnim.propertyKey = "__translateY";
                    exitAnim.fromValue = 0.0;
                    exitAnim.toValue = -300.0;
                    animationEngine_.animate(exitAnim, now);
                    {
                        Animation fadeExit = exitAnim;
                        fadeExit.propertyKey = "__opacity";
                        fadeExit.fromValue = 1.0;
                        fadeExit.toValue = 0.0;
                        animationEngine_.animate(fadeExit, now);
                    }
                    break;

                case AnimationType::SlideDown:
                    exitAnim.propertyKey = "__translateY";
                    exitAnim.fromValue = 0.0;
                    exitAnim.toValue = 300.0;
                    animationEngine_.animate(exitAnim, now);
                    {
                        Animation fadeExit = exitAnim;
                        fadeExit.propertyKey = "__opacity";
                        fadeExit.fromValue = 1.0;
                        fadeExit.toValue = 0.0;
                        animationEngine_.animate(fadeExit, now);
                    }
                    break;

                case AnimationType::ScaleDown:
                case AnimationType::ScaleUp:
                    exitAnim.propertyKey = "__scale";
                    exitAnim.fromValue = 1.0;
                    exitAnim.toValue = 0.1;
                    animationEngine_.animate(exitAnim, now);
                    {
                        Animation fadeExit = exitAnim;
                        fadeExit.propertyKey = "__opacity";
                        fadeExit.fromValue = 1.0;
                        fadeExit.toValue = 0.0;
                        animationEngine_.animate(fadeExit, now);
                    }
                    break;

                default:
                    // أي نوع آخر: اختفاء تدريجي افتراضي
                    exitAnim.propertyKey = "__opacity";
                    exitAnim.fromValue = 1.0;
                    exitAnim.toValue = 0.0;
                    animationEngine_.animate(exitAnim, now);
                    break;
                }
            }

            // ─── (2) تخزين المحتوى الجديد للتبديل بعد انتهاء الخروج ───
            pendingContent_ = std::move(root);
            pendingEntryTransition_ = entryTransition;
            pendingEntryDuration_ = durationSec;
            exitTransitionStartMs_ = now;
            exitTransitionDurationMs_ = durationMs;
        }

        bool PlatformWindow::checkPendingTransition()
        {
            if (!pendingContent_)
                return false;

            uint32_t now = getCurrentTimeMs();
            uint32_t elapsed = now - exitTransitionStartMs_;

            // الخروج انتهى — نُبدّل المحتوى مع تحريك الدخول
            if (elapsed >= exitTransitionDurationMs_)
            {
                // إلغاء أنيميشن الخروج المتبقية
                animationEngine_.cancelAnimations("__page_root__");

                auto newContent = std::move(pendingContent_);
                pendingContent_.reset();

                // تطبيق تحريك الدخول على المحتوى الجديد
                setContentWithTransition(
                    std::move(newContent),
                    pendingEntryTransition_,
                    pendingEntryDuration_);

                pendingEntryTransition_.clear();
                exitTransitionStartMs_ = 0;
                exitTransitionDurationMs_ = 0;
                return true;
            }

            return false;
        }

        // ═══════════════════════════════════════════════════════════════════════════════
        // PlatformFactory — مصنع المنصات
        // ═══════════════════════════════════════════════════════════════════════════════

        TargetPlatform PlatformFactory::detectPlatform()
        {
#ifdef __EMSCRIPTEN__
            return TargetPlatform::Web;
#elif defined(__ANDROID__)
            return TargetPlatform::Android;
#elif defined(__APPLE__)
#if TARGET_OS_IOS
            return TargetPlatform::iOS;
#else
            return TargetPlatform::MacOS;
#endif
#else
            return TargetPlatform::Desktop;
#endif
        }

        std::unique_ptr<PlatformWindow> PlatformFactory::createWindow(TargetPlatform platform)
        {
            switch (platform)
            {
#ifdef __EMSCRIPTEN__
            case TargetPlatform::Web:
            case TargetPlatform::WASM:
                return std::make_unique<web::WebWindow>();
#endif
#ifdef __ANDROID__
            case TargetPlatform::Android:
                return std::make_unique<android::AndroidWindow>();
#endif
#if defined(__APPLE__) && TARGET_OS_IOS
            case TargetPlatform::iOS:
                return std::make_unique<ios::IOSWindow>();
#endif
#if defined(__APPLE__) && !TARGET_OS_IOS && defined(SAD_UI_ENABLE_MACOS_NATIVE)
            case TargetPlatform::MacOS:
                return std::make_unique<macos::MacOSWindow>();
#endif
            default:
                return nullptr;
            }
        }

        std::unique_ptr<PlatformRenderer> PlatformFactory::createRenderer(TargetPlatform platform)
        {
            switch (platform)
            {
#ifdef __EMSCRIPTEN__
            case TargetPlatform::Web:
            case TargetPlatform::WASM:
                return std::make_unique<web::WebRenderer>();
#endif
#ifdef __ANDROID__
            case TargetPlatform::Android:
                return std::make_unique<android::AndroidRenderer>();
#endif
#if defined(__APPLE__) && TARGET_OS_IOS
            case TargetPlatform::iOS:
                return std::make_unique<ios::IOSRenderer>();
#endif
#if defined(__APPLE__) && !TARGET_OS_IOS && defined(SAD_UI_ENABLE_MACOS_NATIVE)
            case TargetPlatform::MacOS:
                return std::make_unique<macos::MacOSRenderer>();
#endif
            default:
                return nullptr;
            }
        }

    } // namespace ui
} // namespace sad
