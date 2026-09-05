/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: mouse_processor.h
 * المسار: features/graphics/core/include/sad_ui/mouse_processor.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * (AR) معالج أحداث الفأرة/المؤشر الموحد عبر جميع المنصات
 *
 * يوفر طبقة مشتركة لمعالجة:
 *   - النقر (Tap): نقرة واحدة، نقرة مزدوجة، ضغط مطول
 *   - القائمة السياقية (ContextMenu): نقرة يمنى أو ضغط مطول
 *   - الإيماءات (Gestures): سحب (Swipe) بجميع الاتجاهات
 *   - التحويم (Hover/HoverExit): دخول وخروج المؤشر من العناصر
 *   - السحب والإسقاط (Drag/DragStart/DragEnd/Drop): سحب عناصر
 *   - التمرير (Scroll): عجلة الفأرة + Ctrl = تكبير/تصغير
 *   - التحكم بالعناصر التفاعلية: Toggle, Checkbox, Radio, Slider
 *   - إدارة التركيز (Focus): تحويل التركيز عند النقر
 *   - تغيير شكل المؤشر (Cursor): يد عند التحويم على عناصر تفاعلية
 *
 * كل منصة توصّل أحداثها الأصلية إلى هذا المعالج عبر الدوال العامة:
 *   - onMouseDown(زر، x، y)
 *   - onMouseUp(زر، x، y)
 *   - onMouseMove(x، y، أزرار_مضغوطة)
 *   - onMouseWheel(deltaX، deltaY، x، y، ctrl_مضغوط)
 *
 * المنطق المعقد (كشف الإيماءات، Hover tracking، Slider drag) مُركّز هنا
 * بدلاً من تكراره في كل منصة.
 *
 * (EN) Unified mouse/pointer event processor — shared across all platforms.
 *
 * ═══════════════════════════════════════════════════════════════════════════════
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_MOUSE_PROCESSOR_H
#define SAD_UI_MOUSE_PROCESSOR_H

#include "sad_ui/ir.h"
#include "sad_ui/keyboard_processor.h" // KeyModifiers
#include "sad_ui/event_dispatch.h" // hasListenerInPath
#include "sad_ui/types.h"
#include "sad_ui/layout.h"

#include <functional>
#include <string>
#include <vector>
#include <cstdint>

namespace sad
{
    namespace ui
    {

        // ═══════════════════════════════════════════════════════════════════════════════
        // ثوابت كشف الإيماءات
        // ═══════════════════════════════════════════════════════════════════════════════

        /// (AR) عتبة السحب (بالبكسل) — الحركة التي تزيد عن هذه القيمة تُعتبر سحباً
        constexpr float MOUSE_SWIPE_THRESHOLD = 50.0f;

        /// (AR) مدة الضغط المطول (بالمللي ثانية) — الضغط الأطول من هذه المدة = LongPress
        constexpr uint32_t MOUSE_LONG_PRESS_MS = 500;

        /// (AR) مدة النقر المزدوج (بالمللي ثانية) — نقرتان خلال هذه المدة = DoubleTap
        constexpr uint32_t MOUSE_DOUBLE_TAP_MS = 400;

        /// (AR) عتبة بدء السحب (بالبكسل) — الحركة أثناء الضغط أكبر من هذه تبدأ عملية السحب
        constexpr float MOUSE_DRAG_THRESHOLD = 5.0f;

        // ═══════════════════════════════════════════════════════════════════════════════
        // أزرار الفأرة
        // ═══════════════════════════════════════════════════════════════════════════════

        /// (AR) أزرار الفأرة الموحدة — مستقلة عن المنصة
        enum class MouseButton : uint8_t
        {
            Left = 0,   ///< الزر الأيسر (النقرة الرئيسية)
            Middle = 1, ///< الزر الأوسط (عجلة الفأرة)
            Right = 2,  ///< الزر الأيمن (قائمة سياقية)
            Unknown = 255
        };

        // ═══════════════════════════════════════════════════════════════════════════════
        // حالة الأزرار المضغوطة
        // ═══════════════════════════════════════════════════════════════════════════════

