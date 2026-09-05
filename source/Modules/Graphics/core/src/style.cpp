/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: style.cpp
 * المسار: features/graphics/core/src/style.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * تنفيذ نظام الأنماط (Style Resolver) والسمات (Themes).
 *
 * يحتوي على:
 * 1. دمج المعدّلات إلى نمط مُحل (ResolvedStyle)
 * 2. وراثة الأنماط من الأب
 * 3. سمات جاهزة (فاتح/داكن)
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/style.h"

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// تنفيذ StyleResolver
// ═══════════════════════════════════════════════════════════════════════════════

ResolvedStyle StyleResolver::resolve(
    const std::vector<Modifier>& modifiers,
    const ResolvedStyle* parentStyle
) const {
    ResolvedStyle style;

    // تطبيق وراثة الأنماط من الأب (إن وجد)
    if (parentStyle) {
        applyInheritance(style, *parentStyle);
    }

    // تطبيق المعدّلات بالترتيب — الأخير يفوز عند التعارض
    for (const auto& mod : modifiers) {
        switch (mod.type) {
            // ─── الألوان ──────────────────
            case ModifierType::ForegroundColor:
                if (auto* c = std::get_if<Color>(&mod.value))
                    style.foregroundColor = *c;
                break;

            case ModifierType::BackgroundColor:
                if (auto* c = std::get_if<Color>(&mod.value))
                    style.backgroundColor = *c;
                break;

            case ModifierType::TintColor:
                if (auto* c = std::get_if<Color>(&mod.value))
                    style.tintColor = *c;
                break;

            // ─── الخط ─────────────────────
            case ModifierType::FontSize:
                if (auto* v = std::get_if<float>(&mod.value))
                    style.fontSize = *v;
                break;

            case ModifierType::FontWeight:
                if (auto* v = std::get_if<float>(&mod.value))
                    style.fontWeight = static_cast<FontWeight>(static_cast<int>(*v));
                break;

            case ModifierType::FontFamily:
                if (auto* v = std::get_if<std::string>(&mod.value))
                    style.fontFamily = *v;
                break;

            case ModifierType::TextAlign:
                if (auto* v = std::get_if<AlignmentType>(&mod.value))
                    style.textAlign = *v;
                break;

            case ModifierType::LineSpacing:
                if (auto* v = std::get_if<float>(&mod.value))
                    style.lineSpacing = *v;
                break;

            // ─── الأبعاد ──────────────────
            case ModifierType::Width:
                if (auto* d = std::get_if<Dimension>(&mod.value))
                    style.width = *d;
                else if (auto* v = std::get_if<float>(&mod.value))
                    style.width = Dimension::dp(*v);
                break;

            case ModifierType::Height:
                if (auto* d = std::get_if<Dimension>(&mod.value))
                    style.height = *d;
                else if (auto* v = std::get_if<float>(&mod.value))
                    style.height = Dimension::dp(*v);
                break;

            case ModifierType::MinWidth:
                if (auto* v = std::get_if<float>(&mod.value))
                    style.minWidth = Dimension::dp(*v);
                break;

            case ModifierType::MinHeight:
                if (auto* v = std::get_if<float>(&mod.value))
                    style.minHeight = Dimension::dp(*v);
                break;

            case ModifierType::MaxWidth:
                if (auto* v = std::get_if<float>(&mod.value))
                    style.maxWidth = Dimension::dp(*v);
                break;

            case ModifierType::MaxHeight:
                if (auto* v = std::get_if<float>(&mod.value))
                    style.maxHeight = Dimension::dp(*v);
                break;

            // ─── الحشوة ──────────────────
            case ModifierType::Padding:
                if (auto* e = std::get_if<EdgeInsets>(&mod.value))
                    style.padding = *e;
                else if (auto* v = std::get_if<float>(&mod.value))
                    style.padding = EdgeInsets::all(*v);
                break;

            // ─── الهامش ───────────────────
            case ModifierType::Margin:
                if (auto* e = std::get_if<EdgeInsets>(&mod.value))
                    style.margin = *e;
                else if (auto* v = std::get_if<float>(&mod.value))
                    style.margin = EdgeInsets::all(*v);
                break;

            // ─── الحدود ───────────────────
            case ModifierType::BorderRadius:
                if (auto* v = std::get_if<float>(&mod.value)) {
                    if (!style.border) style.border = BorderStyle{};
                    style.border->radius = *v;
                }
                break;

            case ModifierType::BorderWidth:
                if (auto* v = std::get_if<float>(&mod.value)) {
                    if (!style.border) style.border = BorderStyle{};
                    style.border->width = *v;
                }
                break;

            case ModifierType::BorderColor:
                if (auto* c = std::get_if<Color>(&mod.value)) {
                    if (!style.border) style.border = BorderStyle{};
                    style.border->color = *c;
                }
                break;

            // ─── التأثيرات ────────────────
            case ModifierType::Opacity:
                if (auto* v = std::get_if<float>(&mod.value))
                    style.opacity = *v;
                break;

            case ModifierType::Blur:
                if (auto* v = std::get_if<float>(&mod.value))
                    style.blur = *v;
                break;

            // ─── التحويلات ────────────────
            case ModifierType::Rotation:
                if (auto* v = std::get_if<float>(&mod.value))
                    style.rotation = *v;
                break;

            case ModifierType::Scale:
                if (auto* v = std::get_if<float>(&mod.value)) {
                    style.scaleX = *v;
                    style.scaleY = *v;
                }
                break;

            // ─── المحاذاة ────────────────
            case ModifierType::Alignment:
                if (auto* v = std::get_if<AlignmentType>(&mod.value))
                    style.alignment = *v;
                break;

            default:
                // معدّلات غير بصرية (أحداث، سهولة الوصول) — تُتجاهل هنا
                break;
        }
    }

    return style;
}

