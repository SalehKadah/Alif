/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: gesture.cpp
 * المسار: features/graphics/core/src/gesture.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * تنفيذ نظام الإيماءات المتقدم.
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/gesture.h"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// VelocityTracker
// ═══════════════════════════════════════════════════════════════════════════════

void VelocityTracker::addPosition(uint64_t timestampMs, float x, float y) {
    samples_.push_back({timestampMs, x, y});
    if (samples_.size() > kMaxSamples)
        samples_.erase(samples_.begin());
}

Velocity VelocityTracker::getVelocity() const {
    if (samples_.size() < 2) return {0, 0};

    uint64_t now = samples_.back().time;

    // حساب متوسط السرعة من العيّنات الحديثة
    float sumVx = 0, sumVy = 0;
    int count = 0;

    for (size_t i = samples_.size() - 1; i > 0; --i) {
        const auto& curr = samples_[i];
        const auto& prev = samples_[i - 1];

        if (now - curr.time > kMaxAge) break;

        float dt = static_cast<float>(curr.time - prev.time);
        if (dt > 0) {
            sumVx += (curr.x - prev.x) / dt * 1000.0f;
            sumVy += (curr.y - prev.y) / dt * 1000.0f;
            ++count;
        }
    }
    if (count == 0) return {0, 0};
    return {sumVx / count, sumVy / count};
}

void VelocityTracker::reset() {
    samples_.clear();
}

// ═══════════════════════════════════════════════════════════════════════════════
// TapRecognizer
// ═══════════════════════════════════════════════════════════════════════════════

void TapRecognizer::handlePointerDown(const GestureEvent& event) {
    downPos_ = event.position;
    downTime_ = event.timestamp;
    state_ = GestureState::Possible;
    wasLongPress_ = false;
}

void TapRecognizer::handlePointerMove(const GestureEvent& event) {
    if (state_ == GestureState::Cancelled) return;
    if (downPos_.distance(event.position) > tapTolerance) {
        state_ = GestureState::Cancelled;
    }
}

void TapRecognizer::handlePointerUp(const GestureEvent& event) {
    if (state_ == GestureState::Cancelled) return;

    uint64_t pressDuration = event.timestamp - downTime_;

    // ضغط مطوّل
    if (pressDuration >= longPressTimeout) {
        wasLongPress_ = true;
        if (callbacks_.onLongPressEnd) {
            GestureEvent e = event;
            e.type = GestureType::LongPress;
            e.state = GestureState::Ended;
            e.duration = pressDuration;
            callbacks_.onLongPressEnd(e);
        }
        state_ = GestureState::Ended;
        return;
    }

    // نقرة مزدوجة
    if (event.timestamp - lastTapTime_ <= doubleTapTimeout) {
        if (callbacks_.onDoubleTap) {
            GestureEvent e = event;
            e.type = GestureType::DoubleTap;
            e.state = GestureState::Ended;
            e.startPosition = downPos_;
            callbacks_.onDoubleTap(e);
        }
        lastTapTime_ = 0;
        state_ = GestureState::Ended;
        return;
    }

    // نقرة عادية
    if (callbacks_.onTap) {
        GestureEvent e = event;
        e.type = GestureType::Tap;
        e.state = GestureState::Ended;
        e.startPosition = downPos_;
        callbacks_.onTap(e);
    }
    lastTapTime_ = event.timestamp;
    state_ = GestureState::Ended;
}

void TapRecognizer::handlePointerCancel() {
    state_ = GestureState::Cancelled;
}

