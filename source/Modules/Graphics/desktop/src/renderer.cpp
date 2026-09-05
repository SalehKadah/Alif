/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: renderer.cpp
 * المسار: sad_ui/backends/desktop/src/renderer.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * تنفيذ مُصيّر سطح المكتب (Desktop Renderer).
 *
 * يرسم عناصر الواجهة عبر SDL2:
 * - SDL_RenderFillRect للمستطيلات
 * - SDL_ttf للنصوص
 * - SDL_RenderCopy للصور
 *
 * ═══════════════════════════════════════════════════════════════════════════════
 * تم التفعيل الكامل — SDL2 مربوط ويعمل
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/desktop/renderer.h"
#include "sad_ui/prop_keys.h" // مفاتيح الخصائص القانونيّة (SoT) — لا literals خام
#include "sad_ui/types.h" // arabicNameToColor

#ifdef SAD_UI_USE_SDL2
#include <SDL.h>
// SDL_ttf.h يأتي من renderer.h (يُحدِّد فيه TTF_Font الحقيقي)
#endif

#include <cmath>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <algorithm>

// ─── الدوال المساعدة المشتركة ───
#include "renderer_internal.h"
#include "text_editing.h"

namespace sad
{
    namespace ui
    {
        namespace desktop
        {

            // ═══════════════════════════════════════════════════════════════════════════════
            // تنفيذ DesktopRenderer
            // ═══════════════════════════════════════════════════════════════════════════════

            DesktopRenderer::DesktopRenderer() = default;

            DesktopRenderer::~DesktopRenderer()
            {
                shutdown();
            }

            bool DesktopRenderer::initialize(SDL_Renderer *sdlRenderer, const RenderOptions &options)
            {
                if (!sdlRenderer)
                    return false;

                sdlRenderer_ = sdlRenderer;
                setOptions(options); // يعكسها في خيارات الأساس أيضًا
                initialized_ = true;

#ifdef SAD_UI_USE_SDL2
                // تفعيل وضع المزج للشفافية عالمياً
                SDL_SetRenderDrawBlendMode(sdlRenderer_, SDL_BLENDMODE_BLEND);
                // تحسين جودة القياس للصور والنصوص
                SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"); // تصفية خطية
#endif

#ifdef SAD_UI_HAS_SDL_TTF
                // تهيئة SDL_ttf
                if (TTF_Init() == -1)
                {
                    std::cerr << "خطأ تهيئة SDL_ttf: " << TTF_GetError() << std::endl;
                    return false;
                }
#endif

                return true;
            }

            void DesktopRenderer::shutdown()
            {
#ifdef SAD_UI_HAS_SDL_TTF
                // تنظيف ذاكرة التخزين المؤقت للنصوص
                for (auto &[key, cached] : g_textCache_)
                {
                    if (cached.texture)
                    {
                        SDL_DestroyTexture(cached.texture);
                    }
                }
                g_textCache_.clear();

                // تنظيف الخطوط
                for (auto &[size, font] : g_fontSizeCache_)
                {
                    if (font)
                    {
                        TTF_CloseFont(font);
                    }
                }
                g_fontSizeCache_.clear();

                // تنظيف خطوط الإيموجي
                for (auto &[size, font] : g_emojiFontCache_)
                {
                    if (font)
                    {
                        TTF_CloseFont(font);
                    }
                }
                g_emojiFontCache_.clear();
                g_emojiFont_ = nullptr;

                if (defaultFont_)
                {
                    TTF_CloseFont(defaultFont_);
                    defaultFont_ = nullptr;
                }

                TTF_Quit();
#endif

                sdlRenderer_ = nullptr;
                initialized_ = false;
            }

            bool DesktopRenderer::loadFont(const std::string &fontPath, float fontSize)
            {
#ifdef SAD_UI_HAS_SDL_TTF
                defaultFont_ = TTF_OpenFont(fontPath.c_str(), static_cast<int>(fontSize));
                if (!defaultFont_)
                {
                    std::cerr << "خطأ تحميل الخط: " << TTF_GetError() << std::endl;
                    // محاولة تحميل خط بديل حسب المنصة
                    const char *fallbackFonts[] = {
#ifdef _WIN32
                        "C:/Windows/Fonts/arial.ttf",
                        "C:/Windows/Fonts/tahoma.ttf",
                        "C:/Windows/Fonts/segoeui.ttf",
#elif defined(__APPLE__)
                        "/System/Library/Fonts/Helvetica.ttc",
                        "/Library/Fonts/Arial.ttf",
#else
                        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                        "/usr/share/fonts/TTF/DejaVuSans.ttf",
                        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
#endif
                        nullptr};
                    for (int i = 0; fallbackFonts[i] != nullptr; ++i)
                    {
                        defaultFont_ = TTF_OpenFont(fallbackFonts[i], static_cast<int>(fontSize));
                        if (defaultFont_)
                        {
                            std::cout << "تم تحميل الخط البديل: " << fallbackFonts[i] << std::endl;
                            break;
                        }
                    }
                }
                if (defaultFont_)
                {
                    // تفعيل HarfBuzz للخط الافتراضي — دعم العربية
                    TTF_SetFontDirection(defaultFont_, TTF_DIRECTION_RTL);
                    TTF_SetFontScriptName(defaultFont_, "Arab");
                }
                return defaultFont_ != nullptr;
#else
                (void)fontPath;
                (void)fontSize;
                return true;
#endif
            }

            void DesktopRenderer::beginFrame(const Color &clearColor)
            {
                if (!initialized_)
                    return;

#ifdef SAD_UI_USE_SDL2
                // تحديث أبعاد منفذ العرض لقص العناصر خارج الشاشة
                SDL_GetRendererOutputSize(sdlRenderer_, &viewportWidth_, &viewportHeight_);

                SDL_SetRenderDrawColor(sdlRenderer_,
                                       static_cast<uint8_t>(clearColor.r * 255),
                                       static_cast<uint8_t>(clearColor.g * 255),
                                       static_cast<uint8_t>(clearColor.b * 255),
                                       static_cast<uint8_t>(clearColor.a * 255));
                SDL_RenderClear(sdlRenderer_);
#else
                (void)clearColor;
#endif
            }

            void DesktopRenderer::endFrame()
            {
                if (!initialized_)
                    return;

#ifdef SAD_UI_USE_SDL2
                SDL_RenderPresent(sdlRenderer_);
#endif
            }

            // ═══════════════════════════════════════════════════════════════════
            // (AR) شوكةُ الرسمِ حُذِفت — كان هنا render() وrenderNode() ينسخان
            //      مُرسِّمَ المكتبةِ المشترَك: ٨٢ عقدةً مكرَّرةً لا تطابق واحدةٌ
            //      منها نظيرَتَها، فتُصلَح العلّةُ في نسخةٍ وتبقى في الأخرى.
            //      اليومَ يرثُ سطحُ المكتبِ PlatformRenderer::render، فلا يبقى
            //      له إلّا الأوّليّاتُ (SDL2/TTF) وحالةُ التمريرِ التي يملكها.
            //      انظر: features/graphics/core/src/platform_renderer.cpp
            // ═══════════════════════════════════════════════════════════════════

            float DesktopRenderer::platformScrollOffset(const IRNode &node) const
            {
                return getScrollOffset(&node);
            }

        } // namespace desktop
    } // namespace ui
} // namespace sad