        /// (AR) أقنعة الأزرار المضغوطة أثناء حركة المؤشر
        struct MouseButtonState
        {
            bool leftPressed = false;   ///< الزر الأيسر مضغوط
            bool middlePressed = false; ///< الزر الأوسط مضغوط
            bool rightPressed = false;  ///< الزر الأيمن مضغوط
        };

        // ═══════════════════════════════════════════════════════════════════════════════
        // نتيجة البحث عن منطقة قابلة للتمرير
        // ═══════════════════════════════════════════════════════════════════════════════

        /// (AR) بنية تحتوي على العقدة القابلة للتمرير ومعلومات التخطيط
        struct ScrollableHitInfo
        {
            const IRNode *node = nullptr;     ///< عقدة ScrollView/LazyColumn
            float viewHeight = 0.0f;          ///< ارتفاع المنطقة المرئية
            float contentHeight = 0.0f;       ///< ارتفاع المحتوى الكامل
            float currentScrollOffset = 0.0f; ///< الإزاحة الحالية
        };

        // ═══════════════════════════════════════════════════════════════════════════════
        // معالج أحداث الفأرة الموحد
        // ═══════════════════════════════════════════════════════════════════════════════

        /**
         * @brief (AR) المعالج الموحد لأحداث الفأرة/المؤشر — يُستخدم من جميع المنصات
         *
         * كل منصة تُوصّل أحداثها الأصلية (SDL_MOUSEBUTTONDOWN، NSEvent، MotionEvent...)
         * إلى الدوال العامة الأربع:
         *   1. onMouseDown(button, x, y)    — ضغط زر
         *   2. onMouseUp(button, x, y)      — رفع زر
         *   3. onMouseMove(x, y, buttons)   — حركة المؤشر
         *   4. onMouseWheel(dx, dy, x, y, ctrl) — عجلة التمرير
         *
         * المنطق المشترك:
         *   - كشف إيماءات (Swipe/LongPress/DoubleTap) بناءً على المسافة والزمن
         *   - تتبع Hover (دخول/خروج المؤشر) وإطلاق OnHover/OnHoverExit
         *   - سحب وإسقاط (Drag/DragStart/DragEnd/Drop)
         *   - تمرير ScrollView + تكبير Ctrl+Scroll
         *   - معالجة Toggle/Checkbox/Radio/Slider
         *   - تحديث التركيز عند النقر
         *   - تغيير شكل المؤشر
         *
         * (EN) Unified mouse event processor used by all platforms.
         */
        class MouseEventProcessor
        {
        public:
            MouseEventProcessor() = default;
            ~MouseEventProcessor() = default;

            // ═══════════════════════════════════════════════════════════════════
            // الأحداث الرئيسية (الواجهة العامة)
            // ═══════════════════════════════════════════════════════════════════

            /**
             * @brief (AR) ضغط زر الفأرة
             * يعالج: ContextMenu (يمنى)، تحديث التركيز، Toggle/Checkbox/Radio/Slider،
             *         بدء تتبع الإيماءات، OnTap
             * @param button زر الفأرة (Left/Right/Middle)
             * @param x إحداثي X بالبكسل
             * @param y إحداثي Y بالبكسل
             */
            void onMouseDown(MouseButton button, float x, float y);

            /**
             * @brief (AR) رفع زر الفأرة
             * يعالج: كشف الإيماءات (Swipe/LongPress/DoubleTap)،
             *         DragEnd/Drop، إنهاء سحب Slider
             * @param button زر الفأرة
             * @param x إحداثي X بالبكسل
             * @param y إحداثي Y بالبكسل
             */
            void onMouseUp(MouseButton button, float x, float y);

            /**
             * @brief (AR) حركة المؤشر
             * يعالج: سحب Slider، Hover/HoverExit، DragStart/DragMove،
             *         تغيير شكل المؤشر
             * @param x إحداثي X بالبكسل
             * @param y إحداثي Y بالبكسل
             * @param buttons حالة الأزرار المضغوطة
             */
            void onMouseMove(float x, float y, const MouseButtonState &buttons);

