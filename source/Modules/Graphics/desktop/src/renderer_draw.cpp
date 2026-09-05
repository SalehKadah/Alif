/**
 * ═══════════════════════════════════════════════════════════════════
 * ملف: renderer_draw.cpp
 * المسار: features/graphics/backends/desktop/src/renderer_draw.cpp
 * ═══════════════════════════════════════════════════════════════════
 *
 * دوال الرسم الأساسية: مستطيلات، نصوص، صور، حدود، ظلال، تدرجات
 *
 * ═══════════════════════════════════════════════════════════════════
 */

#include "renderer_internal.h"

// ─── تحميل الصور بـ stb_image ───
#ifdef SAD_UI_USE_SDL2
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#include "stb_image.h"
#endif

namespace sad
{
    namespace ui
    {
        namespace desktop
        {

// ═══ تعريف المتغيرات العامة (مرة واحدة فقط) ═══
#ifdef SAD_UI_HAS_SDL_TTF
            TTF_Font *g_emojiFont_ = nullptr;
            std::unordered_map<int, TTF_Font *> g_emojiFontCache_;
#endif

#ifdef SAD_UI_USE_SDL2
#ifdef SAD_UI_HAS_SDL_TTF
            std::unordered_map<TextCacheKey, TextCacheEntry, TextCacheKeyHash> g_textCache_;
            std::unordered_map<int, TTF_Font *> g_fontSizeCache_;
#endif
#endif

            // ─── دعم التمرير (Scroll Support) ─────────

            void DesktopRenderer::setScrollOffset(const IRNode *node, float offset)
            {
                if (node)
                    scrollOffsets_[node] = offset;
            }

            float DesktopRenderer::getScrollOffset(const IRNode *node) const
            {
                if (!node)
                    return 0.0f;
                auto it = scrollOffsets_.find(node);
                return (it != scrollOffsets_.end()) ? it->second : 0.0f;
            }

            // ═══════════════════════════════════════════════════════════════════
            // الرسم الأساسي
            // ═══════════════════════════════════════════════════════════════════

            void DesktopRenderer::drawFilledRect(const LayoutRect &rect, const Color &color)
            {
#ifdef SAD_UI_USE_SDL2
                SDL_SetRenderDrawBlendMode(sdlRenderer_, SDL_BLENDMODE_BLEND);
                float effectiveAlpha = color.a * currentNodeOpacity_;
                SDL_SetRenderDrawColor(sdlRenderer_,
                                       static_cast<uint8_t>(color.r * 255),
                                       static_cast<uint8_t>(color.g * 255),
                                       static_cast<uint8_t>(color.b * 255),
                                       static_cast<uint8_t>(effectiveAlpha * 255));

                SDL_Rect sdlRect = {
                    static_cast<int>(rect.x),
                    static_cast<int>(rect.y),
                    static_cast<int>(rect.width),
                    static_cast<int>(rect.height)};
                SDL_RenderFillRect(sdlRenderer_, &sdlRect);
#else
                (void)rect;
                (void)color;
#endif
            }

