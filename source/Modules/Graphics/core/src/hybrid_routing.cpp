/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: hybrid_routing.cpp
 * المسار: features/graphics/core/src/hybrid_routing.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * تنفيذ نظام التوجيه الهجين الذكي.
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/hybrid_routing.h"

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// تنفيذ HybridRoutingTable
// ═══════════════════════════════════════════════════════════════════════════════

HybridRoutingTable::HybridRoutingTable() {
    // التهيئة الافتراضية لسطح المكتب
    initializeForPlatform(TargetPlatform::Desktop);
}

void HybridRoutingTable::initializeForPlatform(TargetPlatform platform) {
    platform_ = platform;
    routingTable_.clear();

    switch (platform) {
        case TargetPlatform::Desktop:
        case TargetPlatform::MacOS:
            initializeDesktopTable();
            break;

        case TargetPlatform::Android:
            initializeAndroidTable();
            break;

        case TargetPlatform::IOS:
            initializeiOSTable();
            break;

        case TargetPlatform::Web:
        case TargetPlatform::WASM:
            initializeWebTable();
            break;

        default:
            initializeDesktopTable();
            break;
    }
}

// ─── تهيئة جدول سطح المكتب (SDL2 لكل شيء) ───

void HybridRoutingTable::initializeDesktopTable() {
    // على سطح المكتب، SDL2 يرسم كل شيء
    // لكن بعض العناصر تحتاج تعامل خاص (مثل TextField مع IME)

    // عناصر التخطيط — مرسومة
    routingTable_[UINodeType::Column] = {
        RenderingHint::Canvas, false, true, 0, 95, ""
    };
    routingTable_[UINodeType::Row] = {
        RenderingHint::Canvas, false, true, 0, 95, ""
    };
    routingTable_[UINodeType::Stack] = {
        RenderingHint::Canvas, false, true, 0, 95, ""
    };
    routingTable_[UINodeType::Grid] = {
        RenderingHint::Canvas, false, true, 0, 90, ""
    };
    routingTable_[UINodeType::Container] = {
        RenderingHint::Canvas, false, true, 0, 95, ""
    };
    routingTable_[UINodeType::Box] = {
        RenderingHint::Canvas, false, true, 0, 95, ""
    };
    routingTable_[UINodeType::Scaffold] = {
        RenderingHint::Canvas, false, true, 0, 90, ""
    };

    // عناصر النص — SDL2_ttf
    routingTable_[UINodeType::Text] = {
        RenderingHint::Canvas, false, true, 0, 90, ""
    };

    // الأزرار — مرسومة
    routingTable_[UINodeType::Button] = {
        RenderingHint::Canvas, false, true, 0, 85, ""
    };

    // حقول الإدخال — هجين (SDL2 + IME للإدخال العربي)
    routingTable_[UINodeType::TextField] = {
        RenderingHint::Hybrid, false, true, 0, 80, ""
    };

    // الصور — SDL2_image
    routingTable_[UINodeType::Image] = {
        RenderingHint::Canvas, false, true, 0, 95, ""
    };

    // المنزلقات والتبديل
    routingTable_[UINodeType::Slider] = {
        RenderingHint::Canvas, false, true, 0, 85, ""
    };
    routingTable_[UINodeType::Toggle] = {
        RenderingHint::Canvas, false, true, 0, 90, ""
    };

    // القوائم
    routingTable_[UINodeType::List] = {
        RenderingHint::Canvas, false, true, 0, 85, ""
    };
    routingTable_[UINodeType::ScrollView] = {
        RenderingHint::Canvas, false, true, 0, 80, ""
    };

    // Canvas والعناصر المرسومة — دائماً Canvas
    routingTable_[UINodeType::Canvas] = {
        RenderingHint::Canvas, false, true, 0, 100, ""
    };
    routingTable_[UINodeType::CustomWidget] = {
        RenderingHint::Canvas, false, true, 0, 100, ""
    };
}

// ─── تهيئة جدول أندرويد (Native + Canvas) ───

