/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: keyboard_processor.h
 * المسار: features/graphics/core/include/sad_ui/keyboard_processor.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * (AR) معالج أحداث لوحة المفاتيح الموحد — طبقة مشتركة بين جميع المنصات
 *
 * كل منصة (Desktop/Android/iOS/macOS/Web) تُحوّل أحداث المفاتيح الأصلية إلى
 * استدعاءات بسيطة: onKeyDown/onKeyUp/onTextInput.
 * هذه الطبقة تتولى:
 *   1. إطلاق أحداث IR: OnKeyDown, OnKeyUp
 *   2. تنقل التركيز (Tab / Shift+Tab)
 *   3. تفعيل العنصر المُركّز (Enter / Space)
 *   4. تحرير حقول النصوص (Backspace, Delete, Arrows, Home, End)
 *   5. اختصارات لوحة المفاتيح (Ctrl+C/X/V/A/Z/Y)
 *   6. إغلاق التطبيق (Escape)
 *
 * الاستخدام:
 *   KeyboardEventProcessor kbProcessor;
 *   kbProcessor.setFireEventCallback(...);
 *   kbProcessor.setGetFocusedNodeCallback(...);
 *   kbProcessor.setGetContentRootCallback(...);
 *   kbProcessor.setCollectFocusableNodesCallback(...);
 *   kbProcessor.setSetFocusedNodeCallback(...);
 *   kbProcessor.setInvalidateCallback(...);
 *   kbProcessor.setCloseWindowCallback(...);
 *   kbProcessor.setTextEditCallbacks(...);
 *   // ثم من كل منصة:
 *   kbProcessor.onKeyDown(keyCode, keyName, shift, ctrl, alt);
 *   kbProcessor.onKeyUp(keyCode, keyName, shift, ctrl, alt);
 *   kbProcessor.onTextInput(text);
 *
 * (EN) Unified keyboard event processor — shared layer across all platforms
 *
 * Each platform converts its native key events to simple calls:
 * onKeyDown/Up/TextInput. This layer handles focus management,
 * text editing, shortcuts, and fires unified IR events via callback.
 *
 * ═══════════════════════════════════════════════════════════════════════════════
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_KEYBOARD_PROCESSOR_H
#define SAD_UI_KEYBOARD_PROCESSOR_H

#include "sad_ui/types.h"
#include "sad_ui/ir.h"
#include "sad_ui/event_dispatch.h" // hasListenerInPath

#include <functional>
#include <string>
#include <vector>
#include <cstdint>

namespace sad
{
    namespace ui
    {

        // ═══════════════════════════════════════════════════════════════════════════════
        // تعريفات مُقدّمة — لتجنب الاعتماد على text_editing.h المحلي
        // ═══════════════════════════════════════════════════════════════════════════════
        // (AR) TextEditState و TextEditUndoManager موجودان في
        //      desktop/src/text_editing.h — المعالج المشترك لا يعتمد عليهما مباشرة
        //      بل يستخدم callbacks عامة. أسماء الأنواع في callbacks هنا هي void*
        //      والمنصة تُعيد التفسير في سياقها الخاص.
        // ═══════════════════════════════════════════════════════════════════════════════

        // ═══════════════════════════════════════════════════════════════════════════════
        // رموز المفاتيح الموحدة — مستقلة عن المنصة
        // ═══════════════════════════════════════════════════════════════════════════════
        //
        // (AR) بدلاً من استخدام SDL_Scancode أو VK_* أو UIKeyboardHIDUsage مباشرة،
        //      نُعرّف رموزاً موحدة يُحوّل إليها كل منصة.
        //      هذا يضمن أن منطق Tab/Enter/Escape يعمل بشكل متطابق في كل مكان.
        //
        // (EN) Platform-independent key codes. Each platform maps its native codes
        //      to these unified codes before calling onKeyDown/onKeyUp.
        // ═══════════════════════════════════════════════════════════════════════════════
        enum class UnifiedKeyCode : int
        {
            Unknown = 0,

            // ─── حروف ───
            A = 4,
            B,
            C,
            D,
            E,
            F,
            G,
            H,
            I,
            J,
            K,
            L,
            M,
            N,
            O,
            P,
            Q,
            R,
            S,
            T,
            U,
            V,
            W,
            X,
            Y,
            Z,

            // ─── أرقام ───
            Num1 = 30,
            Num2,
            Num3,
            Num4,
            Num5,
            Num6,
            Num7,
            Num8,
            Num9,
            Num0,

            // ─── مفاتيح تحكم ───
            Return = 40,    ///< Enter / Return
            Escape = 41,    ///< Escape
            Backspace = 42, ///< Backspace / ←حذف
            Tab = 43,       ///< Tab
            Space = 44,     ///< مسافة

            // ─── أسهم ───
            Right = 79,
            Left = 80,
            Down = 81,
            Up = 82,

            // ─── تحرير ───
            Insert = 73,
            Home = 74,
            PageUp = 75,
            Delete = 76,
            End = 77,
            PageDown = 78,

            // ─── F-keys ───
            F1 = 58,
            F2,
            F3,
            F4,
            F5,
            F6,
            F7,
            F8,
            F9,
            F10,
            F11,
            F12,
        };

        // ═══════════════════════════════════════════════════════════════════════════════
        // مُعدّلات المفاتيح (Key Modifiers)
        // ═══════════════════════════════════════════════════════════════════════════════
        struct KeyModifiers
        {
            bool shift = false;
            bool ctrl = false;
            bool alt = false;
            bool meta = false; ///< Command (macOS) / Super (Linux) / Win (Windows)
        };