            void DesktopRenderer::drawRoundedRect(const LayoutRect &rect, const Color &color, float radius)
            {
#ifdef SAD_UI_USE_SDL2
                SDL_SetRenderDrawBlendMode(sdlRenderer_, SDL_BLENDMODE_BLEND);
                float effectiveAlpha = color.a * currentNodeOpacity_;
                SDL_SetRenderDrawColor(sdlRenderer_,
                                       static_cast<uint8_t>(color.r * 255),
                                       static_cast<uint8_t>(color.g * 255),
                                       static_cast<uint8_t>(color.b * 255),
                                       static_cast<uint8_t>(effectiveAlpha * 255));

                int x = static_cast<int>(rect.x);
                int y = static_cast<int>(rect.y);
                int w = static_cast<int>(rect.width);
                int h = static_cast<int>(rect.height);
                int r = static_cast<int>(std::min(radius, std::min(rect.width, rect.height) / 2.0f));

                if (r <= 0)
                {
                    SDL_Rect sdlRect = {x, y, w, h};
                    SDL_RenderFillRect(sdlRenderer_, &sdlRect);
                }
                else
                {
                    SDL_Rect center = {x + r, y, w - 2 * r, h};
                    SDL_RenderFillRect(sdlRenderer_, &center);
                    SDL_Rect left = {x, y + r, r, h - 2 * r};
                    SDL_RenderFillRect(sdlRenderer_, &left);
                    SDL_Rect right = {x + w - r, y + r, r, h - 2 * r};
                    SDL_RenderFillRect(sdlRenderer_, &right);

                    int cx, cy, d;
                    cx = 0;
                    cy = r;
                    d = 1 - r;
                    while (cx <= cy)
                    {
                        SDL_RenderDrawLine(sdlRenderer_, x + w - r + cx - 1, y + r - cy, x + w - r + cx - 1, y + r - cy);
                        SDL_RenderDrawLine(sdlRenderer_, x + w - r + cy - 1, y + r - cx, x + w - r + cy - 1, y + r - cx);
                        SDL_RenderDrawLine(sdlRenderer_, x + r - cx, y + r - cy, x + w - r + cx - 1, y + r - cy);
                        SDL_RenderDrawLine(sdlRenderer_, x + r - cy, y + r - cx, x + w - r + cy - 1, y + r - cx);
                        SDL_RenderDrawLine(sdlRenderer_, x + r - cx, y + h - r + cy - 1, x + w - r + cx - 1, y + h - r + cy - 1);
                        SDL_RenderDrawLine(sdlRenderer_, x + r - cy, y + h - r + cx - 1, x + w - r + cy - 1, y + h - r + cx - 1);
                        if (d < 0)
                        {
                            d += 2 * cx + 3;
                        }
                        else
                        {
                            d += 2 * (cx - cy) + 5;
                            cy--;
                        }
                        cx++;
                    }
                }
#else
                (void)rect;
                (void)color;
                (void)radius;
#endif
            }

            // ═══════════════════════════════════════════════════════════════════
            // النصوص
            // ═══════════════════════════════════════════════════════════════════

            TTF_Font *DesktopRenderer::getFontForSize(float fontSize)
            {
#ifdef SAD_UI_HAS_SDL_TTF
                int size = static_cast<int>(fontSize);
                if (size < 1)
                    size = 16;
                auto it = g_fontSizeCache_.find(size);
                if (it != g_fontSizeCache_.end())
                {
                    return it->second;
                }

                const char *fontPaths[] = {
#ifdef _WIN32
                    "C:/Windows/Fonts/arial.ttf",
                    "C:/Windows/Fonts/tahoma.ttf",
                    "C:/Windows/Fonts/segoeui.ttf",
#elif defined(__APPLE__)
                    "/System/Library/Fonts/Helvetica.ttc",
                    "/System/Library/Fonts/SFArabic.ttf",
                    "/Library/Fonts/Arial.ttf",
#else
                    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                    "/usr/share/fonts/TTF/DejaVuSans.ttf",
                    "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
                    "/usr/share/fonts/noto/NotoSansArabic-Regular.ttf",
#endif
                    nullptr};

                TTF_Font *font = nullptr;
                for (int i = 0; fontPaths[i] != nullptr; ++i)
                {
                    font = TTF_OpenFont(fontPaths[i], size);
                    if (font)
                        break;
                }

                if (font)
                {
                    TTF_SetFontDirection(font, TTF_DIRECTION_RTL);
                    TTF_SetFontScriptName(font, "Arab");
                    g_fontSizeCache_[size] = font;
                    return font;
                }

                return defaultFont_;
#else
                (void)fontSize;
                return nullptr;
#endif
            }

