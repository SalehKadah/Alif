/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: hybrid_routing.h
 * المسار: features/graphics/core/include/sad_ui/hybrid_routing.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * الوصف:
 * ------
 * نظام التوجيه الهجين الذكي — يحدد تلقائياً أفضل طريقة لعرض كل عنصر واجهة.
 *
 * المعمارية:
 * ──────────
 * كل عنصر واجهة (Widget) يمكن عرضه بطريقتين:
 *   1. Native: استخدام مكوّن أصلي للمنصة (Button, TextField, etc.)
 *   2. Canvas: الرسم المباشر عبر SDL2/OpenGL
 *
 * النظام الهجين يختار تلقائياً الطريقة الأفضل بناءً على:
 *   - المنصة المُستهدفة (Desktop, Android, iOS, Web)
 *   - نوع العنصر (قياسي أم مخصص)
 *   - تفضيلات المطور (@عرض_أصلي، @عرض_مرسوم)
 *   - الأداء المتوقع لكل طريقة
 *
 * جدول التوجيه:
 * ─────────────
 * | Widget Type     | Desktop | Android | iOS   | Web   |
 * |-----------------|---------|---------|-------|-------|
 * | Text            | Canvas  | Native  | Native| Canvas|
 * | Button          | Canvas  | Native  | Native| Canvas|
 * | TextField       | SDL2*   | Native  | Native| Canvas|
 * | Slider          | Canvas  | Native  | Native| Canvas|
 * | Toggle          | Canvas  | Native  | Native| Canvas|
 * | Image           | Canvas  | Canvas  | Canvas| Canvas|
 * | Charts          | Canvas  | Canvas  | Canvas| Canvas|
 * | Custom/Canvas   | Canvas  | Canvas  | Canvas| Canvas|
 *
 * (*) SDL2 مع IME لدعم الإدخال العربي على سطح المكتب
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_HYBRID_ROUTING_H
#define SAD_UI_HYBRID_ROUTING_H

#include "types.h"
#include "ir.h"

#include <unordered_map>
#include <functional>
#include <optional>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// سياسة التوجيه (Routing Policy)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief سياسة التوجيه تحدد كيف يُختار نوع العرض
 */
enum class RoutingPolicy : uint8_t {
    Auto,               ///< النظام يقرر تلقائياً (الافتراضي)
    PreferNative,       ///< يفضل Native إذا توفر
    PreferCanvas,       ///< يفضل Canvas دائماً (تناسق عبر المنصات)
    ForceNative,        ///< يجبر Native (يفشل إذا غير متوفر)
    ForceCanvas,        ///< يجبر Canvas دائماً
};

// ═══════════════════════════════════════════════════════════════════════════════
// معلومات التوجيه (Routing Info)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief معلومات التوجيه لعنصر واحد على منصة محددة
 */
struct RoutingInfo {
    RenderingHint recommendedHint;  ///< الطريقة المُوصى بها
    bool nativeSupported;           ///< هل يوجد مكافئ أصلي؟
    bool canvasSupported;           ///< هل يمكن الرسم على Canvas؟
    int nativePerformanceScore;     ///< درجة الأداء للطريقة الأصلية (0-100)
    int canvasPerformanceScore;     ///< درجة الأداء للرسم (0-100)
    std::string nativeWidget;       ///< اسم المكوّن الأصلي (إن وُجد)
};

// ═══════════════════════════════════════════════════════════════════════════════
// جدول التوجيه الهجين (Hybrid Routing Table)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief جدول التوجيه الهجين الرئيسي
 *
 * يحتوي على معلومات التوجيه لكل نوع عنصر على كل منصة.
 * يمكن تخصيصه عبر ملفات config أو برمجياً.
 */
class HybridRoutingTable {
public:
    /// إنشاء جدول التوجيه مع الإعدادات الافتراضية
    HybridRoutingTable();

    /// تهيئة الجدول لمنصة محددة
    void initializeForPlatform(TargetPlatform platform);

    // ─── الاستعلام ────────────────────────────

    /**
     * @brief الحصول على معلومات التوجيه لنوع عنصر على المنصة الحالية
     */
    RoutingInfo getRoutingInfo(UINodeType type) const;

