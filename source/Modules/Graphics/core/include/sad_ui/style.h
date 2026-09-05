/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: style.h
 * المسار: features/graphics/core/include/sad_ui/style.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * الوصف:
 * ------
 * نظام الأنماط الموحّد (Unified Style System).
 *
 * هذا الملف يُعرّف نظام الأنماط الذي يُحدد مظهر كل عنصر واجهة.
 * النظام مستوحى من CSS لكن مبسّط ومصمّم للعربية أولاً.
 *
 * المفاهيم الأساسية:
 * ──────────────────────
 * 1. **النمط المُحل (ResolvedStyle)**: النتيجة النهائية بعد دمج كل
 *    المعدّلات. يحتوي على كل خاصية بصرية بقيمة محددة.
 *
 * 2. **وراثة الأنماط (Style Inheritance)**: بعض الخصائص ترثها
 *    العناصر الفرعية تلقائياً (لون الخط، حجم الخط) وبعضها لا (الحشوة).
 *
 * 3. **السمة (Theme)**: مجموعة ألوان وخطوط وأحجام محددة مسبقاً.
 *    يمكن تبديل السمة بسهولة (فاتح/داكن).
 *
 * 4. **تتالي الأنماط (Cascade)**: ترتيب الأولوية:
 *    معدّل مباشر > نمط مسمّى > سمة > افتراضي
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_STYLE_H
#define SAD_UI_STYLE_H

#include "node.h"  // Color, Dimension, EdgeInsets

#include <string>
#include <optional>
#include <unordered_map>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// وزن الخط (Font Weight)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief أوزان الخط المتاحة
 *
 * تتوافق مع أوزان CSS القياسية.
 */
enum class FontWeight : uint16_t {
    Thin       = 100,   ///< رفيع جداً
    Light      = 300,   ///< خفيف
    Regular    = 400,   ///< عادي (الافتراضي)
    Medium     = 500,   ///< متوسط
    SemiBold   = 600,   ///< شبه سميك
    Bold       = 700,   ///< سميك
    ExtraBold  = 800,   ///< سميك جداً
    Black      = 900    ///< أسود
};

// ═══════════════════════════════════════════════════════════════════════════════
// نمط الخط (Font Style)
// ═══════════════════════════════════════════════════════════════════════════════

enum class FontStyle : uint8_t {
    Normal,     ///< عادي
    Italic      ///< مائل
};

// ═══════════════════════════════════════════════════════════════════════════════
// زخرفة النص (Text Decoration)
// ═══════════════════════════════════════════════════════════════════════════════

enum class TextDecoration : uint8_t {
    None,           ///< بلا زخرفة
    Underline,      ///< خط تحتي
    LineThrough,    ///< خط وسطي
    Overline        ///< خط فوقي
};

// ═══════════════════════════════════════════════════════════════════════════════
// شكل القص (Clip Shape)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief أشكال القص المتاحة للعناصر
 *
 * تُحدد شكل حدود العنصر:
 * - None: بلا قص
 * - RoundedRect: مستطيل بزوايا دائرية
 * - Circle: دائرة
 * - Capsule: كبسولة (مستطيل بنهايات دائرية)
 */
enum class ClipShape : uint8_t {
    None,           ///< بلا قص
    RoundedRect,    ///< مستطيل بزوايا دائرية
    Circle,         ///< دائرة
    Capsule         ///< كبسولة
};

// ═══════════════════════════════════════════════════════════════════════════════
// نمط الحدود (Border Style)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief معلومات حدود العنصر
 */
struct BorderStyle {
    float width = 0.0f;             ///< سمك الحد (dp)
    Color color;                     ///< لون الحد
    float radius = 0.0f;            ///< نصف قطر الزوايا (dp)
};

// ═══════════════════════════════════════════════════════════════════════════════
// نمط الظل (Shadow Style)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief معلومات ظل العنصر
 */
struct ShadowStyle {
    Color color;             ///< لون الظل
    float offsetX = 0.0f;   ///< إزاحة أفقية (dp)
    float offsetY = 2.0f;   ///< إزاحة عمودية (dp)
    float blur = 4.0f;      ///< نصف قطر الضبابية (dp)
    float spread = 0.0f;    ///< انتشار الظل (dp)
};

// ═══════════════════════════════════════════════════════════════════════════════
// النمط المُحل (Resolved Style)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief النمط النهائي المُحل لعنصر واحد
 *
 * هذه البنية تحتوي على **كل** الخصائص البصرية لعنصر واحد
 * بعد دمج المعدّلات والوراثة والسمة.
 *
 * كل خاصية مغلفة بـ optional — إذا لم تُحدد، تُستخدم القيمة الافتراضية.
 *
 * سير العمل:
 *   Modifier[] → StyleResolver::resolve() → ResolvedStyle
 */
struct ResolvedStyle {
    // ─── الألوان ──────────────────────────
    std::optional<Color> foregroundColor;      ///< لون النص/المحتوى
    std::optional<Color> backgroundColor;      ///< لون الخلفية
    std::optional<Color> tintColor;            ///< لون التظليل

    // ─── الخط ─────────────────────────────
    std::optional<float> fontSize;             ///< حجم الخط (sp)
    std::optional<FontWeight> fontWeight;      ///< وزن الخط
    std::optional<std::string> fontFamily;     ///< عائلة الخط
    std::optional<FontStyle> fontStyle;        ///< نمط الخط (عادي/مائل)
    std::optional<AlignmentType> textAlign;    ///< محاذاة النص
    std::optional<float> lineSpacing;          ///< تباعد الأسطر
    std::optional<TextDecoration> textDecoration; ///< زخرفة النص

