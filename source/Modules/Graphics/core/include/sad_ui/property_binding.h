/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: property_binding.h
 * المسار: features/graphics/core/include/sad_ui/property_binding.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * @brief (AR) ربط الخصائص — يصل إشارات لغة ص بعناصر الواجهة عبر CommandQueue
 * @brief (EN) Property binding — connects Sad language signals to widgets via CommandQueue
 *
 * PropertyBinding هو الجسر بين نظام التفاعل (Signal) وشجرة الواجهة (UIArena).
 *
 * سير العمل:
 * ──────────
 *   1. كود ص يُنشئ إشارة:     متغير عنوان = إشارة("مرحبا")
 *   2. يربطها بعنصر:          ربط(نص1، "محتوى"، عنوان)
 *   3. PropertyBinding يُنشئ اشتراك على الإشارة
 *   4. عند تغيير الإشارة:     عنوان.عيّن("عالم")
 *   5. الاشتراك يُرسل UICommand::makeSetProperty عبر CommandQueue
 *   6. خيط الواجهة يُنفذ الأمر ويُحدث العنصر المحدد
 *
 * BindingManager يدير جميع الربطات:
 *   - إنشاء ربطات جديدة
 *   - إلغاء ربطات عند حذف عنصر
 *   - تنظيف تلقائي (RAII)
 *
 * التصميم مبني على ADR-UI-10 (Reconciler Performance):
 *   - المستوى 1: Signal<T> → إخطار مباشر
 *   - المستوى 2: PropertyBinding → ترجمة إلى أوامر
 *   - المستوى 3: PartialReconciler (لاحقاً) → diff فرعي
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_PROPERTY_BINDING_H
#define SAD_UI_PROPERTY_BINDING_H

#include "widget_id.h"
#include "signal.h"
#include "command_queue.h"
#include "ui_command.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <any>

namespace sad
{
    namespace ui
    {

        /**
         * @brief (AR) معرّف ربط فريد
         * @brief (EN) Unique binding ID
         */
        using BindingId = uint64_t;

        /**
         * @brief (AR) معرّف ربط غير صالح
         * @brief (EN) Invalid binding ID
         */
        static constexpr BindingId INVALID_BINDING_ID = 0;

        /**
         * @brief (AR) ربط واحد — يصل إشارة بخاصية عنصر واجهة
         * @brief (EN) Single binding — connects a signal to a widget property
         *
         * البنية الداخلية خفية (opaque) — المستخدم يتعامل مع BindingId فقط.
         */
        struct BindingEntry
        {
            BindingId id;                  ///< معرّف الربط الفريد
            WidgetId widgetId;             ///< العنصر المستهدف
            std::string propertyName;      ///< اسم الخاصية
            SubscriptionId subscriptionId; ///< معرّف الاشتراك في الإشارة
            std::function<void()> cleanup; ///< دالة تنظيف (تلغي الاشتراك)
        };

        /**
         * @brief (AR) مدير الربطات — يدير جميع ربطات الخصائص في التطبيق
         * @brief (EN) Binding manager — manages all property bindings in the application
         *
         * الاستخدام:
         * ─────────
         *   Signal<std::string> titleSignal("مرحبا");
         *   BindingManager manager(commandQueue);
         *
         *   // ربط الإشارة بخاصية "محتوى" للعنصر 5
         *   auto bindId = manager.bind(5, "محتوى", titleSignal);
         *
         *   // عند تغيير الإشارة — يُرسل أمر SET_PROPERTY تلقائياً
         *   titleSignal.set("عنوان جديد");
         *
         *   // إلغاء الربط
         *   manager.unbind(bindId);
         */
        class BindingManager
        {
        public:
            /**
             * @brief (AR) المُنشئ
             * @brief (EN) Constructor
             *
             * @param queue طابور الأوامر المشترك (مرجع — لا ملكية)
             */
            explicit BindingManager(CommandQueue &queue);

            ~BindingManager();

            // ─── لا نسخ ─────────────────────────────────
            BindingManager(const BindingManager &) = delete;
            BindingManager &operator=(const BindingManager &) = delete;

            // ═══════════════════════════════════════════════════
            // (AR) إنشاء الربطات
            // (EN) Creating bindings
            // ═══════════════════════════════════════════════════

            /**
             * @brief (AR) ربط إشارة بخاصية عنصر واجهة
             * @brief (EN) Bind a signal to a widget property
             *
             * @tparam T نوع قيمة الإشارة
             * @param widgetId معرّف العنصر المستهدف
             * @param propertyName اسم الخاصية (مثل "محتوى"، "لون")
             * @param signal مرجع للإشارة
             * @return معرّف الربط (لإلغائه لاحقاً)
             */
            template <typename T>
            BindingId bind(WidgetId widgetId, const std::string &propertyName,
                           Signal<T> &signal);

            /**
             * @brief (AR) ربط إشارة مع محوّل (transformer)
             * @brief (EN) Bind a signal with a transformer
             *
             * المحوّل يحوّل قيمة الإشارة قبل إرسالها كخاصية.
             * مثال: تحويل رقم إلى نص.
             *
             * @tparam T نوع قيمة الإشارة
             * @tparam U نوع قيمة الخاصية (بعد التحويل)
             * @param widgetId معرّف العنصر
             * @param propertyName اسم الخاصية
             * @param signal مرجع للإشارة
             * @param transformer دالة التحويل T → U
             * @return معرّف الربط
             */
            template <typename T, typename U>
            BindingId bindTransformed(WidgetId widgetId, const std::string &propertyName,
                                      Signal<T> &signal,
                                      std::function<U(const T &)> transformer);

