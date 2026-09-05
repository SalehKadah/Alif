/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: window.h
 * المسار: features/graphics/backends/desktop/include/sad_ui/desktop/window.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * الوصف:
 * ------
 * إدارة نافذة سطح المكتب (Desktop Window Manager).
 *
 * هذا الملف يُعرّف النافذة الرئيسية للتطبيق على سطح المكتب
 * ويُدير حلقة الأحداث (Event Loop) والتفاعل مع المستخدم.
 *
 * المسؤوليات:
 * ────────────
 * 1. إنشاء نافذة SDL2 مع إعدادات OpenGL
 * 2. حلقة الأحداث — استقبال ومعالجة أحداث SDL2
 * 3. تحويل أحداث SDL2 إلى أحداث SadUI
 * 4. معالجة تغيير حجم النافذة (resize)
 * 5. إدارة مفاتيح الاختصار
 *
 * حلقة الحياة:
 * ─────────────
 *   create() → show() → [حلقة الأحداث] → close() → destroy()
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_DESKTOP_WINDOW_H
#define SAD_UI_DESKTOP_WINDOW_H

#include "sad_ui/ir.h"
#include "sad_ui/layout.h"
#include "sad_ui/state.h"
#include "sad_ui/animation.h"
#include "sad_ui/reconciler.h"
#include "sad_ui/touch_processor.h"
#include "sad_ui/keyboard_processor.h"
#include "sad_ui/mouse_processor.h"
#include "renderer.h"

#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <cstdint>
#include <cmath>
#include <unordered_map>
#include <atomic>

// تصريحات أمامية
struct SDL_Window;
struct SDL_Renderer;

namespace sad
{
    namespace ui
    {
        namespace desktop
        {

            // تصريحات أمامية لنظام التحرير
            class TextEditStateMap;

            // ═══════════════════════════════════════════════════════════════════════════════
            // خيارات النافذة (Window Options)
            // ═══════════════════════════════════════════════════════════════════════════════

            /**
             * @brief خيارات إنشاء النافذة
             */
            struct WindowOptions
            {
                std::string title = "تطبيق ص";                    ///< عنوان النافذة
                int width = 800;                                  ///< العرض الأولي (بكسل)
                int height = 600;                                 ///< الارتفاع الأولي (بكسل)
                bool resizable = true;                            ///< هل يمكن تغيير الحجم
                bool fullscreen = false;                          ///< ملء الشاشة
                bool borderless = false;                          ///< بدون إطار
                bool highDPI = true;                              ///< دعم الشاشات عالية الكثافة
                LayoutDirection direction = LayoutDirection::RTL; ///< اتجاه التخطيط
            };

            // ═══════════════════════════════════════════════════════════════════════════════
            // حدث واجهة المستخدم (UI Event)
            // ═══════════════════════════════════════════════════════════════════════════════

            /**
             * @brief أنواع أحداث واجهة المستخدم
             */
            enum class UIEventType : uint8_t
            {
                Tap,        ///< نقر (click)
                DoubleTap,  ///< نقر مزدوج
                LongPress,  ///< ضغط مطول
                SwipeLeft,  ///< سحب لليسار
                SwipeRight, ///< سحب لليمين
                SwipeUp,    ///< سحب للأعلى
                SwipeDown,  ///< سحب للأسفل
                DragStart,  ///< بداية سحب
                DragMove,   ///< حركة أثناء السحب
                DragEnd,    ///< نهاية السحب
                KeyPress,   ///< ضغط مفتاح
                TextInput,  ///< إدخال نصي
                Scroll,     ///< تمرير
                Resize,     ///< تغيير حجم النافذة
                Focus,      ///< حصول على التركيز
                Blur        ///< فقدان التركيز
            };

            /**
             * @brief حدث واجهة مستخدم واحد
             */
            struct UIEvent
            {
                UIEventType type;      ///< نوع الحدث
                float x = 0;           ///< موقع X (للأحداث المكانية)
                float y = 0;           ///< موقع Y
                int keyCode = 0;       ///< رمز المفتاح (للوحة المفاتيح)
                std::string text;      ///< نص مُدخل
                float scrollDelta = 0; ///< مقدار التمرير
            };

