/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: nav.cpp
 * المسار: features/graphics/core/src/nav.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * @brief (AR) تنفيذ مكدّس التنقّل (مصدر الحقيقة المشترك) — انظر nav.h.
 * @brief (EN) Navigation stack implementation (shared source of truth) — see nav.h.
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/nav.h"

namespace sad
{
    namespace ui
    {
        void NavStack::releaseEntry(NavEntry &e)
        {
            // (AR) ملكيّة مُدارة (Q5): حرّر بيئة الإغلاق مرّةً واحدة ثمّ صفّر لمنع
            //      التحرير المزدوج. الإدخالات بلا مُحرِّر (عمق-فقط) لا-عمل.
            if (e.release && e.data)
                e.release(e.data);
            e.release = nullptr;
            e.data = nullptr;
        }

        void NavStack::navigate(const NavEntry &e)
        {
            // (AR) ادفع الحاليّة (تنتقل ملكيّتها إلى المكدّس) واجعل `e` الحاليّة.
            //      نظير UIBridge::navigateTo: push(rootWidget_) ثمّ rootWidget_=page.
            //      ننقل current_ إلى المكدّس (لا نسخة) إذ نُعيد تعيينه فورًا من `e`.
            stack_.push_back(std::move(current_));
            current_ = e;
            dirty_ = true;
        }

        bool NavStack::back()
        {
            // (AR) نظير UIBridge::navigateBack: إن فرغ المكدّس فلا عودة.
            if (stack_.empty())
                return false;
            // (AR) الصفحة الحاليّة تُسقَط عند العودة ⇒ تُحرَّر ملكيّتها.
            releaseEntry(current_);
            current_ = std::move(stack_.back());
            stack_.pop_back();
            dirty_ = true;
            return true;
        }

        void NavStack::root()
        {
            // (AR) نظير UIBridge::navigateToRoot: إن فرغ المكدّس فلا شيء.
            if (stack_.empty())
                return;
            // (AR) تُحرَّر الحاليّة وكلّ الوسيطات؛ يبقى قاع المكدّس (أوّل صفحة، stack_[0])
            //      الذي يصير الحاليّة — فلا نُحرّره (نبدأ الحلقة من 1، لا من 0).
            releaseEntry(current_);
            for (std::size_t i = 1; i < stack_.size(); ++i)
                releaseEntry(stack_[i]);
            current_ = std::move(stack_.front());
            stack_.clear();
            dirty_ = true;
        }

        void NavStack::replace(const NavEntry &e)
        {
            // (AR) نظير UIBridge::replacePage: بدّل الحاليّة بلا دفع (تُحرَّر المُسقَطة).
            releaseEntry(current_);
            current_ = e;
            dirty_ = true;
        }

        void NavStack::reset()
        {
            releaseEntry(current_);
            for (auto &e : stack_)
                releaseEntry(e);
            stack_.clear();
            current_ = NavEntry{};
            dirty_ = false;
            pendingTransType_.clear();
            pendingDuration_ = kDefaultTransitionSec; // (م2) صفّر المدّة أيضًا لاتّساق reset الكامل (الحارس = النوع الفارغ)
        }

        NavStack &nav()
        {
            // (AR) مُلاحٌ عامٌّ واحد (نظير s_currentTheme؛ واقع النافذة الواحدة).
            //      يُرقَّى لاحقًا إلى مُلاح «التطبيق النشط» (§9/Q4).
            //      **مُسرَّبٌ عمدًا** (never-destroyed): لا يعمل مُدمِّره عند خروج البرنامج،
            //      فلا يُستدعى `release` (الذي قد يشير إلى statics في وحدة أخرى — المفسّر)
            //      أثناء إتلاف statics ذي الترتيب غير المحدَّد ⇒ لا UB عند الخروج. المُدمِّر
            //      يبقى للمثيلات المستقبليّة لكلّ App (تُتلَف أثناء التشغيل، لا عند الخروج).
            static NavStack *instance = new NavStack();
            return *instance;
        }

    } // namespace ui
} // namespace sad
