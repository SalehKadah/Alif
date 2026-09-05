/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: signal.h
 * المسار: features/graphics/core/include/sad_ui/signal.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * @brief (AR) إشارة تفاعلية — الوحدة الذرية في نظام التفاعل الدقيق
 * @brief (EN) Reactive signal — atomic unit in the fine-grained reactivity system
 *
 * Signal<T> هو حاوية لقيمة واحدة تُخطر مراقبيها تلقائياً عند التغيير.
 *
 * نظام التفاعل الدقيق (Fine-Grained Reactivity — ADR-UI-10):
 * ──────────────────────────────────────────────────────────
 * بدلاً من مقارنة شجرة كاملة (Virtual DOM diff)، كل خاصية
 * مربوطة بإشارة. عند تغيير الإشارة:
 *
 *   signal.set(newValue)
 *       ↓
 *   notify observers
 *       ↓
 *   PropertyBinding يُرسل SET_PROPERTY عبر CommandQueue
 *       ↓
 *   خيط الواجهة يُحدّث العنصر المحدد فقط
 *
 * هذا يتجنب:
 *   1. بناء شجرة افتراضية كاملة (O(n) → O(1))
 *   2. مقارنة diff (O(n) → 0)
 *   3. تصحيح patches (O(changes) → O(1))
 *
 * الإشارة تعيش في خيط اللغة وتُقرأ/تُكتب من خيط اللغة فقط.
 * الإخطار يُنتج أوامر تذهب عبر CommandQueue (آمن للخيوط).
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_SIGNAL_H
#define SAD_UI_SIGNAL_H

#include <functional>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace sad
{
    namespace ui
    {

        /**
         * @brief (AR) معرّف اشتراك — يُستخدم لإلغاء الاشتراك لاحقاً
         * @brief (EN) Subscription ID — used to unsubscribe later
         */
        using SubscriptionId = uint64_t;

        /**
         * @brief (AR) معرّف اشتراك غير صالح
         * @brief (EN) Invalid subscription ID
         */
        static constexpr SubscriptionId INVALID_SUBSCRIPTION_ID = 0;

        /**
         * @brief (AR) إشارة تفاعلية من النوع T
         * @brief (EN) Reactive signal of type T
         *
         * @tparam T نوع القيمة المخزنة
         *
         * مثال استخدام:
         * ─────────────
         *   Signal<std::string> title("عنوان");
         *
         *   auto subId = title.subscribe([](const std::string& newVal) {
         *       std::cout << "تغيّر العنوان إلى: " << newVal << std::endl;
         *   });
         *
         *   title.set("عنوان جديد");  // يُطلق الإخطار
         *
         *   title.unsubscribe(subId);  // إلغاء الاشتراك
         */
        template <typename T>
        class Signal
        {
        public:
            /// (AR) دالة المراقب — تستقبل القيمة الجديدة
            /// (EN) Observer callback — receives new value
            using Observer = std::function<void(const T &newValue)>;

            /**
             * @brief (AR) مُنشئ بقيمة ابتدائية
             * @brief (EN) Constructor with initial value
             */
            explicit Signal(T initialValue = T{})
                : value_(std::move(initialValue)), nextSubId_(1)
            {
            }

            /**
             * @brief (AR) قراءة القيمة الحالية
             * @brief (EN) Read current value
             */
            const T &get() const { return value_; }

            /**
             * @brief (AR) تعيين قيمة جديدة — يُخطر المراقبين إذا تغيّرت
             * @brief (EN) Set new value — notifies observers if changed
             *
             * @param newValue القيمة الجديدة
             * @return true إذا تغيّرت القيمة فعلاً وتم الإخطار
             */
            bool set(T newValue)
            {
                if (value_ == newValue)
                {
                    return false; // لم تتغيّر — لا إخطار
                }

                value_ = std::move(newValue);
                notifyAll();
                return true;
            }

            /**
             * @brief (AR) تعيين قيمة بالقوة — يُخطر المراقبين حتى لو لم تتغيّر
             * @brief (EN) Force set — notifies observers even if value unchanged
             */
            void forceSet(T newValue)
            {
                value_ = std::move(newValue);
                notifyAll();
            }

            /**
             * @brief (AR) تعديل القيمة عبر دالة — يُخطر المراقبين
             * @brief (EN) Modify value via function — notifies observers
             *
             * @param mutator دالة تستقبل مرجعاً للقيمة وتعدّلها
             */
            void modify(std::function<void(T &)> mutator)
            {
                if (mutator)
                {
                    mutator(value_);
                    notifyAll();
                }
            }

            /**
             * @brief (AR) الاشتراك للإخطار عند التغيير
             * @brief (EN) Subscribe to change notifications
             *
             * @param observer دالة المراقبة
             * @return معرّف الاشتراك (لإلغائه لاحقاً)
             */
            SubscriptionId subscribe(Observer observer)
            {
                if (!observer)
                {
                    return INVALID_SUBSCRIPTION_ID;
                }

                SubscriptionId id = nextSubId_++;
                subscriptions_.push_back({id, std::move(observer)});
                return id;
            }

            /**
             * @brief (AR) إلغاء اشتراك بالمعرّف
             * @brief (EN) Unsubscribe by ID
             *
             * @param id معرّف الاشتراك
             * @return true إذا وُجد الاشتراك وأُلغي
             */
            bool unsubscribe(SubscriptionId id)
            {
                auto it = std::find_if(subscriptions_.begin(), subscriptions_.end(),
                                       [id](const Subscription &sub)
                                       { return sub.id == id; });

                if (it == subscriptions_.end())
                {
                    return false;
                }

                subscriptions_.erase(it);
                return true;
            }

            /**
             * @brief (AR) إلغاء جميع الاشتراكات
             * @brief (EN) Clear all subscriptions
             */
            void clearSubscriptions()
            {
                subscriptions_.clear();
            }

            /**
             * @brief (AR) عدد المشتركين الحاليين
             * @brief (EN) Number of current subscribers
             */
            size_t subscriberCount() const { return subscriptions_.size(); }

        private:
            /**
             * @brief (AR) إخطار جميع المراقبين بالقيمة الحالية
             * @brief (EN) Notify all observers with current value
             */
            void notifyAll()
            {
                // (AR) نُكرر على نسخة من المتجه لأن المراقب قد يضيف/يزيل اشتراكات
                //      أثناء التنفيذ (reentrancy safety)
                // (EN) Iterate over a copy because observers may add/remove subscriptions
                //      during execution (reentrancy safety)
                auto snapshot = subscriptions_;
                for (const auto &sub : snapshot)
                {
                    sub.observer(value_);
                }
            }

            /// (AR) بنية الاشتراك الداخلية
            /// (EN) Internal subscription structure
            struct Subscription
            {
                SubscriptionId id;
                Observer observer;
            };

            T value_;                                 ///< القيمة الحالية
            std::vector<Subscription> subscriptions_; ///< قائمة المشتركين
            SubscriptionId nextSubId_;                ///< المعرّف التالي
        };

    } // namespace ui
} // namespace sad

#endif // SAD_UI_SIGNAL_H
