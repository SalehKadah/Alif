/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: theme.cpp
 * المسار: features/graphics/core/src/theme.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * تنفيذ نظام السمات (Theme System).
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/theme.h"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// ThemeColor
// ═══════════════════════════════════════════════════════════════════════════════

ThemeColor ThemeColor::fromHex(const std::string& hex) {
    std::string h = hex;
    if (!h.empty() && h[0] == '#') h = h.substr(1);
    if (h.size() < 6) return {0, 0, 0, 1};

    auto hexByte = [](const std::string& s, size_t pos) -> float {
        unsigned int val = 0;
        for (size_t i = 0; i < 2; ++i) {
            char c = s[pos + i];
            val <<= 4;
            if (c >= '0' && c <= '9') val |= (c - '0');
            else if (c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
        }
        return val / 255.0f;
    };

    float r = hexByte(h, 0), g = hexByte(h, 2), b = hexByte(h, 4);
    float a = (h.size() >= 8) ? hexByte(h, 6) : 1.0f;
    return {r, g, b, a};
}

ThemeColor ThemeColor::fromHSL(float h, float s, float l, float alpha) {
    float c = (1.0f - std::fabs(2.0f * l - 1.0f)) * s;
    float hp = h / 60.0f;
    float x = c * (1.0f - std::fabs(std::fmod(hp, 2.0f) - 1.0f));
    float r1 = 0, g1 = 0, b1 = 0;
    if (hp < 1) { r1 = c; g1 = x; }
    else if (hp < 2) { r1 = x; g1 = c; }
    else if (hp < 3) { g1 = c; b1 = x; }
    else if (hp < 4) { g1 = x; b1 = c; }
    else if (hp < 5) { r1 = x; b1 = c; }
    else { r1 = c; b1 = x; }
    float m = l - c / 2.0f;
    return {r1 + m, g1 + m, b1 + m, alpha};
}

ThemeColor ThemeColor::lerp(const ThemeColor& other, float t) const {
    t = std::clamp(t, 0.0f, 1.0f);
    return {
        r + (other.r - r) * t, g + (other.g - g) * t,
        b + (other.b - b) * t, a + (other.a - a) * t
    };
}

ThemeColor ThemeColor::lighten(float amount) const {
    return {
        std::min(1.0f, r + amount), std::min(1.0f, g + amount),
        std::min(1.0f, b + amount), a
    };
}

ThemeColor ThemeColor::darken(float amount) const {
    return {
        std::max(0.0f, r - amount), std::max(0.0f, g - amount),
        std::max(0.0f, b - amount), a
    };
}

ThemeColor ThemeColor::withOpacity(float opacity) const {
    return {r, g, b, std::clamp(opacity, 0.0f, 1.0f)};
}

bool ThemeColor::operator==(const ThemeColor& o) const {
    return r == o.r && g == o.g && b == o.b && a == o.a;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ColorScheme
// ═══════════════════════════════════════════════════════════════════════════════

ColorScheme ColorScheme::light() {
    ColorScheme cs;
    cs.primary         = ThemeColor::fromHex("#1976D2");
    cs.onPrimary       = {1, 1, 1, 1};
    cs.primaryContainer = ThemeColor::fromHex("#BBDEFB");
    cs.secondary       = ThemeColor::fromHex("#26A69A");
    cs.onSecondary     = {1, 1, 1, 1};
    cs.tertiary        = ThemeColor::fromHex("#7C4DFF");
    cs.error           = ThemeColor::fromHex("#D32F2F");
    cs.onError         = {1, 1, 1, 1};
    cs.background      = {0.98f, 0.98f, 0.98f, 1};
    cs.onBackground    = {0.12f, 0.12f, 0.12f, 1};
    cs.surface         = {1, 1, 1, 1};
    cs.onSurface       = {0.12f, 0.12f, 0.12f, 1};
    cs.surfaceVariant  = {0.93f, 0.93f, 0.95f, 1};
    cs.outline         = {0.73f, 0.73f, 0.73f, 1};
    cs.shadow          = {0, 0, 0, 0.25f};
    cs.success         = ThemeColor::fromHex("#4CAF50");
    cs.warning         = ThemeColor::fromHex("#FF9800");
    cs.info            = ThemeColor::fromHex("#2196F3");
    return cs;
}

ColorScheme ColorScheme::dark() {
    ColorScheme cs;
    cs.primary         = ThemeColor::fromHex("#90CAF9");
    cs.onPrimary       = {0.1f, 0.1f, 0.1f, 1};
    cs.primaryContainer = ThemeColor::fromHex("#1565C0");
    cs.secondary       = ThemeColor::fromHex("#80CBC4");
    cs.onSecondary     = {0.1f, 0.1f, 0.1f, 1};
    cs.tertiary        = ThemeColor::fromHex("#B388FF");
    cs.error           = ThemeColor::fromHex("#EF9A9A");
    cs.onError         = {0.1f, 0.1f, 0.1f, 1};
    cs.background      = {0.07f, 0.07f, 0.09f, 1};
    cs.onBackground    = {0.88f, 0.88f, 0.88f, 1};
    cs.surface         = {0.12f, 0.12f, 0.14f, 1};
    cs.onSurface       = {0.88f, 0.88f, 0.88f, 1};
    cs.surfaceVariant  = {0.17f, 0.17f, 0.2f, 1};
    cs.outline         = {0.45f, 0.45f, 0.5f, 1};
    cs.shadow          = {0, 0, 0, 0.5f};
    cs.success         = ThemeColor::fromHex("#81C784");
    cs.warning         = ThemeColor::fromHex("#FFB74D");
    cs.info            = ThemeColor::fromHex("#64B5F6");
    return cs;
}

ColorScheme ColorScheme::lerp(const ColorScheme& o, float t) const {
    ColorScheme r;
    r.primary = primary.lerp(o.primary, t);
    r.onPrimary = onPrimary.lerp(o.onPrimary, t);
    r.primaryContainer = primaryContainer.lerp(o.primaryContainer, t);
    r.secondary = secondary.lerp(o.secondary, t);
    r.onSecondary = onSecondary.lerp(o.onSecondary, t);
    r.tertiary = tertiary.lerp(o.tertiary, t);
    r.error = error.lerp(o.error, t);
    r.onError = onError.lerp(o.onError, t);
    r.background = background.lerp(o.background, t);
    r.onBackground = onBackground.lerp(o.onBackground, t);
    r.surface = surface.lerp(o.surface, t);
    r.onSurface = onSurface.lerp(o.onSurface, t);
    r.surfaceVariant = surfaceVariant.lerp(o.surfaceVariant, t);
    r.outline = outline.lerp(o.outline, t);
    r.shadow = shadow.lerp(o.shadow, t);
    r.success = success.lerp(o.success, t);
    r.warning = warning.lerp(o.warning, t);
    r.info = info.lerp(o.info, t);
    return r;
}

// ═══════════════════════════════════════════════════════════════════════════════
// TextStyle & TextTheme
// ═══════════════════════════════════════════════════════════════════════════════

TextStyle TextStyle::withSize(float size) const {
    TextStyle s = *this; s.fontSize = size; return s;
}
TextStyle TextStyle::withWeight(int weight) const {
    TextStyle s = *this; s.fontWeight = weight; return s;
}
TextStyle TextStyle::withColor(const ThemeColor& c) const {
    TextStyle s = *this; s.color = c; return s;
}

TextTheme TextTheme::defaults(const ThemeColor& tc) {
    TextTheme t;
    t.displayLarge   = {57, 400, -0.25f, 1.12f, "", tc};
    t.displayMedium  = {45, 400, 0, 1.16f, "", tc};
    t.displaySmall   = {36, 400, 0, 1.22f, "", tc};
    t.headlineLarge  = {32, 400, 0, 1.25f, "", tc};
    t.headlineMedium = {28, 400, 0, 1.29f, "", tc};
    t.headlineSmall  = {24, 400, 0, 1.33f, "", tc};
    t.titleLarge     = {22, 500, 0, 1.27f, "", tc};
    t.titleMedium    = {16, 500, 0.15f, 1.5f, "", tc};
    t.titleSmall     = {14, 500, 0.1f, 1.43f, "", tc};
    t.bodyLarge      = {16, 400, 0.5f, 1.5f, "", tc};
    t.bodyMedium     = {14, 400, 0.25f, 1.43f, "", tc};
    t.bodySmall      = {12, 400, 0.4f, 1.33f, "", tc};
    t.labelLarge     = {14, 500, 0.1f, 1.43f, "", tc};
    t.labelMedium    = {12, 500, 0.5f, 1.33f, "", tc};
    t.labelSmall     = {11, 500, 0.5f, 1.45f, "", tc};
    return t;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ThemeData
// ═══════════════════════════════════════════════════════════════════════════════

ThemeData::ThemeData() {
    colorScheme = ColorScheme::light();
    textTheme = TextTheme::defaults(colorScheme.onBackground);
    applyColorSchemeToComponents();
}

void ThemeData::applyColorSchemeToComponents() {
    auto& cs = colorScheme;

    buttonTheme.backgroundColor = cs.primary;
    buttonTheme.textColor = cs.onPrimary;
    buttonTheme.hoverColor = cs.primary.lighten(0.1f);
    buttonTheme.pressedColor = cs.primary.darken(0.1f);
    buttonTheme.disabledColor = cs.onSurface.withOpacity(0.12f);
    buttonTheme.textStyle = textTheme.labelLarge.withColor(cs.onPrimary);

    inputTheme.backgroundColor = cs.surface;
    inputTheme.textColor = cs.onSurface;
    inputTheme.borderColor = cs.outline;
    inputTheme.focusBorderColor = cs.primary;
    inputTheme.errorBorderColor = cs.error;
    inputTheme.hintColor = cs.onSurface.withOpacity(0.5f);
    inputTheme.cursorColor = cs.primary;
    inputTheme.selectionColor = cs.primary.withOpacity(0.3f);
    inputTheme.textStyle = textTheme.bodyLarge.withColor(cs.onSurface);

    cardTheme.backgroundColor = cs.surface;
    cardTheme.borderColor = cs.outline.withOpacity(0.2f);

    appBarTheme.backgroundColor = cs.surface;
    appBarTheme.titleColor = cs.onSurface;
    appBarTheme.iconColor = cs.onSurface;
    appBarTheme.titleStyle = textTheme.titleLarge.withColor(cs.onSurface);

    dialogTheme.backgroundColor = cs.surface;
    dialogTheme.barrierColor = {0, 0, 0, 0.5f};

    toggleTheme.activeColor = cs.primary;
    toggleTheme.inactiveColor = cs.surfaceVariant;
    toggleTheme.thumbColor = cs.onPrimary;
    toggleTheme.activeTrackColor = cs.primary.withOpacity(0.5f);
    toggleTheme.inactiveTrackColor = cs.onSurface.withOpacity(0.3f);

    sliderTheme.activeTrackColor = cs.primary;
    sliderTheme.inactiveTrackColor = cs.onSurface.withOpacity(0.3f);
    sliderTheme.thumbColor = cs.primary;
    sliderTheme.overlayColor = cs.primary.withOpacity(0.12f);

    scrollbarTheme.thumbColor = cs.onSurface.withOpacity(0.4f);
    scrollbarTheme.trackColor = cs.surfaceVariant.withOpacity(0.3f);

    tooltipTheme.backgroundColor = cs.onSurface.withOpacity(0.9f);
    tooltipTheme.textColor = cs.surface;
    tooltipTheme.textStyle = textTheme.bodySmall.withColor(cs.surface);
}

ThemeData ThemeData::light() {
    ThemeData t;
    t.colorScheme = ColorScheme::light();
    t.textTheme = TextTheme::defaults(t.colorScheme.onBackground);
    t.isDark = false;
    t.applyColorSchemeToComponents();
    return t;
}

ThemeData ThemeData::dark() {
    ThemeData t;
    t.colorScheme = ColorScheme::dark();
    t.textTheme = TextTheme::defaults(t.colorScheme.onBackground);
    t.isDark = true;
    t.applyColorSchemeToComponents();
    return t;
}

ThemeData ThemeData::fromSeed(const ThemeColor& seedColor, ThemeMode mode) {
    ThemeData t;
    bool dark = (mode == ThemeMode::Dark);
    t.isDark = dark;
    t.colorScheme = dark ? ColorScheme::dark() : ColorScheme::light();
    t.colorScheme.primary = dark ? seedColor.lighten(0.2f) : seedColor;
    t.colorScheme.primaryContainer = dark ? seedColor.darken(0.2f) : seedColor.lighten(0.3f);
    t.colorScheme.secondary = ThemeColor::fromHSL(
        std::fmod(seedColor.r * 360 + 30, 360.0f), 0.5f, dark ? 0.7f : 0.4f);
    t.textTheme = TextTheme::defaults(t.colorScheme.onBackground);
    t.applyColorSchemeToComponents();
    return t;
}

ThemeData ThemeData::lerp(const ThemeData& other, float t) const {
    ThemeData r;
    r.colorScheme = colorScheme.lerp(other.colorScheme, t);
    r.isDark = (t < 0.5f) ? isDark : other.isDark;
    r.defaultBorderRadius = defaultBorderRadius + (other.defaultBorderRadius - defaultBorderRadius) * t;
    r.defaultElevation = defaultElevation + (other.defaultElevation - defaultElevation) * t;
    r.textTheme = TextTheme::defaults(r.colorScheme.onBackground);
    r.applyColorSchemeToComponents();
    return r;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ThemeProvider
// ═══════════════════════════════════════════════════════════════════════════════

ThemeProvider::ThemeProvider()
    : lightTheme_(ThemeData::light())
    , darkTheme_(ThemeData::dark())
{}

ThemeProvider& ThemeProvider::instance() {
    static ThemeProvider inst;
    return inst;
}

void ThemeProvider::setTheme(const ThemeData& theme) {
    if (theme.isDark) darkTheme_ = theme;
    else lightTheme_ = theme;
    notifyListeners();
}

void ThemeProvider::setTheme(ThemeMode mode) {
    mode_ = mode;
    notifyListeners();
}

const ThemeData& ThemeProvider::currentTheme() const {
    return resolvedTheme();
}

void ThemeProvider::setSystemDarkMode(bool isDark) {
    systemIsDark_ = isDark;
    if (mode_ == ThemeMode::System) notifyListeners();
}

void ThemeProvider::toggleDarkMode() {
    if (mode_ == ThemeMode::Light) mode_ = ThemeMode::Dark;
    else if (mode_ == ThemeMode::Dark) mode_ = ThemeMode::Light;
    else mode_ = systemIsDark_ ? ThemeMode::Light : ThemeMode::Dark;
    notifyListeners();
}

int ThemeProvider::onThemeChanged(ThemeChangeCallback cb) {
    int id = nextListenerId_++;
    listeners_[id] = std::move(cb);
    return id;
}

void ThemeProvider::removeListener(int id) {
    listeners_.erase(id);
}

void ThemeProvider::pushOverride(const ThemeData& override) {
    overrideStack_.push_back(override);
}

void ThemeProvider::popOverride() {
    if (!overrideStack_.empty()) overrideStack_.pop_back();
}

const ThemeData& ThemeProvider::effectiveTheme() const {
    if (!overrideStack_.empty()) return overrideStack_.back();
    return resolvedTheme();
}

void ThemeProvider::notifyListeners() {
    const auto& theme = resolvedTheme();
    for (auto& [id, cb] : listeners_) {
        if (cb) cb(theme);
    }
}

const ThemeData& ThemeProvider::resolvedTheme() const {
    switch (mode_) {
        case ThemeMode::Dark: return darkTheme_;
        case ThemeMode::System: return systemIsDark_ ? darkTheme_ : lightTheme_;
        default: return lightTheme_;
    }
}

} // namespace ui
} // namespace sad
