/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: reconciler.h
 * المسار: features/graphics/core/include/sad_ui/reconciler.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * الوصف بالعربية:
 * ───────────────
 * نظام المطابقة الذكية (Reconciler) — Virtual DOM على غرار React.
 *
 * بدلاً من إعادة بناء شجرة IR بالكامل عند كل تغيير في الحالة،
 * يُقارن الـ Reconciler الشجرة الجديدة بالشجرة القديمة ويُنتج قائمة
 * "رقع" (Patches) تصف التغييرات الموضعية فقط:
 *
 *   الشجرة القديمة + الشجرة الجديدة
 *       ↓ diff()
 *   DiffResult (قائمة رقع)
 *       ↓ patch()
 *   الشجرة الحية (مُحدّثة موضعياً)
 *
 * المزايا:
 * ────────
 * 1. **أداء**: فقط العناصر المتغيرة تُعاد رسمها (لا إعادة تخطيط كاملة)
 * 2. **حفظ الحالة**: التمرير والتركيز والتحريكات لا تضيع
 * 3. **ذاكرة**: لا يُعاد إنشاء عقد IR لم تتغير
 * 4. **مفاتيح (Keys)**: دعم المفاتيح لتتبع العناصر في القوائم
 *
 * خوارزمية المقارنة:
 * ──────────────────
 * - مقارنة الأنواع: إذا اختلف النوع → REPLACE
 * - مقارنة الخصائص: كل خاصية تغيرت → UPDATE_PROPS
 * - مقارنة الأحداث: معرّفات المعالجات الجديدة → UPDATE_EVENTS
 * - مقارنة الأبناء: خوارزمية بالمفاتيح (keyed diffing):
 *   1. تطابق بالمفتاح (id) → مقارنة تكرارية
 *   2. عنصر جديد بمفتاح غير موجود → INSERT_CHILD
 *   3. عنصر قديم بمفتاح غير موجود في الجديدة → REMOVE_CHILD
 *   4. ترتيب مختلف → REORDER_CHILDREN
 *   5. بدون مفاتيح → مقارنة بالفهرس (positional diffing)
 *
 * English Description:
 * ────────────────────
 * Smart reconciliation system (React-style Virtual DOM).
 * Compares old and new IR trees, produces a minimal list of patches,
 * then applies patches to the live tree for efficient updates.
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_RECONCILER_H
#define SAD_UI_RECONCILER_H

