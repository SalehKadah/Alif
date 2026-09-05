/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: node.h
 * المسار: features/graphics/core/include/sad_ui/node.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * الوصف:
 * ------
 * هذا الملف يُعرّف شجرة عناصر الواجهة (UI Node Tree) — وهي البنية المركزية
 * التي تُمثل واجهة المستخدم في الذاكرة. كل شاشة في التطبيق هي شجرة من
 * عقد UINode، حيث كل عقدة تمثل عنصر واجهة واحد (نص، زر، عمود...) مع
 * خصائصه ومعدّلاته وأبنائه.
 *
 * مثال على الشجرة:
 * ─────────────────
 *   عمود                          ← UINode(Column)
 *   ├── نص("مرحباً")              ← UINode(Text, args=["مرحباً"])
 *   │   └── [معدّل] حجم_خط(24)    ← Modifier(FontSize, 24)
 *   ├── فاصل()                    ← UINode(Spacer)
 *   └── زر("اضغط هنا")           ← UINode(Button, args=["اضغط هنا"])
 *       └── [معدّل] لون(.أزرق)    ← Modifier(ForegroundColor, Blue)
 *
 * المبادئ التصميمية:
 * ──────────────────
 * 1. كل UINode يملك:
 *    - نوع (UINodeType) — ما هو هذا العنصر
 *    - اسم عربي — للتوثيق ورسائل الخطأ
 *    - وسائط (args) — قيم تُمرر عند الإنشاء (مثل نص الزر)
 *    - معدّلات (modifiers) — خصائص بصرية وسلوكية
 *    - أبناء (children) — عقد فرعية
 *    - ربط حالة (state bindings) — متغيرات حالة مرتبطة
 *
 * 2. الشجرة غير قابلة للتعديل بعد البناء (Immutable) — أي تغيير
 *    يتطلب إعادة بناء شجرة جديدة (مثل React Virtual DOM).
 *
 * 3. الشجرة تُبنى بواسطة UIParser أو برمجياً عبر UINodeBuilder.
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_NODE_H
#define SAD_UI_NODE_H

#include "types.h"

#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <optional>
#include <unordered_map>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// بنية اللون (Color Structure)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief تمثيل لون RGBA
 *
 * يدعم الألوان بصيغة RGBA حيث كل مكوّن من 0.0 إلى 1.0.
 * يمكن إنشاؤه من:
 * - قيم RGBA مباشرة: Color(1.0, 0.0, 0.0, 1.0) — أحمر
 * - قيمة سداسية: Color::fromHex(0xFF0000FF)
 * - لون مسمّى: Color::fromNamed(NamedColor::Red)
 */
struct Color {
    float r = 0.0f;     ///< الأحمر (0.0 - 1.0)
    float g = 0.0f;     ///< الأخضر (0.0 - 1.0)
    float b = 0.0f;     ///< الأزرق (0.0 - 1.0)
    float a = 1.0f;     ///< العتامة (0.0 شفاف - 1.0 معتم)

    /// إنشاء لون من قيمة سداسية عشرية (0xRRGGBBAA)
    static Color fromHex(uint32_t hex);

    /// إنشاء لون من لون مسمّى
    static Color fromNamed(NamedColor named);

    /// تحويل اللون إلى قيمة سداسية عشرية
    uint32_t toHex() const;

    /// تحويل اللون إلى نص "#RRGGBB" أو "#RRGGBBAA"
    std::string toHexString() const;
};

// ═══════════════════════════════════════════════════════════════════════════════
// نظام الثيم (Theme System)
// ═══════════════════════════════════════════════════════════════════════════════

enum class ThemeMode : uint8_t {
    Light,  ///< وضع فاتح
    Dark,   ///< وضع داكن
};

/// الحصول على وضع الثيم الحالي
ThemeMode getCurrentTheme();

/// تعيين وضع الثيم
void setTheme(ThemeMode mode);

/// تبديل الثيم (فاتح ↔ داكن)
void toggleTheme();

/// هل الوضع الحالي داكن؟
bool isDarkMode();

// ═══════════════════════════════════════════════════════════════════════════════
// بنية البُعد (Dimension Structure)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief تمثيل قيمة بُعدية مع وحدة القياس
 *
 * كل أبعاد الواجهة (عرض، ارتفاع، حشوة...) تُمثل بهذه البنية.
 * يدعم النظام وحدات متعددة مع تحويل تلقائي لكل منصة:
 *
 *   .عرض(100)          → Dimension(100, Dp)     // dp هي الافتراضية
 *   .عرض(50%)          → Dimension(50, Percent)
 *   .عرض(10.em)        → Dimension(10, Em)
 */
struct Dimension {
    float value = 0.0f;                     ///< القيمة الرقمية
    DimensionUnit unit = DimensionUnit::Dp; ///< وحدة القياس

    /// إنشاء بُعد بوحدة dp (الافتراضية)
    static Dimension dp(float v) { return {v, DimensionUnit::Dp}; }