            // ═══════════════════════════════════════════════════════════════════════════════
            // نافذة سطح المكتب (Desktop Window)
            // ═══════════════════════════════════════════════════════════════════════════════

            /**
             * @brief النافذة الرئيسية لتطبيق سطح المكتب
             *
             * تُنشئ نافذة SDL2، تُدير حلقة الأحداث، وتربط بين:
             * - شجرة IR (ماذا نعرض)
             * - محرك التخطيط (أين نعرض)
             * - المُصيّر (كيف نرسم)
             * - مخزن الحالة (البيانات التفاعلية)
             *
             * الاستخدام:
             * @code
             *   DesktopWindow window;
             *   window.create(options);
             *   window.setContent(irRoot);
             *
             *   // حلقة الأحداث
             *   window.run();  // ← تعمل حتى يُغلق المستخدم النافذة
             *
             *   window.destroy();
             * @endcode
             */
            class DesktopWindow : public PlatformWindow
            {
            public:
                DesktopWindow();
                ~DesktopWindow() override;

                /**
                 * @brief إنشاء النافذة (من واجهة PlatformWindow)
                 */
                bool create(const PlatformWindowOptions &options) override;

                /**
                 * @brief إنشاء النافذة
                 *
                 * يُنشئ نافذة SDL2 مع إعدادات OpenGL.
                 *
                 * @param options خيارات النافذة
                 * @return true إذا نجح الإنشاء
                 */
                bool create(const WindowOptions &options);

                /// تدمير النافذة وتنظيف الموارد
                void destroy() override;

                /**
                 * @brief تعيين محتوى الواجهة
                 *
                 * @param root جذر شجرة IR
                 */
                void setContent(std::shared_ptr<IRNode> root) override;

                /**
                 * @brief (AR) تطبيق تحديث جزئي بدون استبدال الشجرة بالكامل
                 * @brief (EN) Apply partial update without full tree replacement
                 *
                 * (AR) بدلاً من استبدال الشجرة وإعادة تهيئة كل التحريكات والتخطيط،
                 *      هذه الدالة تُعلّم فقط أن إعادة رسم مطلوبة.
                 *      التحريكات الموجودة تبقى نشطة — فقط الجديدة تُهيّأ.
                 *
                 * @param patchedNodes عدد العقد المتأثرة (للتشخيص)
                 * @param needsRelayout هل التغييرات تتطلب إعادة تخطيط
                 */
                void applyPatches(size_t patchedNodes, bool needsRelayout = true) override;

                /**
                 * @brief تعيين مخزن الحالة
                 *
                 * @param store مخزن الحالة التفاعلي
                 */
                void setStateStore(std::shared_ptr<StateStore> store) override;

                /**
                 * @brief تشغيل حلقة الأحداث الرئيسية
                 *
                 * هذه الدالة تعمل حتى يُغلق المستخدم النافذة.
                 * في كل إطار:
                 * 1. معالجة أحداث SDL2
                 * 2. تحديث التخطيط (إذا تغيّرت الحالة)
                 * 3. إعادة الرسم
                 */
                void run() override;

                /**
                 * @brief طلب إعادة رسم الواجهة
                 *
                 * يُستدعى عند تغيّر الحالة أو حجم النافذة.
                 */
                void invalidate() override { needsRedraw_ = true; }

                /// محرك الانيميشن — للتحكم في التحريكات من خارج النافذة
                // يُستخدم المحرك الموروث من PlatformWindow

                /// هل النافذة مفتوحة؟
                bool isOpen() const override { return isOpen_; }

                /// الحصول على عرض النافذة الحالي
                int getWidth() const override { return width_; }

                /// الحصول على ارتفاع النافذة الحالي
                int getHeight() const override { return height_; }

                /// الحصول على مؤشر نافذة SDL (لتعيين العنوان وغيره)
                SDL_Window *getSDLWindow() const { return sdlWindow_; }

