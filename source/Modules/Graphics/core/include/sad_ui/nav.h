/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: nav.h
 * المسار: features/graphics/core/include/sad_ui/nav.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * @brief (AR) مكدّس التنقّل — مصدر الحقيقة الوحيد لمنطق تنقّل صفحات SadUI
 * @brief (EN) Navigation stack — single source of truth for SadUI page navigation
 *
 * (AR) منطق التنقّل (دفع/عودة/جذر/استبدال/عمق) يعيش هنا في المكتبة، ويجسر إليه
 *      المحرّكان (المفسّر والمترجم) على «رمز صفحة معتم» void*: المترجم يمرّر
 *      SadWidget، والمفسّر يمرّر مقبضًا إلى صفحته. فلا يعتمد القلب على أنواع
 *      المحرّكات (Data::Value/SadWidget) ⇒ مصدرُ حقيقةٍ واحد بلا تكرار.
 * (EN) Navigation logic lives here in the library; both engines bridge to it over
 *      an opaque void* page token (compiler: SadWidget, interpreter: a handle).
 *      The core stays independent of engine types ⇒ single source of truth.
 *
 * (AR) الملكيّة المُدارة (Q5): كلّ إدخال يحمل مُحرِّرًا اختياريًّا `release` يُستدعى
 *      عند إسقاط الإدخال (عودة/استبدال/جذر) لتحرير بيئة الإغلاق `data`؛ فلا تعلّق
 *      ولا تسريب. الحقول build/data/release مُفعَّلة في نموذج البانِي (م1-ج): إعادة
 *      الرسم من باني الصفحة عند كلّ إطار — انظر خطة التنقّل §4/§9.
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_NAV_H
#define SAD_UI_NAV_H

#include <cstddef>
#include <string>
#include <vector>

namespace sad
{
    namespace ui
    {
        /// (AR) باني صفحة: يُنتج رمز الصفحة عند الطلب (لإعادة الرسم، م1-ج) — مُفعَّل.
        /// (EN) Page builder: produces the page token on demand (for re-render, م1-ج).
        using PageBuilder = void *(*)(void *data);

        /// (AR) مُحرِّر بيئة الإغلاق (ملكيّة مُدارة Q5).
        /// (EN) Closure-environment releaser (managed ownership, Q5).
        using NavRelease = void (*)(void *data);

        /// (AR) مدّة الانتقال البصريّ الافتراضيّة بالثواني (م2) — مصدرٌ واحد لتفادي
        ///      «الأرقام السحريّة» المكرَّرة في التهيئة وreset.
        /// (EN) Default visual-transition duration in seconds (single source).
        inline constexpr float kDefaultTransitionSec = 0.3f;

        /// (AR) إدخال تنقّل واحد. لقطة: `page` رمزٌ معتم (SadWidget/مقبض). بانٍ (م1-ج):
        ///      `build(data)` يُنتج الرمز طازجًا كلّ رسم، و`release(data)` يحرّره عند الإسقاط.
        /// (EN) One navigation entry — snapshot (`page`) or builder (`build`/`data`/`release`).
        struct NavEntry
        {
            void *page = nullptr;         ///< رمز الصفحة المعتم للقطة (SadWidget/handle)
            PageBuilder build = nullptr;  ///< باني الصفحة (م1-ج، إعادة الرسم) — nullptr = لقطة
            void *data = nullptr;         ///< بيئة الإغلاق التي يمرّرها البانِي/يحرّرها release
            NavRelease release = nullptr; ///< مُحرِّر `data` عند الإسقاط (Q5)
        };

        /**
         * @brief (AR) مكدّس تنقّل: يطابق دلالة مكدّس المفسّر بالضبط (ضمانُ تكافؤ).
         * @brief (EN) Navigation stack: mirrors the interpreter stack semantics exactly.
         *
         * (AR) الدلالة (نظير UIBridge): `navigate` يدفع الحاليّة ويجعل الجديدة حاليّةً؛
         *      `back` يستعيد قمّة المكدّس؛ `root` يستعيد قاع المكدّس ويُفرغه؛ `replace`
         *      يبدّل الحاليّة بلا دفع. `depth` = حجم المكدّس (عدد ما يُعاد إليه).
         *
         * (AR) فخّ الحالة الابتدائيّة (مقصود، متكافئ مع المفسّر): إن استُدعي `navigate`
         *      و`current_` ما زالت فارغة (لم تُعيَّن أوّل صفحة بعد)، يُدفَع إدخالٌ فارغ
         *      يصير قاع المكدّس، فـ`root` لاحقًا يعود إلى صفحةٍ فارغة. هذا مطابقٌ
         *      لـUIBridge::navigateTo الذي يدفع `rootWidget_` الابتدائيّ (قد يكون فارغًا).
         *
         * (AR) الملكيّة (Q5): RAII كامل — المُدمِّر يحرّر ما يملكه، والنسخ محذوف (كائنٌ
         *      يملك موارد). يمكن أيضًا استدعاء `reset()` يدويًّا (idempotent). يعمل بأمان
         *      للـaccessor العامّ (static) ولترقية «مُلاح لكلّ App» (§Q4).
         */
        class NavStack
        {
        public:
            NavStack() = default;