void HybridRoutingTable::initializeAndroidTable() {
    // عناصر التخطيط — Compose Layouts
    routingTable_[UINodeType::Column] = {
        RenderingHint::Native, true, true, 95, 70, "Column"
    };
    routingTable_[UINodeType::Row] = {
        RenderingHint::Native, true, true, 95, 70, "Row"
    };
    routingTable_[UINodeType::Stack] = {
        RenderingHint::Native, true, true, 90, 75, "Box"
    };
    routingTable_[UINodeType::Grid] = {
        RenderingHint::Native, true, true, 85, 70, "LazyVerticalGrid"
    };
    routingTable_[UINodeType::Container] = {
        RenderingHint::Native, true, true, 90, 75, "Box"
    };
    routingTable_[UINodeType::Box] = {
        RenderingHint::Native, true, true, 95, 75, "Box"
    };
    routingTable_[UINodeType::Scaffold] = {
        RenderingHint::Native, true, true, 95, 60, "Scaffold"
    };

    // عناصر النص — Compose Text
    routingTable_[UINodeType::Text] = {
        RenderingHint::Native, true, true, 95, 80, "Text"
    };

    // الأزرار — Material3 Button
    routingTable_[UINodeType::Button] = {
        RenderingHint::Native, true, true, 95, 75, "Button"
    };

    // حقول الإدخال — TextField
    routingTable_[UINodeType::TextField] = {
        RenderingHint::Native, true, true, 98, 50, "TextField"
    };

    // الصور — AsyncImage (Coil)
    routingTable_[UINodeType::Image] = {
        RenderingHint::Native, true, true, 90, 85, "AsyncImage"
    };

    // المنزلقات والتبديل
    routingTable_[UINodeType::Slider] = {
        RenderingHint::Native, true, true, 95, 70, "Slider"
    };
    routingTable_[UINodeType::Toggle] = {
        RenderingHint::Native, true, true, 95, 75, "Switch"
    };

    // القوائم — LazyColumn
    routingTable_[UINodeType::List] = {
        RenderingHint::Native, true, true, 95, 60, "LazyColumn"
    };
    routingTable_[UINodeType::ScrollView] = {
        RenderingHint::Native, true, true, 95, 65, "LazyColumn"
    };

    // Canvas والعناصر المرسومة دائماً Canvas
    routingTable_[UINodeType::Canvas] = {
        RenderingHint::Canvas, false, true, 0, 100, ""
    };
    routingTable_[UINodeType::CustomWidget] = {
        RenderingHint::Canvas, false, true, 0, 100, ""
    };
}

// ─── تهيئة جدول iOS (SwiftUI + Canvas) ───

void HybridRoutingTable::initializeiOSTable() {
    // عناصر التخطيط — SwiftUI
    routingTable_[UINodeType::Column] = {
        RenderingHint::Native, true, true, 95, 70, "VStack"
    };
    routingTable_[UINodeType::Row] = {
        RenderingHint::Native, true, true, 95, 70, "HStack"
    };
    routingTable_[UINodeType::Stack] = {
        RenderingHint::Native, true, true, 95, 70, "ZStack"
    };
    routingTable_[UINodeType::Grid] = {
        RenderingHint::Native, true, true, 85, 70, "LazyVGrid"
    };
    routingTable_[UINodeType::Container] = {
        RenderingHint::Native, true, true, 90, 75, "VStack"
    };
    routingTable_[UINodeType::Box] = {
        RenderingHint::Native, true, true, 95, 75, "ZStack"
    };
    routingTable_[UINodeType::Scaffold] = {
        RenderingHint::Native, true, true, 90, 60, "NavigationView"
    };

    // عناصر النص — SwiftUI Text
    routingTable_[UINodeType::Text] = {
        RenderingHint::Native, true, true, 95, 80, "Text"
    };

    // الأزرار
    routingTable_[UINodeType::Button] = {
        RenderingHint::Native, true, true, 95, 75, "Button"
    };

    // حقول الإدخال
    routingTable_[UINodeType::TextField] = {
        RenderingHint::Native, true, true, 98, 50, "TextField"
    };

    // الصور
    routingTable_[UINodeType::Image] = {
        RenderingHint::Native, true, true, 95, 80, "AsyncImage"
    };

    // المنزلقات والتبديل
    routingTable_[UINodeType::Slider] = {
        RenderingHint::Native, true, true, 95, 70, "Slider"
    };
    routingTable_[UINodeType::Toggle] = {
        RenderingHint::Native, true, true, 95, 75, "Toggle"
    };

    // القوائم
    routingTable_[UINodeType::List] = {
        RenderingHint::Native, true, true, 95, 60, "List"
    };
    routingTable_[UINodeType::ScrollView] = {
        RenderingHint::Native, true, true, 95, 65, "ScrollView"
    };

    // Canvas والعناصر المرسومة
    routingTable_[UINodeType::Canvas] = {
        RenderingHint::Canvas, false, true, 0, 100, ""
    };
    routingTable_[UINodeType::CustomWidget] = {
        RenderingHint::Canvas, false, true, 0, 100, ""
    };
}

