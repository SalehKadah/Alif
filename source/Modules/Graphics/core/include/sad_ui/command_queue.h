/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: command_queue.h
 * المسار: features/graphics/core/include/sad_ui/command_queue.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * @brief (AR) طابور الأوامر الآمن للخيوط — القناة بين خيط اللغة وخيط الواجهة
 * @brief (EN) Thread-safe command queue — channel between language and UI threads
 *
 * التصميم (ADR-UI-09):
 * ───────────────────
 * الطابور يستخدم تقنية "المخازن المزدوجة" (double buffering):
 *
 *   خيط اللغة يكتب → [مخزن الكتابة]
 *                      ↕ swap (ذري)
 *   خيط الواجهة يقرأ ← [مخزن القراءة]
 *
 * فوائد المخازن المزدوجة:
 *   1. خيط اللغة لا يُحجب أبداً أثناء الكتابة (إلا لحظة swap)
 *   2. خيط الواجهة يقرأ دفعة كاملة بدون منافسة (contention-free)
 *   3. أقل قفل ممكن — قفل واحد فقط أثناء swap
 *
 * البديل المرفوض: std::queue مع mutex على كل push/pop
 * (يُسبّب lock contention عالي عند 60 FPS).
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_COMMAND_QUEUE_H
#define SAD_UI_COMMAND_QUEUE_H

#include "ui_command.h"

#include <vector>
#include <mutex>
#include <atomic>
#include <condition_variable>

namespace sad
{
    namespace ui
    {

        /**
         * @brief (AR) طابور أوامر مُخزّن مزدوج — آمن للخيوط
         * @brief (EN) Double-buffered command queue — thread-safe
         *
         * الاستخدام:
         * ─────────
         *   // خيط اللغة (المُنتج):
         *   queue.push(UICommand::makeSetProperty(...));
         *   queue.push(UICommand::makeCreateChild(...));
         *
         *   // خيط الواجهة (المُستهلك) — كل إطار:
         *   auto batch = queue.drain();
         *   for (auto& cmd : batch) {
         *       executeCommand(cmd);
         *   }
         */
        class CommandQueue
        {
        public:
            CommandQueue();
            ~CommandQueue() = default;

            // ─── لا نسخ ─────────────────────────────────
            CommandQueue(const CommandQueue &) = delete;
            CommandQueue &operator=(const CommandQueue &) = delete;

            // ═══════════════════════════════════════════════════
            // (AR) واجهة المُنتج (خيط اللغة)
            // (EN) Producer interface (language thread)
            // ═══════════════════════════════════════════════════

            /**
             * @brief (AR) إضافة أمر للطابور
             * @brief (EN) Push a command to the queue
             *
             * آمن للاستدعاء من أي خيط.
             * القفل يُحتجز فقط لنسخ/نقل الأمر إلى المخزن — سريع جداً.
             *
             * @param cmd الأمر المراد إرساله
             */
            void push(UICommand cmd);

            /**
             * @brief (AR) إيقاظ خيط الواجهة إذا كان نائماً
             * @brief (EN) Wake up UI thread if sleeping
             *
             * يُستدعى بعد push إذا أردنا معالجة فورية.
             */
            void notify();

            // ═══════════════════════════════════════════════════
            // (AR) واجهة المُستهلك (خيط الواجهة)
            // (EN) Consumer interface (UI thread)
            // ═══════════════════════════════════════════════════

            /**
             * @brief (AR) تصريف جميع الأوامر المعلقة — مُبادلة ذرية
             * @brief (EN) Drain all pending commands — atomic swap
             *
             * يُبدِّل مخزن الكتابة بمخزن القراءة (swap) ثم يُعيد المخزن القديم.
             * خيط الواجهة يتلقى دفعة كاملة بدون حجب خيط اللغة.
             *
             * @return متجه بجميع الأوامر المعلقة (قد يكون فارغاً)
             */
            std::vector<UICommand> drain();

            /**
             * @brief (AR) انتظار وصول أوامر (أو مهلة زمنية)
             * @brief (EN) Wait for commands (or timeout)
             *
             * يُحجب خيط الواجهة حتى:
             *   1. وصول أوامر جديدة (notify)
             *   2. انتهاء المهلة (لتحديث الشاشة على أي حال)
             *
             * @param timeoutMs المهلة بالمللي ثانية (0 = بدون انتظار)
             * @return true إذا وصلت أوامر، false إذا انتهت المهلة
             */
            bool waitForCommands(uint32_t timeoutMs);

            // ═══════════════════════════════════════════════════
            // (AR) إحصائيات
            // (EN) Statistics
            // ═══════════════════════════════════════════════════

            /**
             * @brief (AR) عدد الأوامر المعلقة (تقريبي)
             * @brief (EN) Number of pending commands (approximate)
             */
            size_t pendingCount() const;

            /**
             * @brief (AR) هل الطابور فارغ؟
             * @brief (EN) Is the queue empty?
             */
            bool empty() const;

        private:
            // ─── المخازن المزدوجة ────────────────────────

            /// (AR) مخزن الكتابة — خيط اللغة يكتب هنا
            /// (EN) Write buffer — language thread writes here
            std::vector<UICommand> writeBuffer_;

            /// (AR) مخزن القراءة — خيط الواجهة يقرأ منه
            /// (EN) Read buffer — UI thread reads from this
            std::vector<UICommand> readBuffer_;

            /// (AR) قفل لحماية مخزن الكتابة أثناء swap
            /// (EN) Mutex protecting write buffer during swap
            mutable std::mutex mutex_;

            /// (AR) متغير شرط لإيقاظ خيط الواجهة
            /// (EN) Condition variable to wake UI thread
            std::condition_variable cv_;

            /// (AR) عدد الأوامر المعلقة (ذري — بدون قفل للقراءة)
            /// (EN) Pending count (atomic — lock-free reads)
            std::atomic<size_t> pendingCount_{0};
        };

    } // namespace ui
} // namespace sad

#endif // SAD_UI_COMMAND_QUEUE_H
