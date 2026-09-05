/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: renderer.h
 * المسار: features/graphics/backends/desktop/include/sad_ui/desktop/renderer.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * الوصف:
 * ------
 * مُصيّر سطح المكتب (Desktop Renderer) — يرسم عناصر الواجهة باستخدام
 * SDL2 + OpenGL.
 *
 * على سطح المكتب (Windows/Linux/macOS) و WASM:
 * - لا نستخدم عناصر أصلية (لا Win32 Controls ولا GTK)
 * - نرسم كل شيء بأنفسنا عبر SDL2+OpenGL
 * - هذا يعطي تحكم كامل بالمظهر وتجربة موحدة عبر المنصات
 *
 * سلسلة العرض (Rendering Pipeline):
 * ───────────────────────────────────
 *   IRNode Tree + LayoutResult → Renderer → SDL2/OpenGL → الشاشة
 *
 * الخطوات:
 * 1. عبور شجرة IR مع نتائج التخطيط
 * 2. لكل عقدة: رسم الخلفية → الحدود → المحتوى → الأبناء
 * 3. تطبيق التحويلات (دوران، تكبير)
 * 4. تطبيق القص (clip) إذا لزم الأمر
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_DESKTOP_RENDERER_H
#define SAD_UI_DESKTOP_RENDERER_H

#include "sad_ui/ir.h"
#include "sad_ui/style.h"
#include "sad_ui/layout.h"
#include "sad_ui/animation.h"
#include "sad_ui/platform_renderer.h"

#include <string>
#include <memory>
#include <vector>
#include <utility>
#include <functional>
#include <unordered_map>
#include <unordered_set>

// تصريحات أمامية لـ SDL2 (لا نحتاج include هنا)
struct SDL_Window;
struct SDL_Renderer;
#ifdef SAD_UI_HAS_SDL_TTF
// (AR) اسم البنية الداخلي لـ TTF_Font غير موحَّد بين إصدارات SDL_ttf: النسخ
//      الأقدم (مثل apt على Ubuntu) تستخدم «struct _TTF_Font»، والنسخ ≥ 2.24
//      (مثل Homebrew على macOS) تستخدم «struct TTF_Font». تخمين الاسم هنا
//      عبر typedef يدوي يتعارض مع تعريف SDL_ttf.h الحقيقي على أحد النظامين،
//      فنُضمّن الرأس الحقيقي مباشرة ليُحدِّد هو التعريف الصحيح لكل حالة.
// (EN) TTF_Font's internal struct tag isn't consistent across SDL_ttf
//      versions: older builds (e.g. apt on Ubuntu) use "struct _TTF_Font",
//      while ≥ 2.24 builds (e.g. Homebrew on macOS) use "struct TTF_Font".
//      Guessing the tag via a manual typedef conflicts with SDL_ttf.h's real
//      definition on one platform or the other, so include the real header
//      instead and let it define the correct type.
#include <SDL_ttf.h>
#else
typedef void TTF_Font; // placeholder عند عدم توفر SDL_ttf
#endif

namespace sad
{
    namespace ui
    {
        namespace desktop
        {

            // ═══════════════════════════════════════════════════════════════════════════════
            // خيارات العرض (Render Options)
            // ═══════════════════════════════════════════════════════════════════════════════

            /**
             * @brief خيارات التحكم في العرض
             */
            struct RenderOptions
            {
                bool antialiasing = true; ///< تمكين التنعيم
                bool vsync = true;        ///< مزامنة عمودية
                float scaleFactor = 1.0f; ///< معامل التكبير (للشاشات عالية الكثافة)
                bool debugBounds = false; ///< رسم حدود العناصر (للتصحيح)
                bool debugLayout = false; ///< عرض معلومات التخطيط (للتصحيح)
            };

            // ═══════════════════════════════════════════════════════════════════════════════
            // المُصيّر (Renderer)
            // ═══════════════════════════════════════════════════════════════════════════════

