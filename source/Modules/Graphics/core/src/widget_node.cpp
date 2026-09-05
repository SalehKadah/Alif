/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: widget_node.cpp
 * المسار: features/graphics/core/src/widget_node.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * @brief (AR) تنفيذ عقدة عنصر الواجهة
 * @brief (EN) Widget node implementation
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/widget_node.h"

#include <algorithm>

namespace sad
{
    namespace ui
    {

        // ─── المُنشئ الخاص (يُستدعى فقط من UIArena) ──

        WidgetNode::WidgetNode(WidgetId id, std::string type, WidgetNode *parent)
            : id_(id), type_(std::move(type)), parent_(parent)
        {
        }

        // ─── إدارة الخصائص ──

        void WidgetNode::setProperty(const std::string &name, std::any value)
        {
            properties_[name] = std::move(value);
        }

        std::any WidgetNode::getProperty(const std::string &name) const
        {
            auto it = properties_.find(name);
            if (it != properties_.end())
            {
                return it->second;
            }
            return std::any{}; // فارغ — لم تُوجد الخاصية
        }

        bool WidgetNode::hasProperty(const std::string &name) const
        {
            return properties_.find(name) != properties_.end();
        }

        bool WidgetNode::removeProperty(const std::string &name)
        {
            return properties_.erase(name) > 0;
        }

        // ─── إدارة الأبناء ──

        WidgetNode *WidgetNode::getChild(size_t index) const
        {
            if (index >= children_.size())
            {
                return nullptr;
            }
            return children_[index].get();
        }

        std::vector<WidgetNode *> WidgetNode::getChildren() const
        {
            std::vector<WidgetNode *> result;
            result.reserve(children_.size());
            for (const auto &child : children_)
            {
                result.push_back(child.get());
            }
            return result;
        }

        void WidgetNode::addChild(std::unique_ptr<WidgetNode> child)
        {
            if (child)
            {
                children_.push_back(std::move(child));
            }
        }

        std::unique_ptr<WidgetNode> WidgetNode::removeChild(WidgetId childId)
        {
            auto it = std::find_if(children_.begin(), children_.end(),
                                   [childId](const std::unique_ptr<WidgetNode> &c)
                                   {
                                       return c && c->getId() == childId;
                                   });

            if (it == children_.end())
            {
                return nullptr; // الابن غير موجود
            }

            // استخراج الابن من المتجه (نقل الملكية للخارج)
            std::unique_ptr<WidgetNode> removed = std::move(*it);
            children_.erase(it);
            return removed;
        }

    } // namespace ui
} // namespace sad
