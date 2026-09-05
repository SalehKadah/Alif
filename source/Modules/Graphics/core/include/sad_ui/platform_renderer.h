/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: platform_renderer.h
 * المسار: features/graphics/core/include/sad_ui/platform_renderer.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * الوصف:
 * ------
 * الواجهة المجردة الموحدة لمُصيّرات جميع المنصات.
 *
 * كل منصة (Desktop/Web/Android/iOS/macOS) تُطبق هذه الواجهة عبر
 * API الرسم الأصلي الخاص بها:
 *   - Desktop: SDL2 + OpenGL
 *   - Web: Emscripten Canvas 2D API
 *   - Android: OpenGL ES 3.0 + EGL
 *   - iOS: Metal + CoreGraphics
 *   - macOS: Metal + CoreGraphics (AppKit)
 *
 * المبدأ: كل مُصيّر يُطبق ~20 دالة رسم أساسية فقط.
 * منطق رسم الـ widgets (30+ عنصر) مشترك في renderNode().
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_PLATFORM_RENDERER_H
#define SAD_UI_PLATFORM_RENDERER_H

#include "sad_ui/ir.h"
#include "sad_ui/layout.h"
#include "sad_ui/node.h"
#include "sad_ui/state.h"
#include "sad_ui/animation.h"

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <cstdint>
#include <chrono>
#include <set>
#include <iostream>
#include <utility>

namespace sad
{
    namespace ui
    {

        // ═══════════════════════════════════════════════════════════════════════════════
        // خيارات العرض المشتركة (Common Render Options)
        // ═══════════════════════════════════════════════════════════════════════════════

        struct PlatformRenderOptions
        {
            bool antialiasing = true;
            bool vsync = true;
            float scaleFactor = 1.0f;
            bool debugBounds = false;
            bool debugLayout = false;
        };

        // ═══════════════════════════════════════════════════════════════════════════════
        // خيارات النافذة المشتركة (Common Window Options)
        // ═══════════════════════════════════════════════════════════════════════════════

        struct PlatformWindowOptions
        {
            std::string title = "\xd8\xaa\xd8\xb7\xd8\xa8\xd9\x8a\xd9\x82 \xd8\xb5"; // تطبيق ص
            int width = 800;
            int height = 600;
            bool resizable = true;
            bool fullscreen = false;
            bool borderless = false;
            bool highDPI = true;
            LayoutDirection direction = LayoutDirection::RTL;
        };

        // ═══════════════════════════════════════════════════════════════════════════════
        // حدث واجهة المستخدم (Platform UI Event)
        // ═══════════════════════════════════════════════════════════════════════════════

        enum class PlatformEventType : uint8_t
        {
            Tap,
            DoubleTap,
            LongPress,
            SwipeLeft,
            SwipeRight,
            SwipeUp,
            SwipeDown,
            DragStart,
            DragMove,
            DragEnd,
            KeyPress,
            TextInput,
            Scroll,
            Resize,
            Focus,
            Blur
        };

        struct PlatformEvent
        {
            PlatformEventType type;
            float x = 0, y = 0;
            int keyCode = 0;
            std::string text;
            float scrollDelta = 0;
        };

        // ═══════════════════════════════════════════════════════════════════════════════
        // المُصيّر المجرد (Abstract Platform Renderer)
        // ═══════════════════════════════════════════════════════════════════════════════

        /**
         * @brief واجهة مجردة لمُصيّرات جميع المنصات
         *
         * كل منصة تُطبق دوال الرسم الأساسية فقط (~20 دالة).
         * منطق رسم الـ widgets مشترك عبر renderNode().
         */
        class PlatformRenderer
        {
        public:
            virtual ~PlatformRenderer() = default;

            // ── دورة الحياة ──────────────────────────────

            /// تهيئة المُصيّر مع سطح الرسم الخاص بالمنصة
            virtual bool initialize(void *nativeSurface,
                                    const PlatformRenderOptions &options = {}) = 0;