            /**
             * @brief مُصيّر سطح المكتب — يرسم واجهة المستخدم عبر SDL2
             *
             * الاستخدام:
             * @code
             *   DesktopRenderer renderer;
             *   renderer.initialize(sdlRenderer, options);
             *
             *   // في حلقة الرسم:
             *   renderer.beginFrame();
             *   renderer.render(irRoot, layoutResult, resolvedStyles);
             *   renderer.endFrame();
             * @endcode
             */
            class DesktopRenderer : public PlatformRenderer
            {
            public:
                DesktopRenderer();
                ~DesktopRenderer();

                /**
                 * @brief تهيئة المُصيّر مع SDL Renderer
                 *
                 * @param sdlRenderer مؤشر SDL_Renderer (يجب أن يكون صالحاً)
                 * @param options خيارات العرض
                 * @return true إذا نجحت التهيئة
                 */
                bool initialize(SDL_Renderer *sdlRenderer, const RenderOptions &options = {});

                // ════════════════════════════════════════════════════════════════════════
                // تطبيق واجهة PlatformRenderer المجردة
                // ════════════════════════════════════════════════════════════════════════

                bool initialize(void *nativeSurface,
                                const PlatformRenderOptions &options = {}) override;
                void shutdown() override;
                bool isReady() const override;
                bool loadFont(const std::string &fontPath, float fontSize = 16.0f) override;
                void beginFrame(const Color &clearColor = {}) override;
                void endFrame() override;

                void drawFilledRect(float x, float y, float w, float h,
                                    const Color &color) override;
                void drawRoundedRect(float x, float y, float w, float h,
                                     const Color &color, float radius) override;
                void drawRectOutline(float x, float y, float w, float h,
                                     const Color &color, float thickness = 1.0f) override;
                std::pair<float, float> drawText(const std::string &text,
                                                 float x, float y,
                                                 const Color &color,
                                                 float fontSize) override;
                std::pair<float, float> measureText(const std::string &text,
                                                    float fontSize) override;
                void drawImage(const std::string &path,
                               float x, float y, float w, float h) override;
                void drawCircle(float cx, float cy, float radius,
                                const Color &color) override;
                void drawCircleOutline(float cx, float cy, float radius,
                                       const Color &color, float thickness = 1.0f) override;
                void drawLine(float x1, float y1, float x2, float y2,
                              const Color &color, float thickness = 1.0f) override;
                void drawLinearGradient(float x, float y, float w, float h,
                                        const Color &startColor, const Color &endColor,
                                        bool vertical = true, float radius = 0.0f) override;
                void setClipRect(float x, float y, float w, float h) override;
                void clearClipRect() override;

                /**
                 * @brief (AR) إزاحةُ التمريرِ التي تملكها المنصّة (عجلةُ الفأرة)
                 *
                 * تخصيصُ خُطّافِ المكتبة: المُرسِّمُ المشترَك يسألُ عنها لكلِّ حاويةٍ
                 * قابلةٍ للتمرير فتَسري على أبنائها وأحفادِها.
                 */
                float platformScrollOffset(const IRNode &node) const override;

                // ─── دعم التمرير (Scroll Support) ─────────

                /// تعيين إزاحة التمرير لعنصر قابل للتمرير
                void setScrollOffset(const IRNode *node, float offset);

                /// الحصول على إزاحة التمرير الحالية
                float getScrollOffset(const IRNode *node) const;

                /// مسح جميع إزاحات التمرير (عند تغيير المحتوى)
                void clearScrollOffsets() { scrollOffsets_.clear(); }

                // ─── تحديث الخيارات ───────────────────────

