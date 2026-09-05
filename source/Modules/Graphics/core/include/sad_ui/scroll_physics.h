/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: scroll_physics.h
 * المسار: features/graphics/core/include/sad_ui/scroll_physics.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * نظام فيزياء التمرير (Scroll Physics System).
 *
 * يدعم:
 * - فيزياء ارتدادية (iOS-style bouncing)
 * - فيزياء مشبكية (Android-style clamping)
 * - فيزياء صفحات (page snapping)
 * - محاكاة الزنبرك (Spring Simulation)
 * - محاكاة الاحتكاك (Friction Simulation)
 * - تمرير بسرعة (velocity-based scrolling)
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_SCROLL_PHYSICS_H
#define SAD_UI_SCROLL_PHYSICS_H

#include <cmath>
#include <functional>
#include <algorithm>
#include <cstdint>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// محاكاة الزنبرك (Spring Simulation)
// ═══════════════════════════════════════════════════════════════════════════════

struct SpringDescription {
    float mass = 1.0f;
    float stiffness = 100.0f;
    float damping = 20.0f;

    static SpringDescription defaultSpring() { return {1.0f, 100.0f, 20.0f}; }
    static SpringDescription tightSpring()   { return {1.0f, 400.0f, 40.0f}; }
    static SpringDescription softSpring()    { return {1.0f, 50.0f, 12.0f}; }
};

class SpringSimulation {
public:
    SpringSimulation(const SpringDescription& spring, float start, float end, float velocity);

    float position(float time) const;
    float velocityAt(float time) const;
    bool isDone(float time) const;

    float endPosition() const { return end_; }

private:
    SpringDescription spring_;
    float start_;
    float end_;
    float initialVelocity_;
    float omega_;       // التردد الطبيعي
    float zeta_;        // نسبة التخميد
    float omegaD_;      // التردد المخمّد

    static constexpr float kTolerance = 0.5f;
    static constexpr float kVelocityTolerance = 50.0f;
};

// ═══════════════════════════════════════════════════════════════════════════════
// محاكاة الاحتكاك (Friction Simulation)
// ═══════════════════════════════════════════════════════════════════════════════

class FrictionSimulation {
public:
    FrictionSimulation(float drag, float position, float velocity);

    float positionAt(float time) const;
    float velocityAt(float time) const;
    bool isDone(float time) const;
    float finalPosition() const;

private:
    float drag_;
    float startPosition_;
    float startVelocity_;
    float dragLog_;

    static constexpr float kVelocityTolerance = 10.0f;
};

// ═══════════════════════════════════════════════════════════════════════════════
// حالة التمرير (ScrollPosition)
// ═══════════════════════════════════════════════════════════════════════════════

struct ScrollMetrics {
    float pixels = 0;         ///< الموقع الحالي
    float minScrollExtent = 0;
    float maxScrollExtent = 0;
    float viewportDimension = 0;

