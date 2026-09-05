/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: ir.h
 * المسار: features/graphics/core/include/sad_ui/ir.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * الوصف:
 * ------
 * التمثيل الوسيط للواجهات (Intermediate Representation — IR).
 *
 * هذه الطبقة تقع بين شجرة UINode والأكواد المولّدة لكل منصة:
 *
 *     كود ص  →  Parser  →  UINode Tree  →  IR  →  {Compose, SwiftUI, SDL2, HTML}
 *
 * لماذا نحتاج IR؟
 * ────────────────
 * 1. **التجريد**: IR يُجرّد الفروقات بين المنصات. بدلاً من أن يعرف كل
 *    مولّد كود عن UINode مباشرة، يتعامل مع IR موحد.
 *
 * 2. **التوجيه الذكي (Smart Routing)**: IR يحتوي على معلومات عن
 *    ما إذا كان العنصر "قياسي" (يوجد مكافئ أصلي) أو "مخصص" (يحتاج Canvas).
 *    هذا هو جوهر الهندسة الهجينة الذكية.
 *
 * 3. **التحسين**: يمكن تطبيق تحسينات على IR قبل توليد الكود:
 *    - دمج المعدّلات المتكررة
 *    - حذف العناصر المخفية
 *    - تبسيط الشجرة
 *
 * بنية IR:
 * ─────────
 * - IRNode: عقدة واحدة في شجرة IR (مشابهة لـ UINode لكن أبسط)
 * - IRProperty: خاصية واحدة لعقدة IR (عرض، لون، نص...)
 * - IREvent: حدث مرتبط بعقدة (نقر، سحب...)
 * - IRStateRef: إشارة لمتغير حالة (ربط تفاعلي)
 * - RenderingHint: تلميح للمولّد (استخدم أصلي أم Canvas)
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_IR_H
#define SAD_UI_IR_H

#include "types.h"
#include "text_normalize.h" // (AR) تجريد التشكيل من الوسائط النصّيّة قبل المطابقة
// (AR) قوائم X-macro مولَّدة من language-truth (مصدر الحقيقة الوحيد لأسماء المفردات).
#include "generated/animation_vocab_generated.h"
#include "generated/easing_vocab_generated.h"

#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <optional>
#include <unordered_map>

namespace sad
{
    namespace ui
    {

        // ═══════════════════════════════════════════════════════════════════════════════
        // تلميح العرض (Rendering Hint)
        // ═══════════════════════════════════════════════════════════════════════════════

        /**
         * @brief تلميحات لمولّد الكود عن كيفية عرض العنصر
         *
         * هذا هو قلب نظام التوجيه الذكي:
         * - Native: استخدم Widget الأصلي للمنصة (Button, Text, etc.)
         * - Canvas: ارسم العنصر يدوياً على Canvas
         * - Auto: اتركه للمولّد ليقرر (الافتراضي)
         */
        enum class RenderingHint : uint8_t
        {
            Auto,   ///< المولّد يقرر تلقائياً
            Native, ///< استخدم العنصر الأصلي للمنصة
            Canvas, ///< ارسم يدوياً على Canvas
            Hybrid  ///< جزء أصلي + جزء Canvas (مثل: زر بشكل مخصص)
        };

        // ═══════════════════════════════════════════════════════════════════════════════
        // خاصية IR (IR Property)
        // ═══════════════════════════════════════════════════════════════════════════════

        /**
         * @brief خاصية واحدة لعقدة IR
         *
         * كل خاصية تحتوي على:
         * - اسم (key) — مثل "عرض"، "لون"، "نص"
         * - قيمة — يمكن أن تكون رقم، نص، لون، أو بُعد
         *
         * الخصائص تُستخرج من معدّلات UINode وتُخزن بصيغة أبسط في IR.
         */
        struct IRProperty
        {
            /// القيمة يمكن أن تكون أي نوع من هذه الأنواع
            using Value = std::variant<
                int64_t,    // عدد صحيح
                double,     // عدد عشري
                bool,       // منطقي
                std::string // نص (يشمل ألوان مشفرة، أبعاد، تعبيرات)
                >;

            std::string key; ///< اسم الخاصية
            Value value;     ///< قيمة الخاصية
        };

        // ═══════════════════════════════════════════════════════════════════════════════
        // حدث IR (IR Event)
        // ═══════════════════════════════════════════════════════════════════════════════

