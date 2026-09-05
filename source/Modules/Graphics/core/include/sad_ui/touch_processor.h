/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: touch_processor.h
 * المسار: features/graphics/core/include/sad_ui/touch_processor.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * (AR) معالج أحداث اللمس الموحد — طبقة مشتركة بين جميع المنصات
 *
 * كل منصة (Desktop/Android/iOS/macOS/Web) تُحوّل أحداثها الأصلية إلى
 * استدعاءات بسيطة: onFingerDown/onFingerUp/onFingerMotion.
 * هذه الطبقة تتولى:
 *   1. تتبع الأصابع المتعددة (Multi-Touch Tracking)
 *   2. كشف الإيماءات (Gesture Recognition):
 *      - Tap / DoubleTap / LongPress
 *      - Swipe (4 اتجاهات)
 *      - Drag / DragStart / DragEnd
 *      - Pinch-to-Zoom (إصبعين)
 *      - Rotate (إصبعين)
 *   3. إطلاق أحداث IR موحدة عبر callback
 *
 * الاستخدام:
 *   TouchEventProcessor processor;
 *   processor.setViewportSize(width, height);
 *   processor.setHitTestCallback([](float x, float y) { return hitTest(x,y); });
 *   processor.setFireEventCallback([](IREventType t, ...) { fireEvent(...); });
 *   // ثم من كل منصة:
 *   processor.onFingerDown(touchId, fingerId, x, y, pressure);
 *   processor.onFingerUp(touchId, fingerId, x, y);
 *   processor.onFingerMotion(touchId, fingerId, x, y, dx, dy, pressure);
 *
 * (EN) Unified touch event processor — shared layer across all platforms
 *
 * Each platform converts its native events to simple calls:
 * onFingerDown/Up/Motion. This layer handles multi-touch tracking,
 * gesture recognition, and fires unified IR events via callback.
 *
 * ═══════════════════════════════════════════════════════════════════════════════
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_TOUCH_PROCESSOR_H
#define SAD_UI_TOUCH_PROCESSOR_H

#include "sad_ui/types.h"
#include "sad_ui/ir.h"
#include "sad_ui/event_dispatch.h" // hasListenerInPath

#include <unordered_map>
#include <functional>
#include <cstdint>
#include <cmath>
#include <string>

namespace sad
{
    namespace ui
    {

        // ═══════════════════════════════════════════════════════════════════════════════
        // بيانات إصبع واحد نشط على الشاشة
        // ═══════════════════════════════════════════════════════════════════════════════
        struct FingerState
        {
            int64_t touchId = 0;      ///< معرّف جهاز اللمس
            int64_t fingerId = 0;     ///< معرّف الإصبع
            float startX = 0;         ///< موقع اللمس الأول X (بكسل)
            float startY = 0;         ///< موقع اللمس الأول Y (بكسل)
            float currentX = 0;       ///< الموقع الحالي X (بكسل)
            float currentY = 0;       ///< الموقع الحالي Y (بكسل)
            float pressure = 1.0f;    ///< ضغط اللمس (0.0 → 1.0)
            uint32_t startTime = 0;   ///< وقت بدء اللمس (ms)
            bool dragStarted = false; ///< هل بدأ السحب (تجاوز حد الحركة)
            /// (AR) العنصر المُمسَك للسحب (تحت نقطة بدء اللمس) — يبقى مسحوبًا وإن
            ///      غادره الإصبع، وهو ضروريّ لأنّ العنصر نفسه يتحرّك أثناء السحب
            ///      فلا يصحّ إعادة اختبار النقر بموضع البداية.
            const IRNode *dragNode = nullptr;
        };

        // ═══════════════════════════════════════════════════════════════════════════════
        // معالج أحداث اللمس الموحد
        // ═══════════════════════════════════════════════════════════════════════════════
        class TouchEventProcessor
        {
        public:
            TouchEventProcessor() = default;
            ~TouchEventProcessor() = default;

            // ─── إعدادات ────────────────────────────────

            /// تعيين أبعاد النافذة/العرض (لتحويل إحداثيات نسبية → بكسل)
            void setViewportSize(int width, int height)
            {
                viewportWidth_ = width;
                viewportHeight_ = height;
            }

            /// تعيين دالة اختبار الموقع — تُرجع عقدة IR عند نقطة (x,y) أو nullptr
            void setHitTestCallback(std::function<const IRNode *(float, float)> cb)
            {
                hitTestCb_ = std::move(cb);
            }

            /// تعيين دالة إطلاق الحدث — تُرسل الحدث للمفسر
            void setFireEventCallback(
                std::function<void(IREventType, const std::string &, const IRNode *, const EventData &)> cb)
            {
                fireEventCb_ = std::move(cb);
            }

            /// تعيين دالة الحصول على الوقت الحالي بالمللي ثانية
            void setGetTimeMsCallback(std::function<uint32_t()> cb)
            {
                getTimeMsCb_ = std::move(cb);
            }

            // ─── ثوابت كشف الإيماءات ────────────────────

            /// الحد الأدنى للسحب (بكسل) لكشف Swipe
            float swipeThreshold = 50.0f;

            /// الحد الأدنى للحركة (بكسل) لكشف بداية السحب
            float dragThreshold = 5.0f;

            /// مدة الضغط المطول (مللي ثانية)
            uint32_t longPressMs = 500;

            /// مدة النقر المزدوج القصوى (مللي ثانية)
            uint32_t doubleTapMs = 300;

            /// الحد الأدنى لتغيير الزاوية (راديان) لكشف الدوران
            float rotateThreshold = 0.01f;

            // ─── واجهة الإدخال — تُستدعى من كل منصة ───────