    bool outOfRange() const {
        return pixels < minScrollExtent || pixels > maxScrollExtent;
    }
    float overscroll() const {
        if (pixels < minScrollExtent) return pixels - minScrollExtent;
        if (pixels > maxScrollExtent) return pixels - maxScrollExtent;
        return 0;
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// فيزياء التمرير الأساسية
// ═══════════════════════════════════════════════════════════════════════════════

class ScrollPhysics {
public:
    virtual ~ScrollPhysics() = default;

    /// هل يُسمح بالتمرير خارج الحدود؟
    virtual bool allowOverscroll() const { return false; }

    /// تطبيق حدود التمرير على الموقع
    virtual float applyBoundaryConditions(const ScrollMetrics& metrics, float newPixels) const;

    /// إنشاء محاكاة بعد رفع الإصبع
    virtual bool createBallisticSimulation(const ScrollMetrics& metrics, float velocity,
                                           float& outPosition, float& outVelocity,
                                           bool& useSpring) const;

    /// الاحتكاك
    virtual float frictionFactor() const { return 0.015f; }

    /// الحد الأدنى للسرعة لبدء التمرير بالقصور
    float minFlingVelocity = 50.0f;
    float maxFlingVelocity = 8000.0f;

protected:
    SpringDescription overscrollSpring_ = SpringDescription::defaultSpring();
};

// ═══════════════════════════════════════════════════════════════════════════════
// فيزياء ارتدادية (Bouncing — على نمط iOS)
// ═══════════════════════════════════════════════════════════════════════════════

class BouncingScrollPhysics : public ScrollPhysics {
public:
    bool allowOverscroll() const override { return true; }
    float applyBoundaryConditions(const ScrollMetrics& metrics, float newPixels) const override;
    bool createBallisticSimulation(const ScrollMetrics& metrics, float velocity,
                                   float& outPosition, float& outVelocity,
                                   bool& useSpring) const override;
    float frictionFactor() const override { return 0.02f; }
};

// ═══════════════════════════════════════════════════════════════════════════════
// فيزياء مشبكية (Clamping — على نمط Android)
// ═══════════════════════════════════════════════════════════════════════════════

class ClampingScrollPhysics : public ScrollPhysics {
public:
    bool allowOverscroll() const override { return false; }
    float applyBoundaryConditions(const ScrollMetrics& metrics, float newPixels) const override;
    bool createBallisticSimulation(const ScrollMetrics& metrics, float velocity,
                                   float& outPosition, float& outVelocity,
                                   bool& useSpring) const override;
};

// ═══════════════════════════════════════════════════════════════════════════════
// فيزياء الصفحات (Page Snapping)
// ═══════════════════════════════════════════════════════════════════════════════

class PageScrollPhysics : public ScrollPhysics {
public:
    bool allowOverscroll() const override { return false; }
    bool createBallisticSimulation(const ScrollMetrics& metrics, float velocity,
                                   float& outPosition, float& outVelocity,
                                   bool& useSpring) const override;
    float snapThreshold = 0.3f;  ///< 30% من حجم الصفحة للانتقال
};

// ═══════════════════════════════════════════════════════════════════════════════
// مراقب التمرير (Scroll Controller)
// ═══════════════════════════════════════════════════════════════════════════════

class ScrollController {
public:
    explicit ScrollController(ScrollPhysics* physics = nullptr);

    /// تحديث الموقع بناء على سحب المستخدم
    float applyUserOffset(float delta);

    /// بدء التمرير بالقصور (بعد رفع الإصبع)
    void startFling(float velocity);

    /// تحديث المحاكاة (يُستدعى كل إطار)
    bool update(float dt);

    /// إيقاف المحاكاة
    void stop();

    /// تعيين النطاق
    void setExtents(float min, float max, float viewport);

    /// الانتقال إلى موقع معين
    void jumpTo(float position);
    void animateTo(float position, float durationMs = 300);

    /// الحالة
    float position() const { return metrics_.pixels; }
    bool isScrolling() const { return isAnimating_; }
    const ScrollMetrics& metrics() const { return metrics_; }

    /// مستمعي التغيير
    using ScrollCallback = std::function<void(float position)>;
    void onScroll(ScrollCallback cb) { scrollCallback_ = cb; }

private:
    ScrollMetrics metrics_;
    ScrollPhysics* physics_ = nullptr;
    ScrollPhysics defaultPhysics_;

    // حالة المحاكاة
    bool isAnimating_ = false;
    float simTime_ = 0;
    float simStartPos_ = 0;
    float simTargetPos_ = 0;
    float simStartVel_ = 0;
    bool simUseSpring_ = false;

    // animate
    bool isAnimatingTo_ = false;
    float animStartPos_ = 0;
    float animTargetPos_ = 0;
    float animDuration_ = 0;
    float animElapsed_ = 0;

    ScrollCallback scrollCallback_;

    ScrollPhysics* activePhysics() const {
        return physics_ ? physics_ : const_cast<ScrollPhysics*>(&defaultPhysics_);
    }
    void notifyScroll();
};

} // namespace ui
} // namespace sad

#endif // SAD_UI_SCROLL_PHYSICS_H