            /// تنظيف الموارد
            virtual void shutdown() = 0;

            /// هل المُصيّر جاهز للرسم؟
            virtual bool isReady() const = 0;

            // ── تحميل الموارد ─────────────────────────

            /// تحميل خط نصي
            virtual bool loadFont(const std::string &fontPath, float fontSize = 16.0f) = 0;

            // ── حلقة الرسم ───────────────────────────

            /// بدء إطار جديد (مسح الشاشة)
            virtual void beginFrame(const Color &clearColor = {}) = 0;

            /// إنهاء الإطار (عرض على الشاشة)
            virtual void endFrame() = 0;

            // ════════════════════════════════════════════════════════════════════════
            // دوال الرسم الأساسية (يجب تطبيقها في كل منصة)
            // ════════════════════════════════════════════════════════════════════════

            /// رسم مستطيل مملوء
            virtual void drawFilledRect(float x, float y, float w, float h,
                                        const Color &color) = 0;

            /// رسم مستطيل بزوايا دائرية
            virtual void drawRoundedRect(float x, float y, float w, float h,
                                         const Color &color, float radius) = 0;

            /// رسم حدود مستطيل
            virtual void drawRectOutline(float x, float y, float w, float h,
                                         const Color &color, float thickness = 1.0f) = 0;

            /// رسم نص — يُرجع (عرض, ارتفاع) النص المرسوم
            virtual std::pair<float, float> drawText(const std::string &text,
                                                     float x, float y,
                                                     const Color &color,
                                                     float fontSize) = 0;

            /// قياس حجم نص بدون رسم
            virtual std::pair<float, float> measureText(const std::string &text,
                                                        float fontSize) = 0;

            /// رسم صورة من ملف
            virtual void drawImage(const std::string &path,
                                   float x, float y, float w, float h) = 0;

            /// رسم دائرة مملوءة
            virtual void drawCircle(float cx, float cy, float radius,
                                    const Color &color) = 0;

            /// رسم دائرة فارغة
            virtual void drawCircleOutline(float cx, float cy, float radius,
                                           const Color &color, float thickness = 1.0f) = 0;

            /// رسم خط
            virtual void drawLine(float x1, float y1, float x2, float y2,
                                  const Color &color, float thickness = 1.0f) = 0;

            /// رسم تدرج خطي
            virtual void drawLinearGradient(float x, float y, float w, float h,
                                            const Color &startColor, const Color &endColor,
                                            bool vertical = true, float radius = 0.0f) = 0;

            /// تعيين منطقة القص
            virtual void setClipRect(float x, float y, float w, float h) = 0;

            /// إلغاء القص
            virtual void clearClipRect() = 0;

            // ════════════════════════════════════════════════════════════════════════
            // رسم الـ widgets (منطق مشترك — غير مجرد)
            // ════════════════════════════════════════════════════════════════════════

            /**
             * @brief رسم شجرة واجهة كاملة
             *
             * يستخدم دوال الرسم الأساسية أعلاه لرسم كل أنواع العناصر.
             * هذه الدالة مشتركة — لا تحتاج إعادة تطبيق في كل منصة.
             */
            void render(const std::shared_ptr<IRNode> &root,
                        const std::shared_ptr<LayoutResult> &layout);

            /// حالة التفاعل
            void setInteractionState(const IRNode *hovered,
                                     const IRNode *focused,
                                     const IRNode *pressed)
            {
                hoveredNode_ = hovered;
                focusedNode_ = focused;
                pressedNode_ = pressed;
            }

            /// حالة تحرير النص (المؤشر والتحديد)
            struct TextEditInteraction
            {
                size_t cursorPos = 0;
                size_t selectionStart = 0;
                size_t selectionEnd = 0;
            };

            /// تعيين حالة تحرير النص للعقدة المركّزة
            void setTextEditState(size_t cursor, size_t selStart, size_t selEnd)
            {
                textEditInteraction_.cursorPos = cursor;
                textEditInteraction_.selectionStart = selStart;
                textEditInteraction_.selectionEnd = selEnd;
            }

