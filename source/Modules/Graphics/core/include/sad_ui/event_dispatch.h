/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: event_dispatch.h
 * المسار: features/graphics/core/include/sad_ui/event_dispatch.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * (AR) إرسال الأحداث عبر شجرة العناصر — مصدر الحقيقة الوحيد لأطوار الانتشار.
 *
 * كان منطق التفرّع محبوسًا في جسر سطح المكتب وحده، فالوضع الحرّ والمفسّر
 * وبقيّة الخلفيّات تُطلق على العقدة المُصابة مباشرةً بلا تفرّع. هذا الملفّ
 * يرفع المنطق إلى المكتبة المشتركة كي تسلكه كلّ المنصّات بلا ازدواج.
 *
 * الأطوار الثلاثة (نموذج DOM):
 *   ١) الالتقاط (Capture): من الجذر نزولًا إلى الأب المباشر — تُطلَق معالِجات
 *      الأجداد المُعلَنة «التقاط» فقط.
 *   ٢) الهدف: تُطلَق **كلّ** معالِجات العقدة المُصابة المطابقة للنوع.
 *   ٣) الفقاعات (Bubble): من الأب صعودًا إلى الجذر — تُطلَق معالِجات الأجداد
 *      المُعلَنة «تفرّع» فقط.
 *
 * الطور مُعلَن على **المعالِج** (IREvent::propagation) لا على الحدث، تمامًا
 * كنموذج DOM. والافتراض `None` ⇒ المعالِج لا يشارك في انتشار أبنائه، فسلوك
 * كلّ البرامج القائمة محفوظ (اشتراكٌ صريح لا ضمنيّ).
 *
 * يُحترم `EventData::propagationStopped` بين كلّ معالِجَين وفي كلّ طور.
 *
 * (EN) Single source of truth for event propagation across the element tree.
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_EVENT_DISPATCH_H
#define SAD_UI_EVENT_DISPATCH_H

#include "sad_ui/ir.h"
#include "sad_ui/types.h"

#include <functional>
#include <string>

namespace sad
{
    namespace ui
    {
        /**
         * @brief (AR) مصرف الحدث: يُنفّذ معالِجًا واحدًا مُحدَّدًا على عقدة بعينها.
         *
         * الاختيار (أيّ معالِج على أيّ عقدة) مسؤوليّة `dispatchEvent`؛ ومهمّة
         * المصرف تنفيذُ ما اختير فقط — فلا يكرّر المستهلك منطق المرور على
         * الأحداث (وهو ما سبّب تباعد المحرّكين: المفسّر كان يُطلق أوّل معالِج
         * مطابق ويتوقّف، ووقت تشغيل المترجم يُطلقها كلّها).
         *
         * (EN) Executes one selected handler on one node; selection belongs to
         *      dispatchEvent so consumers never re-implement traversal.
         */
        using EventSink = std::function<void(IREventType type,
                                             const std::string &expression,
                                             const IRNode *node,
                                             const EventData &data)>;

        /**
         * @brief (AR) يُرسل حدثًا إلى عقدةٍ عبر أطوار الانتشار الثلاثة.
         *
         * @param type   نوع الحدث المُرسَل.
         * @param target العقدة المُصابة (يُتجاهَل الإرسال إن كانت فارغة).
         * @param data   بيانات الحدث؛ `propagationStopped` فيها يُحترم ويُفحص
         *               بين كلّ معالِجَين (ولذلك هي مرجعٌ ثابت بحقلٍ mutable).
         * @param sink   مُنفِّذ المعالِج المختار.
         */
        void dispatchEvent(IREventType type,
                           const IRNode *target,
                           const EventData &data,
                           const EventSink &sink);

        /**
         * @brief (AR) نطاقٌ يربط بيانات الحدث الجاري بخيطٍ حاليّ، فيصل إليها
         *        مدمجُ `أوقف_الانتشار` المستدعى من داخل معالِج ص.
         *
         * يُنشَأ حول كلّ نداءِ معالِج (RAII): يحفظ السابق ويستعيده عند الخروج
         * (فيصحّ التعشيش أثناء الانتشار). خارج أيّ نطاق `أوقف_الانتشار` لا أثر له.
         */
        class ActiveEventScope
        {
        public:
            explicit ActiveEventScope(const EventData &data);
            ~ActiveEventScope();
            ActiveEventScope(const ActiveEventScope &) = delete;
            ActiveEventScope &operator=(const ActiveEventScope &) = delete;

        private:
            const EventData *previous_;
        };

        /**
         * @brief (AR) يوقف انتشار الحدث الجاري على هذا الخيط (مدمج `أوقف_الانتشار`).
         *        لا أثر إن استُدعي خارج معالِجِ حدث.
         */
        void stopActiveEventPropagation();

        /**
         * @brief (AR) هل لهذا الحدث مستمِعٌ على العقدة أو على أيٍّ من أجدادها؟
         *
         * يستعمله المُطلِقون قبل النداء: الإطلاق لعقدةٍ لا مستمِع لها في مسارها
         * كلّه لا أثر له، وتخطّيه يحفظ العقد القديم (لا ردّ نداء بلا معالِج) —
         * وهو ما يمنع تمرير عقدةٍ قد تكون خرجت من الشجرة إلى المستهلك.
         */
        bool hasListenerInPath(IREventType type, const IRNode *target);

        /**
         * @brief (AR) يُطلق زوج تغيّر التركيز: فقدٌ على السابقة ثمّ اكتسابٌ على
         *        الجديدة — بترتيب DOM (blur قبل focus).
         *
         * كان `عند_التركيز`/`عند_فقد_التركيز` مُعرَّفَين في المفردة بلا مُطلِقٍ
         * قطّ. وُضِع المُطلِق في القلب لا في كلّ منصّة كي يعمّ المنصّات جميعًا
         * ولا يتباعد سلوكها.
         *
         * @param prev العقدة التي فقدت التركيز (قد تكون فارغة).
         * @param next العقدة التي اكتسبته (قد تكون فارغة — نقرة على الفراغ).
         * @param x,y  موضع المؤشّر المُسبِّب (صفر للتنقّل بلوحة المفاتيح).
         * @param sink مُصرِف الحدث (يمرّ بالأطوار داخليًّا).
         */
        void fireFocusChange(const IRNode *prev,
                             const IRNode *next,
                             float x, float y,
                             const EventSink &sink);

    } // namespace ui
} // namespace sad

#endif // SAD_UI_EVENT_DISPATCH_H