                /**
                 * @brief تحديث خيارات العرض الأصلية (SDL2-specific)
                 *
                 * (AR) تُعكَس في `options_` الخاصّة بالأساس أيضًا: المُرسِّمُ
                 *      المشترَك يقرأ منها (حدودُ التصحيح مثلًا)، وRenderOptions
                 *      هنا نسخةٌ حقلًا بحقلٍ من PlatformRenderOptions — بقيّةُ
                 *      الشوكةِ نفسِها. دَينٌ معلَن: توحيدُ البنيتَين يمسُّ واجهةَ
                 *      النافذةِ العامّةَ فأُفرِد عن هذا التغيير.
                 */
                void setOptions(const RenderOptions &options)
                {
                    desktopOptions_ = options;
                    options_.antialiasing = options.antialiasing;
                    options_.vsync = options.vsync;
                    options_.scaleFactor = options.scaleFactor;
                    options_.debugBounds = options.debugBounds;
                    options_.debugLayout = options.debugLayout;
                }

                /// الحصول على خيارات العرض (SDL2-specific)
                const RenderOptions &getDesktopOptions() const { return desktopOptions_; }

                /// تعيين العقد التفاعلية (استخدم واجهة PlatformRenderer)
                using PlatformRenderer::setInteractionState;

            private:
                // ─── رسم العناصر الفردية ─────────────────

                /// رسم مستطيل مملوء (API داخلي بـ LayoutRect)
                void drawFilledRect(const LayoutRect &rect, const Color &color);

                /// رسم مستطيل بزوايا دائرية (API داخلي بـ LayoutRect)
                void drawRoundedRect(const LayoutRect &rect, const Color &color, float radius);

                /// رسم نص (API داخلي)
                void drawTextInternal(const std::string &text, float x, float y,
                                      const Color &color, float fontSize);

                /// رسم صورة (API داخلي بـ LayoutRect)
                void drawImage(const std::string &path, const LayoutRect &rect);

                /// رسم حدود
                void drawBorder(const LayoutRect &rect, const BorderStyle &border);

                /// رسم ظل
                void drawShadow(const LayoutRect &rect, const ShadowStyle &shadow);

                // ════════════════════════════════════════════════════════════════════════
                // دوال رسم إضافية (Desktop-specific Canvas API)
                // ════════════════════════════════════════════════════════════════════════

                /// رسم قطع ناقص (ellipse)
                void drawEllipse(float cx, float cy, float rx, float ry, const Color &color);

                /// رسم مثلث مملوء
                void drawTriangle(float x1, float y1, float x2, float y2, float x3, float y3, const Color &color);

                /// رسم مثلث فارغ
                void drawTriangleOutline(float x1, float y1, float x2, float y2, float x3, float y3, const Color &color, float thickness = 1.0f);

                /// رسم مضلع مملوء
                void drawPolygon(const std::vector<std::pair<float, float>> &points, const Color &color);

                /// رسم قوس
                void drawArc(float cx, float cy, float radius, float startAngle, float endAngle, const Color &color, float thickness = 1.0f);

                /// رسم منحنى بيزيه تربيعي
                void drawBezier(float x1, float y1, float cx, float cy, float x2, float y2, const Color &color, float thickness = 1.0f);

                /// رسم نقطة
                void drawPoint(float x, float y, const Color &color, float size = 1.0f);

                /// رسم تدرج خطي (API داخلي بـ LayoutRect)
                void drawLinearGradient(const LayoutRect &rect, const Color &startColor,
                                        const Color &endColor, bool vertical = true, float radius = 0.0f);

                /// تعيين وضع المزج
                void setBlendMode(int mode);

                /// تعيين منطقة القص (API داخلي بـ LayoutRect)
                void setClipRect(const LayoutRect *rect);

                /// الحصول على خط بحجم محدد (مع التخزين المؤقت)
                TTF_Font *getFontForSize(float fontSize);

                /// مسح ذاكرة التخزين المؤقت للنصوص
                void clearTextCache();

                // ─── البيانات ─────────────────────────────