        /**
         * @brief حدث مرتبط بعقدة IR
         *
         * الأحداث تمثل التفاعلات (نقر، سحب، ظهور...).
         * كل حدث يحتوي على:
         * - نوعه (onTap, onLongPress...)
         * - التعبير الذي يُنفذ عند حدوث الحدث
         * - (اختياري) متغيرات الحالة التي يُعدّلها
         */
        struct IREvent
        {
            IREventType type = IREventType::Custom;                ///< نوع الحدث (enum موحد)
            std::string expression;                                ///< تعبير الاستدعاء
            std::vector<std::string> modifiedStates;               ///< متغيرات الحالة المتأثرة
            std::string customEventName;                           ///< اسم الحدث المخصص (فقط عندما type == Custom)
            EventPropagation propagation = EventPropagation::None; ///< طور الانتشار
            /// (AR) بيانات حرّة يربطها المبرمج بهذا المعالِج عند التسجيل
            ///      (`.عند_النقر(دالّة، "بيانات")`) — تصل الحقلَ «بيانات» في بنية
            ///      «حدث». كان الحقل يُقرأ في المحرّكين ولا يكتبه أحد.
            std::string userData;

            /// الحصول على اسم الحدث (العربي الأساسي، أو المخصص)
            const std::string &getEventName() const
            {
                if (type == IREventType::Custom)
                {
                    return customEventName;
                }
                return irEventTypeToString(type);
            }
        };

        // ═══════════════════════════════════════════════════════════════════════════════
        // تحريك IR (IR Animation) — بيانات التحريك المرتبطة بعقدة
        // ═══════════════════════════════════════════════════════════════════════════════

        /**
         * @brief (AR) نوع التحريك — يحدد ما يتغير أثناء الحركة
         * @brief (EN) Animation type — specifies what changes during animation
         *
         * أنواع مدعومة:
         *   ظهور (FadeIn)، اختفاء (FadeOut)، تكبير (ScaleUp)، تصغير (ScaleDown)
         *   انزلاق_يمين/يسار/أعلى/أسفل (SlideRight/Left/Up/Down)
         *   دوران (Rotate)، نبض (Pulse)، اهتزاز (Shake)، ارتداد (Bounce)
         *   تلاشي (CrossFade)، مخصص (Custom)
         */
        enum class AnimationType : uint8_t
        {
            FadeIn,     ///< ظهور — من شفاف إلى معتم
            FadeOut,    ///< اختفاء — من معتم إلى شفاف
            ScaleUp,    ///< تكبير — من صغير إلى الحجم الطبيعي
            ScaleDown,  ///< تصغير — من الحجم الطبيعي إلى صغير
            SlideRight, ///< انزلاق_يمين — دخول من اليسار
            SlideLeft,  ///< انزلاق_يسار — دخول من اليمين
            SlideUp,    ///< انزلاق_أعلى — دخول من الأسفل
            SlideDown,  ///< انزلاق_أسفل — دخول من الأعلى
            Rotate,     ///< دوران — تدوير العنصر
            Pulse,      ///< نبض — تكبير وتصغير متكرر
            Shake,      ///< اهتزاز — حركة أفقية سريعة
            Bounce,     ///< ارتداد — نطة لطيفة
            CrossFade,  ///< تلاشي — تبديل سلس بين حالتين
            ZoomIn,     ///< تقريب — تكبير مع ظهور تدريجي
            ZoomOut,    ///< تبعيد — تصغير مع اختفاء تدريجي
            DropIn,     ///< سقوط — نزول من أعلى مع ارتداد
            FlyOut,     ///< طيران — انزلاق للخارج مع اختفاء
            Swing,      ///< تأرجح — حركة بندولية مستمرة
            FlipX,      ///< قلب_أفقي — قلب 180° حول المحور الأفقي (محاكاة 3D عبر scale Y)
            FlipY,      ///< قلب_عمودي — قلب 180° حول المحور العمودي (محاكاة 3D عبر scale X)
            Wobble,     ///< ترنّح — اهتزاز دوراني سريع مثل جرس
            BounceIn,   ///< ارتداد_دخول — انزلاق مع ارتداد مرن على المحور X
            Blink,      ///< وميض — ظهور/اختفاء متكرر سريع
            Stretch,    ///< تمطّي — تمدد مطاطي إلى الحجم الطبيعي
            Explode,    ///< انفجار — تكبير سريع مع اختفاء (خروج انفجاري)
            Spin3D,     ///< دورة_3D — دوران مع تغيّر حجم (محاكاة عمق 3D)
            Custom      ///< مخصص — يُحدد بالخصائص
        };