            /// (AR) RAII: يحرّر كلّ بيئات الإغلاق المملوكة عند الإتلاف (يستدعي reset).
            ///      idempotent مع reset (release يُصفَّر بعد أوّل تحرير). ضروريّ لترقية
            ///      «مُلاح لكلّ App» (§Q4) حيث تُتلَف المُلاحات فعليًّا، ومقبولٌ للـstatic.
            /// (EN) RAII: releases all owned closure envs on destruction. Idempotent
            ///      with reset. Needed for per-App upgrade; harmless for the static.
            ~NavStack() { reset(); }

            NavStack(const NavStack &) = delete;
            NavStack &operator=(const NavStack &) = delete;

            /// انتقل: ادفع الحاليّة إلى المكدّس واجعل `e` هي الحاليّة.
            void navigate(const NavEntry &e);

            /// عودة: استعد قمّة المكدّس كحاليّة. يُرجع false إن كان المكدّس فارغًا.
            [[nodiscard]] bool back();

            /// عودة_للبداية: استعد قاع المكدّس (أوّل صفحة) وأفرغ المكدّس.
            void root();

            /// استبدل: بدّل الحاليّة بـ`e` دون دفع (تُحرَّر الحاليّة المُسقَطة).
            void replace(const NavEntry &e);

            /// عدد_الصفحات: حجم المكدّس (عدد الصفحات القابلة للعودة إليها).
            [[nodiscard]] std::size_t depth() const { return stack_.size(); }

            /// الصفحة الحاليّة (رمزها المعتم وحقولها). صالحة دائمًا؛ قبل أوّل
            /// `navigate`/`replace` تكون إدخالًا فارغًا (page=nullptr) لا مؤشّرًا معلّقًا.
            [[nodiscard]] const NavEntry &current() const { return current_; }

            /// (م1-ج، توقيع البانِي) ابنِ الصفحة الحاليّة وأعِد رمزها المعتم:
            ///   • إن حمل الإدخال بانيًا (`build != nullptr`) فاستدعِه `build(data)` ⇒
            ///     شجرةٌ **طازجةٌ** عند كلّ رسم ⇒ تفاعليّةٌ داخل الصفحة (نموذج routes).
            ///   • وإلّا فأعِد اللقطة المخزّنة `page` (نموذج اللقطة، توافقٌ خلفيّ).
            /// (AR) هذا **مصدر الرسم الوحيد** الذي تستهلكه حلقتا النافذة (المترجم)
            ///      وكشفُ الجذر (الصفحة_الحالية headless) في المحرّكين ⇒ توحيد المسار:
            ///      العمقُ والبنيةُ والرسمُ الحيُّ كلُّها من إدخال nav واحد لا قناتين.
            /// (EN) The single render source consumed by the window loop (compiler) and
            ///      root detection (الصفحة_الحالية, headless) in both engines ⇒ unified path.
            [[nodiscard]] void *buildCurrent() const
            {
                if (current_.build)
                    return current_.build(current_.data);
                return current_.page;
            }

            /// (AR) هل تغيّرت الصفحة الحاليّة منذ آخر فحص؟ (م1-ب) — حلقة النافذة تستهلكه
            ///      لإعادة الرسم من الصفحة الجديدة. يُصفَّر عند القراءة. كلّ عمليّة تُغيّر
            ///      الحاليّة (navigate/back/root/replace) تضبطه. headless: يُضبَط ولا
            ///      يُستهلَك (لا حلقة) ⇒ لا أثر على تكافؤ الحالة.
            /// (EN) Did the current page change since last check? Consumed on read by the
            ///      window loop to re-render. Set by navigate/back/root/replace.
            /// (AR) قد يُستدعى أيضًا لاستهلاك العَلَم الابتدائيّ وتجاهُل نتيجته (تصفير فقط)،
            ///      لذا لا نَسِمه [[nodiscard]].
            bool takeDirty()
            {
                const bool wasDirty = dirty_;
                dirty_ = false;
                return wasDirty;
            }