        // ═══════════════════════════════════════════════════════════════════════════════
        // معالج أحداث لوحة المفاتيح الموحد
        // ═══════════════════════════════════════════════════════════════════════════════
        class KeyboardEventProcessor
        {
        public:
            KeyboardEventProcessor() = default;
            ~KeyboardEventProcessor() = default;

            // ─── Callbacks — تُعيّنها المنصة ──────────────────────

            /// إطلاق حدث IR (OnKeyDown, OnKeyUp, OnTap, OnChange, OnInput)
            void setFireEventCallback(
                std::function<void(IREventType, const std::string &, const IRNode *, const EventData &)> cb)
            {
                fireEventCb_ = std::move(cb);
            }

            /// الحصول على العقدة المُركّزة حالياً (أو nullptr)
            void setGetFocusedNodeCallback(std::function<const IRNode *()> cb)
            {
                getFocusedNodeCb_ = std::move(cb);
            }

            /// الحصول على جذر المحتوى
            void setGetContentRootCallback(std::function<const IRNode *()> cb)
            {
                getContentRootCb_ = std::move(cb);
            }

            /// جمع العناصر القابلة للتركيز
            void setCollectFocusableNodesCallback(std::function<void(std::vector<const IRNode *> &)> cb)
            {
                collectFocusableCb_ = std::move(cb);
            }

            /// تغيير العقدة المُركّزة
            void setSetFocusedNodeCallback(std::function<void(const IRNode *)> cb)
            {
                setFocusedNodeCb_ = std::move(cb);
            }

            /// طلب إعادة رسم
            void setInvalidateCallback(std::function<void()> cb)
            {
                invalidateCb_ = std::move(cb);
            }

            /// إغلاق النافذة (عند Escape)
            void setCloseWindowCallback(std::function<void()> cb)
            {
                closeWindowCb_ = std::move(cb);
            }

            // ─── Callbacks خاصة بتحرير النصوص ──────────────────

            /**
             * @brief (AR) معالجة مفتاح تحرير (Backspace/Delete/Arrows/Home/End/Ctrl+...)
             * كل منصة تُوفّر تنفيذها الخاص (Desktop يستخدم TextEditState ومدير SDL)
             * @param callback يأخذ: العقدة المُركّزة، رمز المفتاح الموحد، المُعدّلات
             *                 يُرجع true إذا تمت المعالجة
             */
            void setHandleTextEditKeyCallback(
                std::function<bool(const IRNode *, UnifiedKeyCode, const KeyModifiers &)> cb)
            {
                handleTextEditKeyCb_ = std::move(cb);
            }

            /**
             * @brief (AR) معالجة إدخال نص في حقل نص مُركّز
             * @param callback يأخذ: العقدة المُركّزة، النص المُدخل
             *                 يُرجع true إذا تمت المعالجة
             */
            void setHandleTextInputCallback(
                std::function<bool(const IRNode *, const std::string &)> cb)
            {
                handleTextInputCb_ = std::move(cb);
            }

            // ─── واجهة الإدخال — تُستدعى من كل منصة ───────

            /**
             * @brief (AR) مفتاح تم ضغطه
             * @param keyCode رمز المفتاح الموحد
             * @param keyName اسم المفتاح (للعرض: "Enter", "A", "Tab")
             * @param mods مُعدّلات المفاتيح (Shift, Ctrl, Alt, Meta)
             */
            void onKeyDown(UnifiedKeyCode keyCode, const std::string &keyName,
                           const KeyModifiers &mods = {});

            /**
             * @brief (AR) مفتاح تم رفعه
             * @param keyCode رمز المفتاح الموحد
             * @param keyName اسم المفتاح
             * @param mods مُعدّلات المفاتيح
             */
            void onKeyUp(UnifiedKeyCode keyCode, const std::string &keyName,
                         const KeyModifiers &mods = {});

            /**
             * @brief (AR) نص تم إدخاله (من IME أو لوحة المفاتيح)
             * @param text النص المُدخل (UTF-8)
             */
            void onTextInput(const std::string &text);

        private:
            // ─── منطق داخلي ─────────────────────────────

            /// معالجة تنقل التركيز (Tab / Shift+Tab)
            void handleFocusNavigation(bool reverse);

            /// معالجة تفعيل العنصر المُركّز (Enter / Space)
            void handleActivation(int keyCode, const std::string &keyName);

            /// تجهيز EventData لحدث مفتاح
            EventData makeKeyEventData(int keyCode, const std::string &keyName,
                                       const KeyModifiers &mods) const;

            /// هل العقدة حقل إدخال نص؟
            bool isTextInputNode(const IRNode *node) const;

            /// الحصول على العقدة الهدف للحدث (المُركّزة أو الجذر)
            const IRNode *getTargetNode() const;

            // ─── Callbacks ──────────────────────────────

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


            std::function<const IRNode *()> getFocusedNodeCb_;
            std::function<const IRNode *()> getContentRootCb_;
            std::function<void(std::vector<const IRNode *> &)> collectFocusableCb_;
            std::function<void(const IRNode *)> setFocusedNodeCb_;
            std::function<void()> invalidateCb_;
            std::function<void()> closeWindowCb_;
            std::function<bool(const IRNode *, UnifiedKeyCode, const KeyModifiers &)> handleTextEditKeyCb_;
            std::function<bool(const IRNode *, const std::string &)> handleTextInputCb_;
        };

    } // namespace ui
} // namespace sad

#endif // SAD_UI_KEYBOARD_PROCESSOR_H