                /// الحصول على المُصيّر
                PlatformRenderer *getRenderer() override { return renderer_.get(); }

                /**
                 * @brief تسجيل معالج أحداث سطح المكتب
                 *
                 * @param handler دالة تُستدعى عند أي حدث واجهة (UIEvent محلي)
                 */
                void setDesktopEventHandler(std::function<void(const UIEvent &)> handler)
                {
                    desktopEventHandler_ = std::move(handler);
                }

                /**
                 * @brief تعيين callback للمفسر
                 *
                 * يُستدعى عند أي حدث للسماح للمفسر بمعالجة الأحداث.
                 * يستخدم الدالة الموروثة من PlatformWindow.
                 *
                 * @param callback دالة تستقبل (اسم_الحدث، معرّف_العنصر، عقدة_IR)
                 */
                void setOnEventCallback(
                    std::function<void(IREventType, const std::string &, const IRNode *, const EventData &)> callback)
                {
                    onEventCallback_ = std::move(callback);
                }

                /**
                 * @brief تنفيذ إطار واحد فقط
                 *
                 * مفيد لـ WASM و Emscripten حيث لا نستخدم حلقة لانهائية.
                 *
                 * @return true إذا كانت النافذة لا تزال مفتوحة
                 */
                bool runOneFrame() override;

                /**
                 * @brief الحصول على مقبض النافذة الأصلي
                 *
                 * للتكامل مع مكتبات خارجية تحتاج SDL_Window* مباشرة.
                 *
                 * @return مؤشر SDL_Window
                 */
                void *getNativeHandle() const;

                // ─── تطبيقات واجهة PlatformWindow الجديدة ───

                /// (AR) إغلاق النافذة — يُرسل حدث SDL_QUIT
                void close() override;

                /// (AR) تعيين عنوان النافذة عبر SDL
                void setTitle(const std::string &title) override;

                /// (AR) تبديل وضع ملء الشاشة عبر SDL
                void setFullscreen(bool fullscreen) override;

                /// (AR) عرض رسالة تنبيه عبر SDL MessageBox
                void showAlertDialog(const std::string &title, const std::string &message) override;

                /// (AR) عرض حوار تأكيد عبر SDL MessageBox
                bool showConfirmDialog(const std::string &title, const std::string &message) override;

                /// (AR) الحصول على نسبة تكبير الشاشة عبر SDL
                float getDisplayScale() const override;

                /// (AR) التقاط لقطة شاشة وحفظها في ملف BMP
                bool takeScreenshot(const std::string &outputPath) override;

                /// (AR) الحصول على الوقت الحالي بالمللي ثانية عبر SDL_GetTicks
                uint32_t getTicksMs() const override;

                /// (AR) نسخ نص إلى الحافظة عبر SDL
                bool copyToClipboard(const std::string &text) override;

                /// (AR) لصق نص من الحافظة عبر SDL
                std::string pasteFromClipboard() override;

                /// (AR) الحصول على موقع الماوس عبر SDL
                std::pair<int, int> getMousePosition() const override;

                /// (AR) تغيير شكل المؤشر عبر SDL
                void setCursorStyle(const std::string &style) override;

                /// (AR) إظهار/إخفاء المؤشر عبر SDL
                void showCursor(bool visible) override;

                /// (AR) عدد أنوية المعالج عبر SDL
                int getCPUCount() const override;

                /// (AR) حجم ذاكرة النظام عبر SDL
                int getRAMSizeMB() const override;

                /// (AR) عرض الشاشة الكامل عبر SDL
                int getScreenWidth() const override;

                /// (AR) ارتفاع الشاشة الكامل عبر SDL
                int getScreenHeight() const override;

                /// (AR) كثافة البكسلات عبر SDL
                double getPixelDensity() const override;

                /// (AR) نسبة تكبير خط النظام عبر DPI
                double getSystemFontScale() const override;

                /// (AR) اهتزاز الجهاز عبر SDL Haptic
                void vibrate(int durationMs) override;

