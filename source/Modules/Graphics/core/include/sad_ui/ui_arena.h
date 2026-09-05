/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: ui_arena.h
 * المسار: features/graphics/core/include/sad_ui/ui_arena.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * @brief (AR) الساحة — المالك الحصري لجميع عناصر الواجهة
 * @brief (EN) Arena — sole owner of all UI widget nodes
 *
 * UIArena تملك شجرة الواجهة بالكامل. كل عنصر يُنشأ ويُحذف عبرها.
 *
 * القواعد المعمارية (ADR-UI-08):
 * ──────────────────────────────
 * 1. UIArena تعيش فقط في خيط الواجهة (UI thread)
 * 2. خيط اللغة لا يصل إلى WidgetNode مباشرة — فقط عبر WidgetHandle
 * 3. كل العمليات التعديلية (إضافة/حذف/تعديل خاصية) تُنفذ في خيط الواجهة
 *    بعد وصول الأمر عبر CommandQueue
 * 4. المعرّفات تصاعدية ولا تُعاد استخدامها (monotonically increasing)
 * 5. البحث بالمعرّف O(1) عبر فهرس unordered_map
 *
 * نموذج الملكية:
 * ─────────────
 *       UIArena
 *         │ owns root (unique_ptr)
 *         ▼
 *      WidgetNode (root)
 *       ├── unique_ptr<WidgetNode> child1
 *       ├── unique_ptr<WidgetNode> child2
 *       │    ├── unique_ptr<WidgetNode> grandchild1
 *       │    └── unique_ptr<WidgetNode> grandchild2
 *       └── unique_ptr<WidgetNode> child3
 *
 *       UIArena.index_: id → raw WidgetNode*  (للبحث السريع)
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_ARENA_H
#define SAD_UI_ARENA_H

#include "widget_id.h"
#include "widget_node.h"
#include "widget_handle.h"

#include <memory>
#include <unordered_map>
#include <string>

namespace sad
{
    namespace ui
    {

        /**
         * @brief (AR) ساحة الواجهة — تملك شجرة العناصر بالكامل
         * @brief (EN) UI Arena — owns the entire widget tree
         */
        class UIArena
        {
        public:
            UIArena();
            ~UIArena();

            // ─── لا نسخ ولا نقل ─────────────────────────
            UIArena(const UIArena &) = delete;
            UIArena &operator=(const UIArena &) = delete;
            UIArena(UIArena &&) = delete;
            UIArena &operator=(UIArena &&) = delete;

            // ═══════════════════════════════════════════════════════════
            // (AR) إنشاء العناصر
            // (EN) Widget creation
            // ═══════════════════════════════════════════════════════════

            /**
             * @brief (AR) إنشاء عنصر جذر — يُستبدل الجذر السابق (إن وُجد)
             * @brief (EN) Create root widget — replaces previous root (if any)
             *
             * @param type نوع العنصر (مثل "عمود"، "صف"، "حاوية")
             * @return مقبض للجذر الجديد
             */
            WidgetHandle createRoot(const std::string &type);

            /**
             * @brief (AR) إنشاء عنصر ابن وإلحاقه بالأب
             * @brief (EN) Create child widget and attach to parent
             *
             * @param parentId معرّف الأب
             * @param type نوع العنصر الابن
             * @return مقبض للابن الجديد، أو مقبض فارغ إذا لم يُوجد الأب
             */
            WidgetHandle createChild(WidgetId parentId, const std::string &type);

            // ═══════════════════════════════════════════════════════════
            // (AR) البحث والوصول
            // (EN) Lookup and access
            // ═══════════════════════════════════════════════════════════

            /**
             * @brief (AR) البحث عن عنصر بالمعرّف — O(1)
             * @brief (EN) Find widget by ID — O(1)
             *
             * @param id المعرّف
             * @return مؤشر خام أو nullptr إذا لم يُوجد
             */
            WidgetNode *findNode(WidgetId id) const;

            /**
             * @brief (AR) الحصول على الجذر
             * @brief (EN) Get root node
             *
             * @return مؤشر للجذر أو nullptr إذا كانت الشجرة فارغة
             */
            WidgetNode *getRoot() const;

            /**
             * @brief (AR) هل يُوجد عنصر بهذا المعرّف؟
             * @brief (EN) Does a widget with this ID exist?
             */
            bool exists(WidgetId id) const;

            // ═══════════════════════════════════════════════════════════
            // (AR) الحذف
            // (EN) Deletion
            // ═══════════════════════════════════════════════════════════

            /**
             * @brief (AR) حذف عنصر وجميع أبنائه من الشجرة
             * @brief (EN) Remove a widget and all its descendants from the tree
             *
             * العنصر يُزال من أبنائه لدى الأب، ثم يُحذف تعاقبياً مع أبنائه.
             * جميع المعرّفات المحذوفة تُزال من الفهرس.
             *
             * @param id معرّف العنصر
             * @return true إذا وُجد وحُذف بنجاح
             */
            bool removeNode(WidgetId id);

            /**
             * @brief (AR) حذف الشجرة بالكامل (بما في ذلك الجذر)
             * @brief (EN) Clear entire tree (including root)
             */
            void clear();

            // ═══════════════════════════════════════════════════════════
            // (AR) الإحصائيات
            // (EN) Statistics
            // ═══════════════════════════════════════════════════════════

            /**
             * @brief (AR) العدد الإجمالي للعناصر في الشجرة
             * @brief (EN) Total number of widgets in the tree
             */
            size_t getNodeCount() const { return index_.size(); }

            /**
             * @brief (AR) آخر معرّف تم توليده (للتشخيص)
             * @brief (EN) Last generated ID (for diagnostics)
             */
            WidgetId getLastId() const { return nextId_ - 1; }

        private:
            /**
             * @brief (AR) توليد معرّف فريد جديد (تصاعدي)
             * @brief (EN) Generate next unique ID (monotonically increasing)
             */
            WidgetId generateId();

            /**
             * @brief (AR) إزالة عنصر وأبنائه من الفهرس (تعاقبي)
             * @brief (EN) Remove node and descendants from index (recursive)
             */
            void unindexSubtree(WidgetNode *node);

            // ─── الحقول ────────────────────────────────

            /// (AR) الجذر — ملكية حصرية / (EN) Root — exclusive ownership
            std::unique_ptr<WidgetNode> root_;

            /// (AR) فهرس البحث: معرّف → مؤشر خام / (EN) Lookup index: ID → raw pointer
            std::unordered_map<WidgetId, WidgetNode *> index_;

            /// (AR) رموز حياة العناصر: تنتهي عند الحذف لاكتشاف المقابض اليتيمة
            /// (EN) Widget liveness tokens: expire on deletion to detect orphan handles
            std::unordered_map<WidgetId, std::shared_ptr<uint8_t>> livenessTokens_;

            /// (AR) المعرّف التالي (يبدأ من 1) / (EN) Next ID (starts at 1)
            WidgetId nextId_ = 1;
        };

    } // namespace ui
} // namespace sad

#endif // SAD_UI_ARENA_H