void StyleResolver::applyInheritance(ResolvedStyle& child, const ResolvedStyle& parent) const {
    // الخصائص الموروثة — تُنسخ فقط إذا لم تُحدد في الابن
    // هذه هي الخصائص التي تُورث تلقائياً (مثل CSS inherit)

    if (!child.foregroundColor && parent.foregroundColor)
        child.foregroundColor = parent.foregroundColor;

    if (!child.fontSize && parent.fontSize)
        child.fontSize = parent.fontSize;

    if (!child.fontWeight && parent.fontWeight)
        child.fontWeight = parent.fontWeight;

    if (!child.fontFamily && parent.fontFamily)
        child.fontFamily = parent.fontFamily;

    if (!child.fontStyle && parent.fontStyle)
        child.fontStyle = parent.fontStyle;

    if (!child.textAlign && parent.textAlign)
        child.textAlign = parent.textAlign;

    if (!child.lineSpacing && parent.lineSpacing)
        child.lineSpacing = parent.lineSpacing;

    // ملاحظة: الخصائص التالية لا تُورث:
    // backgroundColor, padding, margin, border, shadow, opacity, blur,
    // clipShape, rotation, scale, translation, alignment
    // هذا متعمّد — مشابه لسلوك CSS
}

// ═══════════════════════════════════════════════════════════════════════════════
// تنفيذ Theme
// ═══════════════════════════════════════════════════════════════════════════════

Theme Theme::light() {
    Theme t;
    // ألوان Material Design Light
    t.primaryColor      = Color::fromHex(0x1E88E5FF);  // أزرق
    t.secondaryColor    = Color::fromHex(0x43A047FF);  // أخضر
    t.backgroundColor   = Color::fromHex(0xFAFAFAFF);  // رمادي فاتح جداً
    t.surfaceColor      = Color::fromHex(0xFFFFFFFF);  // أبيض
    t.errorColor        = Color::fromHex(0xE53935FF);  // أحمر
    t.onPrimaryColor    = Color::fromHex(0xFFFFFFFF);  // أبيض على الأساسي
    t.onBackgroundColor = Color::fromHex(0x212121FF);  // أسود تقريباً على الخلفية
    t.onSurfaceColor    = Color::fromHex(0x212121FF);  // أسود تقريباً على السطح
    // الخطوط
    t.fontFamily = "Cairo";
    t.fontSizeSmall    = 12.0f;
    t.fontSizeBody     = 16.0f;
    t.fontSizeTitle    = 22.0f;
    t.fontSizeHeadline = 28.0f;
    // الأبعاد
    t.defaultPadding = 16.0f;
    t.defaultSpacing = 8.0f;
    t.defaultRadius  = 8.0f;
    return t;
}

Theme Theme::dark() {
    Theme t;
    // ألوان Material Design Dark
    t.primaryColor      = Color::fromHex(0x64B5F6FF);  // أزرق فاتح
    t.secondaryColor    = Color::fromHex(0x81C784FF);  // أخضر فاتح
    t.backgroundColor   = Color::fromHex(0x121212FF);  // أسود تقريباً
    t.surfaceColor      = Color::fromHex(0x1E1E1EFF);  // رمادي داكن
    t.errorColor        = Color::fromHex(0xEF5350FF);  // أحمر فاتح
    t.onPrimaryColor    = Color::fromHex(0x000000FF);  // أسود على الأساسي
    t.onBackgroundColor = Color::fromHex(0xE0E0E0FF);  // رمادي فاتح على الخلفية
    t.onSurfaceColor    = Color::fromHex(0xE0E0E0FF);  // رمادي فاتح على السطح
    // الخطوط — نفس القيم
    t.fontFamily = "Cairo";
    t.fontSizeSmall    = 12.0f;
    t.fontSizeBody     = 16.0f;
    t.fontSizeTitle    = 22.0f;
    t.fontSizeHeadline = 28.0f;
    // الأبعاد
    t.defaultPadding = 16.0f;
    t.defaultSpacing = 8.0f;
    t.defaultRadius  = 8.0f;
    return t;
}

} // namespace ui
} // namespace sad
