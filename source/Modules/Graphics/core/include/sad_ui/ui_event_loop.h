/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: ui_event_loop.h
 * المسار: features/graphics/core/include/sad_ui/ui_event_loop.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * @brief (AR) حلقة أحداث الواجهة — القلب النابض لخيط الواجهة
 * @brief (EN) UI event loop — the heartbeat of the UI thread
 *
 * حلقة الأحداث تتبع نموذج الإطار الثابت (fixed-frame model):
 *
 *   while (running) {
 *       1. استقبال أحداث النظام (SDL_PollEvent)
 *       2. تصريف أوامر خيط اللغة (CommandQueue::drain)
 *       3. تنفيذ الأوامر على UIArena
 *       4. حساب التخطيط (layout) إذا لزم
 *       5. الرسم (paint) إذا لزم
 *       6. تقديم الإطار (present)
 *       7. انتظار الإطار التالي (vsync أو 16ms)
 *   }
 *
 * ميزانية الإطار (ADR-UI-09):
 *   16.6ms = إطار واحد عند 60 FPS
 *   الأوامر: ≤ 4ms
 *   التخطيط: ≤ 4ms
 *   الرسم: ≤ 6ms
 *   هامش: ≥ 2.6ms
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_EVENT_LOOP_H
#define SAD_UI_EVENT_LOOP_H

#include "ui_arena.h"
#include "command_queue.h"

#include <functional>
#include <atomic>
#include <chrono>

namespace sad
{
    namespace ui
    {

        /**
         * @brief (AR) معالج أوامر — يُنفّذ أمراً واحداً على UIArena
         * @brief (EN) Command handler — executes one command on UIArena
         *
         * يُمرَّر للحلقة عند الإنشاء. يمكن تخصيصه لإضافة منطق إضافي
         * (مثل تشغيل property bindings بعد SET_PROPERTY).
         */
        using CommandHandler = std::function<void(UIArena &arena, const UICommand &cmd)>;

        /**
         * @brief (AR) دالة الرسم — تُرسم الشجرة على الشاشة
         * @brief (EN) Paint callback — renders the tree to screen
         *
         * تُستدعى كل إطار (أو عند الحاجة فقط إذا فُعِّل dirty tracking).
         * تستقبل مؤشراً للجذر (nullptr إذا فارغة).
         */
        using PaintCallback = std::function<void(const WidgetNode *root)>;

        /**
         * @brief (AR) دالة أحداث النظام — تعالج أحداث SDL/OS
         * @brief (EN) System event callback — handles SDL/OS events
         *
         * تُستدعى لكل حدث نظام (ضغط مفتاح، حركة فأرة...).
         * تُرجع true إذا يجب إيقاف الحلقة (مثل إغلاق النافذة).
         */
        using SystemEventCallback = std::function<bool()>;

        /**
         * @brief (AR) إعدادات حلقة الأحداث
         * @brief (EN) Event loop configuration
         */
        struct UIEventLoopConfig
        {
            /// (AR) هل نرسم فقط عند التغيير؟ (dirty tracking)
            /// (EN) Paint only when changed? (dirty tracking)
            bool dirtyTracking = true;

            /// (AR) مهلة الانتظار بالمللي ثانية (0 = بدون انتظار، يدور بأقصى سرعة)
            /// (EN) Wait timeout in ms (0 = no wait, spins at max speed)
            uint32_t waitTimeoutMs = 16;

            /// (AR) الحد الأقصى للأوامر لكل إطار (لمنع التجمد)
            /// (EN) Max commands per frame (to prevent stalling)
            size_t maxCommandsPerFrame = 256;
        };

        /**
         * @brief (AR) حلقة أحداث الواجهة
         * @brief (EN) UI event loop
         *
         * تدير دورة حياة الإطار الواحد وتنسّق بين:
         *   - CommandQueue (الأوامر من خيط اللغة)
         *   - UIArena (شجرة العناصر)
         *   - الرسم والتخطيط
         */
        class UIEventLoop
        {
        public:
            /**
             * @brief (AR) المُنشئ
             * @brief (EN) Constructor
             *
             * @param arena الساحة (ملكية خارجية — الحلقة لا تملكها)
             * @param queue طابور الأوامر المشترك بين الخيطين
             * @param config إعدادات الحلقة
             */
            UIEventLoop(UIArena &arena, CommandQueue &queue,
                        UIEventLoopConfig config = {});