            // ════════════════════════════════════════════════════════════════════════
            // دعم التحريكات والشفافية (مشترك — تستخدمه جميع المنصات)
            // ════════════════════════════════════════════════════════════════════════

            /// تعيين محرك التحريكات (يُستدعى من النافذة قبل initializeAnimations)
            void setAnimationEngine(AnimationEngine *engine)
            {
                animationEngine_ = engine;
            }

            /// تهيئة تحريكات شجرة IR من IRAnimation إلى AnimationEngine
            void initializeAnimations(const std::shared_ptr<IRNode> &root, uint32_t currentTimeMs);

            /// إعادة تعيين حالة التحريكات (لإعادة البناء)
            void resetAnimationState();

            /// شفافية العقدة الحالية (يُستخدم أثناء الرسم لتطبيق الشفافية)
            float getCurrentNodeOpacity() const { return currentNodeOpacity_; }

            /// الحصول على زاوية دوران العقدة الحالية بالدرجات
            float getCurrentNodeRotation() const { return currentNodeRotation_; }

        protected:
            /// رسم عقدة واحدة مع أبنائها (مشترك — يدعم التحريكات والشفافية)
            void renderNode(const IRNode &node, const LayoutResult &layout);

            // ════════════════════════════════════════════════════════════════════════
            // (AR) تحميلاتٌ تأخذ LayoutRect — راحةٌ نصّيّةٌ فوقَ الأوّليّاتِ نفسِها،
            //      لا واجهةٌ جديدةٌ تُطبّقها المنصّات. وُجِدت نسخةٌ منها في مُرسِّم
            //      سطحِ المكتبِ وحدَه فكانت شوكةُ الرسمِ تُكتَب هناك؛ رفعُها هنا
            //      شرطُ توحيدِ المسار.
            // ════════════════════════════════════════════════════════════════════════

            void drawFilledRect(const LayoutRect &r, const Color &color)
            {
                drawFilledRect(r.x, r.y, r.width, r.height, color);
            }

            void drawRoundedRect(const LayoutRect &r, const Color &color, float radius)
            {
                drawRoundedRect(r.x, r.y, r.width, r.height, color, radius);
            }

            void drawRectOutline(const LayoutRect &r, const Color &color, float thickness = 1.0f)
            {
                drawRectOutline(r.x, r.y, r.width, r.height, color, thickness);
            }

            void drawLinearGradient(const LayoutRect &r, const Color &startColor,
                                    const Color &endColor, bool vertical = true,
                                    float radius = 0.0f)
            {
                drawLinearGradient(r.x, r.y, r.width, r.height, startColor, endColor,
                                   vertical, radius);
            }

            void drawImage(const std::string &path, const LayoutRect &r)
            {
                drawImage(path, r.x, r.y, r.width, r.height);
            }

            void setClipRect(const LayoutRect *r)
            {
                if (r)
                    setClipRect(r->x, r->y, r->width, r->height);
                else
                    clearClipRect();
            }

            /**
             * @brief (AR) ظلٌّ ناعمٌ متعدّد الطبقات حولَ مستطيل
             * @brief (EN) Multi-layer soft shadow around a rect
             *
             * مبنيٌّ على `drawRoundedRect` وحدَها ⇒ يعمل على كلِّ منصّةٍ بلا تخصيص.
             */
            void drawSoftShadow(const LayoutRect &rect, float radius, float blur,
                                float offsetX, float offsetY, const Color &color);

            /**
             * @brief (AR) إزاحةُ تمريرٍ تملكها المنصّةُ للعقدة (عجلةُ الفأرة مثلًا)
             * @brief (EN) Platform-owned scroll offset for a node (e.g. mouse wheel)
             *
             * الافتراضيُّ صفر: منصّةٌ بلا تمريرٍ تفاعليٍّ لا تحتاج تخصيصًا. القيمةُ
             * موجبةٌ نزولًا كإزاحةِ رسمٍ تُضاف إلى مستطيلاتِ الأبناء.
             */
            virtual float platformScrollOffset(const IRNode &node) const
            {
                (void)node;
                return 0.0f;
            }

