/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: theme.h
 * المسار: features/graphics/core/include/sad_ui/theme.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * نظام السمات (Theme System) لواجهات لغة ص.
 *
 * يدعم:
 * - وضع داكن/فاتح مع تبديل تلقائي
 * - مخططات ألوان كاملة (ColorScheme) مثل Material3
 * - أنماط نصوص موحدة (TextTheme)
 * - سمات مكونات فردية (ButtonTheme, InputTheme, CardTheme...)
 * - سمات مخصصة قابلة للتوسيع
 * - دعم RTL كامل
 * - سمات متداخلة (Theme nesting)
 *
 * يتفوق على Flutter بـ:
 * - دعم RTL أصيل في السمات
 * - سمات مكونات أكثر تفصيلاً
 * - تبديل حي بدون إعادة بناء
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_THEME_H
#define SAD_UI_THEME_H

#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <vector>
#include <cstdint>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// لون RGBA (0.0 - 1.0)
// ═══════════════════════════════════════════════════════════════════════════════

struct ThemeColor {
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;

    ThemeColor() = default;
    ThemeColor(float r_, float g_, float b_, float a_ = 1.0f)
        : r(r_), g(g_), b(b_), a(a_) {}

    /// إنشاء من hex (مثل "#FF5722" أو "FF5722")
    static ThemeColor fromHex(const std::string& hex);

    /// إنشاء من HSL
    static ThemeColor fromHSL(float h, float s, float l, float a = 1.0f);

    /// مزج بين لونين
    ThemeColor lerp(const ThemeColor& other, float t) const;

    /// تفتيح/تغميق
    ThemeColor lighten(float amount) const;
    ThemeColor darken(float amount) const;

    /// شفافية
    ThemeColor withOpacity(float opacity) const;

    bool operator==(const ThemeColor& o) const;
    bool operator!=(const ThemeColor& o) const { return !(*this == o); }
};

// ═══════════════════════════════════════════════════════════════════════════════
// مخطط الألوان (ColorScheme) — مستوحى من Material3
// ═══════════════════════════════════════════════════════════════════════════════

struct ColorScheme {
    ThemeColor primary;          ///< اللون الرئيسي
    ThemeColor onPrimary;        ///< نص على الرئيسي
    ThemeColor primaryContainer; ///< حاوية رئيسية
    ThemeColor secondary;        ///< اللون الثانوي
    ThemeColor onSecondary;      ///< نص على الثانوي
    ThemeColor tertiary;         ///< اللون الثالث
    ThemeColor error;            ///< لون الخطأ
    ThemeColor onError;          ///< نص على الخطأ
    ThemeColor background;       ///< خلفية
    ThemeColor onBackground;     ///< نص على الخلفية
    ThemeColor surface;          ///< سطح
    ThemeColor onSurface;        ///< نص على السطح
    ThemeColor surfaceVariant;   ///< سطح فرعي
    ThemeColor outline;          ///< حدود
    ThemeColor shadow;           ///< ظل
    ThemeColor success;          ///< نجاح
    ThemeColor warning;          ///< تحذير
    ThemeColor info;             ///< معلومات

    /// مخطط فاتح افتراضي
    static ColorScheme light();
    /// مخطط داكن افتراضي
    static ColorScheme dark();
    /// مزج بين مخططين
    ColorScheme lerp(const ColorScheme& other, float t) const;
};

// ═══════════════════════════════════════════════════════════════════════════════
// أنماط النصوص (TextTheme)
// ═══════════════════════════════════════════════════════════════════════════════

struct TextStyle {
    float fontSize = 14.0f;
    int fontWeight = 400;       ///< 100-900
    float letterSpacing = 0.0f;
    float lineHeight = 1.5f;
    std::string fontFamily;
    ThemeColor color;

    TextStyle withSize(float size) const;
    TextStyle withWeight(int weight) const;
    TextStyle withColor(const ThemeColor& c) const;
};

struct TextTheme {
    TextStyle displayLarge;   ///< عنوان كبير جداً (57px)
    TextStyle displayMedium;  ///< عنوان كبير (45px)
    TextStyle displaySmall;   ///< عنوان متوسط (36px)
    TextStyle headlineLarge;  ///< رأس كبير (32px)
    TextStyle headlineMedium; ///< رأس متوسط (28px)
    TextStyle headlineSmall;  ///< رأس صغير (24px)
    TextStyle titleLarge;     ///< عنوان كبير (22px)
    TextStyle titleMedium;    ///< عنوان متوسط (16px)
    TextStyle titleSmall;     ///< عنوان صغير (14px)
    TextStyle bodyLarge;      ///< نص كبير (16px)
    TextStyle bodyMedium;     ///< نص متوسط (14px)
    TextStyle bodySmall;      ///< نص صغير (12px)
    TextStyle labelLarge;     ///< تسمية كبيرة (14px)
    TextStyle labelMedium;    ///< تسمية متوسطة (12px)
    TextStyle labelSmall;     ///< تسمية صغيرة (11px)

    static TextTheme defaults(const ThemeColor& textColor);
};

// ═══════════════════════════════════════════════════════════════════════════════
// سمات المكونات (Component Themes)
// ═══════════════════════════════════════════════════════════════════════════════

struct ButtonTheme {
    ThemeColor backgroundColor;
    ThemeColor textColor;
    ThemeColor hoverColor;
    ThemeColor pressedColor;
    ThemeColor disabledColor;
    float borderRadius = 8.0f;
    float elevation = 2.0f;
    float paddingH = 24.0f;
    float paddingV = 12.0f;
    TextStyle textStyle;
};