            /**
             * @brief (AR) عجلة التمرير
             * يعالج: Ctrl+Scroll → Zoom، تمرير ScrollView/LazyColumn، OnScroll
             * @param deltaX إزاحة أفقية
             * @param deltaY إزاحة رأسية
             * @param mouseX إحداثي المؤشر X
             * @param mouseY إحداثي المؤشر Y
             * @param ctrlPressed هل مفتاح Ctrl مضغوط
             */
            void onMouseWheel(float deltaX, float deltaY,
                              float mouseX, float mouseY,
                              bool ctrlPressed);

            // ═══════════════════════════════════════════════════════════════════
            // قراءة الحالة الداخلية (للمُصيّر وأجزاء المنصة الأخرى)
            // ═══════════════════════════════════════════════════════════════════

            /// (AR) العنصر تحت المؤشر حالياً — يُستخدم لتلوين حالة Hover في المُصيّر
            const IRNode *getHoveredNode() const { return hoveredNode_; }

            /// (AR) العنصر المضغوط حالياً — يُستخدم لتلوين حالة الضغط في المُصيّر
            const IRNode *getPressedNode() const { return pressedNode_; }

            /// (AR) هل يتم السحب حالياً
            bool isDragging() const { return dragging_; }

            // ═══════════════════════════════════════════════════════════════════
            // Callbacks — يجب تعيينها من المنصة قبل الاستخدام
            // ═══════════════════════════════════════════════════════════════════

            // ─── أساسية (مطلوبة) ─────────────────────────

            /// (AR) البحث عن العنصر في إحداثيات (x, y) — hit testing
            using HitTestCallback = std::function<const IRNode *(float x, float y)>;
            void setHitTestCallback(HitTestCallback cb) { hitTestCb_ = std::move(cb); }

            /// (AR) إطلاق حدث IR على عقدة
            using FireEventCallback = std::function<void(
                IREventType type, const std::string &expression,
                const IRNode *node, const EventData &data)>;
            void setFireEventCallback(FireEventCallback cb) { fireEventCb_ = std::move(cb); }

            /// (AR) الحصول على الوقت الحالي بالمللي ثانية
            using GetTimeMsCallback = std::function<uint32_t()>;
            void setGetTimeMsCallback(GetTimeMsCallback cb) { getTimeMsCb_ = std::move(cb); }

            /// (AR) طلب إعادة الرسم
            using InvalidateCallback = std::function<void()>;
            void setInvalidateCallback(InvalidateCallback cb) { invalidateCb_ = std::move(cb); }

            // ─── التركيز ─────────────────────────────────

            /// (AR) الحصول على العقدة المُركّزة حالياً
            using GetFocusedNodeCallback = std::function<const IRNode *()>;
            void setGetFocusedNodeCallback(GetFocusedNodeCallback cb) { getFocusedNodeCb_ = std::move(cb); }

            /// (AR) تعيين العقدة المُركّزة — المنصة تدير SDL_StartTextInput وغيرها
            using SetFocusedNodeCallback = std::function<void(const IRNode *node)>;
            void setSetFocusedNodeCallback(SetFocusedNodeCallback cb) { setFocusedNodeCb_ = std::move(cb); }

            /// (AR) حالة مفاتيح التعديل الحيّة من المنصّة — تُثري أحداث الفأرة
            ///      بحقول مفتاح_عالي/تحكم/بديل (كانت أصفارًا لغياب المصدر).
            using GetKeyModifiersCallback = std::function<KeyModifiers()>;
            void setGetKeyModifiersCallback(GetKeyModifiersCallback cb) { getKeyModifiersCb_ = std::move(cb); }

            // ─── عناصر خاصة ──────────────────────────────

            /// (AR) البحث عن مستطيل تخطيط عقدة (للـ Slider)
            using FindNodeRectCallback = std::function<LayoutRect(const IRNode *node)>;
            void setFindNodeRectCallback(FindNodeRectCallback cb) { findNodeRectCb_ = std::move(cb); }