            void DesktopRenderer::drawTextInternal(
                const std::string &text, float x, float y,
                const Color &color, float fontSize)
            {
                if (text.empty())
                    return;

#ifdef SAD_UI_HAS_SDL_TTF
                // ═══ دعم الإيموجي ═══
                if (containsEmoji(text))
                {
                    auto segments = splitTextByEmoji(text);
                    float curX = x;
                    for (const auto &seg : segments)
                    {
                        if (seg.text.empty())
                            continue;

                        TTF_Font *segFont = nullptr;
                        if (seg.isEmoji)
                        {
                            segFont = getEmojiFontForSize(static_cast<int>(fontSize));
                        }
                        if (!segFont)
                        {
                            segFont = getFontForSize(fontSize);
                            if (!segFont)
                                segFont = defaultFont_;
                        }
                        if (!segFont)
                            continue;

                        if (seg.isEmoji)
                        {
                            TTF_SetFontDirection(segFont, TTF_DIRECTION_LTR);
                            TTF_SetFontScriptName(segFont, "Latn");
                        }
                        else
                        {
                            bool arabic = isArabicUTF8(seg.text);
                            if (arabic)
                            {
                                TTF_SetFontDirection(segFont, TTF_DIRECTION_RTL);
                                TTF_SetFontScriptName(segFont, "Arab");
                            }
                            else
                            {
                                TTF_SetFontDirection(segFont, TTF_DIRECTION_LTR);
                                TTF_SetFontScriptName(segFont, "Latn");
                            }
                        }

                        SDL_Color sdlColor = {
                            static_cast<uint8_t>(color.r * 255),
                            static_cast<uint8_t>(color.g * 255),
                            static_cast<uint8_t>(color.b * 255),
                            static_cast<uint8_t>(color.a * 255)};

                        SDL_Surface *surface = TTF_RenderUTF8_Blended(segFont, seg.text.c_str(), sdlColor);
                        if (!surface)
                        {
                            if (seg.isEmoji)
                            {
                                segFont = getFontForSize(fontSize);
                                if (segFont)
                                {
                                    surface = TTF_RenderUTF8_Blended(segFont, seg.text.c_str(), sdlColor);
                                }
                            }
                            if (!surface)
                                continue;
                        }

                        SDL_Texture *texture = SDL_CreateTextureFromSurface(sdlRenderer_, surface);
                        if (texture)
                        {
                            // تطبيق شفافية العقدة على النص (من التحريكات أو خاصية الشفافية)
                            if (currentNodeOpacity_ < 1.0f)
                            {
                                SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
                                SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(currentNodeOpacity_ * 255));
                            }
                            SDL_Rect dstRect = {
                                static_cast<int>(curX),
                                static_cast<int>(y),
                                surface->w,
                                surface->h};
                            if (currentNodeRotation_ != 0.0f)
                            {
                                // دوران النص حول مركزه
                                SDL_Point center = {surface->w / 2, surface->h / 2};
                                SDL_RenderCopyEx(sdlRenderer_, texture, nullptr, &dstRect,
                                                 static_cast<double>(currentNodeRotation_),
                                                 &center, SDL_FLIP_NONE);
                            }
                            else
                            {
                                SDL_RenderCopy(sdlRenderer_, texture, nullptr, &dstRect);
                            }
                            curX += surface->w;
                            SDL_DestroyTexture(texture);
                        }
                        SDL_FreeSurface(surface);
                    }
                    return;
                }

                // ═══ مسار النص العادي — مع تخزين مؤقت ═══
                uint32_t colorRGBA = (static_cast<uint8_t>(color.r * 255) << 24) |
                                     (static_cast<uint8_t>(color.g * 255) << 16) |
                                     (static_cast<uint8_t>(color.b * 255) << 8) |
                                     static_cast<uint8_t>(color.a * 255);

                ::sad::ui::desktop::TextCacheKey key{text, fontSize, colorRGBA};
                auto it = g_textCache_.find(key);

                if (it != g_textCache_.end())
                {
                    // تطبيق شفافية العقدة على النص المخزّن مؤقتاً
                    if (currentNodeOpacity_ < 1.0f)
                    {
                        SDL_SetTextureBlendMode(it->second.texture, SDL_BLENDMODE_BLEND);
                        SDL_SetTextureAlphaMod(it->second.texture, static_cast<Uint8>(currentNodeOpacity_ * 255));
                    }
                    else
                    {
                        SDL_SetTextureAlphaMod(it->second.texture, 255);
                    }
                    SDL_Rect dstRect = {
                        static_cast<int>(x),
                        static_cast<int>(y),
                        it->second.width,
                        it->second.height};
                    if (currentNodeRotation_ != 0.0f)
                    {
                        SDL_Point center = {it->second.width / 2, it->second.height / 2};
                        SDL_RenderCopyEx(sdlRenderer_, it->second.texture, nullptr, &dstRect,
                                         static_cast<double>(currentNodeRotation_),
                                         &center, SDL_FLIP_NONE);
                    }
                    else
                    {
                        SDL_RenderCopy(sdlRenderer_, it->second.texture, nullptr, &dstRect);
                    }
                    return;
                }

