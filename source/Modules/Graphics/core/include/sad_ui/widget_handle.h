/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: widget_handle.h
 * المسار: features/graphics/core/include/sad_ui/widget_handle.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * @brief (AR) مقبض عنصر — واجهة آمنة للوصول لعناصر الواجهة من خيط اللغة
 * @brief (EN) Widget handle — safe interface for accessing widgets from language thread
 *
 * WidgetHandle هو "تذكرة" غير مالكة يحملها خيط لغة ص للتعامل مع
 * عنصر واجهة. بدلاً من إعطاء مؤشر خام (خطير — dangling) أو
 * shared_ptr (دورات مرجعية)، نعطي المعرّف فقط.
 *
 * كل عملية على المقبض (تغيير خاصية، إضافة ابن...) تُترجم إلى
 * أمر يُرسل عبر CommandQueue ليُنفَّذ في خيط الواجهة (UI thread).
 *
 * التصميم مستوحى من:
 * - winit WindowId / wgpu DeviceId (Rust)
 * - ECS Entity handle (Unity/Bevy)
 * - ADR-UI-08 في architecture-sadui.md
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_WIDGET_HANDLE_H
#define SAD_UI_WIDGET_HANDLE_H

#include "widget_id.h"

#include <memory>
#include <string>

namespace sad
{
    namespace ui
    {

        /**
         * @brief (AR) مقبض لعنصر واجهة — يحمل فقط المعرّف والنوع
         * @brief (EN) Handle to a widget — holds only ID and type
         *
         * خفيف الوزن — يُنسخ بحرية (copyable, trivial).
         * لا يمنع حذف العنصر — إذا حُذف العنصر يصبح المقبض يتيماً
         * (orphan handle) ويُرجع UIArena nullptr عند البحث بالمعرّف.
         */
        class WidgetHandle
        {
        public:
            /**
             * @brief (AR) مُنشئ مقبض صالح
             * @brief (EN) Constructor for a valid handle
             */
            explicit WidgetHandle(WidgetId id, std::string type)
                : id_(id), type_(std::move(type)), aliveToken_(), hasLivenessToken_(false) {}

            /**
             * @brief (AR) مُنشئ مقبض صالح مع تتبّع عمر العقدة
             * @brief (EN) Constructor for a valid handle with node liveness tracking
             */
            explicit WidgetHandle(WidgetId id, std::string type, std::weak_ptr<uint8_t> aliveToken)
                : id_(id), type_(std::move(type)), aliveToken_(std::move(aliveToken)), hasLivenessToken_(true) {}

            /**
             * @brief (AR) مُنشئ مقبض فارغ (غير صالح)
             * @brief (EN) Default constructor — invalid handle
             */
            WidgetHandle()
                : id_(INVALID_WIDGET_ID), type_(), aliveToken_(), hasLivenessToken_(false) {}

            /** @brief (AR) المعرّف الفريد / (EN) Unique ID */
            WidgetId getId() const { return id_; }

            /** @brief (AR) نوع العنصر / (EN) Widget type */
            const std::string &getType() const { return type_; }

            /** @brief (AR) هل المقبض صالح؟ / (EN) Is the handle valid? */
            bool isValid() const
            {
                if (id_ == INVALID_WIDGET_ID)
                {
                    return false;
                }

                // (AR) إذا كان المقبض مرتبطاً برمز حياة، فصلاحيته تعكس وجود العقدة فعلياً.
                // (EN) If liveness token exists, validity mirrors real node lifetime.
                if (hasLivenessToken_)
                {
                    return !aliveToken_.expired();
                }

                // (AR) مسار التوافق الخلفي: المقابض القديمة تتحقق فقط من المعرّف.
                // (EN) Backward-compat path: legacy handles validate by ID only.
                return true;
            }

            /** @brief (AR) تحويل منطقي — true إذا كان صالحاً / (EN) Boolean conversion */
            explicit operator bool() const { return isValid(); }

            // ─── مقارنة ─────────────────────────────────

            bool operator==(const WidgetHandle &other) const { return id_ == other.id_; }
            bool operator!=(const WidgetHandle &other) const { return id_ != other.id_; }

        private:
            WidgetId id_;                       ///< المعرّف الفريد
            std::string type_;                  ///< نوع العنصر (نسخة — لا مؤشر)
            std::weak_ptr<uint8_t> aliveToken_; ///< رمز حياة العنصر (ينتهي عند الحذف)
            bool hasLivenessToken_;             ///< هل يدعم هذا المقبض تتبّع الحياة الفعلية؟
        };

    } // namespace ui
} // namespace sad

// ── دعم hash لاستخدام WidgetHandle كمفتاح ──
namespace std
{
    template <>
    struct hash<sad::ui::WidgetHandle>
    {
        size_t operator()(const sad::ui::WidgetHandle &h) const noexcept
        {
            return std::hash<uint64_t>{}(h.getId());
        }
    };
} // namespace std

#endif // SAD_UI_WIDGET_HANDLE_H