    // ─── الأبعاد ──────────────────────────
    std::optional<Dimension> width;            ///< العرض
    std::optional<Dimension> height;           ///< الارتفاع
    std::optional<Dimension> minWidth;         ///< أدنى عرض
    std::optional<Dimension> minHeight;        ///< أدنى ارتفاع
    std::optional<Dimension> maxWidth;         ///< أقصى عرض
    std::optional<Dimension> maxHeight;        ///< أقصى ارتفاع

    // ─── المسافات ─────────────────────────
    std::optional<EdgeInsets> padding;          ///< الحشوة الداخلية
    std::optional<EdgeInsets> margin;           ///< الهامش الخارجي

    // ─── الحدود ───────────────────────────
    std::optional<BorderStyle> border;          ///< الحدود

    // ─── التأثيرات ────────────────────────
    std::optional<ShadowStyle> shadow;          ///< الظل
    std::optional<float> opacity;               ///< العتامة (0.0 - 1.0)
    std::optional<float> blur;                  ///< ضبابية (dp)
    std::optional<ClipShape> clipShape;         ///< شكل القص

    // ─── التحويلات ────────────────────────
    std::optional<float> rotation;              ///< الدوران (درجات)
    std::optional<float> scaleX;                ///< التكبير الأفقي
    std::optional<float> scaleY;                ///< التكبير العمودي
    std::optional<float> translationX;          ///< الإزاحة الأفقية
    std::optional<float> translationY;          ///< الإزاحة العمودية

    // ─── التخطيط ──────────────────────────
    std::optional<AlignmentType> alignment;     ///< محاذاة العنصر
};

// ═══════════════════════════════════════════════════════════════════════════════
// حالّ الأنماط (Style Resolver)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief يحل ويدمج المعدّلات إلى نمط نهائي
 *
 * يأخذ قائمة المعدّلات ويُعيد ResolvedStyle مُحل بعد:
 * 1. تطبيق المعدّلات بالترتيب (الأخير يفوز عند التعارض)
 * 2. دمج الوراثة من النمط الأب
 * 3. تطبيق قيم السمة الافتراضية
 */
class StyleResolver {
public:
    /**
     * @brief حل المعدّلات إلى نمط نهائي
     *
     * @param modifiers قائمة المعدّلات المُطبّقة
     * @param parentStyle نمط العنصر الأب (للوراثة)
     * @return النمط المُحل النهائي
     */
    ResolvedStyle resolve(
        const std::vector<Modifier>& modifiers,
        const ResolvedStyle* parentStyle = nullptr
    ) const;

    /**
     * @brief تطبيق وراثة الأنماط
     *
     * الخصائص التي تُورث تلقائياً:
     * - foregroundColor (لون النص)
     * - fontSize (حجم الخط)
     * - fontWeight (وزن الخط)
     * - fontFamily (عائلة الخط)
     * - fontStyle (نمط الخط)
     * - textAlign (محاذاة النص)
     * - lineSpacing (تباعد الأسطر)
     *
     * الخصائص التي لا تُورث:
     * - backgroundColor, padding, margin, border, shadow, opacity, ...
     */
    void applyInheritance(ResolvedStyle& child, const ResolvedStyle& parent) const;
};

// ═══════════════════════════════════════════════════════════════════════════════
// السمة (Theme)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief سمة واجهة مستخدم (مثل: فاتح/داكن)
 *
 * السمة تحدد مجموعة من القيم الافتراضية:
 * - ألوان (أساسي، ثانوي، خلفية، سطح...)
 * - خطوط (عائلة، أحجام)
 * - أبعاد (حشوة افتراضية، تباعد، أنصاف أقطار)
 *
 * مثال:
 * @code
 *   auto theme = Theme::dark();
 *   // أو إنشاء سمة مخصصة:
 *   Theme custom;
 *   custom.primaryColor = Color::fromHex(0x1E88E5FF);
 *   custom.fontFamily = "Cairo";
 * @endcode
 */
struct Theme {
    // ─── ألوان السمة ──────────────────────
    Color primaryColor;         ///< اللون الأساسي
    Color secondaryColor;       ///< اللون الثانوي
    Color backgroundColor;      ///< لون الخلفية
    Color surfaceColor;          ///< لون السطح (بطاقات، أوراق...)
    Color errorColor;            ///< لون الخطأ
    Color onPrimaryColor;        ///< لون النص على الأساسي
    Color onBackgroundColor;     ///< لون النص على الخلفية
    Color onSurfaceColor;        ///< لون النص على السطح

    // ─── الخطوط ───────────────────────────
    std::string fontFamily = "Cairo";  ///< عائلة الخط الافتراضية
    float fontSizeSmall  = 12.0f;      ///< حجم خط صغير (sp)
    float fontSizeBody   = 16.0f;      ///< حجم خط النص (sp)
    float fontSizeTitle  = 22.0f;      ///< حجم خط العنوان (sp)
    float fontSizeHeadline = 28.0f;    ///< حجم خط العنوان الكبير (sp)

    // ─── الأبعاد الافتراضية ──────────────
    float defaultPadding  = 16.0f;     ///< حشوة افتراضية (dp)
    float defaultSpacing  = 8.0f;      ///< تباعد افتراضي (dp)
    float defaultRadius   = 8.0f;      ///< نصف قطر افتراضي (dp)

    /// إنشاء سمة فاتحة (Material Design)
    static Theme light();

    /// إنشاء سمة داكنة (Material Design)
    static Theme dark();
};

} // namespace ui
} // namespace sad

#endif // SAD_UI_STYLE_H
