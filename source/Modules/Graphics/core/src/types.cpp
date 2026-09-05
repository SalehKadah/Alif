/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: types.cpp
 * المسار: features/graphics/core/src/types.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * تنفيذ دوال تحويل الأنواع (أسماء عربية ↔ enum).
 *
 * هذا الملف يُنفّذ الدوال المعلنة في types.h لتحويل أنواع العناصر
 * والمعدّلات بين قيم enum وأسمائها العربية.
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/types.h"
#include "sad_ui/prop_keys.h" // (AR) SoT مفاتيح الخصائص المولَّد (props::PADDING …)
#include "sad_ui/text_normalize.h" // (AR) تجريد التشكيل من اسم الحدث قبل المطابقة
// (AR) قائمة X-macro لأنواع الأحداث مولَّدة من language-truth/ui_events.yaml.
#include "sad_ui/generated/event_vocab_generated.h"
#include "sad_ui/generated/propagation_vocab_generated.h"
// (AR) جداول الألوان الموحَّدة مولَّدة من language-truth/ui_colors.yaml.
#include "sad_ui/generated/color_table_generated.h"

#include <unordered_map>

namespace sad
{
    namespace ui
    {

        // ═══════════════════════════════════════════════════════════════════════════════
        // جداول التحويل — أنواع العناصر
        // ═══════════════════════════════════════════════════════════════════════════════

        /// جدول: UINodeType → الاسم العربيّ القانونيّ
        /// (AR) مولَّدٌ كلُّه من language-truth/ui_nodes.yaml عبر X-macro — لا اسمَ
        ///   عقدةٍ حرفيّ هنا، فيستحيل انحرافُ الجدول عن التعداد أو عن مصدر الحقيقة.
        static const std::unordered_map<UINodeType, std::string> &getNodeTypeNames()
        {
            static const std::unordered_map<UINodeType, std::string> table = {
#define SAD_UI_NODE_NAME_ENTRY(Id, CanonicalName) {UINodeType::Id, CanonicalName},
                SAD_UI_NODE_TYPE_LIST(SAD_UI_NODE_NAME_ENTRY)
#undef SAD_UI_NODE_NAME_ENTRY
            };
            return table;
        }

        /// جدول: الاسم العربيّ → UINodeType
        /// (AR) يقبل الاسمَ القانونيّ **وأسماءَ القراءة الإضافيّة** (أسماءُ المصانع
        ///   المدمَجة المشتقّة آليًّا وحدَها؛ لا اسمَ انتقاليًّا) — كلُّها من مصدر
        ///   الحقيقة. الاسمُ القانونيّ وحدَه هو ما يُطبَع (getNodeTypeNames).
        static const std::unordered_map<std::string, UINodeType> &getNodeTypeByName()
        {
            static const std::unordered_map<std::string, UINodeType> table = {
#define SAD_UI_NODE_BY_NAME_ENTRY(Id, Name) {Name, UINodeType::Id},
                SAD_UI_NODE_TYPE_LIST(SAD_UI_NODE_BY_NAME_ENTRY)
                SAD_UI_NODE_ALT_NAME_LIST(SAD_UI_NODE_BY_NAME_ENTRY)
#undef SAD_UI_NODE_BY_NAME_ENTRY
            };
            return table;
        }

        std::string nodeTypeToArabicName(UINodeType type)
        {
            const auto &table = getNodeTypeNames();
            auto it = table.find(type);
            if (it != table.end())
            {
                return it->second;
            }
            return "غير_معروف";
        }

        std::optional<UINodeType> arabicNameToNodeType(const std::string &name)
        {
            const auto &table = getNodeTypeByName();
            auto it = table.find(name);
            if (it != table.end())
            {
                return it->second;
            }
            return std::nullopt;
        }

        // ═══════════════════════════════════════════════════════════════════════════════
        // جداول التحويل — أنواع المعدّلات
        // ═══════════════════════════════════════════════════════════════════════════════

