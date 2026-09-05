/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: gesture.h
 * المسار: features/graphics/core/include/sad_ui/gesture.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * نظام الإيماءات المتقدم (Advanced Gesture System) لواجهات لغة ص.
 *
 * يدعم:
 * - نقرة، نقرة مزدوجة، ضغط مطوّل
 * - سحب (أفقي، عمودي، حر) مع تتبع السرعة
 * - تحجيم (pinch-to-zoom) بإصبعين
 * - تدوير بإصبعين
 * - سحب وإفلات (Drag & Drop)
 * - ساحة الإيماءات (Gesture Arena) لحل التعارضات
 * - تتبع السرعة (Velocity Tracker)
 *
 * يتفوق على Flutter بـ:
 * - دعم إيماءات مخصصة عبر أنماط
 * - ساحة إيماءات بأولويات قابلة للتخصيص
 * - تتبع متعدد اللمس أصيل
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_GESTURE_H
#define SAD_UI_GESTURE_H

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <cstdint>
#include <cmath>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// أنواع الإيماءات
// ═══════════════════════════════════════════════════════════════════════════════

enum class GestureType : uint8_t {
    Tap,            ///< نقرة
    DoubleTap,      ///< نقرة مزدوجة
    LongPress,      ///< ضغط مطوّل
    Pan,            ///< سحب حر
    HorizontalDrag, ///< سحب أفقي
    VerticalDrag,   ///< سحب عمودي
    Scale,          ///< تحجيم (pinch)
    Rotation,       ///< تدوير بإصبعين
    Fling,          ///< رمي (سحب سريع)
    ForceTap,       ///< ضغط قوي (3D Touch)
    EdgeDrag        ///< سحب من الحافة
};

enum class GestureState : uint8_t {
    Possible,   ///< يمكن أن تبدأ
    Began,      ///< بدأت
    Changed,    ///< تتغير
    Ended,      ///< انتهت
    Cancelled,  ///< أُلغيت
    Failed      ///< فشلت
};

enum class DragDirection : uint8_t {
    Any,        ///< أي اتجاه
    Horizontal, ///< أفقي فقط
    Vertical    ///< عمودي فقط
};

// ═══════════════════════════════════════════════════════════════════════════════
// نقطة وسرعة
// ═══════════════════════════════════════════════════════════════════════════════

struct GesturePoint {
    float x = 0, y = 0;
    float distance(const GesturePoint& o) const {
        float dx = x - o.x, dy = y - o.y;
        return std::sqrt(dx * dx + dy * dy);
    }
    GesturePoint operator-(const GesturePoint& o) const { return {x - o.x, y - o.y}; }
    GesturePoint operator+(const GesturePoint& o) const { return {x + o.x, y + o.y}; }
};

