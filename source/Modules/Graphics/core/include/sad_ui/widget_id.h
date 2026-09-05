/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: widget_id.h
 * المسار: features/graphics/core/include/sad_ui/widget_id.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * @brief (AR) معرّف عنصر الواجهة — رقم فريد يُعرّف كل عنصر في شجرة الواجهة
 * @brief (EN) Widget identifier — unique numeric ID for each widget in the UI tree
 *
 * المعرّف هو رقم صحيح موجب يُولَّد تصاعدياً بواسطة UIArena.
 * القيمة 0 محجوزة لتمثيل "لا عنصر" (Invalid).
 *
 * هذا الملف مُنفصل عن widget_node.h لأن WidgetId يُستخدم في عدة ملفات
 * (command_queue.h, property_binding.h, widget_handle.h) بدون الحاجة
 * لتضمين شجرة العناصر الكاملة.
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_WIDGET_ID_H
#define SAD_UI_WIDGET_ID_H

#include <cstdint>
#include <functional>

namespace sad
{
    namespace ui
    {

        /**
         * @brief (AR) معرّف عنصر الواجهة — رقم فريد غير سالب
         * @brief (EN) Widget ID — unique non-negative integer
         *
         * القيمة 0 تعني "غير صالح" (مثل nullptr للمؤشرات).
         * يُولَّد تصاعدياً بواسطة UIArena — لا يُعاد استخدامه أبداً
         * (حتى بعد حذف العنصر) لمنع dangling references.
         */
        using WidgetId = uint64_t;

        /**
         * @brief (AR) معرّف غير صالح — يُستخدم للإشارة إلى عدم وجود عنصر
         * @brief (EN) Invalid ID — used to indicate no widget
         */
        static constexpr WidgetId INVALID_WIDGET_ID = 0;

    } // namespace ui
} // namespace sad

#endif // SAD_UI_WIDGET_ID_H