        /// جدول: ModifierType → اسم عربي
        static const std::unordered_map<ModifierType, std::string> &getModifierTypeNames()
        {
            static std::unordered_map<ModifierType, std::string> table = {
                // ─── الألوان ──────────────
                {ModifierType::ForegroundColor, "لون"},
                {ModifierType::BackgroundColor, "لون_خلفية"},
                {ModifierType::TintColor, "لون_تظليل"},
                {ModifierType::GradientColor, "تدرج"},
                // ─── الخط ─────────────────
                {ModifierType::FontSize, "حجم_خط"},
                {ModifierType::FontWeight, "وزن_خط"},
                {ModifierType::FontFamily, "عائلة_خط"},
                {ModifierType::FontStyle, "نمط_خط"},
                {ModifierType::TextAlign, "محاذاة_نص"},
                {ModifierType::LineSpacing, "تباعد_أسطر"},
                {ModifierType::TextDecoration, "زخرفة_نص"},
                // ─── الأبعاد ──────────────
                {ModifierType::Width, "عرض"},
                {ModifierType::Height, "ارتفاع"},
                {ModifierType::MinWidth, "أدنى_عرض"},
                {ModifierType::MinHeight, "أدنى_ارتفاع"},
                {ModifierType::MaxWidth, "أقصى_عرض"},
                {ModifierType::MaxHeight, "أقصى_ارتفاع"},
                // ─── المسافات ─────────────
                {ModifierType::Padding, props::PADDING}, // SoT: القانونيّ «حشوة»
                {ModifierType::PaddingTop, props::PADDING_TOP},       // SoT «حشوة_أعلى»
                {ModifierType::PaddingBottom, props::PADDING_BOTTOM}, // SoT «حشوة_أسفل»
                {ModifierType::PaddingStart, props::PADDING_START},   // SoT «حشوة_بداية» (منطقيّ RTL)
                {ModifierType::PaddingEnd, props::PADDING_END},       // SoT «حشوة_نهاية» (منطقيّ RTL)
                {ModifierType::Margin, "هامش"},
                // ─── الحدود ───────────────
                {ModifierType::Border, "حد"},
                {ModifierType::BorderRadius, "نصف_قطر"},
                {ModifierType::BorderWidth, "سمك_حد"},
                {ModifierType::BorderColor, "لون_حد"},
                // ─── الموضع ───────────────
                {ModifierType::Alignment, "محاذاة"},
                {ModifierType::Position, "موقع"},
                {ModifierType::Offset, "إزاحة"},
                // ─── التأثيرات ────────────
                {ModifierType::Shadow, "ظل"},
                {ModifierType::Opacity, "عتامة"},
                {ModifierType::Blur, "ضبابية"},
                {ModifierType::ClipShape, "شكل_قص"},
                // ─── التحويلات ────────────
                {ModifierType::Rotation, "دوران"},
                {ModifierType::Scale, "مقياس"},
                {ModifierType::Translation, "ترجمة"},
                // ─── سهولة الوصول ─────────
                {ModifierType::AccessibilityLabel, "تسمية_وصول"},
                {ModifierType::AccessibilityHint, "تلميح_وصول"},
                // ─── الرسوم المتحركة ─────
                {ModifierType::Animation, "رسوم_متحركة"},
                {ModifierType::Transition, "انتقال"},
            };

            // (AR) أسماء معدّلات الأحداث تُشتقّ من ui_events.yaml (عبر
            //      irEventTypeToString) لا تُكتب حرفيًّا هنا: الاسمُ القانونيّ
            //      للمعدّل هو اسمُ الحدث نفسه. فمعدّلٌ جديد في الخريطة يكتسب
            //      اسمه تلقائيًّا، ولا يمكن أن ينحرف اسمُه عن اسم الحدث.
            static const bool eventsSeeded = []()
            {
#define X(modId, evtId)                          \
    table[ModifierType::modId] =                 \
        irEventTypeToString(IREventType::evtId);
                SAD_UI_EVENT_MODIFIER_MAP(X)
#undef X
                return true;
            }();
            (void)eventsSeeded;

            return table;
        }

        /// جدول معكوس: اسم عربي → ModifierType
        static const std::unordered_map<std::string, ModifierType> &getModifierTypeByName()
        {
            static std::unordered_map<std::string, ModifierType> table;
            static bool initialized = false;
            if (!initialized)
            {
                for (const auto &[type, name] : getModifierTypeNames())
                {
                    table[name] = type;
                }
                initialized = true;
            }
            return table;
        }

        std::string modifierTypeToArabicName(ModifierType type)
        {
            const auto &table = getModifierTypeNames();
            auto it = table.find(type);
            if (it != table.end())
            {
                return it->second;
            }
            return "غير_معروف";
        }

        std::optional<ModifierType> arabicNameToModifierType(const std::string &name)
        {
            const auto &table = getModifierTypeByName();
            auto it = table.find(name);
            if (it != table.end())
            {
                return it->second;
            }
            return std::nullopt;
        }