// ─── تهيئة جدول الويب (HTML/CSS + Canvas) ───

void HybridRoutingTable::initializeWebTable() {
    // على الويب، Emscripten + SDL2 أو DOM مباشر
    // سنستخدم Canvas لمعظم العناصر مع fallback إلى DOM

    // عناصر التخطيط — CSS Flexbox
    routingTable_[UINodeType::Column] = {
        RenderingHint::Canvas, true, true, 80, 90, "flexbox"
    };
    routingTable_[UINodeType::Row] = {
        RenderingHint::Canvas, true, true, 80, 90, "flexbox"
    };
    routingTable_[UINodeType::Stack] = {
        RenderingHint::Canvas, true, true, 75, 90, "position:absolute"
    };
    routingTable_[UINodeType::Grid] = {
        RenderingHint::Canvas, true, true, 80, 85, "grid"
    };
    routingTable_[UINodeType::Container] = {
        RenderingHint::Canvas, true, true, 80, 90, "div"
    };
    routingTable_[UINodeType::Box] = {
        RenderingHint::Canvas, true, true, 80, 90, "div"
    };
    routingTable_[UINodeType::Scaffold] = {
        RenderingHint::Canvas, true, true, 70, 85, "div"
    };

    // عناصر النص — span/p
    routingTable_[UINodeType::Text] = {
        RenderingHint::Canvas, true, true, 85, 90, "span"
    };

    // الأزرار — button
    routingTable_[UINodeType::Button] = {
        RenderingHint::Canvas, true, true, 85, 85, "button"
    };

    // حقول الإدخال — input (مفضل للـ IME)
    routingTable_[UINodeType::TextField] = {
        RenderingHint::Native, true, true, 95, 40, "input"
    };

    // الصور — img
    routingTable_[UINodeType::Image] = {
        RenderingHint::Canvas, true, true, 90, 90, "img"
    };

    // المنزلقات والتبديل
    routingTable_[UINodeType::Slider] = {
        RenderingHint::Canvas, true, true, 80, 85, "input[range]"
    };
    routingTable_[UINodeType::Toggle] = {
        RenderingHint::Canvas, true, true, 80, 90, "input[checkbox]"
    };

    // القوائم
    routingTable_[UINodeType::List] = {
        RenderingHint::Canvas, true, true, 75, 85, "ul/ol"
    };
    routingTable_[UINodeType::ScrollView] = {
        RenderingHint::Canvas, true, true, 80, 80, "overflow:scroll"
    };

    // Canvas والعناصر المرسومة
    routingTable_[UINodeType::Canvas] = {
        RenderingHint::Canvas, false, true, 0, 100, ""
    };
    routingTable_[UINodeType::CustomWidget] = {
        RenderingHint::Canvas, false, true, 0, 100, ""
    };
}

// ─── الاستعلام ───

RoutingInfo HybridRoutingTable::getRoutingInfo(UINodeType type) const {
    auto it = routingTable_.find(type);
    if (it != routingTable_.end()) {
        return it->second;
    }
    return getDefaultRoutingInfo(type);
}

RenderingHint HybridRoutingTable::determineHint(
    UINodeType type,
    RoutingPolicy policy) const
{
    // تحقق من سياسة خاصة للعنصر
    RoutingPolicy effectivePolicy = policy;
    auto policyIt = widgetPolicies_.find(type);
    if (policyIt != widgetPolicies_.end()) {
        effectivePolicy = policyIt->second;
    }

    // إذا السياسة العامة ليست Auto وليست محددة، استخدمها
    if (effectivePolicy == RoutingPolicy::Auto && policy_ != RoutingPolicy::Auto) {
        effectivePolicy = policy_;
    }

    auto info = getRoutingInfo(type);

    switch (effectivePolicy) {
        case RoutingPolicy::ForceNative:
            if (!info.nativeSupported) {
                // لا يوجد دعم أصلي — fallback إلى Canvas
                return RenderingHint::Canvas;
            }
            return RenderingHint::Native;

        case RoutingPolicy::ForceCanvas:
            return RenderingHint::Canvas;

        case RoutingPolicy::PreferNative:
            return info.nativeSupported ? RenderingHint::Native : RenderingHint::Canvas;

        case RoutingPolicy::PreferCanvas:
            return RenderingHint::Canvas;

        case RoutingPolicy::Auto:
        default:
            // استخدم التوصية من الجدول
            return info.recommendedHint;
    }
}