            /// (AR) علّم إعادة رسمٍ مطلوبة دون تغيير المكدّس (تحديث_حالة/عين_الحالة):
            ///      حلقة النافذة تستهلكه (takeDirty) فتعيد بناء الصفحة الحاليّة عبر
            ///      buildCurrent ⇒ يلتقط تغيّر الحالة المُلتقَطة في بانِي الصفحة. headless:
            ///      يُضبَط ولا يُستهلَك (لا حلقة) ⇒ لا أثر على تكافؤ الحالة. نظير المفسّر
            ///      (UIBridge::rebuildUI) الذي تستدعيه تحديث_حالة/عين_الحالة.
            /// (EN) Mark a redraw as needed without changing the stack (setState): the
            ///      window loop consumes it (takeDirty) and rebuilds the current page via
            ///      buildCurrent. Headless: set but not consumed ⇒ no parity effect.
            void markDirty() { dirty_ = true; }

            /// (م2) اضبط انتقالًا بصريًّا مُعلَّقًا للتبديل التالي (نوع + مدة بالثواني).
            ///      تستهلكه حلقة النافذة (setContentWithTransition). لا أثر headless.
            void setPendingTransition(const std::string &type, float durationSec)
            {
                pendingTransType_ = type;
                pendingDuration_ = durationSec;
            }
            /// (م2) خذ الانتقال المُعلَّق (يُفرَّغ). يُرجع false إن لا انتقال.
            [[nodiscard]] bool takePendingTransition(std::string &type, float &durationSec)
            {
                if (pendingTransType_.empty())
                    return false;
                type = pendingTransType_;
                durationSec = pendingDuration_;
                pendingTransType_.clear();
                return true;
            }

            /// (AR) إعادة تهيئة كاملة (تحرّر كلّ ما تملكه) — للتطبيق/الاختبار.
            void reset();

        private:
            /// (AR) استدعِ مُحرِّر الإدخال إن وُجد (ملكيّة مُدارة).
            static void releaseEntry(NavEntry &e);

            std::vector<NavEntry> stack_; ///< مصدر الحقيقة: الصفحات القابلة للعودة
            NavEntry current_{};          ///< الصفحة الحاليّة
            bool dirty_ = false;          ///< (م1-ب) تغيّرت الحاليّة ⇒ إعادة رسم مطلوبة
            std::string pendingTransType_;///< (م2) انتقال بصريّ مُعلَّق (فارغ = لا انتقال)
            float pendingDuration_ = kDefaultTransitionSec;///< (م2) مدة الانتقال المُعلَّق بالثواني
        };

        /**
         * @brief (AR) المُلاح النشط (accessor عامّ مؤقّت).
         * @brief (EN) The active navigation stack (temporary global accessor).
         *
         * (AR) عامٌّ الآن (نظير حالة الثيم s_currentTheme، وواقع النافذة الواحدة).
         *      يُرقَّى لاحقًا ليحلّ «التطبيق النشط» فيُرجع مُلاحه — دون تغيير مواقع
         *      الاستدعاء (خطة التنقّل §9/Q4: مُلاحات متداخلة لكلّ App).
         *
         * (AR) ⚠ فخّ الحالة الحيّة (state carry-over): المُلاح static فيتراكم مكدّسه
         *      عبر تشغيلاتٍ متعدّدة **داخل العمليّة الواحدة**. الرَنر الحاليّ يشغّل كلّ
         *      برنامج في عمليّة منفصلة (subprocess) ⇒ يبدأ نظيفًا، فالتكافؤ سليم.
         *      لكن أيّ مستهلك يشغّل برنامجين في عمليّة واحدة (REPL/اختبار وحدة C++
         *      يعيد استخدام المفسّر أو يستدعي Interpreter::reset) **يجب** أن يستدعي
         *      `nav().reset()` عند بدء كلّ برنامج، وإلّا صار العمق الابتدائيّ ≠ 0
         *      (انحراف عمق صامت). بخلاف الثيم (ضبط idempotent) المكدّس تراكميّ.
         */
        NavStack &nav();

    } // namespace ui
} // namespace sad

#endif // SAD_UI_NAV_H