            /// (AR) البحث عن أقرب ScrollView/LazyColumn يحتوي على النقطة
            using FindScrollableAtCallback = std::function<ScrollableHitInfo(float x, float y)>;
            void setFindScrollableAtCallback(FindScrollableAtCallback cb) { findScrollableAtCb_ = std::move(cb); }

            /// (AR) تعيين إزاحة التمرير لعنصر ScrollView
            using SetScrollOffsetCallback = std::function<void(const IRNode *node, float offset)>;
            void setSetScrollOffsetCallback(SetScrollOffsetCallback cb) { setScrollOffsetCb_ = std::move(cb); }

            /// (AR) الحصول على جذر المحتوى (للبحث عن Radio siblings)
            using GetContentRootCallback = std::function<const IRNode *()>;
            void setGetContentRootCallback(GetContentRootCallback cb) { getContentRootCb_ = std::move(cb); }

            // ─── المؤشر ──────────────────────────────────

            /// (AR) تغيير شكل المؤشر — المنصة تنفذ SDL_SetCursor أو ما يكافئه
            ///      cursorType: 0 = سهم (Arrow)، 1 = يد (Hand/Pointer)
            using SetCursorCallback = std::function<void(int cursorType)>;
            void setSetCursorCallback(SetCursorCallback cb) { setCursorCb_ = std::move(cb); }

            /**
             * @brief (AR) تصفير كلّ المؤشّرات الخام للعقد (سحب/ضغط/تحويم/منزلق).
             *
             * تُنادى من المنصّة حين تُستبدل شجرة المحتوى (setContent / إعادة بناء)
             * لأنّ العقد القديمة تتحرّر حينها، فبقاء مؤشّراتها ⇒ استعمال-بعد-التحرير.
             * (EN) Reset all raw node pointers; call when the content tree is replaced.
             */
            void clearNodeRefs();

        private:
            // ═══════════════════════════════════════════════════════════════════
            // Callbacks
            // ═══════════════════════════════════════════════════════════════════
            HitTestCallback hitTestCb_;
            GetKeyModifiersCallback getKeyModifiersCb_;

            FireEventCallback fireEventCb_;

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
                if (!fireEventCb_ || !hasListenerInPath(type, node))
                    return;
                if (!getKeyModifiersCb_)
                {
                    fireEventCb_(type, std::string(), node, data);
                    return;
                }
                // (AR) مفاتيح التعديل (عالي/تحكّم/بديل) كانت أصفارًا دائمًا في
                //      أحداث الفأرة: لا مصدر لحالتها في القلب. تُقرأ الآن من
                //      المنصّة عند الإطلاق فتصل ثلاثةُ حقولٍ في بنية «حدث».
                EventData enriched = data;
                const auto mods = getKeyModifiersCb_();
                enriched.shiftKey = mods.shift;
                enriched.ctrlKey = mods.ctrl;
                enriched.altKey = mods.alt;
                fireEventCb_(type, std::string(), node, enriched);
            }


            GetTimeMsCallback getTimeMsCb_;
            InvalidateCallback invalidateCb_;
            GetFocusedNodeCallback getFocusedNodeCb_;
            SetFocusedNodeCallback setFocusedNodeCb_;
            FindNodeRectCallback findNodeRectCb_;
            FindScrollableAtCallback findScrollableAtCb_;
            SetScrollOffsetCallback setScrollOffsetCb_;
            GetContentRootCallback getContentRootCb_;
            SetCursorCallback setCursorCb_;

            // ═══════════════════════════════════════════════════════════════════
            // حالة التتبع الداخلية
            // ═══════════════════════════════════════════════════════════════════

            // ─── موقع المؤشر ─────────────────────────────
            float mouseX_ = 0.0f; ///< (AR) آخر موقع X للمؤشر
            float mouseY_ = 0.0f; ///< (AR) آخر موقع Y للمؤشر

            // ─── حالة الإيماءات ──────────────────────────
            float gestureStartX_ = 0.0f;    ///< (AR) موقع بداية الضغط X
            float gestureStartY_ = 0.0f;    ///< (AR) موقع بداية الضغط Y
            uint32_t gestureStartTime_ = 0; ///< (AR) وقت بداية الضغط (ms)
            uint32_t lastTapTime_ = 0;      ///< (AR) وقت آخر نقرة (للنقر المزدوج)
            bool gestureActive_ = false;    ///< (AR) هل هناك إيماءة نشطة