                TTF_Font *font = getFontForSize(fontSize);
                if (!font)
                    font = defaultFont_;
                if (!font)
                    return;

                bool arabic = isArabicUTF8(text);
                if (arabic)
                {
                    TTF_SetFontDirection(font, TTF_DIRECTION_RTL);
                    TTF_SetFontScriptName(font, "Arab");
                }
                else
                {
                    TTF_SetFontDirection(font, TTF_DIRECTION_LTR);
                    TTF_SetFontScriptName(font, "Latn");
                }

                SDL_Color sdlColor = {
                    static_cast<uint8_t>(color.r * 255),
                    static_cast<uint8_t>(color.g * 255),
                    static_cast<uint8_t>(color.b * 255),
                    static_cast<uint8_t>(color.a * 255)};

                SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text.c_str(), sdlColor);
                if (!surface)
                    return;

                SDL_Texture *texture = SDL_CreateTextureFromSurface(sdlRenderer_, surface);
                if (!texture)
                {
                    SDL_FreeSurface(surface);
                    return;
                }

                if (g_textCache_.size() > 512)
                {
                    for (auto &[k, v] : g_textCache_)
                    {
                        if (v.texture)
                            SDL_DestroyTexture(v.texture);
                    }
                    g_textCache_.clear();
                }
                TextCacheEntry cached;
                cached.texture = texture;
                cached.width = surface->w;
                cached.height = surface->h;
                g_textCache_[key] = cached;

                // تطبيق شفافية العقدة على النص الجديد
                if (currentNodeOpacity_ < 1.0f)
                {
                    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
                    SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(currentNodeOpacity_ * 255));
                }
                SDL_Rect dstRect = {
                    static_cast<int>(x),
                    static_cast<int>(y),
                    surface->w,
                    surface->h};
                // تطبيق الدوران على النص الجديد (غير المخزّن)
                if (currentNodeRotation_ != 0.0f)
                {
                    SDL_Point center = {surface->w / 2, surface->h / 2};
                    SDL_RenderCopyEx(sdlRenderer_, texture, nullptr, &dstRect,
                                     static_cast<double>(currentNodeRotation_),
                                     &center, SDL_FLIP_NONE);
                }
                else
                {
                    SDL_RenderCopy(sdlRenderer_, texture, nullptr, &dstRect);
                }

