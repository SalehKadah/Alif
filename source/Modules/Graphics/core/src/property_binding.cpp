/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: property_binding.cpp
 * المسار: features/graphics/core/src/property_binding.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * @brief (AR) تنفيذ مدير ربط الخصائص
 * @brief (EN) Property binding manager implementation
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/property_binding.h"

#include <algorithm>

namespace sad
{
    namespace ui
    {

        BindingManager::BindingManager(CommandQueue &queue)
            : queue_(queue), nextBindingId_(1)
        {
        }

        BindingManager::~BindingManager()
        {
            unbindAll();
        }

        // ═══════════════════════════════════════════════════
        // إلغاء الربطات
        // ═══════════════════════════════════════════════════

        bool BindingManager::unbind(BindingId id)
        {
            auto it = std::find_if(bindings_.begin(), bindings_.end(),
                                   [id](const BindingEntry &entry)
                                   { return entry.id == id; });

            if (it == bindings_.end())
            {
                return false;
            }

            // (AR) تنظيف — إلغاء الاشتراك في الإشارة
            // (EN) Cleanup — unsubscribe from signal
            if (it->cleanup)
            {
                it->cleanup();
            }

            // (AR) إزالة من فهرس العنصر
            // (EN) Remove from widget index
            auto widgetIt = widgetBindings_.find(it->widgetId);
            if (widgetIt != widgetBindings_.end())
            {
                auto &vec = widgetIt->second;
                vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
                if (vec.empty())
                {
                    widgetBindings_.erase(widgetIt);
                }
            }

            bindings_.erase(it);
            return true;
        }

        size_t BindingManager::unbindWidget(WidgetId widgetId)
        {
            auto widgetIt = widgetBindings_.find(widgetId);
            if (widgetIt == widgetBindings_.end())
            {
                return 0;
            }

            // (AR) نسخة من المعرّفات لأن unbind يعدّل widgetBindings_
            // (EN) Copy IDs because unbind modifies widgetBindings_
            auto bindingIds = widgetIt->second;
            size_t count = 0;

            for (auto bindId : bindingIds)
            {
                if (unbind(bindId))
                {
                    ++count;
                }
            }

            return count;
        }

        void BindingManager::unbindAll()
        {
            // (AR) تنظيف جميع الاشتراكات
            // (EN) Cleanup all subscriptions
            for (auto &entry : bindings_)
            {
                if (entry.cleanup)
                {
                    entry.cleanup();
                }
            }

            bindings_.clear();
            widgetBindings_.clear();
        }

        // ═══════════════════════════════════════════════════
        // استعلامات
        // ═══════════════════════════════════════════════════

        size_t BindingManager::getWidgetBindingCount(WidgetId widgetId) const
        {
            auto it = widgetBindings_.find(widgetId);
            if (it != widgetBindings_.end())
            {
                return it->second.size();
            }
            return 0;
        }

        // ═══════════════════════════════════════════════════
        // مساعدات داخلية
        // ═══════════════════════════════════════════════════

        BindingId BindingManager::generateBindingId()
        {
            return nextBindingId_++;
        }

    } // namespace ui
} // namespace sad