bool HybridRoutingTable::isNativelySupported(UINodeType type) const {
    return getRoutingInfo(type).nativeSupported;
}

bool HybridRoutingTable::shouldUseCanvas(UINodeType type) const {
    auto hint = determineHint(type, RoutingPolicy::Auto);
    return hint == RenderingHint::Canvas || hint == RenderingHint::Hybrid;
}

// ─── التخصيص ───

void HybridRoutingTable::registerNativeWidget(
    UINodeType type,
    const std::string& nativeWidgetName,
    int performanceScore)
{
    auto it = routingTable_.find(type);
    if (it != routingTable_.end()) {
        it->second.nativeSupported = true;
        it->second.nativeWidget = nativeWidgetName;
        it->second.nativePerformanceScore = performanceScore;
        // تحديث التوصية إذا كان Native أفضل
        if (performanceScore > it->second.canvasPerformanceScore) {
            it->second.recommendedHint = RenderingHint::Native;
        }
    } else {
        RoutingInfo info;
        info.nativeSupported = true;
        info.canvasSupported = true;
        info.nativeWidget = nativeWidgetName;
        info.nativePerformanceScore = performanceScore;
        info.canvasPerformanceScore = 70;
        info.recommendedHint = RenderingHint::Native;
        routingTable_[type] = info;
    }
}

void HybridRoutingTable::setWidgetPolicy(UINodeType type, RoutingPolicy policy) {
    widgetPolicies_[type] = policy;
}

RoutingInfo HybridRoutingTable::getDefaultRoutingInfo(UINodeType type) const {
    // افتراضي: Canvas مدعوم، Native غير مدعوم
    return {
        RenderingHint::Canvas,  // recommendedHint
        false,                  // nativeSupported
        true,                   // canvasSupported
        0,                      // nativePerformanceScore
        75,                     // canvasPerformanceScore
        ""                      // nativeWidget
    };
}

// ═══════════════════════════════════════════════════════════════════════════════
// تنفيذ HybridRouter
// ═══════════════════════════════════════════════════════════════════════════════

HybridRouter::HybridRouter(TargetPlatform platform) {
    table_.initializeForPlatform(platform);
}

RenderingHint HybridRouter::route(UINodeType type) const {
    return table_.determineHint(type, RoutingPolicy::Auto);
}

void HybridRouter::routeNode(IRNode& node) const {
    node.setRenderingHint(route(node.getType()));
}

void HybridRouter::routeTree(std::shared_ptr<IRNode> root) const {
    if (!root) return;

    routeNode(*root);

    for (const auto& child : root->getChildren()) {
        routeTree(child);
    }
}

void HybridRouter::setPolicy(RoutingPolicy policy) {
    table_.setGlobalPolicy(policy);
}

// ═══════════════════════════════════════════════════════════════════════════════
// دوال مساعدة
// ═══════════════════════════════════════════════════════════════════════════════

std::optional<RoutingPolicy> parseRoutingDirective(const std::string& directive) {
    if (directive == "@عرض_أصلي" || directive == "@native") {
        return RoutingPolicy::PreferNative;
    }
    if (directive == "@عرض_مرسوم" || directive == "@canvas") {
        return RoutingPolicy::PreferCanvas;
    }
    if (directive == "@عرض_تلقائي" || directive == "@auto") {
        return RoutingPolicy::Auto;
    }
    if (directive == "@إجبار_أصلي" || directive == "@force_native") {
        return RoutingPolicy::ForceNative;
    }
    if (directive == "@إجبار_مرسوم" || directive == "@force_canvas") {
        return RoutingPolicy::ForceCanvas;
    }
    return std::nullopt;
}

std::string routingPolicyToArabic(RoutingPolicy policy) {
    switch (policy) {
        case RoutingPolicy::Auto:         return "تلقائي";
        case RoutingPolicy::PreferNative: return "تفضيل_أصلي";
        case RoutingPolicy::PreferCanvas: return "تفضيل_مرسوم";
        case RoutingPolicy::ForceNative:  return "إجبار_أصلي";
        case RoutingPolicy::ForceCanvas:  return "إجبار_مرسوم";
        default:                          return "غير_معروف";
    }
}

std::string renderingHintToArabic(RenderingHint hint) {
    switch (hint) {
        case RenderingHint::Auto:   return "تلقائي";
        case RenderingHint::Native: return "أصلي";
        case RenderingHint::Canvas: return "مرسوم";
        case RenderingHint::Hybrid: return "هجين";
        default:                    return "غير_معروف";
    }
}

} // namespace ui
} // namespace sad
