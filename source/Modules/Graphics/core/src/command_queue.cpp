/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: command_queue.cpp
 * المسار: features/graphics/core/src/command_queue.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * @brief (AR) تنفيذ طابور الأوامر المُخزّن مزدوج
 * @brief (EN) Double-buffered command queue implementation
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/command_queue.h"

#include <chrono>

namespace sad
{
    namespace ui
    {

        CommandQueue::CommandQueue()
        {
            // (AR) حجز مساحة مبدئية لتقليل إعادة التخصيص
            //      64 أمر = حجم معقول لإطار واحد عند 60 FPS
            // (EN) Pre-allocate to reduce reallocations
            //      64 commands = reasonable size for one frame at 60 FPS
            writeBuffer_.reserve(64);
            readBuffer_.reserve(64);
        }

        // ═══════════════════════════════════════════════════
        // واجهة المُنتج (خيط اللغة)
        // ═══════════════════════════════════════════════════

        void CommandQueue::push(UICommand cmd)
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                writeBuffer_.push_back(std::move(cmd));
            }
            pendingCount_.fetch_add(1, std::memory_order_release);
        }

        void CommandQueue::notify()
        {
            cv_.notify_one();
        }

        // ═══════════════════════════════════════════════════
        // واجهة المُستهلك (خيط الواجهة)
        // ═══════════════════════════════════════════════════

        std::vector<UICommand> CommandQueue::drain()
        {
            // (AR) المبادلة الذرية:
            //   1. نأخذ القفل
            //   2. نبدّل المخزنين (swap) — O(1)
            //   3. نحرر القفل
            //   4. نُعيد المخزن القديم (كان مخزن الكتابة)
            //
            //   خيط اللغة يستمر بالكتابة في المخزن الجديد فوراً
            //   خيط الواجهة يعالج المخزن القديم بدون منافسة
            //
            // (EN) Atomic swap:
            //   1. Acquire lock
            //   2. Swap buffers — O(1)
            //   3. Release lock
            //   4. Return old buffer (was write buffer)
            {
                std::lock_guard<std::mutex> lock(mutex_);
                std::swap(writeBuffer_, readBuffer_);
            }

            // (AR) تحديث العدّاد الذري
            // (EN) Update atomic counter
            pendingCount_.store(0, std::memory_order_release);

            // (AR) نقل المخزن القديم للخارج وتنظيف مخزن القراءة
            // (EN) Move old buffer out and clear read buffer
            std::vector<UICommand> result = std::move(readBuffer_);
            readBuffer_.clear();
            // (AR) نحتفظ بالسعة المحجوزة لتقليل إعادة التخصيص
            // (EN) Keep reserved capacity to reduce reallocations

            return result;
        }

        bool CommandQueue::waitForCommands(uint32_t timeoutMs)
        {
            if (timeoutMs == 0)
            {
                // (AR) بدون انتظار — فحص فوري فقط
                // (EN) No wait — immediate check only
                return pendingCount_.load(std::memory_order_acquire) > 0;
            }

            std::unique_lock<std::mutex> lock(mutex_);
            return cv_.wait_for(lock,
                                std::chrono::milliseconds(timeoutMs),
                                [this]()
                                {
                                    return !writeBuffer_.empty();
                                });
        }

        // ═══════════════════════════════════════════════════
        // إحصائيات
        // ═══════════════════════════════════════════════════

        size_t CommandQueue::pendingCount() const
        {
            return pendingCount_.load(std::memory_order_acquire);
        }

        bool CommandQueue::empty() const
        {
            return pendingCount_.load(std::memory_order_acquire) == 0;
        }

    } // namespace ui
} // namespace sad
