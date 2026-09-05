/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: scroll_physics.cpp
 * المسار: features/graphics/core/src/scroll_physics.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * تنفيذ فيزياء التمرير.
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/scroll_physics.h"
#include <algorithm>
#include <cmath>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// SpringSimulation
// ═══════════════════════════════════════════════════════════════════════════════

SpringSimulation::SpringSimulation(const SpringDescription& spring,
                                   float start, float end, float velocity)
    : spring_(spring), start_(start), end_(end), initialVelocity_(velocity) {
    omega_ = std::sqrt(spring_.stiffness / spring_.mass);
    zeta_ = spring_.damping / (2.0f * std::sqrt(spring_.stiffness * spring_.mass));

    if (zeta_ < 1.0f) {
        // تحت التخميد (oscillatory)
        omegaD_ = omega_ * std::sqrt(1.0f - zeta_ * zeta_);
    } else {
        omegaD_ = 0;
    }
}

float SpringSimulation::position(float time) const {
    float displacement = start_ - end_;
    if (zeta_ < 1.0f) {
        // تحت التخميد
        float envelope = std::exp(-zeta_ * omega_ * time);
        float A = displacement;
        float B = (zeta_ * omega_ * displacement + initialVelocity_) / omegaD_;
        return end_ + envelope * (A * std::cos(omegaD_ * time) + B * std::sin(omegaD_ * time));
    } else {
        // فوق/على التخميد
        float r1 = -omega_ * (zeta_ - std::sqrt(zeta_ * zeta_ - 1.0f));
        float r2 = -omega_ * (zeta_ + std::sqrt(zeta_ * zeta_ - 1.0f));
        float C2 = (initialVelocity_ - r1 * displacement) / (r2 - r1);
        float C1 = displacement - C2;
        return end_ + C1 * std::exp(r1 * time) + C2 * std::exp(r2 * time);
    }
}

float SpringSimulation::velocityAt(float time) const {
    float displacement = start_ - end_;
    if (zeta_ < 1.0f) {
        float envelope = std::exp(-zeta_ * omega_ * time);
        float A = displacement;
        float B = (zeta_ * omega_ * displacement + initialVelocity_) / omegaD_;
        float cosT = std::cos(omegaD_ * time);
        float sinT = std::sin(omegaD_ * time);
        return envelope * ((-zeta_ * omega_) * (A * cosT + B * sinT) +
                           omegaD_ * (-A * sinT + B * cosT));
    } else {
        float r1 = -omega_ * (zeta_ - std::sqrt(zeta_ * zeta_ - 1.0f));
        float r2 = -omega_ * (zeta_ + std::sqrt(zeta_ * zeta_ - 1.0f));
        float C2 = (initialVelocity_ - r1 * displacement) / (r2 - r1);
        float C1 = displacement - C2;
        return C1 * r1 * std::exp(r1 * time) + C2 * r2 * std::exp(r2 * time);
    }
}

bool SpringSimulation::isDone(float time) const {
    return std::abs(position(time) - end_) < kTolerance &&
           std::abs(velocityAt(time)) < kVelocityTolerance;
}

// ═══════════════════════════════════════════════════════════════════════════════
// FrictionSimulation
// ═══════════════════════════════════════════════════════════════════════════════

FrictionSimulation::FrictionSimulation(float drag, float position, float velocity)
    : drag_(drag), startPosition_(position), startVelocity_(velocity) {
    dragLog_ = std::log(drag_);
}

float FrictionSimulation::positionAt(float time) const {
    return startPosition_ + startVelocity_ * std::pow(drag_, time) / dragLog_
           - startVelocity_ / dragLog_;
}

float FrictionSimulation::velocityAt(float time) const {
    return startVelocity_ * std::pow(drag_, time);
}

bool FrictionSimulation::isDone(float time) const {
    return std::abs(velocityAt(time)) < kVelocityTolerance;
}