            /// تهيئة تحريكات عقدة فردية
            void initNodeAnimations(const IRNode &node, uint32_t startMs);

            /// التأكد من وجود معرّف فريد للعقدة
            void ensureNodeId(IRNode &node);

            /// تحويل منحنى تسهيل IR إلى نوع AnimationEngine
            EasingType curveToEasing(EasingCurve curve) const;

            /// تطبيق شفافية العقدة الحالية على لون — تُستخدم في دوال الرسم
            Color applyOpacity(const Color &c) const
            {
                return {c.r, c.g, c.b, c.a * currentNodeOpacity_};
            }

            /// تطبيق تحويل الدوران حول مركز العنصر (بالدرجات)
            /// تُستدعى تلقائياً من renderNode() قبل رسم المحتوى
            /// كل منصة تُنفّذها بطريقتها (SDL_RenderCopyEx, CoreGraphics, Canvas2D, OpenGL)
            virtual void pushRotation(float centerX, float centerY, float angleDegrees)
            {
                (void)centerX;
                (void)centerY;
                (void)angleDegrees;
            }

            /// إلغاء تحويل الدوران — تُستدعى بعد رسم المحتوى
            virtual void popRotation() {}

            /// تطبيق تحويل التكبير/التصغير حول مركز العنصر
            /// تُستدعى تلقائياً من renderNode() عند وجود scale != 1.0
            virtual void pushScale(float centerX, float centerY, float scaleX, float scaleY)
            {
                (void)centerX;
                (void)centerY;
                (void)scaleX;
                (void)scaleY;
            }

            /// إلغاء تحويل التكبير — تُستدعى بعد رسم المحتوى
            virtual void popScale() {}

            /// دوال مساعدة مشتركة
            bool isArabicText(const std::string &text) const;
            size_t utf8Length(const std::string &text) const;

            PlatformRenderOptions options_;
            const IRNode *hoveredNode_ = nullptr;
            const IRNode *focusedNode_ = nullptr;
            const IRNode *pressedNode_ = nullptr;
            TextEditInteraction textEditInteraction_; ///< حالة تحرير النص

            /// أبعاد Viewport للقص (Culling) — 0 يعني بدون قص
            int viewportWidth_ = 0;
            int viewportHeight_ = 0;

            /// (AR) إزاحةُ التمريرِ المتراكمةُ عبرَ الحاوياتِ المتداخلة — تُطبَّق على
            ///      مستطيلِ العقدةِ في مطلعِ renderNode فتَسري على الأحفادِ أيضًا.
            ///      (تعديلُ مستطيلِ الابنِ وحدَه كان يترك الأحفادَ بلا إزاحة.)
            float scrollTranslateX_ = 0.0f;
            float scrollTranslateY_ = 0.0f;

            /// محرك التحريكات (مشترك — تملكه النافذة، المُصيّر يشير إليه)
            AnimationEngine *animationEngine_ = nullptr;

            /// شفافية العقدة الحالية (مشتركة بين جميع المنصات)
            float currentNodeOpacity_ = 1.0f;

            /// زاوية دوران العقدة الحالية بالدرجات (مشتركة بين جميع المنصات)
            float currentNodeRotation_ = 0.0f;

            /// عداد معرّفات تلقائية للعقد بدون معرّف
            int autoIdCounter_ = 0;

            /// مجموعة العقد التي تم تهيئة تحريكاتها (لمنع التكرار)
            std::set<std::string> initializedAnimNodes_;

            /// callback لانتهاء التحريك
            std::function<void(const std::string &)> onAnimationEndCallback_;
        };