        // ═══════════════════════════════════════════════════════════════════════════════
        // جداول التحويل — أطوار الانتشار (EventPropagation) — SoT: ui_propagation.yaml
        // ═══════════════════════════════════════════════════════════════════════════════

        namespace
        {
            /// ترتيب المفردة كما ولّدها المولّد من ui_propagation.yaml.
            enum class PropagationVocabOrder : uint8_t
            {
#define X(id, str) id,
                SAD_UI_PROPAGATION_VOCAB(X)
#undef X
                    _Count
            };

#define X(id, str)                                                                  static_assert(static_cast<int>(PropagationVocabOrder::id) ==                                      static_cast<int>(EventPropagation::id),                                     "ترتيب ui_propagation.yaml خالف enum EventPropagation.");
            SAD_UI_PROPAGATION_VOCAB(X)
#undef X
        } // namespace

        EventPropagation stringToEventPropagation(const std::string &name)
        {
            // (AR) الاسم وسيطٌ نصّيّ قد يحمل تشكيلًا — يُجرَّد قبل المطابقة كالأحداث.
            static const std::unordered_map<std::string, EventPropagation> table = {
#define X(id, str) {str, EventPropagation::id},
                SAD_UI_PROPAGATION_VOCAB(X)
#undef X
            };
            const auto it = table.find(stripArabicDiacritics(name));
            // (AR) فشل-آمن: اسمٌ مجهول ⇒ لا انتشار (لا نُفعّل تفرّعًا لم يُطلَب).
            return it != table.end() ? it->second : EventPropagation::None;
        }

        const std::string &eventPropagationToString(EventPropagation phase)
        {
            static const std::unordered_map<EventPropagation, std::string> table = {
#define X(id, str) {EventPropagation::id, str},
                SAD_UI_PROPAGATION_VOCAB(X)
#undef X
            };
            static const std::string unknown;
            const auto it = table.find(phase);
            return it != table.end() ? it->second : unknown;
        }

        // ═══════════════════════════════════════════════════════════════════════════════
        // حارس الترتيب: ui_events.yaml ↔ enum IREventType (فحص وقت الترجمة)
        // ═══════════════════════════════════════════════════════════════════════════════
        //
        // (AR) القيمة العدديّة لـIREventType **جزء من ABI**: المترجم يبصمها في
        //      البنيّة المولَّدة (حقل «نوع» في بنية «حدث») ويقرؤها وقتُ التشغيل.
        //      ولمّا كان الترتيب يُكتب مرّتين — في language-truth/ui_events.yaml
        //      وفي enum IREventType يدويًّا — فإنّ إدراج نوعٍ في وسط أحدهما دون
        //      الآخر يزيح كلّ ما بعده صامتًا: يُطلَق حدثٌ ويُفسَّر آخر.
        //      هذا الحارس يجعل التباعد **خطأ ترجمة** لا عيبًا صامتًا في وقت
        //      التشغيل، ريثما يصير الـenum نفسه مُولَّدًا (إنهاء SoT المزدوج).
        //
        // (EN) IREventType's numeric values are ABI: the compiler stamps them into
        //      the generated event struct. The order is written twice (YAML SoT +
        //      hand-written enum), so an insertion in one shifts everything after
        //      it silently. This guard turns that divergence into a compile error.
        // ═══════════════════════════════════════════════════════════════════════════════
        namespace
        {
            /// ترتيب المفردة كما ولّدها gen_ui_vocab.py من ui_events.yaml.
            enum class EventVocabOrder : uint8_t
            {
#define X(id, str) id,
                SAD_UI_EVENT_VOCAB(X)
#undef X
                    _Count
            };

#define X(id, str)                                                              \
    static_assert(static_cast<int>(EventVocabOrder::id) ==                      \
                      static_cast<int>(IREventType::id),                        \
                  "ترتيب ui_events.yaml خالف enum IREventType — القيم العدديّة " \
                  "جزء من ABI (حقل «نوع» في بنية «حدث»).");
            SAD_UI_EVENT_VOCAB(X)
#undef X

            static_assert(static_cast<int>(EventVocabOrder::_Count) ==
                              static_cast<int>(IREventType::_Count),
                          "عدد أنواع الأحداث في ui_events.yaml خالف enum IREventType — "
                          "نوعٌ مُضاف في أحدهما دون الآخر.");
        } // namespace

