/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: widget_node.h
 * المسار: features/graphics/core/include/sad_ui/widget_node.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * @brief (AR) عقدة عنصر الواجهة — الوحدة الأساسية في شجرة العناصر
 * @brief (EN) Widget node — fundamental unit in the widget tree
 *
 * كل عنصر واجهة (نص، زر، عمود...) يُمثَّل بعقدة WidgetNode.
 * العقد تُشكّل شجرة ملكية صريحة:
 *
 *   - الأب يملك أبناءه عبر unique_ptr (ملكية حصرية)
 *   - الابن يحمل مؤشراً خاماً للأب (raw pointer — بدون ملكية)
 *   - لا توجد دورات مرجعية — لا تسريب ذاكرة
 *
 * هذا النموذج مستوحى من Qt's QObject ownership model ومن
 * تحليل وينستون (ADR-UI-08) الذي حدد أن shared_ptr يسبب
 * دورات مرجعية حتمية في شجرة الواجهة.
 *
 * القيود المعمارية:
 * ─────────────────
 * 1. WidgetNode لا يُنشأ مباشرة — فقط عبر UIArena
 * 2. WidgetNode لا يُنسخ — (deleted copy constructor)
 * 3. WidgetNode لا يُنقل بعد الإنشاء — الأب يحمل unique_ptr
 * 4. خصائص العنصر تُخزّن كـ unordered_map<string, Value>
 *    لأن أنواع العناصر مختلفة والخصائص ديناميكية
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_WIDGET_NODE_H
#define SAD_UI_WIDGET_NODE_H

#include "widget_id.h"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <any>

namespace sad
{
    namespace ui
    {

        /**
         * @brief (AR) عقدة عنصر واجهة واحد في الشجرة
         * @brief (EN) A single widget node in the UI tree
         *
         * كل عقدة تحتوي على:
         * - معرّف فريد (id) — لا يتكرر أبداً حتى بعد الحذف
         * - نوع العنصر (type) — مثل "نص"، "زر"، "عمود"
         * - الخصائص (properties) — مثل "عنوان"، "لون_خلفية"
         * - الأبناء (children) — عقد فرعية مملوكة بـ unique_ptr
         * - مؤشر الأب (parent) — raw pointer، لا ملكية
         */
        class WidgetNode
        {
        public:
            // ─── لا نسخ ولا نقل بعد الإنشاء ──────────────
            WidgetNode(const WidgetNode &) = delete;
            WidgetNode &operator=(const WidgetNode &) = delete;
            WidgetNode(WidgetNode &&) = delete;
            WidgetNode &operator=(WidgetNode &&) = delete;

            ~WidgetNode() = default;

            // ─── الوصول للبيانات الأساسية ────────────────

            /** @brief (AR) المعرّف الفريد / (EN) Unique ID */
            WidgetId getId() const { return id_; }

            /** @brief (AR) نوع العنصر / (EN) Widget type string */
            const std::string &getType() const { return type_; }

            /** @brief (AR) مؤشر الأب (nullptr للجذر) / (EN) Parent pointer (nullptr for root) */
            WidgetNode *getParent() const { return parent_; }

            // ─── إدارة الخصائص ──────────────────────────

            /**
             * @brief (AR) تعيين خاصية بالاسم
             * @brief (EN) Set a property by name
             *
             * @param name اسم الخاصية (مثل "عنوان"، "لون_خلفية")
             * @param value القيمة (std::any — يدعم أي نوع)
             */
            void setProperty(const std::string &name, std::any value);

            /**
             * @brief (AR) الحصول على خاصية بالاسم
             * @brief (EN) Get a property by name
             *
             * @param name اسم الخاصية
             * @return القيمة أو std::any فارغ إذا لم تُوجد
             */
            std::any getProperty(const std::string &name) const;

            /**
             * @brief (AR) هل تُوجد خاصية بهذا الاسم؟
             * @brief (EN) Does a property with this name exist?
             */
            bool hasProperty(const std::string &name) const;

            /**
             * @brief (AR) حذف خاصية بالاسم
             * @brief (EN) Remove a property by name
             *
             * @return true إذا كانت موجودة وحُذفت
             */
            bool removeProperty(const std::string &name);

            /**
             * @brief (AR) جميع الخصائص (للقراءة فقط)
             * @brief (EN) All properties (read-only)
             */
            const std::unordered_map<std::string, std::any> &getProperties() const
            {
                return properties_;
            }

            // ─── إدارة الأبناء ──────────────────────────

            /**
             * @brief (AR) عدد الأبناء المباشرين
             * @brief (EN) Number of direct children
             */
            size_t getChildCount() const { return children_.size(); }

            /**
             * @brief (AR) الحصول على ابن بالفهرس
             * @brief (EN) Get child by index
             *
             * @param index فهرس الابن (0-based)
             * @return مؤشر للابن أو nullptr إذا كان الفهرس خارج النطاق
             */
            WidgetNode *getChild(size_t index) const;

            /**
             * @brief (AR) جميع الأبناء (مؤشرات خام — للقراءة فقط)
             * @brief (EN) All children (raw pointers — read-only)
             */
            std::vector<WidgetNode *> getChildren() const;

            /**
             * @brief (AR) هل هذا العنصر هو الجذر؟ (لا أب)
             * @brief (EN) Is this the root node? (no parent)
             */
            bool isRoot() const { return parent_ == nullptr; }

            /**
             * @brief (AR) هل هذا العنصر ورقة؟ (لا أبناء)
             * @brief (EN) Is this a leaf node? (no children)
             */
            bool isLeaf() const { return children_.empty(); }

        private:
            // ─── UIArena هو المُنشئ الوحيد ──────────────
            friend class UIArena;

            /**
             * @brief (AR) المُنشئ الخاص — يُستدعى فقط من UIArena
             * @brief (EN) Private constructor — only called by UIArena
             *
             * @param id المعرّف الفريد
             * @param type نوع العنصر
             * @param parent مؤشر الأب (nullptr للجذر)
             */
            WidgetNode(WidgetId id, std::string type, WidgetNode *parent);

            /**
             * @brief (AR) إضافة ابن — يُستدعى فقط من UIArena
             * @brief (EN) Add child — only called by UIArena
             *
             * ينقل ملكية الابن إلى هذا العنصر.
             *
             * @param child الابن (unique_ptr — نقل الملكية)
             */
            void addChild(std::unique_ptr<WidgetNode> child);

            /**
             * @brief (AR) حذف ابن بالمعرّف — يُستدعى فقط من UIArena
             * @brief (EN) Remove child by ID — only called by UIArena
             *
             * @param childId معرّف الابن المراد حذفه
             * @return unique_ptr للابن المحذوف (لتنظيف الفهرس)
             */
            std::unique_ptr<WidgetNode> removeChild(WidgetId childId);

            // ─── الحقول ─────────────────────────────────

            WidgetId id_;                                          ///< المعرّف الفريد
            std::string type_;                                     ///< نوع العنصر (مثل "نص"، "زر")
            WidgetNode *parent_;                                   ///< مؤشر الأب (raw — لا ملكية)
            std::vector<std::unique_ptr<WidgetNode>> children_;    ///< الأبناء (ملكية حصرية)
            std::unordered_map<std::string, std::any> properties_; ///< خصائص العنصر
        };

    } // namespace ui
} // namespace sad

#endif // SAD_UI_WIDGET_NODE_H