        // ═══════════════════════════════════════════════════════════════════════════════
        // النافذة/السطح المجرد (Abstract Platform Window)
        // ═══════════════════════════════════════════════════════════════════════════════

        /**
         * @brief واجهة مجردة لنافذة/سطح العرض على كل منصة
         */
        class PlatformWindow
        {
        public:
            virtual ~PlatformWindow() = default;

            virtual bool create(const PlatformWindowOptions &options = {}) = 0;
            virtual void destroy() = 0;
            virtual bool isOpen() const = 0;

            virtual void setContent(std::shared_ptr<IRNode> root) = 0;
            virtual void setStateStore(std::shared_ptr<StateStore> store) = 0;

            /// تشغيل حلقة الأحداث (تحجب حتى الإغلاق)
            virtual void run() = 0;

            /// تنفيذ إطار واحد (للمنصات غير المتزامنة مثل WASM)
            virtual bool runOneFrame() = 0;

            /// طلب إعادة رسم
            virtual void invalidate() = 0;

            virtual int getWidth() const = 0;
            virtual int getHeight() const = 0;

            /**
             * @brief (AR) إغلاق النافذة — يطلب إنهاء حلقة الأحداث
             * @brief (EN) Close the window — requests event loop termination
             *
             * التطبيق الافتراضي: لا شيء (المنصة تتحكم بالإغلاق).
             * Desktop يُرسل SDL_QUIT، الويب يخفي Canvas، إلخ.
             */
            virtual void close() {}

            /**
             * @brief (AR) تعيين عنوان النافذة
             * @brief (EN) Set the window title
             *
             * التطبيق الافتراضي: لا شيء (بعض المنصات لا تدعم عناوين).
             * Desktop يستدعي SDL_SetWindowTitle، الويب يغيّر document.title.
             */
            virtual void setTitle(const std::string &title) { (void)title; }

            /**
             * @brief (AR) تبديل وضع ملء الشاشة
             * @brief (EN) Toggle fullscreen mode
             *
             * @param fullscreen true لملء الشاشة, false للعودة
             * التطبيق الافتراضي: لا شيء.
             */
            virtual void setFullscreen(bool fullscreen) { (void)fullscreen; }

            /**
             * @brief (AR) عرض رسالة تنبيه بسيطة
             * @brief (EN) Show a simple alert message box
             *
             * التطبيق الافتراضي: طباعة على وحدة التحكم.
             */
            virtual void showAlertDialog(const std::string &title, const std::string &message)
            {
                std::cout << "[" << title << "] " << message << std::endl;
            }

            /**
             * @brief (AR) عرض حوار تأكيد (موافق/إلغاء)
             * @brief (EN) Show a confirmation dialog (OK/Cancel)
             *
             * التطبيق الافتراضي: طباعة + إرجاع true.
             * @return true إذا أكد المستخدم
             */
            virtual bool showConfirmDialog(const std::string &title, const std::string &message)
            {
                std::cout << "[" << title << "] " << message << std::endl;
                return true;
            }

            /**
             * @brief (AR) الحصول على نسبة تكبير الشاشة
             * @brief (EN) Get the display scale factor
             *
             * التطبيق الافتراضي: 1.0 (بدون تكبير).
             */
            virtual float getDisplayScale() const { return 1.0f; }

            /**
             * @brief (AR) التقاط لقطة شاشة وحفظها في ملف
             * @brief (EN) Take a screenshot and save it to a file
             *
             * التطبيق الافتراضي: false (غير مدعوم).
             * @param outputPath مسار ملف الخرج
             * @return true إذا نجح الحفظ
             */
            virtual bool takeScreenshot(const std::string &outputPath)
            {
                (void)outputPath;
                return false;
            }

            // ─── المؤقتات ──────────────────────────────────────────────────────

            /**
             * @brief (AR) الحصول على الوقت الحالي بالمللي ثانية
             * @brief (EN) Get current time in milliseconds (for timers)
             *
             * التطبيق الافتراضي: chrono steady_clock.
             */
            virtual uint32_t getTicksMs() const
            {
                auto now = std::chrono::steady_clock::now();
                return static_cast<uint32_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch())
                        .count());
            }

