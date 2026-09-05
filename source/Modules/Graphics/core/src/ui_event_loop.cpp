/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: ui_event_loop.cpp
 * المسار: features/graphics/core/src/ui_event_loop.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * @brief (AR) تنفيذ حلقة أحداث الواجهة
 * @brief (EN) UI event loop implementation
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/ui_event_loop.h"

#include <algorithm>

namespace sad
{
    namespace ui
    {

        UIEventLoop::UIEventLoop(UIArena &arena, CommandQueue &queue,
                                 UIEventLoopConfig config)
            : arena_(arena), queue_(queue), config_(config)
        {
            // (AR) تعيين المعالج الافتراضي
            // (EN) Set default command handler
            commandHandler_ = [this](UIArena &a, const UICommand &cmd)
            {
                defaultCommandHandler(a, cmd);
            };
        }

        // ═══════════════════════════════════════════════════
        // تشغيل الحلقة
        // ═══════════════════════════════════════════════════

        void UIEventLoop::run()
        {
            running_.store(true, std::memory_order_release);

            while (running_.load(std::memory_order_acquire))
            {
                auto frameStart = std::chrono::high_resolution_clock::now();

                tick();

                auto frameEnd = std::chrono::high_resolution_clock::now();
                lastFrameTimeMs_ = std::chrono::duration<double, std::milli>(
                                       frameEnd - frameStart)
                                       .count();

                ++frameCount_;

                // (AR) إذا بقي وقت في الإطار — ننتظر أوامر جديدة
                //      بدلاً من الدوران الفارغ (spin-wait)
                // (EN) If time remains in frame — wait for new commands
                //      instead of spin-waiting
                double remainingMs = config_.waitTimeoutMs - lastFrameTimeMs_;
                if (remainingMs > 1.0)
                {
                    queue_.waitForCommands(static_cast<uint32_t>(remainingMs));
                }
            }
        }

        void UIEventLoop::tick()
        {
            // (AR) الخطوة 1: أحداث النظام (SDL/OS)
            // (EN) Step 1: System events (SDL/OS)
            if (systemEventCb_)
            {
                bool shouldStop = systemEventCb_();
                if (shouldStop)
                {
                    stop();
                    return;
                }
            }

            // (AR) الخطوة 2: تصريف ومعالجة أوامر خيط اللغة
            // (EN) Step 2: Drain and process language thread commands
            size_t processed = processCommands();

            // (AR) إذا عُولجت أوامر — الشجرة تغيّرت
            // (EN) If commands processed — tree changed
            if (processed > 0)
            {
                dirty_ = true;
            }

            // (AR) الخطوة 3: الرسم (إذا لزم)
            // (EN) Step 3: Paint (if needed)
            if (paintCallback_)
            {
                if (!config_.dirtyTracking || dirty_)
                {
                    paintCallback_(arena_.getRoot());
                    dirty_ = false;
                }
            }
        }

        void UIEventLoop::stop()
        {
            running_.store(false, std::memory_order_release);
            // (AR) إيقاظ الحلقة إذا كانت تنتظر
            // (EN) Wake loop if waiting
            queue_.notify();
        }

        // ═══════════════════════════════════════════════════
        // تعيين المعالجات
        // ═══════════════════════════════════════════════════

        void UIEventLoop::setCommandHandler(CommandHandler handler)
        {
            if (handler)
            {
                commandHandler_ = std::move(handler);
            }
        }

        void UIEventLoop::setPaintCallback(PaintCallback callback)
        {
            paintCallback_ = std::move(callback);
        }

        void UIEventLoop::setSystemEventCallback(SystemEventCallback callback)
        {
            systemEventCb_ = std::move(callback);
        }

        // ═══════════════════════════════════════════════════
        // معالجة الأوامر
        // ═══════════════════════════════════════════════════

        size_t UIEventLoop::processCommands()
        {
            auto batch = queue_.drain();

            if (batch.empty())
            {
                return 0;
            }

            // (AR) تحديد عدد الأوامر المعالجة (مع الحد الأقصى)
            // (EN) Determine commands to process (with cap)
            size_t toProcess = std::min(batch.size(), config_.maxCommandsPerFrame);

            for (size_t i = 0; i < toProcess; ++i)
            {
                const auto &cmd = batch[i];

                // (AR) فحص أمر الإغلاق أولاً
                // (EN) Check shutdown command first
                if (cmd.type == UICommandType::SHUTDOWN)
                {
                    stop();
                    return i + 1;
                }

                // (AR) تنفيذ الأمر عبر المعالج
                // (EN) Execute command via handler
                if (commandHandler_)
                {
                    commandHandler_(arena_, cmd);
                }
            }

            // (AR) إذا بقيت أوامر لم تُعالج — نُعيدها للطابور
            //      (هذا نادر — يحدث فقط عند maxCommandsPerFrame)
            // (EN) If commands remain — re-queue them
            //      (rare — only when maxCommandsPerFrame exceeded)
            if (toProcess < batch.size())
            {
                for (size_t i = toProcess; i < batch.size(); ++i)
                {
                    queue_.push(std::move(batch[i]));
                }
            }

            return toProcess;
        }

        // ═══════════════════════════════════════════════════
        // المعالج الافتراضي
        // ═══════════════════════════════════════════════════

        void UIEventLoop::defaultCommandHandler(UIArena &arena, const UICommand &cmd)
        {
            switch (cmd.type)
            {
            case UICommandType::CREATE_ROOT:
                arena.createRoot(cmd.name);
                break;

            case UICommandType::CREATE_CHILD:
                arena.createChild(cmd.parentId, cmd.name);
                break;

            case UICommandType::REMOVE_NODE:
                arena.removeNode(cmd.targetId);
                break;

            case UICommandType::CLEAR_TREE:
                arena.clear();
                break;

            case UICommandType::SET_PROPERTY:
            {
                WidgetNode *node = arena.findNode(cmd.targetId);
                if (node)
                {
                    node->setProperty(cmd.name, cmd.value);
                }
                break;
            }

            case UICommandType::REMOVE_PROPERTY:
            {
                WidgetNode *node = arena.findNode(cmd.targetId);
                if (node)
                {
                    node->removeProperty(cmd.name);
                }
                break;
            }

            case UICommandType::REQUEST_LAYOUT:
                dirty_ = true;
                break;

            case UICommandType::REQUEST_PAINT:
                dirty_ = true;
                break;

            case UICommandType::SET_EVENT_HANDLER:
            {
                // (AR) سيُضاف لاحقاً عند تنفيذ نظام الأحداث
                // (EN) Will be added later when event system is implemented
                break;
            }

            case UICommandType::CUSTOM:
                if (cmd.action)
                {
                    cmd.action();
                }
                break;

            case UICommandType::SHUTDOWN:
                // (AR) يُعالج في processCommands قبل الوصول هنا
                // (EN) Handled in processCommands before reaching here
                break;
            }
        }

    } // namespace ui
} // namespace sad
