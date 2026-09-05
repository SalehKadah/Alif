/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: node.cpp
 * المسار: features/graphics/core/src/node.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * تنفيذ شجرة عناصر الواجهة (UINode) والبنى المساعدة (Color, Dimension...).
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/node.h"
#include "sad_ui/types.h"

#include <sstream>
#include <iomanip>
#include <algorithm>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// نظام الثيم (Theme System)
// ═══════════════════════════════════════════════════════════════════════════════

static ThemeMode s_currentTheme = ThemeMode::Light;

ThemeMode getCurrentTheme() { return s_currentTheme; }
void setTheme(ThemeMode mode) { s_currentTheme = mode; }
void toggleTheme() {
    s_currentTheme = (s_currentTheme == ThemeMode::Light) ? ThemeMode::Dark : ThemeMode::Light;
}
bool isDarkMode() { return s_currentTheme == ThemeMode::Dark; }

// ═══════════════════════════════════════════════════════════════════════════════
// تنفيذ Color
// ═══════════════════════════════════════════════════════════════════════════════

Color Color::fromHex(uint32_t hex) {
    Color c;
    c.r = static_cast<float>((hex >> 24) & 0xFF) / 255.0f;
    c.g = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
    c.b = static_cast<float>((hex >> 8)  & 0xFF) / 255.0f;
    c.a = static_cast<float>((hex >> 0)  & 0xFF) / 255.0f;
    return c;
}

Color Color::fromNamed(NamedColor named) {
    bool dark = isDarkMode();
    // ألوان Material Design — تتغير حسب الوضع الحالي
    switch (named) {
        case NamedColor::Black:       return fromHex(0x000000FF);
        case NamedColor::White:       return fromHex(0xFFFFFFFF);
        case NamedColor::Red:         return fromHex(dark ? 0xEF5350FF : 0xF44336FF);
        case NamedColor::Green:       return fromHex(dark ? 0x66BB6AFF : 0x4CAF50FF);
        case NamedColor::Blue:        return fromHex(dark ? 0x42A5F5FF : 0x2196F3FF);
        case NamedColor::Yellow:      return fromHex(dark ? 0xFFF176FF : 0xFFEB3BFF);
        case NamedColor::Orange:      return fromHex(dark ? 0xFFA726FF : 0xFF9800FF);
        case NamedColor::Purple:      return fromHex(dark ? 0xAB47BCFF : 0x9C27B0FF);
        case NamedColor::Pink:        return fromHex(dark ? 0xEC407AFF : 0xE91E63FF);
        case NamedColor::Teal:        return fromHex(dark ? 0x26A69AFF : 0x009688FF);
        case NamedColor::Cyan:        return fromHex(dark ? 0x26C6DAFF : 0x00BCD4FF);
        case NamedColor::Brown:       return fromHex(dark ? 0x8D6E63FF : 0x795548FF);
        case NamedColor::Gray:        return fromHex(dark ? 0x757575FF : 0x9E9E9EFF);
        case NamedColor::Grey:        return fromHex(dark ? 0x757575FF : 0x9E9E9EFF);
        case NamedColor::LightGray:   return fromHex(dark ? 0x424242FF : 0xBDBDBDFF);
        case NamedColor::DarkGray:    return fromHex(dark ? 0x9E9E9EFF : 0x616161FF);
        case NamedColor::Transparent: return {0, 0, 0, 0};
        // ألوان دلالية — تتغير بالكامل في الوضع الداكن
        case NamedColor::Primary:     return fromHex(dark ? 0x64B5F6FF : 0x1E88E5FF);
        case NamedColor::Secondary:   return fromHex(dark ? 0x81C784FF : 0x43A047FF);
        case NamedColor::Background:  return fromHex(dark ? 0x121212FF : 0xFAFAFAFF);
        case NamedColor::Surface:     return fromHex(dark ? 0x1E1E1EFF : 0xFFFFFFFF);
        case NamedColor::Error:       return fromHex(dark ? 0xEF5350FF : 0xE53935FF);
        case NamedColor::OnPrimary:   return fromHex(dark ? 0x000000FF : 0xFFFFFFFF);
        case NamedColor::OnSecondary: return fromHex(dark ? 0x000000FF : 0xFFFFFFFF);
        case NamedColor::OnBackground:return fromHex(dark ? 0xE0E0E0FF : 0x212121FF);
        case NamedColor::OnSurface:   return fromHex(dark ? 0xE0E0E0FF : 0x212121FF);
        case NamedColor::OnError:     return fromHex(0xFFFFFFFF);
        default:                      return fromHex(dark ? 0xE0E0E0FF : 0x000000FF);
    }
}

uint32_t Color::toHex() const {
    auto clamp = [](float v) -> uint8_t {
        if (v < 0.0f) return 0;
        if (v > 1.0f) return 255;
        return static_cast<uint8_t>(v * 255.0f);
    };
    return (static_cast<uint32_t>(clamp(r)) << 24) |
           (static_cast<uint32_t>(clamp(g)) << 16) |
           (static_cast<uint32_t>(clamp(b)) << 8)  |
           (static_cast<uint32_t>(clamp(a)));
}

std::string Color::toHexString() const {
    std::ostringstream ss;
    ss << "#" << std::hex << std::uppercase << std::setfill('0');
    auto clamp = [](float v) -> int {
        if (v < 0.0f) return 0;
        if (v > 1.0f) return 255;
        return static_cast<int>(v * 255.0f);
    };
    ss << std::setw(2) << clamp(r)
       << std::setw(2) << clamp(g)
       << std::setw(2) << clamp(b);
    if (a < 0.999f) {
        ss << std::setw(2) << clamp(a);
    }
    return ss.str();
}

// ═══════════════════════════════════════════════════════════════════════════════
// تنفيذ UINode
// ═══════════════════════════════════════════════════════════════════════════════

UINode::UINode(UINodeType type)
    : type_(type)
    , arabicName_(nodeTypeToArabicName(type))
{
}

std::shared_ptr<UINode> UINode::create(UINodeType type) {
    return std::make_shared<UINode>(type);
}

const Modifier* UINode::findModifier(ModifierType type) const {
    for (const auto& mod : modifiers_) {
        if (mod.type == type) {
            return &mod;
        }
    }
    return nullptr;
}

void UINode::addChild(std::shared_ptr<UINode> child) {
    if (child) {
        children_.push_back(std::move(child));
    }
}

std::shared_ptr<UINode> UINode::getChild(size_t index) const {
    if (index < children_.size()) {
        return children_[index];
    }
    return nullptr;
}

std::string UINode::dump(int indent) const {
    std::ostringstream ss;
    std::string prefix(indent * 2, ' ');

    // طباعة نوع العقدة واسمها العربي
    ss << prefix << arabicName_ << " [" << static_cast<int>(type_) << "]";

    // الوسيطة الأساسية
    if (!primaryArg_.empty()) {
        ss << "(\"" << primaryArg_ << "\")";
    }

    // المعرّف
    if (!id_.empty()) {
        ss << " #" << id_;
    }

    ss << "\n";

    // المعدّلات
    for (const auto& mod : modifiers_) {
        ss << prefix << "  ." << modifierTypeToArabicName(mod.type) << "\n";
    }

    // الأبناء (تكرار)
    for (const auto& child : children_) {
        ss << child->dump(indent + 1);
    }

    return ss.str();
}

} // namespace ui
} // namespace sad