            // ─── الحافظة ───────────────────────────────────────────────────────

            /**
             * @brief (AR) نسخ نص إلى الحافظة
             * @brief (EN) Copy text to clipboard
             *
             * التطبيق الافتراضي: false (غير مدعوم).
             */
            virtual bool copyToClipboard(const std::string &text)
            {
                (void)text;
                return false;
            }

            /**
             * @brief (AR) لصق نص من الحافظة
             * @brief (EN) Paste text from clipboard
             *
             * التطبيق الافتراضي: نص فارغ.
             */
            virtual std::string pasteFromClipboard()
            {
                return "";
            }

            // ─── الماوس ────────────────────────────────────────────────────────

            /**
             * @brief (AR) الحصول على موقع الماوس الحالي
             * @brief (EN) Get current mouse position
             *
             * التطبيق الافتراضي: {0, 0}.
             */
            virtual std::pair<int, int> getMousePosition() const
            {
                return {0, 0};
            }

            // ─── المؤشر ────────────────────────────────────────────────────────

            /**
             * @brief (AR) تغيير شكل المؤشر
             * @brief (EN) Set cursor style (hand, wait, text, move, arrow, no, resize)
             *
             * التطبيق الافتراضي: لا شيء.
             */
            virtual void setCursorStyle(const std::string &style)
            {
                (void)style;
            }

            /**
             * @brief (AR) إظهار/إخفاء المؤشر
             * @brief (EN) Show or hide the cursor
             *
             * التطبيق الافتراضي: لا شيء.
             */
            virtual void showCursor(bool visible)
            {
                (void)visible;
            }

            // ─── معلومات النظام ─────────────────────────────────────────────────

            /**
             * @brief (AR) عدد أنوية المعالج
             * @brief (EN) Get CPU core count
             *
             * التطبيق الافتراضي: 1.
             */
            virtual int getCPUCount() const { return 1; }

            /**
             * @brief (AR) حجم ذاكرة النظام بالميغابايت
             * @brief (EN) Get system RAM size in MB
             *
             * التطبيق الافتراضي: 0.
             */
            virtual int getRAMSizeMB() const { return 0; }

            // ─── معلومات الشاشة ─────────────────────────────────────────────────

            /**
             * @brief (AR) عرض الشاشة الكامل بالبكسل
             * @brief (EN) Get full screen width in pixels
             *
             * التطبيق الافتراضي: 1920.
             */
            virtual int getScreenWidth() const { return 1920; }

            /**
             * @brief (AR) ارتفاع الشاشة الكامل بالبكسل
             * @brief (EN) Get full screen height in pixels
             *
             * التطبيق الافتراضي: 1080.
             */
            virtual int getScreenHeight() const { return 1080; }

            /**
             * @brief (AR) كثافة البكسلات (DPI)
             * @brief (EN) Get pixel density (DPI)
             *
             * التطبيق الافتراضي: 96.0.
             */
            virtual double getPixelDensity() const { return 96.0; }

            /**
             * @brief (AR) نسبة تكبير خط النظام (مبنية على DPI)
             * @brief (EN) Get system font scale factor (based on DPI)
             *
             * التطبيق الافتراضي: 1.0.
             */
            virtual double getSystemFontScale() const { return 1.0; }

            // ─── الاهتزاز ──────────────────────────────────────────────────────

            /**
             * @brief (AR) اهتزاز الجهاز (haptic feedback)
             * @brief (EN) Vibrate the device (haptic feedback)
             *
             * التطبيق الافتراضي: لا شيء.
             * @param durationMs مدة الاهتزاز بالمللي ثانية
             */
            virtual void vibrate(int durationMs) { (void)durationMs; }

            // ─── البطارية ──────────────────────────────────────────────────────