                SDL_Renderer *sdlRenderer_ = nullptr; ///< مُصيّر SDL2
                TTF_Font *defaultFont_ = nullptr;     ///< الخط الافتراضي
                std::string defaultFontPath_;         ///< مسار الخط الافتراضي
                float defaultFontSize_ = 16.0f;       ///< حجم الخط الافتراضي
                RenderOptions desktopOptions_;        ///< خيارات العرض (SDL2-specific)
                bool initialized_ = false;            ///< هل تمت التهيئة
                // (AR) viewportWidth_/viewportHeight_ يسكنان PlatformRenderer. كانا
                //      مُعادَي التصريحِ هنا فيُظلِّلان عضوَي الأساس: يكتب سطحُ المكتبِ
                //      في نسخته ويقرأ المُرسِّمُ المشترَك نسخةَ الأساسِ الصفريّة ⇒
                //      قصُّ ما خرج عن الشاشةِ لا يعمل أصلًا في هذا المسار.

                // ─── دعم التمرير ─────────────────────────
                std::unordered_map<const IRNode *, float> scrollOffsets_; ///< إزاحة التمرير لكل عنصر قابل للتمرير
                // (AR) scrollTranslateY_ حُذِف: صار التراكمُ في المُرسِّمِ المشترَك
                //      ويُطبَّق على مستطيلِ العقدةِ فيَسري على الأحفادِ والقصِّ معًا.

                /// ذاكرة التخزين المؤقت للخطوط بأحجام مختلفة
                std::unordered_map<int, TTF_Font *> fontSizeCache_;

                /// مفتاح ذاكرة التخزين المؤقت للنصوص
                struct TextCacheKey
                {
                    std::string text;
                    int fontSize;
                    uint32_t colorRGBA;

                    bool operator==(const TextCacheKey &other) const
                    {
                        return text == other.text &&
                               fontSize == other.fontSize &&
                               colorRGBA == other.colorRGBA;
                    }
                };

                /// دالة هاش لمفتاح النص
                struct TextCacheKeyHash
                {
                    size_t operator()(const TextCacheKey &key) const
                    {
                        size_t h1 = std::hash<std::string>{}(key.text);
                        size_t h2 = std::hash<int>{}(key.fontSize);
                        size_t h3 = std::hash<uint32_t>{}(key.colorRGBA);
                        return h1 ^ (h2 << 1) ^ (h3 << 2);
                    }
                };

                /// بيانات النص المُصيّر المُخزّنة
                struct CachedTexture
                {
                    void *texture; ///< SDL_Texture*
                    int width;
                    int height;
                };

                /// ذاكرة التخزين المؤقت للنصوص المُصيّرة
                std::unordered_map<TextCacheKey, CachedTexture, TextCacheKeyHash> textCache_;

                /// ذاكرة التخزين المؤقت للصور المحمّلة
                std::unordered_map<std::string, CachedTexture> imageCache_;

                // ─── حالة التفاعل (موروثة من PlatformRenderer) ──

                // ─── محرك الانيميشن (موروث من PlatformRenderer) ─────────────────────
                // animationEngine_, initializedAnimNodes_, autoIdCounter_,
                // ensureNodeId, initNodeAnimations, curveToEasing,
                // setAnimationEngine, initializeAnimations, resetAnimationState,
                // currentNodeOpacity_ — كلها موروثة من PlatformRenderer

            public:
                /// تسجيل callback لحدث انتهاء التحريك (Desktop-specific — يتضمن نوع التحريك)
                void setOnAnimationEndCallback(
                    std::function<void(const std::string &, AnimationType)> cb)
                {
                    desktopAnimEndCallback_ = std::move(cb);
                }

            private:
                /// callback خاص بسطح المكتب يتضمن نوع التحريك
                std::function<void(const std::string &, AnimationType)> desktopAnimEndCallback_;
            };

        } // namespace desktop
    } // namespace ui
} // namespace sad

#endif // SAD_UI_DESKTOP_RENDERER_H