struct Velocity {
    float pixelsPerSecondX = 0;
    float pixelsPerSecondY = 0;
    float speed() const {
        return std::sqrt(pixelsPerSecondX * pixelsPerSecondX +
                         pixelsPerSecondY * pixelsPerSecondY);
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// بيانات الإيماءة
// ═══════════════════════════════════════════════════════════════════════════════

struct GestureEvent {
    GestureType type;
    GestureState state = GestureState::Possible;

    // ─── موقع ───
    GesturePoint position;      ///< الموقع الحالي
    GesturePoint localPosition; ///< الموقع النسبي للعنصر
    GesturePoint startPosition; ///< نقطة البداية
    GesturePoint delta;         ///< الفرق عن الحدث السابق

    // ─── سرعة ───
    Velocity velocity;

    // ─── تحجيم/تدوير ───
    float scale = 1.0f;         ///< نسبة التحجيم (pinch)
    float rotation = 0.0f;      ///< زاوية التدوير (بالراديان)

    // ─── ضغط ───
    float pressure = 1.0f;      ///< قوة الضغط (0-1)
    int pointerCount = 1;       ///< عدد نقاط اللمس

    // ─── زمن ───
    uint64_t timestamp = 0;     ///< الطابع الزمني (مللي ثانية)
    uint64_t duration = 0;      ///< المدة (للضغط المطوّل)

    // ─── معرّف ───
    int pointerId = 0;          ///< معرّف نقطة اللمس
};

// ═══════════════════════════════════════════════════════════════════════════════
// مستمعي الإيماءات
// ═══════════════════════════════════════════════════════════════════════════════

using GestureCallback = std::function<void(const GestureEvent&)>;

struct GestureCallbacks {
    // نقرة
    GestureCallback onTap;
    GestureCallback onDoubleTap;
    GestureCallback onLongPress;
    GestureCallback onLongPressEnd;
    // سحب
    GestureCallback onPanStart;
    GestureCallback onPanUpdate;
    GestureCallback onPanEnd;
    GestureCallback onHorizontalDragStart;
    GestureCallback onHorizontalDragUpdate;
    GestureCallback onHorizontalDragEnd;
    GestureCallback onVerticalDragStart;
    GestureCallback onVerticalDragUpdate;
    GestureCallback onVerticalDragEnd;
    // تحجيم وتدوير
    GestureCallback onScaleStart;
    GestureCallback onScaleUpdate;
    GestureCallback onScaleEnd;
    // رمي
    GestureCallback onFlingLeft;
    GestureCallback onFlingRight;
    GestureCallback onFlingUp;
    GestureCallback onFlingDown;
};

// ═══════════════════════════════════════════════════════════════════════════════
// متتبع السرعة (Velocity Tracker)
// ═══════════════════════════════════════════════════════════════════════════════

class VelocityTracker {
public:
    void addPosition(uint64_t timestampMs, float x, float y);
    Velocity getVelocity() const;
    void reset();

private:
    struct Sample {
        uint64_t time;
        float x, y;
    };
    std::vector<Sample> samples_;
    static constexpr size_t kMaxSamples = 20;
    static constexpr uint64_t kMaxAge = 200; // ms
};

// ═══════════════════════════════════════════════════════════════════════════════
// كاشف الإيماءات (Gesture Recognizer)
// ═══════════════════════════════════════════════════════════════════════════════

class GestureRecognizer {
public:
    virtual ~GestureRecognizer() = default;
    virtual GestureType type() const = 0;
    virtual void handlePointerDown(const GestureEvent& event) = 0;
    virtual void handlePointerMove(const GestureEvent& event) = 0;
    virtual void handlePointerUp(const GestureEvent& event) = 0;
    virtual void handlePointerCancel() = 0;
    virtual void reset() = 0;

    void setCallbacks(const GestureCallbacks& cb) { callbacks_ = cb; }
    int priority() const { return priority_; }
    void setPriority(int p) { priority_ = p; }

protected:
    GestureCallbacks callbacks_;
    GestureState state_ = GestureState::Possible;
    int priority_ = 0;
};

// ─── كاشفات محددة ───

class TapRecognizer : public GestureRecognizer {
public:
    GestureType type() const override { return GestureType::Tap; }
    void handlePointerDown(const GestureEvent& event) override;
    void handlePointerMove(const GestureEvent& event) override;
    void handlePointerUp(const GestureEvent& event) override;
    void handlePointerCancel() override;
    void reset() override;

    float tapTolerance = 18.0f;
    uint64_t doubleTapTimeout = 300;
    uint64_t longPressTimeout = 500;

private:
    GesturePoint downPos_;
    uint64_t downTime_ = 0;
    uint64_t lastTapTime_ = 0;
    bool wasLongPress_ = false;
};

class DragRecognizer : public GestureRecognizer {
public:
    explicit DragRecognizer(DragDirection dir = DragDirection::Any);
    GestureType type() const override;
    void handlePointerDown(const GestureEvent& event) override;
    void handlePointerMove(const GestureEvent& event) override;
    void handlePointerUp(const GestureEvent& event) override;
    void handlePointerCancel() override;
    void reset() override;

    float dragStartThreshold = 8.0f;
    float flingMinVelocity = 300.0f;

private:
    DragDirection direction_;
    GesturePoint startPos_;
    bool isDragging_ = false;
    VelocityTracker velocityTracker_;
};

class ScaleRecognizer : public GestureRecognizer {
public:
    GestureType type() const override { return GestureType::Scale; }
    void handlePointerDown(const GestureEvent& event) override;
    void handlePointerMove(const GestureEvent& event) override;
    void handlePointerUp(const GestureEvent& event) override;
    void handlePointerCancel() override;
    void reset() override;

private:
    std::unordered_map<int, GesturePoint> pointers_;
    float initialDistance_ = 0;
    float initialAngle_ = 0;
    bool isScaling_ = false;
};

// ═══════════════════════════════════════════════════════════════════════════════
// ساحة الإيماءات (Gesture Arena)
// ═══════════════════════════════════════════════════════════════════════════════

class GestureArena {
    friend class GestureDetector;
public:
    void addRecognizer(std::shared_ptr<GestureRecognizer> recognizer);
    void removeRecognizer(GestureType type);
    void clear();

    void handlePointerDown(const GestureEvent& event);
    void handlePointerMove(const GestureEvent& event);
    void handlePointerUp(const GestureEvent& event);
    void handlePointerCancel();

private:
    std::vector<std::shared_ptr<GestureRecognizer>> recognizers_;
    std::shared_ptr<GestureRecognizer> winner_;
    bool resolved_ = false;

    void resolveArena();
};

// ═══════════════════════════════════════════════════════════════════════════════
// كاشف إيماءات مُجمّع (GestureDetector) — واجهة عالية المستوى
// ═══════════════════════════════════════════════════════════════════════════════

class GestureDetector {
public:
    GestureDetector();

    /// تعيين المستمعين
    void setCallbacks(const GestureCallbacks& callbacks);

    /// معالجة أحداث المؤشر
    void onPointerDown(float x, float y, int pointerId = 0, float pressure = 1.0f);
    void onPointerMove(float x, float y, int pointerId = 0, float pressure = 1.0f);
    void onPointerUp(float x, float y, int pointerId = 0);
    void onPointerCancel();

    /// تحديث الموقت (يُستدعى كل إطار)
    void update(uint64_t currentTimeMs);

    /// الحالة
    bool isDragging() const { return isDragging_; }
    bool isScaling() const { return isScaling_; }

    /// إعدادات
    float tapTolerance = 18.0f;
    uint64_t doubleTapTimeout = 300;
    uint64_t longPressTimeout = 500;
    float dragStartThreshold = 8.0f;
    float flingMinVelocity = 300.0f;

private:
    GestureArena arena_;
    GestureCallbacks callbacks_;
    bool isDragging_ = false;
    bool isScaling_ = false;
    uint64_t currentTime_ = 0;

    GestureEvent makeEvent(float x, float y, int pointerId, float pressure) const;
};

} // namespace ui
} // namespace sad

#endif // SAD_UI_GESTURE_H