            /**
             * @brief (AR) مستوى شحن البطارية (0-100 أو -1 إذا غير متاح)
             * @brief (EN) Get battery level (0-100 or -1 if unavailable)
             *
             * التطبيق الافتراضي: -1 (غير متاح).
             */
            virtual int getBatteryLevel() const { return -1; }

            /**
             * @brief (AR) هل الجهاز يشحن؟
             * @brief (EN) Is the device charging?
             *
             * التطبيق الافتراضي: false.
             */
            virtual bool isBatteryCharging() const { return false; }

            // ─── الصوت ─────────────────────────────────────────────────────────

            /**
             * @brief (AR) تهيئة نظام الصوت
             * @brief (EN) Initialize the audio subsystem
             *
             * التطبيق الافتراضي: لا شيء.
             */
            virtual void initAudio() {}

            /**
             * @brief (AR) تنظيف نظام الصوت
             * @brief (EN) Cleanup audio subsystem
             *
             * التطبيق الافتراضي: لا شيء.
             */
            virtual void cleanupAudio() {}

            /**
             * @brief (AR) تشغيل ملف صوتي
             * @brief (EN) Play an audio file
             *
             * @param filePath مسار الملف الصوتي (WAV)
             * @param loop تكرار لا نهائي؟
             * @return معرف الصوت أو -1 عند الفشل
             */
            virtual int playSound(const std::string &filePath, bool loop = false)
            {
                (void)filePath;
                (void)loop;
                return -1;
            }

            /**
             * @brief (AR) إيقاف صوت محدد
             * @brief (EN) Stop a specific sound by ID
             */
            virtual void stopSound(int soundId)
            {
                (void)soundId;
            }

            /**
             * @brief (AR) إيقاف جميع الأصوات
             * @brief (EN) Stop all currently playing sounds
             */
            virtual void stopAllSounds() {}

            /**
             * @brief (AR) تعيين مستوى الصوت الرئيسي (0.0 - 1.0)
             * @brief (EN) Set master volume (0.0 - 1.0)
             */
            virtual void setVolume(float volume) { (void)volume; }

            /**
             * @brief (AR) الحصول على مستوى الصوت الرئيسي
             * @brief (EN) Get master volume
             */
            virtual float getVolume() const { return 1.0f; }

            void setEventHandler(std::function<void(const PlatformEvent &)> handler)
            {
                eventHandler_ = std::move(handler);
            }

            void setOnEventCallback(
                std::function<void(IREventType, const std::string &, const IRNode *, const EventData &)> cb)
            {
                onEventCallback_ = std::move(cb);
            }

            /// الحصول على المُصيّر الخاص بهذه النافذة
            virtual PlatformRenderer *getRenderer() = 0;

            /**
             * @brief (AR) تطبيق رقع جزئية على الشجرة الحية — بدلاً من setContent الكامل
             * @brief (EN) Apply partial patches to the live tree — instead of full setContent
             *
             * يُستدعى من Reconciler بعد diff() + patch() لإبلاغ النافذة
             * بأن الشجرة تم تعديلها موضعياً وتحتاج إعادة تخطيط/رسم فقط.
             *
             * التطبيق الافتراضي يطلب إعادة رسم فقط (invalidate).
             * كل منصة تستطيع تقديم تطبيق محسّن يعيد تهيئة التحريكات إذا لزم.
             *
             * @param patchedNodes عدد العقد المُعدّلة
             * @param needsRelayout هل تحتاج إعادة تخطيط (تغيّر أبعاد/أبناء)
             */
            virtual void applyPatches(size_t patchedNodes, bool needsRelayout = true)
            {
                // (AR) التطبيق الافتراضي: إعادة رسم فقط
                // (EN) Default: just request redraw
                (void)patchedNodes;
                (void)needsRelayout;
                invalidate();
            }

            /// محرك الانيميشن
            AnimationEngine &getAnimationEngine() { return animationEngine_; }

