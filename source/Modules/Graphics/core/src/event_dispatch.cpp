/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: event_dispatch.cpp
 * المسار: features/graphics/core/src/event_dispatch.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * (AR) تنفيذ إرسال الأحداث بأطواره الثلاثة — انظر event_dispatch.h للعقد.
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/event_dispatch.h"

#include <vector>

namespace sad
{
    namespace ui
    {
        namespace
        {
            /// (AR) هل يشارك هذا المعالِج في طور الالتقاط (نزولًا)؟
            bool listensOnCapture(EventPropagation p)
            {
                return p == EventPropagation::Capture || p == EventPropagation::Both;
            }

            /// (AR) هل يشارك هذا المعالِج في طور الفقاعات (صعودًا)؟
            bool listensOnBubble(EventPropagation p)
            {
                return p == EventPropagation::Bubble || p == EventPropagation::Both;
            }

            /**
             * (AR) يُطلق معالِجات عقدةٍ واحدة المطابقة للنوع.
             * @param phaseFilter مُرشِّح الطور؛ فارغ ⇒ كلّ المطابقات (طور الهدف).
             * @return false إن أُوقف الانتشار أثناء التنفيذ (فيتوقّف المرور).
             */
            bool fireOnNode(IREventType type,
                            const IRNode *node,
                            const EventData &data,
                            const EventSink &sink,
                            bool (*phaseFilter)(EventPropagation))
            {
                // (AR) لقطةٌ قبل التنفيذ: المعالِج قد يستدعي «تحديث_حالة» فتُعاد
                //      بناء الشجرة أو تُضاف أحداثٌ للعقدة ⇒ يُبطَل مُكرِّر
                //      getEvents() وسطَ المرور (استعمال بعد التحرير). نختار
                //      أوّلًا ثمّ ننفّذ من اللقطة.
                std::vector<std::pair<std::string, std::string>> selected; // (تعبير، بيانات)
                for (const auto &evt : node->getEvents())
                {
                    if (evt.type != type)
                        continue;
                    if (phaseFilter && !phaseFilter(evt.propagation))
                        continue;
                    selected.emplace_back(evt.expression, evt.userData);
                }

                for (const auto &entry : selected)
                {
                    // (AR) «بيانات» خاصّة بالمعالِج لا بالحدث ⇒ تُضبَط قبل كلّ نداء.
                    data.customData = entry.second;
                    sink(type, entry.first, node, data);

                    // (AR) يُفحص بعد كلّ معالِج: معالِجٌ واحد قد يوقف الانتشار
                    //      فلا يجوز إكمال بقيّة معالِجات العقدة ولا الصعود.
                    if (data.propagationStopped)
                        return false;
                }
                return true;
            }
        } // namespace

        void dispatchEvent(IREventType type,
                           const IRNode *target,
                           const EventData &data,
                           const EventSink &sink)
        {
            if (!target || !sink)
                return;

            // مسار الأجداد: من الأب المباشر صعودًا إلى الجذر.
            const auto ancestors = target->getAncestorPath();

            // ─── ١) طور الالتقاط: من الجذر نزولًا إلى الأب المباشر ───
            for (auto it = ancestors.rbegin(); it != ancestors.rend(); ++it)
            {
                if (data.propagationStopped)
                    return;
                if (!fireOnNode(type, *it, data, sink, &listensOnCapture))
                    return;
            }

            // ─── ٢) طور الهدف: كلّ معالِجات العقدة المُصابة (بلا مُرشِّح طور) ───
            if (data.propagationStopped)
                return;
            if (!fireOnNode(type, target, data, sink, nullptr))
                return;

            // ─── ٣) طور الفقاعات: من الأب صعودًا إلى الجذر ───
            for (const auto *ancestor : ancestors)
            {
                if (data.propagationStopped)
                    return;
                if (!fireOnNode(type, ancestor, data, sink, &listensOnBubble))
                    return;
            }
        }

        namespace
        {
            // (AR) بيانات الحدث الجاري على هذا الخيط — يضبطها ActiveEventScope
            //      حول كلّ نداء معالِج، ويقرؤها أوقف_الانتشار. thread_local كي لا
            //      تتداخل خيوط goroutine إن أطلقت أحداثًا متزامنة.
            thread_local const EventData *g_activeEvent = nullptr;
        } // namespace

        ActiveEventScope::ActiveEventScope(const EventData &data)
            : previous_(g_activeEvent)
        {
            g_activeEvent = &data;
        }

        ActiveEventScope::~ActiveEventScope()
        {
            g_activeEvent = previous_;
        }

        void stopActiveEventPropagation()
        {
            if (g_activeEvent)
                g_activeEvent->stopPropagation(); // propagationStopped قابل للتعديل
        }

        bool hasListenerInPath(IREventType type, const IRNode *target)
        {
            if (!target)
                return false;
            for (const auto &evt : target->getEvents())
                if (evt.type == type)
                    return true;
            for (const auto *ancestor : target->getAncestorPath())
            {
                if (!ancestor)
                    continue;
                for (const auto &evt : ancestor->getEvents())
                {
                    // (AR) الجدّ لا يُحتسب إلّا إن أعلن طورًا — الافتراض None
                    //      يعني «لا انتشار» فلا يستقبل أحداث أبنائه أصلًا.
                    if (evt.type == type && evt.propagation != EventPropagation::None)
                        return true;
                }
            }
            return false;
        }

        void fireFocusChange(const IRNode *prev,
                             const IRNode *next,
                             float x, float y,
                             const EventSink &sink)
        {
            if (!sink || prev == next)
                return;

            // (AR) `sink` هنا هو مدخل إطلاق المنصّة (fireEventCb_) الذي يمرّ
            //      بالأطوار داخليًّا — فلا نستدعي dispatchEvent هنا وإلّا أُرسل
            //      الحدث مرّتين. والتعبير فارغ لأنّ الاختيار يقع في المُرسِل.
            //      بيانات مستقلّة لكلّ طرف: إيقافُ الانتشار في الفقد يجب ألّا
            //      يبتلع اكتساب التركيز — حدثان منفصلان لا حدثٌ واحد.
            const std::string noExpr;
            if (prev)
            {
                EventData blurData;
                blurData.x = x;
                blurData.y = y;
                sink(IREventType::OnBlur, noExpr, prev, blurData);
            }
            if (next)
            {
                EventData focusData;
                focusData.x = x;
                focusData.y = y;
                sink(IREventType::OnFocus, noExpr, next, focusData);
            }
        }

    } // namespace ui
} // namespace sad
