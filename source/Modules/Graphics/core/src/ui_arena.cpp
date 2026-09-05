/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: ui_arena.cpp
 * المسار: features/graphics/core/src/ui_arena.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * @brief (AR) تنفيذ ساحة الواجهة — المالك الحصري لشجرة العناصر
 * @brief (EN) UI Arena implementation — sole owner of widget tree
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/ui_arena.h"

namespace sad
{
    namespace ui
    {

        // ─── مُنشئ ومُدمِّر ──

        UIArena::UIArena()
            : root_(nullptr), nextId_(1)
        {
        }

        UIArena::~UIArena()
        {
            // (AR) clear() يُحرِّر الشجرة بالكامل عبر unique_ptr
            // (EN) clear() frees the entire tree via unique_ptr chain
            clear();
        }

        // ═══════════════════════════════════════════════════════════
        // إنشاء العناصر
        // ═══════════════════════════════════════════════════════════

        WidgetHandle UIArena::createRoot(const std::string &type)
        {
            // (AR) إذا كان هناك جذر سابق — نحذفه أولاً (مع الفهرس)
            // (EN) If there's a previous root — remove it first (with index)
            if (root_)
            {
                unindexSubtree(root_.get());
                root_.reset();
            }

            WidgetId id = generateId();

            // (AR) إنشاء العقدة — لا أب للجذر (nullptr)
            // (EN) Create node — root has no parent (nullptr)
            auto node = std::unique_ptr<WidgetNode>(
                new WidgetNode(id, type, nullptr));

            WidgetNode *rawPtr = node.get();
            root_ = std::move(node);

            // (AR) تسجيل في الفهرس للبحث السريع
            // (EN) Register in index for O(1) lookup
            index_[id] = rawPtr;

            // (AR) إنشاء رمز حياة يربط صلاحية المقابض بعمر العقدة الفعلي.
            // (EN) Create a liveness token that ties handle validity to real node lifetime.
            auto token = std::make_shared<uint8_t>(0);
            livenessTokens_[id] = token;

            return WidgetHandle(id, type, token);
        }

        WidgetHandle UIArena::createChild(WidgetId parentId, const std::string &type)
        {
            // (AR) فحص وجود الأب
            // (EN) Check parent exists
            WidgetNode *parent = findNode(parentId);
            if (!parent)
            {
                return WidgetHandle(); // مقبض فارغ — الأب غير موجود
            }

            WidgetId id = generateId();

            // (AR) إنشاء العقدة مع مؤشر الأب
            // (EN) Create node with parent pointer
            auto node = std::unique_ptr<WidgetNode>(
                new WidgetNode(id, type, parent));

            WidgetNode *rawPtr = node.get();

            // (AR) نقل الملكية للأب
            // (EN) Transfer ownership to parent
            parent->addChild(std::move(node));

            // (AR) تسجيل في الفهرس
            // (EN) Register in index
            index_[id] = rawPtr;

            // (AR) إنشاء رمز حياة للابن ليصبح أي مقبض قديم غير صالح بعد الحذف.
            // (EN) Create child liveness token so stale handles become invalid after removal.
            auto token = std::make_shared<uint8_t>(0);
            livenessTokens_[id] = token;

            return WidgetHandle(id, type, token);
        }

        // ═══════════════════════════════════════════════════════════
        // البحث والوصول
        // ═══════════════════════════════════════════════════════════

        WidgetNode *UIArena::findNode(WidgetId id) const
        {
            auto it = index_.find(id);
            if (it != index_.end())
            {
                return it->second;
            }
            return nullptr;
        }

        WidgetNode *UIArena::getRoot() const
        {
            return root_.get();
        }

        bool UIArena::exists(WidgetId id) const
        {
            return index_.find(id) != index_.end();
        }

        // ═══════════════════════════════════════════════════════════
        // الحذف
        // ═══════════════════════════════════════════════════════════

        bool UIArena::removeNode(WidgetId id)
        {
            WidgetNode *node = findNode(id);
            if (!node)
            {
                return false;
            }

            // (AR) إذا كان الجذر — حذف الشجرة بالكامل
            // (EN) If it's the root — clear entire tree
            if (node == root_.get())
            {
                clear();
                return true;
            }

            // (AR) إزالة من فهرس البحث (العقدة + جميع الأحفاد)
            // (EN) Remove from index (node + all descendants)
            unindexSubtree(node);

            // (AR) إزالة من قائمة أبناء الأب (يحرر unique_ptr تلقائياً)
            // (EN) Remove from parent's children list (unique_ptr frees automatically)
            WidgetNode *parent = node->getParent();
            if (parent)
            {
                // removeChild تُرجع unique_ptr — عند خروجها من النطاق تُحرَّر
                auto removed = parent->removeChild(id);
                // removed يُحذف هنا تلقائياً (RAII)
            }

            return true;
        }

        void UIArena::clear()
        {
            index_.clear();
            livenessTokens_.clear();
            root_.reset(); // يحذف الجذر وجميع الأبناء عبر سلسلة unique_ptr
        }

        // ═══════════════════════════════════════════════════════════
        // مساعدات داخلية
        // ═══════════════════════════════════════════════════════════

        WidgetId UIArena::generateId()
        {
            return nextId_++;
        }

        void UIArena::unindexSubtree(WidgetNode *node)
        {
            if (!node)
                return;

            // (AR) إزالة الأحفاد أولاً (DFS عميق-أول)
            // (EN) Remove descendants first (depth-first)
            for (size_t i = 0; i < node->getChildCount(); ++i)
            {
                unindexSubtree(node->getChild(i));
            }

            // (AR) ثم إزالة العقدة نفسها
            // (EN) Then remove the node itself
            index_.erase(node->getId());
            livenessTokens_.erase(node->getId());
        }

    } // namespace ui
} // namespace sad