        /**
         * @brief (AR) منحنى التحريك — يتحكم في سرعة التحريك عبر الزمن
         * @brief (EN) Easing curve — controls animation speed over time
         */
        enum class EasingCurve : uint8_t
        {
            Linear,    ///< خطي — سرعة ثابتة
            EaseIn,    ///< تسارع — بطيء ثم سريع
            EaseOut,   ///< تباطؤ — سريع ثم بطيء
            EaseInOut, ///< تسارع_وتباطؤ — بطيء ثم سريع ثم بطيء
            Bounce,    ///< ارتداد — ارتداد في النهاية
            Elastic,   ///< مرن — تمدد مرن في النهاية
            Spring     ///< نابض — فيزيائي نابضي
        };

        /**
         * @brief (AR) بنية تحريك كاملة مرتبطة بعقدة IR
         * @brief (EN) Complete animation config attached to an IRNode
         *
         * تُخزّن على IRNode وتُقرأ بواسطة SDL backend لتنفيذ الحركة.
         *
         * الاستخدام في كود ص:
         *   زر("اضغط").حرّك("ظهور").مدة(0.5).منحنى("مرونة").تأخير(0.2)
         *   نص("مرحبا").حرّك("انزلاق_يمين").مدة(0.3).تكرار(3)
         */
        struct IRAnimation
        {
            AnimationType type = AnimationType::FadeIn;  ///< نوع التحريك
            EasingCurve easing = EasingCurve::EaseInOut; ///< المنحنى الزمني
            float duration = 0.3f;                       ///< المدة بالثواني
            float delay = 0.0f;                          ///< التأخير قبل البدء
            int repeatCount = 1;                         ///< عدد التكرارات (0 = لانهائي)
            bool autoReverse = false;                    ///< عكس الحركة تلقائياً
            bool playOnAppear = true;                    ///< تشغيل عند الظهور

            /// قيم مخصصة للتحريك المخصص (مثل زاوية الدوران، مسافة الانزلاق)
            float customFromValue = 0.0f;
            float customToValue = 1.0f;
            std::string customProperty; ///< اسم الخاصية المتحركة (للنوع Custom)
        };

        /**
         * @brief (AR) تحويل اسم تحريك عربي/إنجليزي إلى AnimationType
         * @brief (EN) Convert Arabic/English animation name to AnimationType
         */
        inline AnimationType stringToAnimationType(const std::string &name)
        {
            // (AR) اسمٌ عربيّ قانونيّ واحد لكلّ نوع (من language-truth/ui_animations.yaml).
            //   الوسيط قد يحمل تشكيلًا (وسيطٌ نصّيّ لا معرّف)، فنجرّده أوّلًا فيستوي
            //   «ترنّح» و«ترنح». لا بدائل إنجليزيّة (سياسة الأسماء القانونيّة فقط).
            const std::string s = stripArabicDiacritics(name);
#define X(id, str)          \
    if (s == str)           \
        return AnimationType::id;
            SAD_UI_ANIMATION_VOCAB(X)
#undef X
            return AnimationType::Custom;
        }

        /**
         * @brief (AR) تحويل اسم منحنى عربي/إنجليزي إلى EasingCurve
         * @brief (EN) Convert Arabic/English easing name to EasingCurve
         */
        inline EasingCurve stringToEasingCurve(const std::string &name)
        {
            // (AR) اسمٌ عربيّ قانونيّ واحد لكلّ منحنى (من language-truth/ui_easings.yaml)؛
            //   نجرّد تشكيل الوسيط قبل المطابقة. الافتراض عند عدم التطابق: تسارع_وتباطؤ.
            const std::string s = stripArabicDiacritics(name);
#define X(id, str)          \
    if (s == str)           \
        return EasingCurve::id;
            SAD_UI_EASING_VOCAB(X)
#undef X
            return EasingCurve::EaseInOut;
        }

        /**
         * @brief (AR) تحويل AnimationType إلى اسم عربي
         * @brief (EN) Convert AnimationType to Arabic name
         */
        inline std::string animationTypeToString(AnimationType type)
        {
            // (AR) الاتّجاه المعكوس من نفس المصدر (SAD_UI_ANIMATION_VOCAB) — الاسم
            //   العربيّ القانونيّ بلا تشكيل. Custom مُدرَج في القائمة (احتياطيّ).
            switch (type)
            {
#define X(id, str)              \
    case AnimationType::id:     \
        return str;
                SAD_UI_ANIMATION_VOCAB(X)
#undef X
            }
            return "\xd9\x85\xd8\xae\xd8\xb5\xd8\xb5"; // مخصص (احتياطيّ لقيمةٍ خارج القائمة)
        }