#include "ir.h"
#include "types.h"

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace sad
{
    namespace ui
    {

        // ═══════════════════════════════════════════════════════════════════════════════
        // أنواع الرقع (Patch Types)
        // ═══════════════════════════════════════════════════════════════════════════════

        /**
         * @brief (AR) نوع الرقعة — يصف نوع التغيير الموضعي
         * @brief (EN) Patch type — describes the kind of localized change
         */
        enum class PatchType : uint8_t
        {
            REPLACE,           ///< استبدال عقدة بأخرى (النوع اختلف)
            UPDATE_PROPS,      ///< تحديث خصائص (قيم تغيرت أو خصائص جديدة/محذوفة)
            UPDATE_EVENTS,     ///< تحديث أحداث (معالجات جديدة)
            INSERT_CHILD,      ///< إدراج ابن جديد في موقع محدد
            REMOVE_CHILD,      ///< حذف ابن من موقع محدد
            REORDER_CHILDREN,  ///< إعادة ترتيب الأبناء
            UPDATE_ANIMATIONS, ///< تحديث تحريكات
            UPDATE_STATE_REFS  ///< تحديث إشارات الحالة
        };

        // ═══════════════════════════════════════════════════════════════════════════════
        // رقعة (Patch) — وصف تغيير واحد
        // ═══════════════════════════════════════════════════════════════════════════════

        /**
         * @brief (AR) رقعة واحدة — تصف تغييراً موضعياً واحداً على الشجرة الحية
         * @brief (EN) A single patch — describes one localized change to the live tree
         *
         * كل رقعة تحمل:
         * - المسار للعقدة المستهدفة (سلسلة أفهرسة من الجذر)
         * - نوع التغيير
         * - البيانات اللازمة للتطبيق
         */
        struct Patch
        {
            /// (AR) المسار من الجذر إلى العقدة المستهدفة — كل رقم = فهرس ابن
            /// (EN) Path from root to target node — each number = child index
            std::vector<size_t> path;

            /// (AR) نوع الرقعة / (EN) Patch type
            PatchType type;

            // ─── بيانات حسب النوع ────────────────────────

            /// REPLACE: العقدة البديلة الكاملة
            std::shared_ptr<IRNode> replacementNode;

            /// UPDATE_PROPS: الخصائص المتغيرة (مفتاح → قيمة جديدة)
            std::vector<IRProperty> updatedProps;

            /// UPDATE_PROPS: الخصائص المحذوفة (أسماء فقط)
            std::vector<std::string> removedProps;

            /// UPDATE_EVENTS: الأحداث الجديدة/المحدثة
            std::vector<IREvent> updatedEvents;

            /// INSERT_CHILD: العقدة الجديدة + فهرسها
            std::shared_ptr<IRNode> insertedChild;
            size_t insertIndex = 0;

            /// REMOVE_CHILD: فهرس الابن المحذوف
            size_t removeIndex = 0;

            /// REORDER_CHILDREN: الترتيب الجديد (فهرس قديم → فهرس جديد)
            std::vector<size_t> newOrder;

            /// UPDATE_ANIMATIONS: التحريكات الجديدة
            std::vector<IRAnimation> updatedAnimations;

            /// UPDATE_STATE_REFS: إشارات الحالة الجديدة
            std::vector<IRStateRef> updatedStateRefs;
        };

        // ═══════════════════════════════════════════════════════════════════════════════
        // نتيجة المقارنة (DiffResult)
        // ═══════════════════════════════════════════════════════════════════════════════

        /**
         * @brief (AR) نتيجة مقارنة شجرتي IR — قائمة رقع مرتبة
         * @brief (EN) Result of comparing two IR trees — ordered list of patches
         */
        struct DiffResult
        {
            /// (AR) قائمة الرقع مرتبة حسب العمق (الأعمق أولاً) لتجنب تعارض الأفهرسة
            /// (EN) Patches ordered by depth (deepest first) to avoid index conflicts
            std::vector<Patch> patches;

            /// (AR) هل الشجرتان متطابقتان تماماً (لا رقع)؟
            /// (EN) Are the trees identical (no patches)?
            bool isEmpty() const { return patches.empty(); }

            /// (AR) عدد الرقع
            /// (EN) Number of patches
            size_t size() const { return patches.size(); }

            /// (AR) إحصائيات المقارنة
            /// (EN) Diff statistics
            struct Stats
            {
                size_t nodesCompared = 0;     ///< عدد العقد المُقارَنة
                size_t nodesUnchanged = 0;    ///< عدد العقد غير المتغيرة
                size_t nodesReplaced = 0;     ///< عدد العقد المستبدلة
                size_t propsUpdated = 0;      ///< عدد الخصائص المحدثة
                size_t childrenInserted = 0;  ///< عدد الأبناء المُدرجين
                size_t childrenRemoved = 0;   ///< عدد الأبناء المحذوفين
                size_t childrenReordered = 0; ///< عدد عمليات إعادة الترتيب
            };
            Stats stats;
        };

        // ═══════════════════════════════════════════════════════════════════════════════
        // المطابق (Reconciler)
        // ═══════════════════════════════════════════════════════════════════════════════

        /**
         * @class Reconciler
         * @brief (AR) نظام المطابقة الذكية — يُقارن شجرتي IR ويُطبّق التغييرات الموضعية
         * @brief (EN) Smart reconciliation — diffs two IR trees and applies localized patches
         *
         * الاستخدام:
         * @code
         *   Reconciler reconciler;
         *   auto diff = reconciler.diff(oldTree, newTree);
         *   if (!diff.isEmpty()) {
         *       reconciler.patch(liveTree, diff);
         *   }
         * @endcode
         */
        class Reconciler
        {
        public:
            Reconciler() = default;

            // ─────────────────────────────────────────────────────────────────
            // المقارنة (Diffing)
            // ─────────────────────────────────────────────────────────────────

            /**
             * @brief (AR) مقارنة شجرتي IR وإنتاج قائمة رقع
             * @brief (EN) Compare two IR trees and produce a list of patches
             *
             * @param oldTree الشجرة القديمة (الحالية)
             * @param newTree الشجرة الجديدة (المطلوبة)
             * @return DiffResult يحتوي على قائمة الرقع + إحصائيات
             */
            DiffResult diff(const std::shared_ptr<IRNode> &oldTree,
                            const std::shared_ptr<IRNode> &newTree);

            // ─────────────────────────────────────────────────────────────────
            // التطبيق (Patching)
            // ─────────────────────────────────────────────────────────────────

            /**
             * @brief (AR) تطبيق الرقع على الشجرة الحية
             * @brief (EN) Apply patches to the live tree
             *
             * @param liveTree الشجرة الحية (تُعدّل في المكان)
             * @param diffResult نتيجة المقارنة من diff()
             * @return true إذا نجح التطبيق بدون أخطاء
             */
            bool patch(std::shared_ptr<IRNode> &liveTree,
                       const DiffResult &diffResult);

            // ─────────────────────────────────────────────────────────────────
            // الإعدادات
            // ─────────────────────────────────────────────────────────────────

            /**
             * @brief (AR) تعيين دالة استدعاء عند تحديث عقدة (للإشعار بالتغييرات)
             * @brief (EN) Set callback when a node is updated (for change notification)
             *
             * تُستدعى بعد كل رقعة تُطبّق بنجاح مع مسار العقدة المتأثرة.
             * مفيدة لإعادة تخطيط العقد المتأثرة فقط.
             */
            using OnNodeUpdated = std::function<void(IRNode *node, PatchType type)>;
            void setOnNodeUpdated(OnNodeUpdated callback) { onNodeUpdated_ = std::move(callback); }

        private:
            // ─────────────────────────────────────────────────────────────────
            // خوارزمية المقارنة الداخلية
            // ─────────────────────────────────────────────────────────────────

            /**
             * @brief (AR) مقارنة عقدتين تكرارياً وإنتاج رقع
             * @brief (EN) Recursively compare two nodes and produce patches
             *
             * @param oldNode العقدة القديمة
             * @param newNode العقدة الجديدة
             * @param currentPath المسار الحالي من الجذر
             * @param result نتيجة المقارنة (يُضاف إليها)
             */
            void diffNodes(const IRNode *oldNode,
                           const IRNode *newNode,
                           std::vector<size_t> &currentPath,
                           DiffResult &result);

            /**
             * @brief (AR) مقارنة خصائص عقدتين
             * @brief (EN) Compare properties of two nodes
             */
            void diffProperties(const IRNode *oldNode,
                                const IRNode *newNode,
                                const std::vector<size_t> &path,
                                DiffResult &result);

            /**
             * @brief (AR) مقارنة أحداث عقدتين
             * @brief (EN) Compare events of two nodes
             */
            void diffEvents(const IRNode *oldNode,
                            const IRNode *newNode,
                            const std::vector<size_t> &path,
                            DiffResult &result);

            /**
             * @brief (AR) مقارنة أبناء عقدتين مع دعم المفاتيح
             * @brief (EN) Compare children of two nodes with key support
             *
             * الخوارزمية:
             * 1. بناء خرائط مفاتيح للأبناء القدامى والجدد
             * 2. تحديد الأبناء المضافين (مفتاح جديد غير موجود في القديمة)
             * 3. تحديد الأبناء المحذوفين (مفتاح قديم غير موجود في الجديدة)
             * 4. تحديد الأبناء المتبقين ومقارنتهم تكرارياً
             * 5. إذا اختلف الترتيب → REORDER_CHILDREN
             */
            void diffChildren(const IRNode *oldNode,
                              const IRNode *newNode,
                              std::vector<size_t> &currentPath,
                              DiffResult &result);

            /**
             * @brief (AR) مقارنة تحريكات عقدتين
             * @brief (EN) Compare animations of two nodes
             */
            void diffAnimations(const IRNode *oldNode,
                                const IRNode *newNode,
                                const std::vector<size_t> &path,
                                DiffResult &result);

            /**
             * @brief (AR) الحصول على مفتاح العقدة (id أو فهرس)
             * @brief (EN) Get node key (id or index-based)
             */
            std::string getNodeKey(const IRNode *node, size_t index) const;

            // ─────────────────────────────────────────────────────────────────
            // تطبيق الرقع الداخلي
            // ─────────────────────────────────────────────────────────────────

            /**
             * @brief (AR) الوصول لعقدة في الشجرة عبر المسار
             * @brief (EN) Navigate to a node in the tree by path
             */
            IRNode *navigateToNode(IRNode *root, const std::vector<size_t> &path);

            /**
             * @brief (AR) الوصول لعقدة الأب (المسار بدون آخر عنصر)
             * @brief (EN) Navigate to parent node (path without last element)
             */
            IRNode *navigateToParent(IRNode *root, const std::vector<size_t> &path);

            /**
             * @brief (AR) تطبيق رقعة واحدة
             * @brief (EN) Apply a single patch
             */
            bool applySinglePatch(std::shared_ptr<IRNode> &root, const Patch &patch);

            /// (AR) دالة الاستدعاء عند التحديث
            OnNodeUpdated onNodeUpdated_;
        };

    } // namespace ui
} // namespace sad

#endif // SAD_UI_RECONCILER_H