    /// إنشاء بُعد بالنسبة المئوية
    static Dimension percent(float v) { return {v, DimensionUnit::Percent}; }

    /// إنشاء بُعد تلقائي (يُحسب من المحتوى)
    static Dimension autoSize() { return {0, DimensionUnit::Auto}; }
};

// ═══════════════════════════════════════════════════════════════════════════════
// بنية الحواف (Edge Insets)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief تمثيل أربع قيم لحواف العنصر (أعلى، نهاية، أسفل، بداية)
 *
 * تُستخدم للحشوة (padding) والهامش (margin).
 * ملاحظة: نستخدم start/end بدلاً من left/right لدعم RTL تلقائياً.
 */
struct EdgeInsets {
    float top = 0.0f;       ///< الحافة العلوية
    float end_ = 0.0f;      ///< حافة النهاية (يسار في RTL)
    float bottom = 0.0f;    ///< الحافة السفلية
    float start = 0.0f;     ///< حافة البداية (يمين في RTL)

    /// إنشاء حواف متساوية من الأربع جهات
    static EdgeInsets all(float v) { return {v, v, v, v}; }

    /// إنشاء حواف أفقية وعمودية
    static EdgeInsets symmetric(float horizontal, float vertical) {
        return {vertical, horizontal, vertical, horizontal};
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// بنية المعدّل (Modifier)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief معدّل واحد يُطبّق على عنصر واجهة
 *
 * كل معدّل يحتوي على:
 * - نوعه (ModifierType)
 * - قيمته (يمكن أن تكون رقم، نص، لون، بُعد، حواف، أو استدعاء)
 *
 * المعدّلات تُطبّق بالترتيب — الأول يُطبّق أولاً (من الداخل للخارج):
 *   نص("مرحبا").حشوة(8).لون_خلفية(.أحمر)
 *   ← أولاً الحشوة، ثم الخلفية حول الحشوة
 */
struct Modifier {
    /// نوع القيمة المخزنة في المعدّل (يمكن أن يكون أي نوع)
    using Value = std::variant<
        float,                      // قيمة رقمية بسيطة
        std::string,                // قيمة نصية
        bool,                       // قيمة منطقية
        Color,                      // لون
        Dimension,                  // بُعد مع وحدة
        EdgeInsets,                  // حواف (4 قيم)
        AlignmentType               // محاذاة
    >;

    ModifierType type;              ///< نوع المعدّل
    Value value;                    ///< قيمة المعدّل
    std::string callbackExpr;       ///< تعبير استدعاء (للأحداث مثل عند_النقر)

    /// إنشاء معدّل رقمي (مثل: حجم_خط(24))
    static Modifier numeric(ModifierType t, float v) {
        return {t, v, {}};
    }

    /// إنشاء معدّل لوني (مثل: لون(.أحمر))
    static Modifier color(ModifierType t, Color c) {
        return {t, c, {}};
    }

    /// إنشاء معدّل حدث (مثل: عند_النقر { ... })
    static Modifier event(ModifierType t, const std::string& expr) {
        return {t, false, expr};
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// بنية ربط الحالة (State Binding)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief ربط حالة لعنصر واجهة
 *
 * كل ربط يحتوي على:
 * - اسم المتغير (بالعربية)
 * - نوعه (نص، رقم، منطقي...)
 * - نوع الربط (حالة محلية، ربط، بيئة...)
 * - قيمة افتراضية (اختياري)
 */
struct StateBinding {
    std::string name;               ///< اسم المتغير (مثل: "عداد")
    std::string valueType;          ///< نوع القيمة (مثل: "رقم")
    StateBindingType bindingType;   ///< نوع الربط (@حالة، @ربط...)
    std::string defaultValue;       ///< القيمة الافتراضية (اختياري)
};

// ═══════════════════════════════════════════════════════════════════════════════
// عقدة الواجهة (UINode) — العقدة المركزية في الشجرة
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief عقدة واحدة في شجرة عناصر الواجهة
 *
 * هذا هو الكائن الأساسي الذي يمثل أي عنصر واجهة مستخدم.
 * الشجرة الكاملة للشاشة تتكون من عقد UINode متداخلة.
 *
 * مثال على البناء البرمجي:
 * @code
 *   auto root = UINode::create(UINodeType::Column)
 *       .addChild(UINode::create(UINodeType::Text)
 *           .setArg("مرحباً بالعالم")
 *           .addModifier(Modifier::numeric(ModifierType::FontSize, 24))
 *       )
 *       .addChild(UINode::create(UINodeType::Button)
 *           .setArg("اضغط هنا")
 *           .addModifier(Modifier::event(ModifierType::OnTap, "عداد += 1"))
 *       );
 * @endcode
 */
class UINode {
public:
    // ─── البناء والإنشاء ────────────────────────

    /// إنشاء عقدة من نوع محدد
    explicit UINode(UINodeType type);

    /// إنشاء سريع — يعيد مؤشر ذكي
    static std::shared_ptr<UINode> create(UINodeType type);

    // ─── الخصائص الأساسية ────────────────────────

    /// نوع العنصر (نص، زر، عمود...)
    UINodeType getType() const { return type_; }

    /// الاسم العربي للعنصر (للتوثيق ورسائل الخطأ)
    const std::string& getArabicName() const { return arabicName_; }

    /// تعيين الاسم العربي
    void setArabicName(const std::string& name) { arabicName_ = name; }

    /// معرّف فريد للعنصر (اختياري — للتعريف في الشجرة)
    const std::string& getId() const { return id_; }
    void setId(const std::string& id) { id_ = id; }

    // ─── الوسائط (Arguments) ───────────────────

    /// الوسيطة الأولى — عادة نص المحتوى (مثل: نص("مرحباً"))
    const std::string& getPrimaryArg() const { return primaryArg_; }
    void setPrimaryArg(const std::string& arg) { primaryArg_ = arg; }

    /// وسائط إضافية بأسماء (مثل: عمود(محاذاة: .وسط، تباعد: 8))
    const std::unordered_map<std::string, std::string>& getNamedArgs() const {
        return namedArgs_;
    }
    void setNamedArg(const std::string& key, const std::string& value) {
        namedArgs_[key] = value;
    }

    // ─── المعدّلات (Modifiers) ──────────────────

    /// إضافة معدّل (modifier) للعنصر
    void addModifier(const Modifier& mod) { modifiers_.push_back(mod); }

    /// الحصول على كل المعدّلات
    const std::vector<Modifier>& getModifiers() const { return modifiers_; }

    /// البحث عن أول معدّل من نوع محدد
    const Modifier* findModifier(ModifierType type) const;

    // ─── الأبناء (Children) ────────────────────

    /// إضافة عقدة ابن
    void addChild(std::shared_ptr<UINode> child);

    /// الحصول على كل الأبناء
    const std::vector<std::shared_ptr<UINode>>& getChildren() const {
        return children_;
    }

    /// عدد الأبناء
    size_t childCount() const { return children_.size(); }

    /// الحصول على ابن بفهرس محدد
    std::shared_ptr<UINode> getChild(size_t index) const;

    // ─── ربط الحالة (State Bindings) ────────────

    /// إضافة ربط حالة
    void addStateBinding(const StateBinding& binding) {
        stateBindings_.push_back(binding);
    }

    /// الحصول على كل روابط الحالة
    const std::vector<StateBinding>& getStateBindings() const {
        return stateBindings_;
    }

    // ─── معلومات التصحيح ────────────────────────

    /// رقم السطر في الملف المصدري
    int getSourceLine() const { return sourceLine_; }
    void setSourceLine(int line) { sourceLine_ = line; }

    /// رقم العمود في الملف المصدري
    int getSourceColumn() const { return sourceColumn_; }
    void setSourceColumn(int col) { sourceColumn_ = col; }

    /// طباعة الشجرة بصيغة نصية للتصحيح (مسافة بادئة = العمق)
    std::string dump(int indent = 0) const;

private:
    UINodeType type_;                                   ///< نوع العنصر
    std::string arabicName_;                            ///< الاسم العربي
    std::string id_;                                    ///< معرّف فريد (اختياري)
    std::string primaryArg_;                            ///< الوسيطة الأساسية
    std::unordered_map<std::string, std::string> namedArgs_; ///< وسائط مسمّاة
    std::vector<Modifier> modifiers_;                   ///< قائمة المعدّلات
    std::vector<std::shared_ptr<UINode>> children_;     ///< العناصر الفرعية
    std::vector<StateBinding> stateBindings_;           ///< روابط الحالة
    int sourceLine_ = 0;                                ///< سطر المصدر
    int sourceColumn_ = 0;                              ///< عمود المصدر
};

// ═══════════════════════════════════════════════════════════════════════════════
// تعريف الواجهة (View Definition)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief تعريف واجهة كاملة (View) — تقابل "واجهة" في كود ص
 *
 * كل تعريف واجهة يحتوي على:
 * - اسم الواجهة (مثل: شاشة_رئيسية)
 * - متغيرات الحالة (@حالة، @ربط...)
 * - الشجرة الجذرية لعناصر الواجهة
 *
 * مثال:
 * @code
 * واجهة شاشة_رئيسية {
 *     @حالة عداد: رقم = 0
 *
 *     عرض() {
 *         عمود { ... }   ← هذه هي rootNode
 *     }
 * }
 * @endcode
 */
struct ViewDefinition {
    std::string name;                               ///< اسم الواجهة
    std::vector<StateBinding> stateBindings;        ///< متغيرات الحالة
    std::shared_ptr<UINode> rootNode;               ///< الشجرة الجذرية
    std::string sourceFile;                         ///< ملف المصدر
};

} // namespace ui
} // namespace sad

#endif // SAD_UI_NODE_H