struct InputTheme {
    ThemeColor backgroundColor;
    ThemeColor textColor;
    ThemeColor borderColor;
    ThemeColor focusBorderColor;
    ThemeColor errorBorderColor;
    ThemeColor hintColor;
    ThemeColor cursorColor;
    ThemeColor selectionColor;
    float borderRadius = 8.0f;
    float borderWidth = 1.0f;
    float paddingH = 12.0f;
    float paddingV = 12.0f;
    TextStyle textStyle;
};

struct CardTheme {
    ThemeColor backgroundColor;
    ThemeColor borderColor;
    float borderRadius = 12.0f;
    float elevation = 4.0f;
    float paddingH = 16.0f;
    float paddingV = 16.0f;
};

struct AppBarTheme {
    ThemeColor backgroundColor;
    ThemeColor titleColor;
    ThemeColor iconColor;
    float elevation = 4.0f;
    float height = 56.0f;
    TextStyle titleStyle;
};

struct DialogTheme {
    ThemeColor backgroundColor;
    ThemeColor barrierColor;
    float borderRadius = 16.0f;
    float elevation = 24.0f;
};

struct ToggleTheme {
    ThemeColor activeColor;
    ThemeColor inactiveColor;
    ThemeColor thumbColor;
    ThemeColor activeTrackColor;
    ThemeColor inactiveTrackColor;
};

struct SliderTheme {
    ThemeColor activeTrackColor;
    ThemeColor inactiveTrackColor;
    ThemeColor thumbColor;
    ThemeColor overlayColor;
    float trackHeight = 4.0f;
    float thumbRadius = 10.0f;
};

struct ScrollbarTheme {
    ThemeColor thumbColor;
    ThemeColor trackColor;
    float width = 8.0f;
    float borderRadius = 4.0f;
    float minThumbLength = 48.0f;
};

struct TooltipTheme {
    ThemeColor backgroundColor;
    ThemeColor textColor;
    float borderRadius = 4.0f;
    float padding = 8.0f;
    TextStyle textStyle;
};

// ═══════════════════════════════════════════════════════════════════════════════
// وضع السمة
// ═══════════════════════════════════════════════════════════════════════════════

enum class ThemeMode : uint8_t {
    Light,      ///< فاتح
    Dark,       ///< داكن
    System      ///< حسب النظام
};

// ═══════════════════════════════════════════════════════════════════════════════
// بيانات السمة الكاملة (ThemeData)
// ═══════════════════════════════════════════════════════════════════════════════

class ThemeData {
public:
    ThemeData();

    /// إنشاء سمة فاتحة
    static ThemeData light();
    /// إنشاء سمة داكنة
    static ThemeData dark();
    /// إنشاء سمة من لون أساسي (تُولّد باقي الألوان)
    static ThemeData fromSeed(const ThemeColor& seedColor, ThemeMode mode = ThemeMode::Light);

    // ─── المخططات ───
    ColorScheme colorScheme;
    TextTheme textTheme;

    // ─── سمات المكونات ───
    ButtonTheme buttonTheme;
    InputTheme inputTheme;
    CardTheme cardTheme;
    AppBarTheme appBarTheme;
    DialogTheme dialogTheme;
    ToggleTheme toggleTheme;
    SliderTheme sliderTheme;
    ScrollbarTheme scrollbarTheme;
    TooltipTheme tooltipTheme;

    // ─── إعدادات عامة ───
    float defaultBorderRadius = 8.0f;
    float defaultElevation = 2.0f;
    float defaultSpacing = 8.0f;
    float defaultPadding = 16.0f;
    std::string fontFamily;
    bool isDark = false;

    // ─── بيانات مخصصة ───
    std::unordered_map<std::string, std::string> customStrings;
    std::unordered_map<std::string, float> customFloats;
    std::unordered_map<std::string, ThemeColor> customColors;

    /// مزج بين سمتين
    ThemeData lerp(const ThemeData& other, float t) const;

private:
    void applyColorSchemeToComponents();
};

// ═══════════════════════════════════════════════════════════════════════════════
// مزوّد السمات (ThemeProvider) — مركزي
// ═══════════════════════════════════════════════════════════════════════════════

class ThemeProvider {
public:
    static ThemeProvider& instance();

    /// تعيين السمة الحالية
    void setTheme(const ThemeData& theme);
    void setTheme(ThemeMode mode);

    /// الحصول على السمة الحالية
    const ThemeData& currentTheme() const;

    /// الحصول على الوضع الحالي
    ThemeMode currentMode() const { return mode_; }

    /// تعيين وضع النظام (يُستدعى من المنصة)
    void setSystemDarkMode(bool isDark);

    /// تبديل داكن/فاتح
    void toggleDarkMode();

    /// الاشتراك في تغييرات السمة
    using ThemeChangeCallback = std::function<void(const ThemeData&)>;
    int onThemeChanged(ThemeChangeCallback callback);
    void removeListener(int id);

    /// سمة فرعية (override لجزء من الشجرة)
    void pushOverride(const ThemeData& override);
    void popOverride();
    const ThemeData& effectiveTheme() const;

private:
    ThemeProvider();
    ThemeData lightTheme_;
    ThemeData darkTheme_;
    ThemeMode mode_ = ThemeMode::Light;
    bool systemIsDark_ = false;

    std::vector<ThemeData> overrideStack_;
    std::unordered_map<int, ThemeChangeCallback> listeners_;
    int nextListenerId_ = 0;

    void notifyListeners();
    const ThemeData& resolvedTheme() const;
};

} // namespace ui
} // namespace sad

#endif // SAD_UI_THEME_H