        /**
         * @brief (AR) تحويل EasingCurve إلى اسم عربي
         * @brief (EN) Convert EasingCurve to Arabic name
         */
        inline std::string easingCurveToString(EasingCurve curve)
        {
            // (AR) الاتّجاه المعكوس من نفس المصدر (SAD_UI_EASING_VOCAB).
            switch (curve)
            {
#define X(id, str)              \
    case EasingCurve::id:       \
        return str;
                SAD_UI_EASING_VOCAB(X)
#undef X
            }
            return "\xd8\xaa\xd8\xb3\xd8\xa7\xd8\xb1\xd8\xb9_\xd9\x88\xd8\xaa\xd8\xa8\xd8\xa7\xd8\xb7\xd8\xa4"; // تسارع_وتباطؤ (احتياطيّ)
        }

        // ═══════════════════════════════════════════════════════════════════════════════
        // إشارة حالة IR (IR State Reference)
        // ═══════════════════════════════════════════════════════════════════════════════

        /**
         * @brief إشارة لمتغير حالة في عقدة IR
         *
         * عندما يعتمد عنصر واجهة على متغير حالة (@حالة أو @ربط)،
         * يُخزّن ذلك هنا حتى يعرف المولّد كيف يربط العنصر بتحديثات الحالة.
         */
        struct IRStateRef
        {
            std::string stateName;        ///< اسم المتغير
            std::string propertyBinding;  ///< الخاصية المرتبطة (مثل: "نص" في نص(عداد))
            StateBindingType bindingType; ///< نوع الربط
        };

        // ═══════════════════════════════════════════════════════════════════════════════
        // عقدة IR (IRNode) — العقدة الرئيسية في شجرة IR
        // ═══════════════════════════════════════════════════════════════════════════════

        /**
         * @brief عقدة واحدة في شجرة IR
         *
         * هذه نسخة مبسطة ومُحسّنة من UINode مصممة لتوليد الكود.
         * الفرق عن UINode:
         * - المعدّلات مُحلّلة إلى خصائص ثابتة (IRProperty)
         * - الأحداث مفصولة عن الخصائص البصرية
         * - تلميح العرض (RenderingHint) يوجه المولّد
         * - إشارات الحالة محددة بوضوح
         */
#if defined(SAD_HEAP_TRACE)
        /// (تشخيصٌ مشروط) عددُ عقد IR الحيّة — يُعرَّف في core/src/ir.cpp
        extern long long g_liveIRNodes;
#endif

        class IRNode : public std::enable_shared_from_this<IRNode>
        {
        public:
            /// إنشاء عقدة IR من نوع محدد
            explicit IRNode(UINodeType type);

#if defined(SAD_HEAP_TRACE)
            /// (تشخيصٌ مشروط) هادمٌ مصرَّحٌ به ليَعُدّ العقدَ الحيّة — انظر ir.cpp
            ~IRNode();
#endif

            /// إنشاء سريع — يعيد مؤشر ذكي
            static std::shared_ptr<IRNode> create(UINodeType type);

            // ─── الخصائص الأساسية ────────────────────────

            /// نوع العنصر
            UINodeType getType() const { return type_; }

            /// معرّف فريد (من UINode الأصلي)
            const std::string &getId() const { return id_; }
            void setId(const std::string &id) { id_ = id; }

            /// تلميح العرض
            RenderingHint getRenderingHint() const { return hint_; }
            void setRenderingHint(RenderingHint hint) { hint_ = hint; }

            /// المنصة المُستهدفة
            TargetPlatform getTargetPlatform() const { return platform_; }
            void setTargetPlatform(TargetPlatform p) { platform_ = p; }

            // ─── الخصائص (Properties) ──────────────────

            /// إضافة خاصية
            void addProperty(const IRProperty &prop) { properties_.push_back(prop); }

            /// الحصول على كل الخصائص
            const std::vector<IRProperty> &getProperties() const { return properties_; }

            /// البحث عن خاصية بالاسم
            const IRProperty *findProperty(const std::string &key) const;