    /**
     * @brief تحديد تلميح العرض الأمثل بناءً على السياسة والمنصة
     */
    RenderingHint determineHint(
        UINodeType type,
        RoutingPolicy policy = RoutingPolicy::Auto
    ) const;

    /**
     * @brief هل العنصر مدعوم أصلياً على المنصة الحالية؟
     */
    bool isNativelySupported(UINodeType type) const;

    /**
     * @brief هل العنصر يُوصى برسمه على Canvas؟
     */
    bool shouldUseCanvas(UINodeType type) const;

    // ─── التخصيص ────────────────────────────

    /**
     * @brief تسجيل عنصر كمدعوم أصلياً
     */
    void registerNativeWidget(
        UINodeType type,
        const std::string& nativeWidgetName,
        int performanceScore = 90
    );

    /**
     * @brief تحديد سياسة التوجيه العامة
     */
    void setGlobalPolicy(RoutingPolicy policy) { policy_ = policy; }

    /**
     * @brief تحديد سياسة خاصة لنوع عنصر محدد
     */
    void setWidgetPolicy(UINodeType type, RoutingPolicy policy);

    /**
     * @brief الحصول على المنصة الحالية
     */
    TargetPlatform getCurrentPlatform() const { return platform_; }

private:
    TargetPlatform platform_ = TargetPlatform::Desktop;
    RoutingPolicy policy_ = RoutingPolicy::Auto;

    /// جدول التوجيه لكل نوع عنصر
    std::unordered_map<UINodeType, RoutingInfo> routingTable_;

    /// سياسات خاصة لعناصر محددة
    std::unordered_map<UINodeType, RoutingPolicy> widgetPolicies_;

    // ─── دوال التهيئة الداخلية ───
    void initializeDesktopTable();
    void initializeAndroidTable();
    void initializeiOSTable();
    void initializeWebTable();

    /// الحصول على معلومات افتراضية لعنصر غير مُسجّل
    RoutingInfo getDefaultRoutingInfo(UINodeType type) const;
};

// ═══════════════════════════════════════════════════════════════════════════════
// المُوجّه الهجين (Hybrid Router) — واجهة عالية المستوى
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief المُوجّه الهجين — واجهة سهلة للتوجيه الذكي
 *
 * مثال الاستخدام:
 * ```cpp
 * HybridRouter router(TargetPlatform::Android);
 *
 * auto hint = router.route(UINodeType::Button);
 * // → RenderingHint::Native (لأن Button مدعوم أصلياً على أندرويد)
 *
 * auto hint2 = router.route(UINodeType::Canvas);
 * // → RenderingHint::Canvas (دائماً)
 * ```
 */
class HybridRouter {
public:
    /// إنشاء مُوجّه لمنصة محددة
    explicit HybridRouter(TargetPlatform platform);

    /// توجيه عنصر واحد
    RenderingHint route(UINodeType type) const;

    /// توجيه عقدة IR (يُعدّل hint في مكانها)
    void routeNode(IRNode& node) const;

    /// توجيه شجرة IR بالكامل (تكراري)
    void routeTree(std::shared_ptr<IRNode> root) const;

    /// تعيين سياسة مخصصة
    void setPolicy(RoutingPolicy policy);

    /// الوصول للجدول الداخلي
    HybridRoutingTable& getTable() { return table_; }
    const HybridRoutingTable& getTable() const { return table_; }

private:
    HybridRoutingTable table_;
};

// ═══════════════════════════════════════════════════════════════════════════════
// دوال مساعدة
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief الحصول على سياسة التوجيه من نص التوجيه في كود ص
 *
 * يحوّل توجيهات مثل "@عرض_أصلي" و "@عرض_مرسوم" إلى RoutingPolicy
 */
std::optional<RoutingPolicy> parseRoutingDirective(const std::string& directive);

/**
 * @brief تحويل RoutingPolicy إلى نص عربي
 */
std::string routingPolicyToArabic(RoutingPolicy policy);

/**
 * @brief تحويل RenderingHint إلى نص عربي
 */
std::string renderingHintToArabic(RenderingHint hint);

} // namespace ui
} // namespace sad

#endif // SAD_UI_HYBRID_ROUTING_H