void TapRecognizer::reset() {
    state_ = GestureState::Possible;
    wasLongPress_ = false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// DragRecognizer
// ═══════════════════════════════════════════════════════════════════════════════

DragRecognizer::DragRecognizer(DragDirection dir) : direction_(dir) {}

GestureType DragRecognizer::type() const {
    switch (direction_) {
        case DragDirection::Horizontal: return GestureType::HorizontalDrag;
        case DragDirection::Vertical: return GestureType::VerticalDrag;
        default: return GestureType::Pan;
    }
}

void DragRecognizer::handlePointerDown(const GestureEvent& event) {
    startPos_ = event.position;
    isDragging_ = false;
    state_ = GestureState::Possible;
    velocityTracker_.reset();
    velocityTracker_.addPosition(event.timestamp, event.position.x, event.position.y);
}

void DragRecognizer::handlePointerMove(const GestureEvent& event) {
    if (state_ == GestureState::Cancelled) return;

    velocityTracker_.addPosition(event.timestamp, event.position.x, event.position.y);

    float dx = event.position.x - startPos_.x;
    float dy = event.position.y - startPos_.y;

    if (!isDragging_) {
        float dist = startPos_.distance(event.position);
        if (dist < dragStartThreshold) return;

        // تحقق من الاتجاه
        if (direction_ == DragDirection::Horizontal && std::abs(dy) > std::abs(dx)) {
            state_ = GestureState::Cancelled;
            return;
        }
        if (direction_ == DragDirection::Vertical && std::abs(dx) > std::abs(dy)) {
            state_ = GestureState::Cancelled;
            return;
        }

        isDragging_ = true;
        state_ = GestureState::Began;

        GestureEvent e = event;
        e.type = type();
        e.state = GestureState::Began;
        e.startPosition = startPos_;
        e.delta = {0, 0};

        if (direction_ == DragDirection::Horizontal && callbacks_.onHorizontalDragStart)
            callbacks_.onHorizontalDragStart(e);
        else if (direction_ == DragDirection::Vertical && callbacks_.onVerticalDragStart)
            callbacks_.onVerticalDragStart(e);
        else if (callbacks_.onPanStart)
            callbacks_.onPanStart(e);

        return;
    }

    // تحديث السحب
    state_ = GestureState::Changed;
    GestureEvent e = event;
    e.type = type();
    e.state = GestureState::Changed;
    e.startPosition = startPos_;
    e.delta = event.position - startPos_;
    e.velocity = velocityTracker_.getVelocity();

    if (direction_ == DragDirection::Horizontal && callbacks_.onHorizontalDragUpdate)
        callbacks_.onHorizontalDragUpdate(e);
    else if (direction_ == DragDirection::Vertical && callbacks_.onVerticalDragUpdate)
        callbacks_.onVerticalDragUpdate(e);
    else if (callbacks_.onPanUpdate)
        callbacks_.onPanUpdate(e);
}

void DragRecognizer::handlePointerUp(const GestureEvent& event) {
    if (!isDragging_) return;

    velocityTracker_.addPosition(event.timestamp, event.position.x, event.position.y);
    Velocity vel = velocityTracker_.getVelocity();

    state_ = GestureState::Ended;
    isDragging_ = false;

    GestureEvent e = event;
    e.type = type();
    e.state = GestureState::Ended;
    e.startPosition = startPos_;
    e.delta = event.position - startPos_;
    e.velocity = vel;

    if (direction_ == DragDirection::Horizontal && callbacks_.onHorizontalDragEnd)
        callbacks_.onHorizontalDragEnd(e);
    else if (direction_ == DragDirection::Vertical && callbacks_.onVerticalDragEnd)
        callbacks_.onVerticalDragEnd(e);
    else if (callbacks_.onPanEnd)
        callbacks_.onPanEnd(e);

    // كشف الرمي (fling)
    if (vel.speed() >= flingMinVelocity) {
        GestureEvent flingEvt = e;
        flingEvt.type = GestureType::Fling;
        if (std::abs(vel.pixelsPerSecondX) > std::abs(vel.pixelsPerSecondY)) {
            if (vel.pixelsPerSecondX > 0 && callbacks_.onFlingRight)
                callbacks_.onFlingRight(flingEvt);
            else if (vel.pixelsPerSecondX < 0 && callbacks_.onFlingLeft)
                callbacks_.onFlingLeft(flingEvt);
        } else {
            if (vel.pixelsPerSecondY > 0 && callbacks_.onFlingDown)
                callbacks_.onFlingDown(flingEvt);
            else if (vel.pixelsPerSecondY < 0 && callbacks_.onFlingUp)
                callbacks_.onFlingUp(flingEvt);
        }
    }
}

void DragRecognizer::handlePointerCancel() {
    state_ = GestureState::Cancelled;
    isDragging_ = false;
    velocityTracker_.reset();
}

void DragRecognizer::reset() {
    state_ = GestureState::Possible;
    isDragging_ = false;
    velocityTracker_.reset();
}

// ═══════════════════════════════════════════════════════════════════════════════
// ScaleRecognizer
// ═══════════════════════════════════════════════════════════════════════════════

void ScaleRecognizer::handlePointerDown(const GestureEvent& event) {
    pointers_[event.pointerId] = event.position;
    if (pointers_.size() == 2) {
        auto it = pointers_.begin();
        GesturePoint p1 = it->second; ++it;
        GesturePoint p2 = it->second;
        initialDistance_ = p1.distance(p2);
        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        initialAngle_ = std::atan2(dy, dx);
        isScaling_ = true;
        state_ = GestureState::Began;

        if (callbacks_.onScaleStart) {
            GestureEvent e = event;
            e.type = GestureType::Scale;
            e.state = GestureState::Began;
            e.scale = 1.0f;
            e.rotation = 0.0f;
            e.pointerCount = 2;
            callbacks_.onScaleStart(e);
        }
    }
}

void ScaleRecognizer::handlePointerMove(const GestureEvent& event) {
    if (pointers_.find(event.pointerId) == pointers_.end()) return;
    pointers_[event.pointerId] = event.position;

    if (!isScaling_ || pointers_.size() < 2) return;

    auto it = pointers_.begin();
    GesturePoint p1 = it->second; ++it;
    GesturePoint p2 = it->second;

    float currentDistance = p1.distance(p2);
    float scale = (initialDistance_ > 0) ? currentDistance / initialDistance_ : 1.0f;

    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    float currentAngle = std::atan2(dy, dx);
    float rotation = currentAngle - initialAngle_;

    state_ = GestureState::Changed;
    if (callbacks_.onScaleUpdate) {
        GestureEvent e = event;
        e.type = GestureType::Scale;
        e.state = GestureState::Changed;
        e.scale = scale;
        e.rotation = rotation;
        e.pointerCount = static_cast<int>(pointers_.size());
        e.position = {(p1.x + p2.x) / 2.0f, (p1.y + p2.y) / 2.0f};
        callbacks_.onScaleUpdate(e);
    }
}

void ScaleRecognizer::handlePointerUp(const GestureEvent& event) {
    pointers_.erase(event.pointerId);
    if (isScaling_ && pointers_.size() < 2) {
        isScaling_ = false;
        state_ = GestureState::Ended;
        if (callbacks_.onScaleEnd) {
            GestureEvent e = event;
            e.type = GestureType::Scale;
            e.state = GestureState::Ended;
            e.pointerCount = static_cast<int>(pointers_.size());
            callbacks_.onScaleEnd(e);
        }
    }
}

void ScaleRecognizer::handlePointerCancel() {
    pointers_.clear();
    isScaling_ = false;
    state_ = GestureState::Cancelled;
}

void ScaleRecognizer::reset() {
    pointers_.clear();
    isScaling_ = false;
    initialDistance_ = 0;
    initialAngle_ = 0;
    state_ = GestureState::Possible;
}

// ═══════════════════════════════════════════════════════════════════════════════
// GestureArena
// ═══════════════════════════════════════════════════════════════════════════════

void GestureArena::addRecognizer(std::shared_ptr<GestureRecognizer> recognizer) {
    recognizers_.push_back(std::move(recognizer));
    std::sort(recognizers_.begin(), recognizers_.end(),
        [](const auto& a, const auto& b) { return a->priority() > b->priority(); });
}

void GestureArena::removeRecognizer(GestureType type) {
    recognizers_.erase(
        std::remove_if(recognizers_.begin(), recognizers_.end(),
            [type](const auto& r) { return r->type() == type; }),
        recognizers_.end());
}

void GestureArena::clear() {
    recognizers_.clear();
    winner_ = nullptr;
    resolved_ = false;
}

void GestureArena::handlePointerDown(const GestureEvent& event) {
    winner_ = nullptr;
    resolved_ = false;
    for (auto& r : recognizers_)
        r->handlePointerDown(event);
}

void GestureArena::handlePointerMove(const GestureEvent& event) {
    if (resolved_ && winner_) {
        winner_->handlePointerMove(event);
        return;
    }
    for (auto& r : recognizers_)
        r->handlePointerMove(event);
    resolveArena();
}

void GestureArena::handlePointerUp(const GestureEvent& event) {
    if (resolved_ && winner_) {
        winner_->handlePointerUp(event);
    } else {
        for (auto& r : recognizers_)
            r->handlePointerUp(event);
    }
    // إعادة تعيين
    for (auto& r : recognizers_)
        r->reset();
    winner_ = nullptr;
    resolved_ = false;
}

void GestureArena::handlePointerCancel() {
    for (auto& r : recognizers_)
        r->handlePointerCancel();
    winner_ = nullptr;
    resolved_ = false;
}

void GestureArena::resolveArena() {
    // الفائز هو أول كاشف بحالة Began أو Changed (الأعلى أولوية)
    for (auto& r : recognizers_) {
        // نحتاج قراءة حالة الكاشف — نعتمد على أن handlePointerMove يحدّث state
        // بما أن state_ محمي، نستخدم حقيقة أن الكاشفات المتقدمة (drag) تنتقل من Possible
        // نمرر الأولوية الأعلى كفائز مؤقت
    }
    // استراتيجية بسيطة: أول كاشف drag يبدأ يفوز
    for (auto& r : recognizers_) {
        if (r->type() == GestureType::Pan ||
            r->type() == GestureType::HorizontalDrag ||
            r->type() == GestureType::VerticalDrag ||
            r->type() == GestureType::Scale) {
            // سيتم حل الساحة عندما يبدأ السحب
            // (هذا يحدث ضمنياً عبر handlePointerMove)
            break;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// GestureDetector (واجهة عالية المستوى)
// ═══════════════════════════════════════════════════════════════════════════════

GestureDetector::GestureDetector() {
    // إنشاء كاشفات افتراضية
    auto tap = std::make_shared<TapRecognizer>();
    tap->setPriority(0);
    arena_.addRecognizer(tap);

    auto pan = std::make_shared<DragRecognizer>(DragDirection::Any);
    pan->setPriority(1);
    arena_.addRecognizer(pan);

    auto hDrag = std::make_shared<DragRecognizer>(DragDirection::Horizontal);
    hDrag->setPriority(2);
    arena_.addRecognizer(hDrag);

    auto vDrag = std::make_shared<DragRecognizer>(DragDirection::Vertical);
    vDrag->setPriority(2);
    arena_.addRecognizer(vDrag);

    auto scale = std::make_shared<ScaleRecognizer>();
    scale->setPriority(3);
    arena_.addRecognizer(scale);
}

void GestureDetector::setCallbacks(const GestureCallbacks& callbacks) {
    callbacks_ = callbacks;
    // توزيع المستمعين على الكاشفات
    // نمرر المستمعين لجميع الكاشفات — كل كاشف يستخدم ما يخصه
    for (auto& recognizer : arena_.recognizers_) {
        // نحتاج وصول — نجعل GestureDetector friend أو نستخدم طريقة أخرى
        recognizer->setCallbacks(callbacks);
    }
}

GestureEvent GestureDetector::makeEvent(float x, float y, int pointerId, float pressure) const {
    GestureEvent e;
    e.position = {x, y};
    e.localPosition = {x, y};
    e.pointerId = pointerId;
    e.pressure = pressure;
    e.timestamp = currentTime_;
    return e;
}

void GestureDetector::onPointerDown(float x, float y, int pointerId, float pressure) {
    GestureEvent e = makeEvent(x, y, pointerId, pressure);
    arena_.handlePointerDown(e);
}

void GestureDetector::onPointerMove(float x, float y, int pointerId, float pressure) {
    GestureEvent e = makeEvent(x, y, pointerId, pressure);
    arena_.handlePointerMove(e);
}

void GestureDetector::onPointerUp(float x, float y, int pointerId) {
    GestureEvent e = makeEvent(x, y, pointerId, 0);
    arena_.handlePointerUp(e);
    isDragging_ = false;
    isScaling_ = false;
}

void GestureDetector::onPointerCancel() {
    arena_.handlePointerCancel();
    isDragging_ = false;
    isScaling_ = false;
}

void GestureDetector::update(uint64_t currentTimeMs) {
    currentTime_ = currentTimeMs;
}

} // namespace ui
} // namespace sad