            // ─── حالة السحب ──────────────────────────────
            bool dragging_ = false;               ///< (AR) هل يتم السحب حالياً
            const IRNode *pressedNode_ = nullptr; ///< (AR) العنصر المضغوط (أثناء الضغط)
            const IRNode *dragNode_ = nullptr;    ///< (AR) العنصر المُمسَك للسحب (تحت نقطة بدء الضغط)
            float lastDragX_ = 0.0f;              ///< (AR) آخر X للسحب — مرجع الدلتا التفاضليّة المستمرّة
            float lastDragY_ = 0.0f;              ///< (AR) آخر Y للسحب — مرجع الدلتا التفاضليّة المستمرّة

            // ─── حالة التحويم ─────────────────────────────
            const IRNode *hoveredNode_ = nullptr; ///< (AR) العنصر تحت المؤشر حالياً

            // ─── حالة المنزلق ─────────────────────────────
            const IRNode *draggedSliderNode_ = nullptr; ///< (AR) المنزلق المسحوب حالياً

            // ═══════════════════════════════════════════════════════════════════
            // دوال مساعدة داخلية
            // ═══════════════════════════════════════════════════════════════════

            /// (AR) معالجة النقر على عناصر Toggle/Checkbox — تبديل خاصية مفعّل
            void handleToggleCheckbox(const IRNode *node, float x, float y);

            /// (AR) معالجة النقر على عنصر Radio — تحديد + إلغاء تحديد المجموعة
            void handleRadio(const IRNode *node, float x, float y);

            /// (AR) معالجة بدء سحب المنزلق (Slider) — حساب القيمة من موقع النقر
            void handleSliderDown(const IRNode *node, float x);

            /// (AR) معالجة حركة سحب المنزلق — تحديث القيمة أثناء السحب المستمر
            void handleSliderDrag(float mouseX);

            /// (AR) إلغاء تحديد جميع أزرار Radio في نفس المجموعة باستثناء المحدد
            void deselectRadioSiblings(const IRNode *selectedNode, const std::string &group);

            /// (AR) إطلاق أحداث نقر عادية (OnTap) على العنصر
            void fireTapEvents(const IRNode *node, float x, float y);

            /// (AR) كشف الإيماءة عند رفع الزر (Swipe/LongPress/DoubleTap)
            void detectGesture(float x, float y);

            /// (AR) معالجة تتبع التحويم (Hover) عند حركة المؤشر
            void handleHoverTracking(float x, float y);

            /// (AR) معالجة السحب (Drag) أثناء حركة المؤشر مع الضغط
            void handleDragTracking(float x, float y);

            /// (AR) هل العقدة المُمسَكة ما زالت في الشجرة الحيّة؟ تُصفّرها وتنهي
            ///      السحب إن تحرّرت (إعادة بناء الشجرة أثناء السحب). تُنادى قبل
            ///      كلّ استعمال لـdragNode_ — حركةً كان أم رفعًا.
            bool dragNodeAlive();

            /// (AR) هل العقدة ما زالت في الشجرة الحيّة (جذر المحتوى الحاليّ)؟ مقارنة
            ///      مؤشّرات فقط بلا فكّ إسناد الهدف، فتصحّ حتّى لو تحرّرت. حارسٌ مشترك
            ///      لمساري التحويم/المنزلق ضدّ استعمال-بعد-التحرير حين تُستبدَل الشجرة
            ///      (نبضة الساعة م١-ب/تنقّل) على منصّةٍ لا تُصفّر المؤشّرات الخام عند
            ///      setContent (الوضع الحرّ) — نظير dragNodeAlive للسحب. يُبقي السلوك
            ///      السابق إن غاب جذر المحتوى (لا سبيل للتحقّق).
            bool nodeInLiveTree(const IRNode *node) const;
        };

    } // namespace ui
} // namespace sad

#endif // SAD_UI_MOUSE_PROCESSOR_H
