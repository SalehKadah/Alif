/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: window_control.h
 * المسار: features/graphics/core/include/sad_ui/window_control.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * @brief (AR) متحكّم النافذة — مصدر الحقيقة الوحيد لعمليّات النافذة الحيّة
 *        (عنوان النافذة، طلب الإغلاق) المشتركة بين المحرّكين.
 * @brief (EN) Window controller — single source of truth for live window ops
 *        (set title, request close) shared by both engines.
 *
 * (AR) النموذج (نظير sad::ui::nav المؤجَّل): كلا المحرّكين (المفسّر والمترجم) يكتبان
 *      العمليّة المطلوبة إلى متحكّمٍ عامٍّ واحد، وحلقة النافذة المشتركة (المكتبة —
 *      DesktopWindow::run) تستهلكها كلّ إطار فتطبّقها على النافذة الفعليّة. فلا يعتمد
 *      القلب على أنواع المحرّكات ولا على SDL ⇒ مصدرُ حقيقةٍ واحد بلا تفريعٍ لكلّ محرّك،
 *      وسلوكٌ موحَّد بالبناء.
 * (EN) Model (mirrors the deferred sad::ui::nav): both engines write the requested
 *      op to one global controller; the shared window loop (library — DesktopWindow::run)
 *      consumes it each frame and applies it. Engine/SDL-independent ⇒ single source of
 *      truth, no per-engine branching, unified behavior by construction.
 *
 * (AR) headless: تُكتب العمليّات ولا تُستهلَك (لا نافذة/حلقة) ⇒ لا أثر على التكافؤ.
 *      (نظير nav.dirty/pendingTransition تمامًا.)
 * (EN) Headless: ops are written but never consumed (no window/loop) ⇒ no parity effect.
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_WINDOW_CONTROL_H
#define SAD_UI_WINDOW_CONTROL_H

#include <string>

namespace sad
{
    namespace ui
    {
        /**
         * @brief (AR) متحكّم نافذة مؤجَّل: يخزّن عمليّةً مُعلَّقة (عنوان/إغلاق) تستهلكها
         *        حلقة النافذة. الكتابة من أيّ محرّك؛ الاستهلاك من المكتبة وحدها.
         * @brief (EN) Deferred window controller: stores a pending op (title/close)
         *        consumed by the window loop. Written by any engine; consumed by the library.
         */
        class WindowController
        {
        public:
            WindowController() = default;

            /// (AR) اطلب تغيير عنوان النافذة (يُطبَّق في الإطار التالي).
            void setTitle(const std::string &title)
            {
                pendingTitle_ = title;
                hasPendingTitle_ = true;
            }

            /// (AR) اطلب إغلاق النافذة (يُنفَّذ في الإطار التالي).
            void requestClose() { closeRequested_ = true; }

            /// (AR) خذ العنوان المُعلَّق إن وُجد (يُفرَّغ). يُرجع false إن لا عنوان مُعلَّق.
            [[nodiscard]] bool takePendingTitle(std::string &out)
            {
                if (!hasPendingTitle_)
                    return false;
                out = pendingTitle_;
                hasPendingTitle_ = false;
                return true;
            }

            /// (AR) خذ طلب الإغلاق (يُفرَّغ). يُرجع true إن كان الإغلاق مطلوبًا.
            [[nodiscard]] bool takeCloseRequest()
            {
                const bool requested = closeRequested_;
                closeRequested_ = false;
                return requested;
            }

            /// (AR) إعادة تهيئة (للتطبيق/الاختبار) — تُستدعى عند إنهاء كلّ تشغيل نافذة
            ///      لئلّا تتسرّب عمليّةٌ مُعلَّقة إلى تشغيلٍ لاحقٍ في العمليّة نفسها.
            void reset()
            {
                pendingTitle_.clear();
                hasPendingTitle_ = false;
                closeRequested_ = false;
            }

        private:
            std::string pendingTitle_;    ///< العنوان المُعلَّق (صالح إن hasPendingTitle_)
            bool hasPendingTitle_ = false; ///< هل ثمّة عنوان مُعلَّق؟ (يميّز العنوان الفارغ المقصود)
            bool closeRequested_ = false;  ///< هل طُلِب الإغلاق؟
        };

        /**
         * @brief (AR) المتحكّم النشط (accessor عامّ) — نظير sad::ui::nav().
         * @brief (EN) The active window controller (global accessor) — mirrors nav().
         *
         * (AR) مُسرَّبٌ عمدًا (never-destroyed) نظير nav(): يُستهلَك من حلقة النافذة
         *      (static في backend) وقد يُكتَب إليه من statics أخرى؛ الإسراب يتفادى
         *      اعتماديّة ترتيب إتلاف statics عبر الوحدات. الذاكرة تُستردّ بإنهاء العمليّة.
         */
        WindowController &windowController();

    } // namespace ui
} // namespace sad

#endif // SAD_UI_WINDOW_CONTROL_H
