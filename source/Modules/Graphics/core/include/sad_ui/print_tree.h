/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: print_tree.h
 * المسار: features/graphics/core/include/sad_ui/print_tree.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * @brief (AR) طباعة شجرة عناصر IR (تصحيح) — منطقٌ مشترك في المكتبة
 * @brief (EN) IR widget-tree pretty printer — shared library logic
 *
 * (AR) منطق طباعة شجرة `IRNode` يعيش هنا (المكتبة `features/graphics`) لا في
 *      المحرّكات، فيتشاركه المفسّر (`طباعة_شجرة`) والمترجم (`sad_print_tree`)
 *      عبر جسرٍ رفيع ⇒ **تطابق مخرَجٍ بالبناء** بين المحرّكين. نُقِل من
 *      interpreter/src/ui/ui_core_builtins.cpp (م-تحكّم / طباعة_شجرة).
 *
 * (EN) The IRNode tree printer lives in the library (not the engines) so the
 *      interpreter (`طباعة_شجرة`) and compiler (`sad_print_tree`) share it via a
 *      thin bridge ⇒ byte-identical output by construction.
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_PRINT_TREE_H
#define SAD_UI_PRINT_TREE_H

#include "sad_ui/ir.h"
#include "sad_ui/types.h"

#include <iostream>
#include <ostream>
#include <memory>

namespace sad
{
    namespace ui
    {
        /**
         * @brief (AR) يطبع شجرة IRNode بتنسيق تصحيحٍ عربيّ إلى التيّار المُعطى.
         *        النوع(الخصائص) [الأحداث] {التحريكات}، ثمّ الأبناء بإزاحةٍ متزايدة.
         *        مصدرُ الحقيقة الوحيد لهذا التنسيق (يتشاركه المحرّكان). التعريفُ في
         *        core/src/print_tree.cpp (يُترجَم مرّةً ويُربَط به المحرّكان).
         * @brief (EN) Print an IRNode tree in an Arabic debug format to `os`.
         *        Single source of truth; defined in core/src/print_tree.cpp.
         * @param node   عقدة الجذر
         * @param depth  عمق الإزاحة (0 عند الجذر)
         * @param os     تيّار الخرج (افتراضيًّا std::cout)
         * @param maxDepth حدّ العمق (حماية من التكرار العميق)
         */
        void printIRNodeTree(const std::shared_ptr<IRNode> &node, int depth,
                             std::ostream &os = std::cout, int maxDepth = 50);

    } // namespace ui
} // namespace sad

#endif /* SAD_UI_PRINT_TREE_H */