float FrictionSimulation::finalPosition() const {
    return startPosition_ - startVelocity_ / dragLog_;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ScrollPhysics (أساسية)
// ═══════════════════════════════════════════════════════════════════════════════

float ScrollPhysics::applyBoundaryConditions(const ScrollMetrics& metrics, float newPixels) const {
    // الافتراضي: تشبيك بالحدود
    return std::clamp(newPixels, metrics.minScrollExtent, metrics.maxScrollExtent);
}

bool ScrollPhysics::createBallisticSimulation(const ScrollMetrics& metrics, float velocity,
                                               float& outPosition, float& outVelocity,
                                               bool& useSpring) const {
    // إذا كان خارج النطاق، نرجع بزنبرك
    if (metrics.outOfRange()) {
        float target = (metrics.pixels < metrics.minScrollExtent)
                        ? metrics.minScrollExtent : metrics.maxScrollExtent;
        outPosition = target;
        outVelocity = velocity;
        useSpring = true;
        return true;
    }
    // تمرير بالقصور
    if (std::abs(velocity) >= minFlingVelocity) {
        float clampedVel = std::clamp(velocity, -maxFlingVelocity, maxFlingVelocity);
        outVelocity = clampedVel;
        useSpring = false;
        return true;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// BouncingScrollPhysics
// ═══════════════════════════════════════════════════════════════════════════════

float BouncingScrollPhysics::applyBoundaryConditions(const ScrollMetrics& metrics,
                                                       float newPixels) const {
    // السماح بالتجاوز مع مقاومة
    float overscroll = 0;
    if (newPixels < metrics.minScrollExtent) {
        overscroll = newPixels - metrics.minScrollExtent;
    } else if (newPixels > metrics.maxScrollExtent) {
        overscroll = newPixels - metrics.maxScrollExtent;
    }
    if (overscroll != 0) {
        // مقاومة تزداد مع التجاوز
        float resistance = 1.0f / (1.0f + std::abs(overscroll) / (metrics.viewportDimension * 0.5f));
        if (newPixels < metrics.minScrollExtent) {
            return metrics.minScrollExtent + overscroll * resistance;
        } else {
            return metrics.maxScrollExtent + overscroll * resistance;
        }
    }
    return newPixels;
}

bool BouncingScrollPhysics::createBallisticSimulation(const ScrollMetrics& metrics, float velocity,
                                                        float& outPosition, float& outVelocity,
                                                        bool& useSpring) const {
    // إذا خارج الحدود، ارتداد بزنبرك
    if (metrics.outOfRange()) {
        float target = (metrics.pixels < metrics.minScrollExtent)
                         ? metrics.minScrollExtent : metrics.maxScrollExtent;
        outPosition = target;
        outVelocity = velocity;
        useSpring = true;
        return true;
    }
    // تمرير بالاحتكاك
    if (std::abs(velocity) >= minFlingVelocity) {
        float clampedVel = std::clamp(velocity, -maxFlingVelocity, maxFlingVelocity);
        outVelocity = clampedVel;
        useSpring = false;
        return true;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ClampingScrollPhysics
// ═══════════════════════════════════════════════════════════════════════════════

float ClampingScrollPhysics::applyBoundaryConditions(const ScrollMetrics& metrics,
                                                       float newPixels) const {
    return std::clamp(newPixels, metrics.minScrollExtent, metrics.maxScrollExtent);
}

bool ClampingScrollPhysics::createBallisticSimulation(const ScrollMetrics& metrics, float velocity,
                                                        float& outPosition, float& outVelocity,
                                                        bool& useSpring) const {
    if (std::abs(velocity) < minFlingVelocity) return false;
    float clampedVel = std::clamp(velocity, -maxFlingVelocity, maxFlingVelocity);
    outVelocity = clampedVel;
    useSpring = false;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PageScrollPhysics
// ═══════════════════════════════════════════════════════════════════════════════

bool PageScrollPhysics::createBallisticSimulation(const ScrollMetrics& metrics, float velocity,
                                                    float& outPosition, float& outVelocity,
                                                    bool& useSpring) const {
    if (metrics.viewportDimension <= 0) return false;

    float pageSize = metrics.viewportDimension;
    float currentPage = metrics.pixels / pageSize;
    float targetPage;

    if (std::abs(velocity) > minFlingVelocity) {
        // سحب سريع — انتقل للصفحة التالية/السابقة
        targetPage = (velocity > 0) ? std::ceil(currentPage) : std::floor(currentPage);
    } else {
        // انتقل لأقرب صفحة
        float fraction = currentPage - std::floor(currentPage);
        if (fraction > snapThreshold && fraction < (1.0f - snapThreshold)) {
            targetPage = (fraction > 0.5f) ? std::ceil(currentPage) : std::floor(currentPage);
        } else {
            targetPage = std::round(currentPage);
        }
    }

    outPosition = targetPage * pageSize;
    outPosition = std::clamp(outPosition, metrics.minScrollExtent, metrics.maxScrollExtent);
    outVelocity = velocity;
    useSpring = true;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ScrollController
// ═══════════════════════════════════════════════════════════════════════════════

ScrollController::ScrollController(ScrollPhysics* physics) : physics_(physics) {}

float ScrollController::applyUserOffset(float delta) {
    stop(); // إيقاف أي محاكاة
    float newPos = metrics_.pixels + delta;
    metrics_.pixels = activePhysics()->applyBoundaryConditions(metrics_, newPos);
    notifyScroll();
    return metrics_.pixels;
}

void ScrollController::startFling(float velocity) {
    float outPos = 0, outVel = 0;
    bool useSpring = false;

    if (activePhysics()->createBallisticSimulation(metrics_, velocity, outPos, outVel, useSpring)) {
        isAnimating_ = true;
        simTime_ = 0;
        simStartPos_ = metrics_.pixels;
        simStartVel_ = outVel;
        simTargetPos_ = outPos;
        simUseSpring_ = useSpring;
        isAnimatingTo_ = false;
    }
}

bool ScrollController::update(float dt) {
    if (isAnimatingTo_) {
        animElapsed_ += dt * 1000.0f; // تحويل من ثانية لمللي ثانية
        float t = std::min(animElapsed_ / animDuration_, 1.0f);
        // Ease-out cubic
        float ease = 1.0f - std::pow(1.0f - t, 3.0f);
        metrics_.pixels = animStartPos_ + (animTargetPos_ - animStartPos_) * ease;
        notifyScroll();

        if (t >= 1.0f) {
            metrics_.pixels = animTargetPos_;
            isAnimatingTo_ = false;
            isAnimating_ = false;
            notifyScroll();
            return false;
        }
        return true;
    }

    if (!isAnimating_) return false;

    simTime_ += dt;

    if (simUseSpring_) {
        SpringSimulation sim(SpringDescription::defaultSpring(),
                             simStartPos_, simTargetPos_, simStartVel_);
        metrics_.pixels = sim.position(simTime_);
        notifyScroll();

        if (sim.isDone(simTime_)) {
            metrics_.pixels = simTargetPos_;
            isAnimating_ = false;
            notifyScroll();
            return false;
        }
        return true;
    } else {
        // احتكاك
        float drag = 1.0f - activePhysics()->frictionFactor();
        FrictionSimulation friction(drag, simStartPos_, simStartVel_);
        float newPos = friction.positionAt(simTime_);

        // تشبيك
        newPos = std::clamp(newPos, metrics_.minScrollExtent, metrics_.maxScrollExtent);
        metrics_.pixels = newPos;
        notifyScroll();

        if (friction.isDone(simTime_) ||
            newPos <= metrics_.minScrollExtent ||
            newPos >= metrics_.maxScrollExtent) {
            isAnimating_ = false;
            return false;
        }
        return true;
    }
}

void ScrollController::stop() {
    isAnimating_ = false;
    isAnimatingTo_ = false;
}

void ScrollController::setExtents(float min, float max, float viewport) {
    metrics_.minScrollExtent = min;
    metrics_.maxScrollExtent = max;
    metrics_.viewportDimension = viewport;
}

void ScrollController::jumpTo(float position) {
    stop();
    metrics_.pixels = std::clamp(position, metrics_.minScrollExtent, metrics_.maxScrollExtent);
    notifyScroll();
}

void ScrollController::animateTo(float position, float durationMs) {
    stop();
    isAnimating_ = true;
    isAnimatingTo_ = true;
    animStartPos_ = metrics_.pixels;
    animTargetPos_ = std::clamp(position, metrics_.minScrollExtent, metrics_.maxScrollExtent);
    animDuration_ = durationMs;
    animElapsed_ = 0;
}

void ScrollController::notifyScroll() {
    metrics_.pixels = metrics_.pixels; // تحديث الحالة
    if (scrollCallback_) scrollCallback_(metrics_.pixels);
}

} // namespace ui
} // namespace sad