            /**
             * @brief (AR) إصبع لمس الشاشة
             * @param touchId معرّف جهاز اللمس
             * @param fingerId معرّف الإصبع
             * @param x إحداثي X بالبكسل
             * @param y إحداثي Y بالبكسل
             * @param pressure ضغط اللمس [0.0, 1.0]
             */
            void onFingerDown(int64_t touchId, int64_t fingerId,
                              float x, float y, float pressure = 1.0f);

            /**
             * @brief (AR) إصبع رُفع عن الشاشة
             * @param touchId معرّف جهاز اللمس
             * @param fingerId معرّف الإصبع
             * @param x إحداثي X النهائي بالبكسل
             * @param y إحداثي Y النهائي بالبكسل
             */
            void onFingerUp(int64_t touchId, int64_t fingerId,
                            float x, float y);

            /**
             * @brief (AR) إصبع يتحرك على الشاشة
             * @param touchId معرّف جهاز اللمس
             * @param fingerId معرّف الإصبع
             * @param x الموقع الحالي X بالبكسل
             * @param y الموقع الحالي Y بالبكسل
             * @param dx التغيير الأفقي بالبكسل
             * @param dy التغيير الرأسي بالبكسل
             * @param pressure ضغط اللمس [0.0, 1.0]
             */
            void onFingerMotion(int64_t touchId, int64_t fingerId,
                                float x, float y,
                                float dx, float dy,
                                float pressure = 1.0f);

            // ─── الحصول على معلومات اللمس ────────────────

            /// عدد الأصابع الملامسة حالياً
            int getActiveTouchCount() const { return static_cast<int>(activeFingers_.size()); }

            /// هل هناك لمس نشط؟
            bool isTouching() const { return !activeFingers_.empty(); }

            /// مسح جميع الأصابع — يُستخدم عند فقدان التركيز
            void reset();

            /// (AR) جذر المحتوى الحيّ — للتحقّق من بقاء العقدة المُمسَكة أثناء السحب.
            using GetContentRootCallback = std::function<const IRNode *()>;
            void setGetContentRootCallback(GetContentRootCallback cb) { getContentRootCb_ = std::move(cb); }

        private:
            // ─── مساعدات داخلية ─────────────────────────

            /// مفتاح فريد للإصبع: (touchId << 32) | fingerId
            static int64_t fingerKey(int64_t touchId, int64_t fingerId)
            {
                return (touchId << 32) | (fingerId & 0xFFFFFFFF);
            }

            /// المسافة بين إصبعين
            static float fingerDistance(const FingerState &a, const FingerState &b)
            {
                float dx = a.currentX - b.currentX;
                float dy = a.currentY - b.currentY;
                return std::sqrt(dx * dx + dy * dy);
            }

            /// نقطة المنتصف بين إصبعين
            static void fingerMidpoint(const FingerState &a, const FingerState &b,
                                       float &mx, float &my)
            {
                mx = (a.currentX + b.currentX) * 0.5f;
                my = (a.currentY + b.currentY) * 0.5f;
            }

            /// الزاوية بين إصبعين (راديان)
            static float fingerAngle(const FingerState &a, const FingerState &b)
            {
                return std::atan2(b.currentY - a.currentY, b.currentX - a.currentX);
            }

            /// تجهيز EventData مع بيانات اللمس المشتركة
            EventData makeTouchEventData(float x, float y, int64_t touchId, int64_t fingerId,
                                         float pressure = 1.0f) const;

            /// الحصول على الوقت الحالي بالمللي ثانية
            uint32_t now() const;

            // ─── حالة التتبع ────────────────────────────

            /// خريطة الأصابع النشطة
            std::unordered_map<int64_t, FingerState> activeFingers_;

            /// المسافة الأولية بين إصبعين (لكشف القرص/التكبير)
            float initialPinchDistance_ = 0.0f;

            /// الزاوية الأولية بين إصبعين (لكشف الدوران)
            float initialRotationAngle_ = 0.0f;

            /// هل إيماءة القرص/التكبير نشطة؟
            bool pinchActive_ = false;

            /// هل إيماءة الدوران نشطة؟
            bool rotateActive_ = false;

            /// وقت آخر نقرة (لكشف النقر المزدوج)
            uint32_t lastTapTime_ = 0;

            /// أبعاد العرض
            int viewportWidth_ = 0;
            int viewportHeight_ = 0;

            // ─── Callbacks ─────────────────────────────

            std::function<const IRNode *(float, float)> hitTestCb_;

            /// (AR) هل العقدة ما زالت في الشجرة الحيّة؟ (فشل-مُغلق ضدّ الاستعمال بعد التحرير)
            bool touchNodeAlive(const IRNode *node) const;

            GetContentRootCallback getContentRootCb_;
            std::function<void(IREventType, const std::string &, const IRNode *, const EventData &)> fireEventCb_;

            /**
             * (AR) يُطلق حدثًا **مرّةً واحدة** على العقدة، ويترك اختيارَ المعالِجات
             *      والمرورَ بأطوار الانتشار لـdispatchEvent في مدخل المنصّة.
             *      كانت المعالجات تمرّ على getEvents() بنفسها ثمّ تنادي مدخلًا
             *      يمرّ عليها ثانيةً ⇒ إطلاقٌ N² على الهدف وN مرّة على كلّ جدّ.
             *      التعبير فارغ عمدًا: لم يعد المُطلِق يعرف أيّ معالِجٍ سيُنفَّذ.
             */
            void emitEvent(IREventType type, const IRNode *node,
                           const EventData &data) const
            {
                if (fireEventCb_ && hasListenerInPath(type, node))
                    fireEventCb_(type, std::string(), node, data);
            }


            std::function<uint32_t()> getTimeMsCb_;
        };

    } // namespace ui
} // namespace sad

#endif // SAD_UI_TOUCH_PROCESSOR_H