            /// تعديل أو إضافة خاصية بالاسم
            void setProperty(const std::string &key, IRProperty::Value value)
            {
                for (auto &prop : properties_)
                {
                    if (prop.key == key)
                    {
                        prop.value = value;
                        return;
                    }
                }
                IRProperty newProp;
                newProp.key = key;
                newProp.value = value;
                properties_.push_back(newProp);
            }

            // ─── الأحداث (Events) ──────────────────────

            /// إضافة حدث
            void addEvent(const IREvent &event) { events_.push_back(event); }

            /// الحصول على كل الأحداث
            const std::vector<IREvent> &getEvents() const { return events_; }

            /// الحصول على كل الأحداث (قابل للتعديل — لتحديث معرّفات المعالجات)
            std::vector<IREvent> &getMutableEvents() { return events_; }

            // ─── التحريكات (Animations) ────────────────

            /// إضافة تحريك
            void addAnimation(const IRAnimation &anim) { animations_.push_back(anim); }

            /// الحصول على كل التحريكات
            const std::vector<IRAnimation> &getAnimations() const { return animations_; }

            /// الحصول على كل التحريكات (قابل للتعديل)
            std::vector<IRAnimation> &getMutableAnimations() { return animations_; }

            /// هل يوجد تحريكات؟
            bool hasAnimations() const { return !animations_.empty(); }

            // ─── إشارات الحالة (State References) ──────

            /// إضافة إشارة حالة
            void addStateRef(const IRStateRef &ref) { stateRefs_.push_back(ref); }

            /// الحصول على كل إشارات الحالة
            const std::vector<IRStateRef> &getStateRefs() const { return stateRefs_; }

            // ─── الأبناء (Children) ────────────────────

            /// إضافة عقدة ابن
            void addChild(std::shared_ptr<IRNode> child)
            {
                child->parent_ = shared_from_this();
                children_.push_back(std::move(child));
            }

            /// الحصول على كل الأبناء
            const std::vector<std::shared_ptr<IRNode>> &getChildren() const { return children_; }

            /// عدد الأبناء
            size_t childCount() const { return children_.size(); }

            // ─── العنصر الأب (Parent) ───────────────────

            /// الحصول على الأب (قد يكون nullptr)
            std::shared_ptr<IRNode> getParent() const { return parent_.lock(); }

            /// هل لهذه العقدة أب؟
            bool hasParent() const { return !parent_.expired(); }

            /// بناء مسار الأجداد من هذه العقدة صعوداً إلى الجذر
            std::vector<IRNode *> getAncestorPath() const
            {
                std::vector<IRNode *> path;
                auto p = parent_.lock();
                while (p)
                {
                    path.push_back(p.get());
                    p = p->parent_.lock();
                }
                return path;
            }

            // ─── التصحيح ────────────────────────────────

            /// طباعة شجرة IR بصيغة نصية
            std::string dump(int indent = 0) const;

        private:
            UINodeType type_;                               ///< نوع العنصر
            std::string id_;                                ///< معرّف فريد
            RenderingHint hint_ = RenderingHint::Auto;      ///< تلميح العرض
            TargetPlatform platform_ = TargetPlatform::All; ///< المنصة المستهدفة
            std::vector<IRProperty> properties_;            ///< الخصائص
            std::vector<IREvent> events_;                   ///< الأحداث
            std::vector<IRAnimation> animations_;           ///< التحريكات
            std::vector<IRStateRef> stateRefs_;             ///< إشارات الحالة
            std::vector<std::shared_ptr<IRNode>> children_; ///< الأبناء
            std::weak_ptr<IRNode> parent_;                  ///< الأب (weak لتجنب دورات المراجع)
        };

        // ═══════════════════════════════════════════════════════════════════════════════
        // وحدة IR (IR Module) — يمثل ملف أو شاشة كاملة
        // ═══════════════════════════════════════════════════════════════════════════════

        /**
         * @brief وحدة IR تمثل ملف واحد أو شاشة كاملة
         *
         * تحتوي على:
         * - قائمة تعريفات الواجهات (مثل: واجهة شاشة_رئيسية { ... })
         * - متغيرات الحالة العامة
         * - اسم الوحدة
         */
        struct IRModule
        {
            /// اسم الوحدة (عادة اسم الملف)
            std::string name;

            /// الشجرة الجذرية لـ IR
            std::shared_ptr<IRNode> root;

            /// تعريفات الحالة (اسم → نوع)
            std::unordered_map<std::string, std::string> stateDefinitions;

            /// ملف المصدر
            std::string sourceFile;
        };

    } // namespace ui
} // namespace sad

#endif // SAD_UI_IR_H