                /// (AR) مستوى شحن البطارية عبر SDL
                int getBatteryLevel() const override;

                /// (AR) هل الجهاز يشحن عبر SDL
                bool isBatteryCharging() const override;

                // ─── نظام الصوت (Audio System — SDL2) ────────────────────────

                /// (AR) تهيئة نظام الصوت عبر SDL
                void initAudio() override;

                /// (AR) تنظيف نظام الصوت عبر SDL
                void cleanupAudio() override;

                /// (AR) تشغيل ملف صوتي عبر SDL
                int playSound(const std::string &filePath, bool loop = false) override;

                /// (AR) إيقاف صوت محدد عبر SDL
                void stopSound(int soundId) override;

                /// (AR) إيقاف جميع الأصوات عبر SDL
                void stopAllSounds() override;

                /// (AR) تعيين مستوى الصوت الرئيسي
                void setVolume(float volume) override;

                /// (AR) الحصول على مستوى الصوت الرئيسي
                float getVolume() const override;

            private:
                // ─── دوال داخلية ────────────────────────

                /// معالجة أحداث SDL2
                void processEvents();

                /// تحديث التخطيط
                void updateLayout();

                /// رسم الإطار
                void renderFrame();

                /// البحث عن العنصر تحت نقطة محددة (hit testing)
                const IRNode *hitTest(float x, float y) const;

                /// دالة مساعدة لـ hitTest — تبحث بشكل تكراري
                const IRNode *hitTestNode(
                    const IRNode *node,
                    const LayoutResult *layout,
                    float x,
                    float y) const;

                // ─── إطلاق الأحداث (Event Dispatch) ──────

                /// إطلاق حدث على عقدة — يعالج EventData والفقاعات
                void fireEvent(IREventType type, const std::string &expression,
                               const IRNode *node, const EventData &data = EventData{});

                /// معالجة أحداث مخصصة (Custom Events)
                void dispatchCustomEvent(const std::string &eventName,
                                         const IRNode *node, const EventData &data = EventData{});

                // ─── البيانات ─────────────────────────────

                SDL_Window *sdlWindow_ = nullptr;            ///< نافذة SDL2
                SDL_Renderer *sdlRenderer_ = nullptr;        ///< مُصيّر SDL2
                std::unique_ptr<DesktopRenderer> renderer_;  ///< مُصيّر SadUI
                std::unique_ptr<LayoutEngine> layoutEngine_; ///< محرك التخطيط

                std::shared_ptr<IRNode> contentRoot_;        ///< جذر المحتوى
                std::shared_ptr<LayoutResult> layoutResult_; ///< نتائج التخطيط
                std::shared_ptr<StateStore> stateStore_;     ///< مخزن الحالة

                std::function<void(const UIEvent &)> desktopEventHandler_; ///< معالج الأحداث المحلي (UIEvent)

                /// callback للمفسر — يُستخدم onEventCallback_ الموروث من PlatformWindow

                WindowOptions options_;   ///< خيارات النافذة
                int width_ = 0;           ///< العرض الحالي
                int height_ = 0;          ///< الارتفاع الحالي
                bool isOpen_ = false;     ///< هل النافذة مفتوحة
                bool needsRedraw_ = true; ///< هل تحتاج إعادة رسم

                // ─── نظام Focus ─────────────────
                const IRNode *focusedNode_ = nullptr; ///< العنصر المركّز (للإدخال)
                float mouseX_ = 0;                    ///< موقع المؤشر X (يُحدّث أيضاً من mouseProcessor_)
                float mouseY_ = 0;                    ///< موقع المؤشر Y

                // ─── نظام تحرير النصوص المتقدم ────────────────
                std::unique_ptr<TextEditStateMap> textEditStates_; ///< حالات التحرير لكل حقل نص

                /// معالجة اختصارات تحرير النصوص (Ctrl+C/V/X/Z/Y/A, Arrows, Home/End, Delete)
                void handleTextEditKeyEvent(int keysym, uint16_t mod);

