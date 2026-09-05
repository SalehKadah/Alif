/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: ui_command.h
 * المسار: features/graphics/core/include/sad_ui/ui_command.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * @brief (AR) أنواع أوامر الواجهة — الرسائل بين خيط اللغة وخيط الواجهة
 * @brief (EN) UI command types — messages between language thread and UI thread
 *
 * نموذج التزامن (ADR-UI-09):
 * ──────────────────────────
 * خيط اللغة (يُنفّذ كود ص) لا يلمس شجرة الواجهة مباشرة.
 * بدلاً من ذلك، يُنشئ أوامر (UICommand) ويرسلها عبر
 * طابور آمن للخيوط (CommandQueue) ليُنفّذها خيط الواجهة.
 *
 * كل أمر يحمل:
 *   - نوع العملية (UICommandType)
 *   - معرّف العنصر المستهدف (WidgetId)
 *   - بيانات إضافية حسب نوع العملية
 *
 * هذا النمط يضمن:
 *   1. عدم وجود سباقات بيانات (data races) على الشجرة
 *   2. ترتيب العمليات (FIFO) — الأوامر تُنفَّذ بنفس ترتيب إرسالها
 *   3. عدم حجب خيط اللغة — الإرسال O(1) تقريباً
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_COMMAND_H
#define SAD_UI_COMMAND_H

#include "widget_id.h"

#include <string>
#include <any>
#include <functional>

namespace sad
{
    namespace ui
    {

        /**
         * @brief (AR) أنواع أوامر الواجهة
         * @brief (EN) UI command types
         */
        enum class UICommandType
        {
            // ── إنشاء وحذف ──
            CREATE_ROOT,  ///< إنشاء عنصر جذر جديد
            CREATE_CHILD, ///< إنشاء عنصر ابن
            REMOVE_NODE,  ///< حذف عنصر (وأبنائه)
            CLEAR_TREE,   ///< حذف الشجرة بالكامل

            // ── تعديل الخصائص ──
            SET_PROPERTY,    ///< تعيين خاصية لعنصر
            REMOVE_PROPERTY, ///< حذف خاصية من عنصر

            // ── أحداث ──
            SET_EVENT_HANDLER, ///< تعيين معالج حدث لعنصر

            // ── تخطيط وعرض ──
            REQUEST_LAYOUT, ///< طلب إعادة حساب التخطيط
            REQUEST_PAINT,  ///< طلب إعادة الرسم

            // ── نظام ──
            SHUTDOWN, ///< إغلاق حلقة الأحداث
            CUSTOM    ///< أمر مخصص (دالة lambda)
        };

        /**
         * @brief (AR) أمر واجهة واحد — الوحدة الذرية للتواصل بين الخيوط
         * @brief (EN) Single UI command — atomic unit of inter-thread communication
         */
        struct UICommand
        {
            UICommandType type;           ///< نوع العملية
            WidgetId targetId;            ///< العنصر المستهدف (0 = لا عنصر)
            WidgetId parentId;            ///< الأب (فقط لـ CREATE_CHILD)
            std::string name;             ///< اسم الخاصية/النوع/الحدث
            std::any value;               ///< القيمة (خاصية، معالج...)
            std::function<void()> action; ///< دالة مخصصة (فقط لـ CUSTOM)

            // ── مُنشئات مساعدة (factory methods) ──

            /**
             * @brief (AR) أمر إنشاء جذر
             * @brief (EN) Create root command
             */
            static UICommand makeCreateRoot(const std::string &type)
            {
                UICommand cmd;
                cmd.type = UICommandType::CREATE_ROOT;
                cmd.targetId = INVALID_WIDGET_ID;
                cmd.parentId = INVALID_WIDGET_ID;
                cmd.name = type;
                return cmd;
            }

            /**
             * @brief (AR) أمر إنشاء ابن
             * @brief (EN) Create child command
             */
            static UICommand makeCreateChild(WidgetId parentId, const std::string &type)
            {
                UICommand cmd;
                cmd.type = UICommandType::CREATE_CHILD;
                cmd.targetId = INVALID_WIDGET_ID;
                cmd.parentId = parentId;
                cmd.name = type;
                return cmd;
            }

            /**
             * @brief (AR) أمر حذف عنصر
             * @brief (EN) Remove node command
             */
            static UICommand makeRemoveNode(WidgetId id)
            {
                UICommand cmd;
                cmd.type = UICommandType::REMOVE_NODE;
                cmd.targetId = id;
                cmd.parentId = INVALID_WIDGET_ID;
                return cmd;
            }

            /**
             * @brief (AR) أمر تعيين خاصية
             * @brief (EN) Set property command
             */
            static UICommand makeSetProperty(WidgetId id, const std::string &propName, std::any propValue)
            {
                UICommand cmd;
                cmd.type = UICommandType::SET_PROPERTY;
                cmd.targetId = id;
                cmd.parentId = INVALID_WIDGET_ID;
                cmd.name = propName;
                cmd.value = std::move(propValue);
                return cmd;
            }

            /**
             * @brief (AR) أمر حذف الشجرة بالكامل
             * @brief (EN) Clear tree command
             */
            static UICommand makeClearTree()
            {
                UICommand cmd;
                cmd.type = UICommandType::CLEAR_TREE;
                cmd.targetId = INVALID_WIDGET_ID;
                cmd.parentId = INVALID_WIDGET_ID;
                return cmd;
            }

            /**
             * @brief (AR) أمر إغلاق
             * @brief (EN) Shutdown command
             */
            static UICommand makeShutdown()
            {
                UICommand cmd;
                cmd.type = UICommandType::SHUTDOWN;
                cmd.targetId = INVALID_WIDGET_ID;
                cmd.parentId = INVALID_WIDGET_ID;
                return cmd;
            }

            /**
             * @brief (AR) أمر مخصص (lambda)
             * @brief (EN) Custom command (lambda)
             */
            static UICommand makeCustom(std::function<void()> fn)
            {
                UICommand cmd;
                cmd.type = UICommandType::CUSTOM;
                cmd.targetId = INVALID_WIDGET_ID;
                cmd.parentId = INVALID_WIDGET_ID;
                cmd.action = std::move(fn);
                return cmd;
            }

            /**
             * @brief (AR) أمر طلب إعادة رسم
             * @brief (EN) Request paint command
             */
            static UICommand makeRequestPaint()
            {
                UICommand cmd;
                cmd.type = UICommandType::REQUEST_PAINT;
                cmd.targetId = INVALID_WIDGET_ID;
                cmd.parentId = INVALID_WIDGET_ID;
                return cmd;
            }

            /**
             * @brief (AR) أمر طلب إعادة تخطيط
             * @brief (EN) Request layout command
             */
            static UICommand makeRequestLayout()
            {
                UICommand cmd;
                cmd.type = UICommandType::REQUEST_LAYOUT;
                cmd.targetId = INVALID_WIDGET_ID;
                cmd.parentId = INVALID_WIDGET_ID;
                return cmd;
            }

        private:
            UICommand()
                : type(UICommandType::CUSTOM), targetId(INVALID_WIDGET_ID), parentId(INVALID_WIDGET_ID)
            {
            }
        };

    } // namespace ui
} // namespace sad

#endif // SAD_UI_COMMAND_H