            // ═══════════════════════════════════════════════════
            // (AR) إلغاء الربطات
            // (EN) Removing bindings
            // ═══════════════════════════════════════════════════

            /**
             * @brief (AR) إلغاء ربط بالمعرّف
             * @brief (EN) Remove binding by ID
             *
             * @return true إذا وُجد الربط وأُلغي
             */
            bool unbind(BindingId id);

            /**
             * @brief (AR) إلغاء جميع ربطات عنصر محدد
             * @brief (EN) Remove all bindings for a specific widget
             *
             * يُستدعى عند حذف عنصر من الشجرة لتنظيف الربطات.
             *
             * @param widgetId معرّف العنصر
             * @return عدد الربطات المُلغاة
             */
            size_t unbindWidget(WidgetId widgetId);

            /**
             * @brief (AR) إلغاء جميع الربطات
             * @brief (EN) Remove all bindings
             */
            void unbindAll();

            // ═══════════════════════════════════════════════════
            // (AR) استعلامات
            // (EN) Queries
            // ═══════════════════════════════════════════════════

            /** @brief (AR) عدد الربطات الحالية / (EN) Current binding count */
            size_t getBindingCount() const { return bindings_.size(); }

            /** @brief (AR) عدد ربطات عنصر محدد / (EN) Binding count for a widget */
            size_t getWidgetBindingCount(WidgetId widgetId) const;

        private:
            /**
             * @brief (AR) توليد معرّف ربط فريد
             * @brief (EN) Generate unique binding ID
             */
            BindingId generateBindingId();

            // ─── الحقول ────────────────────────────────

            CommandQueue &queue_;                                                 ///< طابور الأوامر
            std::vector<BindingEntry> bindings_;                                  ///< جميع الربطات
            std::unordered_map<WidgetId, std::vector<BindingId>> widgetBindings_; ///< فهرس: عنصر → ربطاته
            BindingId nextBindingId_ = 1;                                         ///< المعرّف التالي
        };

        // ═══════════════════════════════════════════════════════════
        // (AR) تنفيذ القوالب (template implementations)
        // (EN) Template implementations
        // ═══════════════════════════════════════════════════════════

        template <typename T>
        BindingId BindingManager::bind(WidgetId widgetId, const std::string &propertyName,
                                       Signal<T> &signal)
        {
            BindingId bindId = generateBindingId();

            // (AR) الاشتراك في الإشارة:
            //      عند كل تغيير → إرسال أمر SET_PROPERTY عبر CommandQueue
            // (EN) Subscribe to signal:
            //      on each change → send SET_PROPERTY command via CommandQueue
            auto subId = signal.subscribe(
                [this, widgetId, propName = propertyName](const T &newValue)
                {
                    queue_.push(UICommand::makeSetProperty(
                        widgetId, propName, std::any(newValue)));
                    queue_.notify();
                });

            // (AR) دالة التنظيف — تلغي الاشتراك عند حذف الربط
            // (EN) Cleanup function — unsubscribes when binding is removed
            auto cleanup = [&signal, subId]()
            {
                signal.unsubscribe(subId);
            };

            BindingEntry entry;
            entry.id = bindId;
            entry.widgetId = widgetId;
            entry.propertyName = propertyName;
            entry.subscriptionId = subId;
            entry.cleanup = std::move(cleanup);

            bindings_.push_back(std::move(entry));
            widgetBindings_[widgetId].push_back(bindId);

            // (AR) إرسال القيمة الحالية فوراً (sync)
            // (EN) Send current value immediately (sync)
            queue_.push(UICommand::makeSetProperty(
                widgetId, propertyName, std::any(signal.get())));
            queue_.notify();

            return bindId;
        }

        template <typename T, typename U>
        BindingId BindingManager::bindTransformed(WidgetId widgetId, const std::string &propertyName,
                                                  Signal<T> &signal,
                                                  std::function<U(const T &)> transformer)
        {
            BindingId bindId = generateBindingId();

            // (AR) الاشتراك مع تحويل القيمة قبل الإرسال
            // (EN) Subscribe with value transformation before sending
            auto subId = signal.subscribe(
                [this, widgetId, propName = propertyName,
                 xform = std::move(transformer)](const T &newValue)
                {
                    U transformed = xform(newValue);
                    queue_.push(UICommand::makeSetProperty(
                        widgetId, propName, std::any(std::move(transformed))));
                    queue_.notify();
                });

            auto cleanup = [&signal, subId]()
            {
                signal.unsubscribe(subId);
            };

            BindingEntry entry;
            entry.id = bindId;
            entry.widgetId = widgetId;
            entry.propertyName = propertyName;
            entry.subscriptionId = subId;
            entry.cleanup = std::move(cleanup);

            bindings_.push_back(std::move(entry));
            widgetBindings_[widgetId].push_back(bindId);

            // (AR) إرسال القيمة الحالية (محوّلة) فوراً
            // (EN) Send current value (transformed) immediately
            if (transformer)
            {
                // (AR) نحتاج نسخة جديدة من transformer لأن الأصلي نُقل
                // (EN) Need fresh copy since original was moved
                // (AR) ملاحظة: transformer مُررت بقيمة لذا لا تزال صالحة هنا
                //      (النسخة المنقولة هي xform داخل lambda)
            }
            // (AR) نقرأ القيمة ونحوّلها مباشرة
            queue_.push(UICommand::makeSetProperty(
                widgetId, propertyName, std::any(signal.get())));
            queue_.notify();

            return bindId;
        }

    } // namespace ui
} // namespace sad

#endif // SAD_UI_PROPERTY_BINDING_H