                /// هل العقدة حقل إدخال نص؟
                static bool isTextInputNodeType(const IRNode *node);

                /// معالجة مفتاح تحرير عبر المعالج الموحد (bridge)
                bool handleTextEditKeyViaProcessor(const IRNode *node,
                                                   UnifiedKeyCode keyCode,
                                                   const KeyModifiers &mods);

                /// معالجة إدخال نص عبر المعالج الموحد (bridge)
                bool handleTextInputViaProcessor(const IRNode *node,
                                                 const std::string &text);

                /// البحث عن مستطيل تخطيط عقدة معينة
                LayoutRect findNodeRect(const IRNode *target) const;
                LayoutRect findNodeRectHelper(const IRNode *node, const LayoutResult *layout,
                                              const IRNode *target) const;

                /// البحث عن أقرب ScrollView/LazyColumn يحتوي على نقطة (x,y)
                struct ScrollableHit
                {
                    const IRNode *node = nullptr;
                    const LayoutResult *layout = nullptr;
                };
                ScrollableHit findScrollableAt(float x, float y) const;
                ScrollableHit findScrollableNode(const IRNode *node, const LayoutResult *layout,
                                                 float x, float y) const;

                /// جمع العناصر القابلة للتركيز (Button, TextField, Toggle, Checkbox, Slider)
                void collectFocusableNodes(const IRNode *node, std::vector<const IRNode *> &out) const;

                // ─── نظام تتبع اللمس المتعدد (Multi-Touch Tracking) ─────────
                /// معالج أحداث اللمس الموحد — يُدير تتبع الأصابع وكشف الإيماءات
                TouchEventProcessor touchProcessor_;

                // ─── نظام لوحة المفاتيح الموحد ───────────────────────────────
                /// معالج أحداث لوحة المفاتيح الموحد — Tab/Enter/Escape + تحرير نصوص
                KeyboardEventProcessor kbProcessor_;

                // ─── نظام الفأرة/المؤشر الموحد ──────────────────────────────
                /// معالج أحداث الفأرة الموحد — Tap/Hover/Drag/Scroll/Slider/Toggle
                MouseEventProcessor mouseProcessor_;

                // ─── نظام الانيميشن (موروث من PlatformWindow) ─────────────────────
                // animationEngine_ موروث من PlatformWindow

                // ─── نظام المؤقتات والإفلات ─────────────────────
                std::function<void()> onTimerUpdateCallback_;                 ///< callback تحديث المؤقتات
                std::function<void(const std::string &)> onDropFileCallback_; ///< callback إفلات الملفات

                // ─── نظام الصوت (Audio — SDL2) ─────────────────────
                struct SoundEntry
                {
                    int id = 0;
                    uint8_t *buffer = nullptr;
                    uint32_t length = 0;
                    uint32_t position = 0;
                    bool loop = false;
                    bool playing = false;
                };
                std::vector<SoundEntry> activeSounds_;
                float masterVolume_ = 1.0f;
                bool audioInitialized_ = false;
                std::atomic<int> nextSoundId_{1};
                uint32_t audioDevice_ = 0; ///< SDL_AudioDeviceID (uint32_t alias)
                // SDL_AudioSpec مُخزّن في ملف التطبيق فقط (window.cpp)
                // نستخدم مؤشر void هنا لتجنب تضمين SDL.h في الـ header
                struct AudioSpecStorage;
                std::unique_ptr<AudioSpecStorage> audioSpecStorage_;
                static void audioCallback(void *userdata, uint8_t *stream, int len);

            public:
                /// تسجيل callback لتحديث المؤقتات في كل إطار
                void setTimerUpdateCallback(std::function<void()> cb) { onTimerUpdateCallback_ = std::move(cb); }
                /// تسجيل callback لإفلات الملفات
                void setDropFileCallback(std::function<void(const std::string &)> cb) { onDropFileCallback_ = std::move(cb); }
            };

        } // namespace desktop
    } // namespace ui
} // namespace sad

#endif // SAD_UI_DESKTOP_WINDOW_H