            ~UIEventLoop() = default;

            // ─── لا نسخ ─────────────────────────────────
            UIEventLoop(const UIEventLoop &) = delete;
            UIEventLoop &operator=(const UIEventLoop &) = delete;

            // ═══════════════════════════════════════════════════
            // (AR) تشغيل الحلقة
            // (EN) Running the loop
            // ═══════════════════════════════════════════════════

            /**
             * @brief (AR) بدء الحلقة — تحجب حتى الإيقاف
             * @brief (EN) Start the loop — blocks until stopped
             *
             * يجب استدعاؤها من خيط الواجهة (UI thread).
             * تستمر حتى يُستدعى stop() أو يصل أمر SHUTDOWN.
             */
            void run();

            /**
             * @brief (AR) تنفيذ إطار واحد فقط — للاختبار
             * @brief (EN) Execute a single frame — for testing
             *
             * يصرّف الأوامر، يحسب التخطيط، يرسم.
             * لا يحجب.
             */
            void tick();

            /**
             * @brief (AR) طلب إيقاف الحلقة (آمن من أي خيط)
             * @brief (EN) Request loop stop (safe from any thread)
             */
            void stop();

            /**
             * @brief (AR) هل الحلقة تعمل؟
             * @brief (EN) Is the loop running?
             */
            bool isRunning() const { return running_.load(std::memory_order_acquire); }

            // ═══════════════════════════════════════════════════
            // (AR) تعيين المعالجات
            // (EN) Setting callbacks
            // ═══════════════════════════════════════════════════

            /**
             * @brief (AR) تعيين معالج الأوامر
             * @brief (EN) Set command handler
             *
             * يُنفَّذ لكل أمر بعد drain. المعالج الافتراضي يُنفذ
             * الأوامر الأساسية (CREATE_ROOT, SET_PROPERTY...).
             */
            void setCommandHandler(CommandHandler handler);

            /**
             * @brief (AR) تعيين دالة الرسم
             * @brief (EN) Set paint callback
             */
            void setPaintCallback(PaintCallback callback);

            /**
             * @brief (AR) تعيين معالج أحداث النظام
             * @brief (EN) Set system event callback
             */
            void setSystemEventCallback(SystemEventCallback callback);

            // ═══════════════════════════════════════════════════
            // (AR) إحصائيات (للتشخيص)
            // (EN) Statistics (diagnostics)
            // ═══════════════════════════════════════════════════

            /** @brief (AR) عدد الإطارات منذ البدء / (EN) Frame count since start */
            uint64_t getFrameCount() const { return frameCount_; }

            /** @brief (AR) زمن آخر إطار بالمللي ثانية / (EN) Last frame time in ms */
            double getLastFrameTimeMs() const { return lastFrameTimeMs_; }

        private:
            /**
             * @brief (AR) تصريف ومعالجة الأوامر
             * @brief (EN) Drain and process commands
             *
             * @return عدد الأوامر المعالجة
             */
            size_t processCommands();

            /**
             * @brief (AR) المعالج الافتراضي للأوامر الأساسية
             * @brief (EN) Default handler for basic commands
             */
            void defaultCommandHandler(UIArena &arena, const UICommand &cmd);

            // ─── الحقول ────────────────────────────────

            UIArena &arena_;           ///< الساحة (مرجع — لا ملكية)
            CommandQueue &queue_;      ///< طابور الأوامر (مرجع — مشترك)
            UIEventLoopConfig config_; ///< الإعدادات

            std::atomic<bool> running_{false}; ///< حالة التشغيل

            CommandHandler commandHandler_;     ///< معالج الأوامر (مخصص أو افتراضي)
            PaintCallback paintCallback_;       ///< دالة الرسم
            SystemEventCallback systemEventCb_; ///< معالج أحداث النظام

            bool dirty_ = true; ///< هل الشجرة تغيّرت وتحتاج رسم؟

            uint64_t frameCount_ = 0;      ///< عداد الإطارات
            double lastFrameTimeMs_ = 0.0; ///< زمن آخر إطار
        };

    } // namespace ui
} // namespace sad

#endif // SAD_UI_EVENT_LOOP_H