        // ═══════════════════════════════════════════════════════════════════════════════
        // جداول التحويل — أنواع أحداث IR (IREventType)
        // ═══════════════════════════════════════════════════════════════════════════════

        /// جدول: IREventType → الاسم العربي الأساسي (مولَّد من ui_events.yaml)
        static const std::unordered_map<IREventType, std::string> &getIREventTypeNames()
        {
            static const std::unordered_map<IREventType, std::string> table = {
#define X(id, str) {IREventType::id, str},
                SAD_UI_EVENT_VOCAB(X)
#undef X
            };
            return table;
        }

        /// جدول معكوس: اسم عربيّ قانونيّ → IREventType (مولَّد؛ لا بدائل)
        static const std::unordered_map<std::string, IREventType> &getIREventTypeByName()
        {
            static const std::unordered_map<std::string, IREventType> table = {
#define X(id, str) {str, IREventType::id},
                SAD_UI_EVENT_VOCAB(X)
#undef X
            };
            return table;
        }

        IREventType stringToIREventType(const std::string &name)
        {
            // (AR) اسمُ الحدث وسيطٌ نصّيّ قد يحمل تشكيلًا — نجرّده قبل البحث فيستوي
            //   «عند_التغيّر» و«عند_التغير». لا بدائل إنجليزيّة (قانونيّ فقط).
            const std::string key = stripArabicDiacritics(name);
            const auto &table = getIREventTypeByName();
            auto it = table.find(key);
            if (it != table.end())
            {
                return it->second;
            }
            return IREventType::Custom;
        }

        const std::string &irEventTypeToString(IREventType type)
        {
            static const std::string unknown = "\xd9\x85\xd8\xae\xd8\xb5\xd8\xb5"; // مخصص
            const auto &table = getIREventTypeNames();
            auto it = table.find(type);
            if (it != table.end())
            {
                return it->second;
            }
            return unknown;
        }

        bool isKnownEventName(const std::string &name)
        {
            return stringToIREventType(name) != IREventType::Custom;
        }

        IREventType modifierTypeToIREventType(ModifierType mod)
        {
            // (AR) الحالات مُوسَّعة من خريطة المعدّلات — إضافةُ سطرٍ هناك تُغطّي
            //      التحويل والقائمة البيضاء معًا، فلا يبقى موضعٌ يُنسى.
            switch (mod)
            {
#define X(modId, evtId)              \
    case ModifierType::modId:        \
        return IREventType::evtId;
                SAD_UI_EVENT_MODIFIER_MAP(X)
#undef X
            default:
                return IREventType::Custom;
            }
        }

        bool isScrollableType(UINodeType type)
        {
            return type == UINodeType::ScrollView ||
                   type == UINodeType::LazyColumn ||
                   type == UINodeType::LazyRow ||
                   type == UINodeType::List;
        }

        bool isEventModifier(ModifierType mod)
        {
            switch (mod)
            {
#define X(modId, evtId) case ModifierType::modId:
                SAD_UI_EVENT_MODIFIER_MAP(X)
#undef X
                return true;
            default:
                return false;
            }
        }

        // ═══════════════════════════════════════════════════════════════════════════════
        // جداول التحويل — الألوان المسماة
        // ═══════════════════════════════════════════════════════════════════════════════

        /// جدول: اسم عربيّ قانونيّ → NamedColor (مولَّد من ui_colors.yaml؛ لا بدائل)
        static const std::unordered_map<std::string, NamedColor> &getColorByName()
        {
            static const std::unordered_map<std::string, NamedColor> table = {
#define X(name, id) {name, NamedColor::id},
                SAD_UI_COLOR_BY_NAME(X)
#undef X
            };
            return table;
        }

        std::optional<NamedColor> arabicNameToColor(const std::string &name)
        {
            const auto &table = getColorByName();
            auto it = table.find(name);
            if (it != table.end())
            {
                return it->second;
            }
            return std::nullopt;
        }

        uint32_t namedColorToRGBA(NamedColor color)
        {
            // (AR) مولَّد من ui_colors.yaml (SAD_UI_COLOR_RGBA) — مصدرٌ واحد مع
            //   getColorByName والجدول النصّيّ، فلا تباعد صامت.
            switch (color)
            {
#define X(id, rgba) case NamedColor::id: return rgba;
                SAD_UI_COLOR_RGBA(X)
#undef X
            default:
                return 0x000000FF;
            }
        }

    } // namespace ui
} // namespace sad