                SDL_FreeSurface(surface);
#elif defined(SAD_UI_USE_SDL2)
                float approxWidth = text.length() * fontSize * 0.5f;
                float approxHeight = fontSize;
                SDL_Rect rect = {
                    static_cast<int>(x),
                    static_cast<int>(y),
                    static_cast<int>(approxWidth),
                    static_cast<int>(approxHeight)};
                SDL_SetRenderDrawColor(sdlRenderer_,
                                       static_cast<uint8_t>(color.r * 255),
                                       static_cast<uint8_t>(color.g * 255),
                                       static_cast<uint8_t>(color.b * 255),
                                       static_cast<uint8_t>(color.a * 128));
                SDL_RenderFillRect(sdlRenderer_, &rect);
#else
                (void)text;
                (void)x;
                (void)y;
                (void)color;
                (void)fontSize;
#endif
            }

            // ═══════════════════════════════════════════════════════════════════
            // الصور
            // ═══════════════════════════════════════════════════════════════════

            void DesktopRenderer::drawImage(const std::string &path, const LayoutRect &rect)
            {
#ifdef SAD_UI_USE_SDL2
                auto it = imageCache_.find(path);
                if (it != imageCache_.end())
                {
                    // تطبيق شفافية العقدة على الصورة المخزّنة
                    SDL_Texture *tex = static_cast<SDL_Texture *>(it->second.texture);
                    if (currentNodeOpacity_ < 1.0f)
                    {
                        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
                        SDL_SetTextureAlphaMod(tex, static_cast<Uint8>(currentNodeOpacity_ * 255));
                    }
                    else
                    {
                        SDL_SetTextureAlphaMod(tex, 255);
                    }
                    SDL_Rect dst = {
                        static_cast<int>(rect.x), static_cast<int>(rect.y),
                        static_cast<int>(rect.width), static_cast<int>(rect.height)};
                    // تطبيق الدوران على الصورة المخزّنة
                    if (currentNodeRotation_ != 0.0f)
                    {
                        SDL_Point center = {static_cast<int>(rect.width) / 2, static_cast<int>(rect.height) / 2};
                        SDL_RenderCopyEx(sdlRenderer_, tex, nullptr, &dst,
                                         static_cast<double>(currentNodeRotation_),
                                         &center, SDL_FLIP_NONE);
                    }
                    else
                    {
                        SDL_RenderCopy(sdlRenderer_, tex, nullptr, &dst);
                    }
                    return;
                }

                int w = 0, h = 0, channels = 0;
                unsigned char *data = stbi_load(path.c_str(), &w, &h, &channels, 4);
                if (data)
                {
                    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(
                        data, w, h, 32, w * 4,
                        0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
                    if (surface)
                    {
                        SDL_Texture *texture = SDL_CreateTextureFromSurface(sdlRenderer_, surface);
                        SDL_FreeSurface(surface);
                        if (texture)
                        {
                            // تطبيق شفافية العقدة على الصورة الجديدة
                            if (currentNodeOpacity_ < 1.0f)
                            {
                                SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
                                SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(currentNodeOpacity_ * 255));
                            }
                            imageCache_[path] = {texture, w, h};
                            SDL_Rect dst = {
                                static_cast<int>(rect.x), static_cast<int>(rect.y),
                                static_cast<int>(rect.width), static_cast<int>(rect.height)};
                            // تطبيق الدوران على الصورة الجديدة
                            if (currentNodeRotation_ != 0.0f)
                            {
                                SDL_Point center = {static_cast<int>(rect.width) / 2, static_cast<int>(rect.height) / 2};
                                SDL_RenderCopyEx(sdlRenderer_, texture, nullptr, &dst,
                                                 static_cast<double>(currentNodeRotation_),
                                                 &center, SDL_FLIP_NONE);
                            }
                            else
                            {
                                SDL_RenderCopy(sdlRenderer_, texture, nullptr, &dst);
                            }
                            stbi_image_free(data);
                            return;
                        }
                    }
                    stbi_image_free(data);
                }

                Color placeholder = {0.9f, 0.9f, 0.9f, 1.0f};
                drawFilledRect(rect, placeholder);
                drawText("\xf0\x9f\x96\xbc", rect.x + rect.width / 2 - 8, rect.y + rect.height / 2 - 8,
                         {0.5f, 0.5f, 0.5f, 1.0f}, 16.0f);
#else
                (void)path;
                (void)rect;
#endif
            }

            // ═══════════════════════════════════════════════════════════════════
            // الحدود والظلال والتدرجات
            // ═══════════════════════════════════════════════════════════════════

            void DesktopRenderer::drawBorder(const LayoutRect &rect, const BorderStyle &border)
            {
#ifdef SAD_UI_USE_SDL2
                float effectiveAlpha = border.color.a * currentNodeOpacity_;
                SDL_SetRenderDrawColor(sdlRenderer_,
                                       static_cast<uint8_t>(border.color.r * 255),
                                       static_cast<uint8_t>(border.color.g * 255),
                                       static_cast<uint8_t>(border.color.b * 255),
                                       static_cast<uint8_t>(effectiveAlpha * 255));

                SDL_Rect sdlRect = {
                    static_cast<int>(rect.x),
                    static_cast<int>(rect.y),
                    static_cast<int>(rect.width),
                    static_cast<int>(rect.height)};

                for (int i = 0; i < static_cast<int>(border.width); ++i)
                {
                    SDL_Rect borderRect = {
                        sdlRect.x + i, sdlRect.y + i,
                        sdlRect.w - 2 * i, sdlRect.h - 2 * i};
                    SDL_RenderDrawRect(sdlRenderer_, &borderRect);
                }
#else
                (void)rect;
                (void)border;
#endif
            }

            void DesktopRenderer::drawShadow(const LayoutRect &rect, const ShadowStyle &shadow)
            {
#ifdef SAD_UI_USE_SDL2
                int blur = static_cast<int>(shadow.blur);
                for (int i = blur; i > 0; --i)
                {
                    float alpha = shadow.color.a * (1.0f - static_cast<float>(i) / blur) * 0.3f * currentNodeOpacity_;
                    SDL_SetRenderDrawColor(sdlRenderer_,
                                           static_cast<uint8_t>(shadow.color.r * 255),
                                           static_cast<uint8_t>(shadow.color.g * 255),
                                           static_cast<uint8_t>(shadow.color.b * 255),
                                           static_cast<uint8_t>(alpha * 255));

                    SDL_Rect shadowRect = {
                        static_cast<int>(rect.x + shadow.offsetX - i),
                        static_cast<int>(rect.y + shadow.offsetY - i),
                        static_cast<int>(rect.width + 2 * i),
                        static_cast<int>(rect.height + 2 * i)};
                    SDL_RenderFillRect(sdlRenderer_, &shadowRect);
                }
#else
                (void)rect;
                (void)shadow;
#endif
            }

            void DesktopRenderer::drawLinearGradient(const LayoutRect &rect,
                                                     const Color &startColor, const Color &endColor, bool vertical, float radius)
            {
#ifdef SAD_UI_USE_SDL2
                if (!sdlRenderer_)
                    return;
                SDL_SetRenderDrawBlendMode(sdlRenderer_, SDL_BLENDMODE_BLEND);

                SDL_Color c0 = {static_cast<Uint8>(startColor.r * 255), static_cast<Uint8>(startColor.g * 255), static_cast<Uint8>(startColor.b * 255), static_cast<Uint8>(startColor.a * currentNodeOpacity_ * 255)};
                SDL_Color c1 = {static_cast<Uint8>(endColor.r * 255), static_cast<Uint8>(endColor.g * 255), static_cast<Uint8>(endColor.b * 255), static_cast<Uint8>(endColor.a * currentNodeOpacity_ * 255)};

                float x = rect.x, y = rect.y, w = rect.width, h = rect.height;

                if (vertical)
                {
                    SDL_Vertex verts[4];
                    verts[0].position = {x, y};
                    verts[0].color = c0;
                    verts[0].tex_coord = {0, 0};
                    verts[1].position = {x + w, y};
                    verts[1].color = c0;
                    verts[1].tex_coord = {1, 0};
                    verts[2].position = {x + w, y + h};
                    verts[2].color = c1;
                    verts[2].tex_coord = {1, 1};
                    verts[3].position = {x, y + h};
                    verts[3].color = c1;
                    verts[3].tex_coord = {0, 1};
                    int idx[6] = {0, 1, 2, 0, 2, 3};
                    SDL_RenderGeometry(sdlRenderer_, nullptr, verts, 4, idx, 6);
                }
                else
                {
                    SDL_Vertex verts[4];
                    verts[0].position = {x, y};
                    verts[0].color = c0;
                    verts[0].tex_coord = {0, 0};
                    verts[1].position = {x + w, y};
                    verts[1].color = c1;
                    verts[1].tex_coord = {1, 0};
                    verts[2].position = {x + w, y + h};
                    verts[2].color = c1;
                    verts[2].tex_coord = {1, 1};
                    verts[3].position = {x, y + h};
                    verts[3].color = c0;
                    verts[3].tex_coord = {0, 1};
                    int idx[6] = {0, 1, 2, 0, 2, 3};
                    SDL_RenderGeometry(sdlRenderer_, nullptr, verts, 4, idx, 6);
                }
#else
                (void)rect;
                (void)startColor;
                (void)endColor;
                (void)vertical;
                (void)radius;
#endif
            }

            // (AR) drawSoftShadow انتقل إلى PlatformRenderer: مبنيٌّ على
            //      drawRoundedRect وحدَها فلا يخصُّ SDL في شيء.

            void DesktopRenderer::clearTextCache()
            {
#ifdef SAD_UI_HAS_SDL_TTF
                for (auto &[key, cached] : g_textCache_)
                {
                    if (cached.texture)
                    {
                        SDL_DestroyTexture(cached.texture);
                    }
                }
                g_textCache_.clear();
#endif
            }

        } // namespace desktop
    } // namespace ui
} // namespace sad