            /**
             * @brief (AR) تعيين محتوى جديد مع انتقال بصري (Page Transition)
             * @brief (EN) Set new content with visual page transition
             *
             * يغلّف المحتوى الجديد بتحريك دخول تلقائي حسب نوع الانتقال.
             * أنواع مدعومة: "ظهور"، "انزلاق_يمين"، "انزلاق_يسار"، "انزلاق_أعلى"، "انزلاق_أسفل"، "تكبير"
             *
             * @param root شجرة IR الجديدة
             * @param transitionType اسم نوع الانتقال (عربي أو إنجليزي)
             * @param durationSec مدة الانتقال بالثواني (افتراضي 0.3)
             */
            void setContentWithTransition(
                std::shared_ptr<IRNode> root,
                const std::string &transitionType,
                float durationSec = 0.3f);

            /**
             * @brief (AR) تعيين محتوى جديد مع انتقال دخول + خروج
             * @brief (EN) Set new content with entry + exit page transitions
             *
             * يُطبّق تحريك خروج على المحتوى الحالي، ثم بعد انتهائه
             * يُبدّل للمحتوى الجديد مع تحريك دخول.
             *
             * @param root شجرة IR الجديدة
             * @param entryTransition نوع تحريك الدخول (على المحتوى الجديد)
             * @param exitTransition نوع تحريك الخروج (على المحتوى الحالي)
             * @param durationSec مدة كل تحريك بالثواني
             */
            void setContentWithExitTransition(
                std::shared_ptr<IRNode> root,
                const std::string &entryTransition,
                const std::string &exitTransition,
                float durationSec = 0.3f);

            /**
             * @brief (AR) تحقق وتبديل المحتوى المُعلّق — يُستدعى من حلقة الأحداث
             * @brief (EN) Check and swap pending content — called from event loop
             *
             * يُستدعى في كل frame لتبديل المحتوى بعد انتهاء تحريك الخروج.
             * @return true إذا تم تبديل المحتوى
             */
            bool checkPendingTransition();

            /// الحصول على الوقت الحالي بالمللي ثانية (محمول لكل المنصات)
            static uint32_t getCurrentTimeMs()
            {
                auto now = std::chrono::steady_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch());
                return static_cast<uint32_t>(ms.count());
            }

        protected:
            std::function<void(const PlatformEvent &)> eventHandler_;
            std::function<void(IREventType, const std::string &, const IRNode *, const EventData &)> onEventCallback_;
            AnimationEngine animationEngine_;

            /// (AR) المحتوى المُعلّق للتبديل بعد انتهاء تحريك الخروج
            std::shared_ptr<IRNode> pendingContent_;
            /// (AR) نوع تحريك الدخول المُعلّق
            std::string pendingEntryTransition_;
            /// (AR) مدة تحريك الدخول المُعلّق (ثوانٍ)
            float pendingEntryDuration_ = 0.3f;
            /// (AR) وقت بدء تحريك الخروج (مللي ثانية) — لكشف الانتهاء
            uint32_t exitTransitionStartMs_ = 0;
            /// (AR) مدة تحريك الخروج (مللي ثانية)
            uint32_t exitTransitionDurationMs_ = 0;
        };

        // ═══════════════════════════════════════════════════════════════════════════════
        // مصنع المنصات (Platform Factory)
        // ═══════════════════════════════════════════════════════════════════════════════

        /**
         * @brief ينشئ المُصيّر والنافذة المناسبين للمنصة المستهدفة
         */
        class PlatformFactory
        {
        public:
            /// إنشاء نافذة للمنصة المحددة
            static std::unique_ptr<PlatformWindow> createWindow(TargetPlatform platform);

            /// إنشاء مُصيّر للمنصة المحددة
            static std::unique_ptr<PlatformRenderer> createRenderer(TargetPlatform platform);

            /// الكشف التلقائي عن المنصة الحالية
            static TargetPlatform detectPlatform();
        };

    } // namespace ui
} // namespace sad

#endif // SAD_UI_PLATFORM_RENDERER_H
