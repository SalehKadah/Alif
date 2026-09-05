/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: animation.h
 * المسار: features/graphics/core/include/sad_ui/animation.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * الوصف:
 * ------
 * محرك الانيميشن (Animation Engine) لواجهات لغة ص.
 *
 * يدعم:
 * - تحريك الخصائص الرقمية (عرض، ارتفاع، شفافية، حجم...)
 * - تحريك الألوان (تدرج سلس)
 * - منحنيات التسهيل (Easing): خطي، تسارع، تباطؤ، نابض
 * - تحريك متسلسل ومتزامن
 * - دعم التكرار والانعكاس
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_ANIMATION_H
#define SAD_UI_ANIMATION_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <cmath>
#include <cstdint>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// أنواع التسهيل (Easing Types)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief منحنيات التسهيل المدعومة
 *
 * كل منحنى يُحدد كيف تتغير القيمة بمرور الوقت:
 * - Linear: تغيّر منتظم
 * - EaseIn: بداية بطيئة ثم تسارع
 * - EaseOut: بداية سريعة ثم تباطؤ
 * - EaseInOut: بطيء-سريع-بطيء
 * - Spring: ارتداد نابض
 */
enum class EasingType : uint8_t {
    Linear,          ///< خطي
    EaseIn,          ///< تسارع (Cubic)
    EaseOut,         ///< تباطؤ (Cubic)
    EaseInOut,       ///< تسارع وتباطؤ (Cubic)
    EaseInQuad,      ///< تسارع تربيعي
    EaseOutQuad,     ///< تباطؤ تربيعي
    EaseInOutQuad,   ///< تسارع وتباطؤ تربيعي
    EaseInBack,      ///< تسارع مع تراجع
    EaseOutBack,     ///< تباطؤ مع تجاوز
    EaseOutBounce,   ///< ارتداد عند النهاية
    Spring           ///< نابض (ارتداد متأرجح)
};

// ═══════════════════════════════════════════════════════════════════════════════
// دوال التسهيل (Easing Functions)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief يُطبّق منحنى تسهيل على قيمة t (بين 0.0 و 1.0)
 *
 * @param type نوع المنحنى
 * @param t القيمة الزمنية (0.0 = بداية، 1.0 = نهاية)
 * @return القيمة المُعدّلة (قد تتجاوز 0-1 في بعض المنحنيات)
 */
inline double applyEasing(EasingType type, double t) {
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;

    switch (type) {
        case EasingType::Linear:
            return t;

        case EasingType::EaseIn:
            return t * t * t;

        case EasingType::EaseOut: {
            double p = 1.0 - t;
            return 1.0 - p * p * p;
        }

        case EasingType::EaseInOut:
            return t < 0.5
                ? 4.0 * t * t * t
                : 1.0 - std::pow(-2.0 * t + 2.0, 3.0) / 2.0;

        case EasingType::EaseInQuad:
            return t * t;

        case EasingType::EaseOutQuad:
            return 1.0 - (1.0 - t) * (1.0 - t);

        case EasingType::EaseInOutQuad:
            return t < 0.5
                ? 2.0 * t * t
                : 1.0 - std::pow(-2.0 * t + 2.0, 2.0) / 2.0;

        case EasingType::EaseInBack: {
            const double c = 1.70158;
            return (c + 1.0) * t * t * t - c * t * t;
        }

        case EasingType::EaseOutBack: {
            const double c = 1.70158;
            double p = t - 1.0;
            return 1.0 + (c + 1.0) * p * p * p + c * p * p;
        }

        case EasingType::EaseOutBounce: {
            if (t < 1.0 / 2.75) {
                return 7.5625 * t * t;
            } else if (t < 2.0 / 2.75) {
                double p = t - 1.5 / 2.75;
                return 7.5625 * p * p + 0.75;
            } else if (t < 2.5 / 2.75) {
                double p = t - 2.25 / 2.75;
                return 7.5625 * p * p + 0.9375;
            } else {
                double p = t - 2.625 / 2.75;
                return 7.5625 * p * p + 0.984375;
            }
        }

        case EasingType::Spring: {
            // نابض بسيط: oscillation + decay
            return 1.0 - std::exp(-6.0 * t) * std::cos(8.0 * M_PI * t);
        }

        default:
            return t;
    }
}

/**
 * @brief تحويل اسم منحنى تسهيل (عربي/إنجليزي) إلى نوع
 */
inline EasingType easingFromString(const std::string& name) {
    if (name == "خطي" || name == "linear") return EasingType::Linear;
    if (name == "تسارع" || name == "easeIn") return EasingType::EaseIn;
    if (name == "تباطؤ" || name == "easeOut") return EasingType::EaseOut;
    if (name == "تسارع_وتباطؤ" || name == "easeInOut") return EasingType::EaseInOut;
    if (name == "ارتداد" || name == "bounce") return EasingType::EaseOutBounce;
    if (name == "نابض" || name == "spring") return EasingType::Spring;
    if (name == "تراجع" || name == "easeOutBack") return EasingType::EaseOutBack;
    return EasingType::EaseInOut; // افتراضي
}

// ═══════════════════════════════════════════════════════════════════════════════
// قيمة متحركة (Animated Value)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief تحريكة واحدة لخاصية ما
 *
 * تُمثّل انتقال قيمة من `fromValue` إلى `toValue`
 * خلال `durationMs` مللي ثانية بمنحنى تسهيل محدد.
 */
struct Animation {
    std::string targetNodeId;        ///< معرّف العقدة المستهدفة
    std::string propertyKey;         ///< اسم الخاصية (مثل: "شفافية"، "عرض")
    double fromValue = 0.0;          ///< القيمة الابتدائية
    double toValue = 1.0;            ///< القيمة النهائية
    uint32_t durationMs = 300;       ///< المدة (مللي ثانية)
    uint32_t delayMs = 0;            ///< تأخير قبل البدء
    EasingType easing = EasingType::EaseInOut;  ///< منحنى التسهيل
    int repeatCount = 0;             ///< عدد التكرارات (0 = بدون، -1 = لانهائي)
    bool autoReverse = false;        ///< عكس الحركة عند التكرار
    
    /// callback عند الانتهاء
    std::function<void()> onComplete;
};

/**
 * @brief تحريكة لون (من لون إلى لون)
 */
struct ColorAnimation {
    std::string targetNodeId;
    std::string propertyKey;
    float fromR = 0, fromG = 0, fromB = 0, fromA = 1;
    float toR = 1, toG = 1, toB = 1, toA = 1;
    uint32_t durationMs = 300;
    uint32_t delayMs = 0;
    EasingType easing = EasingType::EaseInOut;
    std::function<void()> onComplete;
};

// ═══════════════════════════════════════════════════════════════════════════════
// حالة تحريكة نشطة (Active Animation State)
// ═══════════════════════════════════════════════════════════════════════════════

struct ActiveAnimation {
    Animation spec;              ///< مواصفات التحريكة
    uint32_t startTimeMs = 0;    ///< وقت البدء (SDL_GetTicks)
    double currentValue = 0.0;   ///< القيمة الحالية
    int currentRepeat = 0;       ///< رقم التكرار الحالي
    bool isReversed = false;     ///< هل في وضع الانعكاس
    bool isFinished = false;     ///< هل انتهت
};

struct ActiveColorAnimation {
    ColorAnimation spec;
    uint32_t startTimeMs = 0;
    float currentR = 0, currentG = 0, currentB = 0, currentA = 1;
    bool isFinished = false;
};

// ═══════════════════════════════════════════════════════════════════════════════
// محرك الانيميشن (Animation Engine)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief محرك الانيميشن — يدير وينفذ جميع التحريكات
 *
 * الاستخدام:
 * @code
 *   AnimationEngine engine;
 *   engine.animate({
 *       .targetNodeId = "btn_1",
 *       .propertyKey = "شفافية",
 *       .fromValue = 0.0,
 *       .toValue = 1.0,
 *       .durationMs = 500,
 *       .easing = EasingType::EaseOut
 *   });
 *
 *   // في حلقة الأحداث:
 *   engine.update(currentTimeMs);
 *   double opacity = engine.getValue("btn_1", "شفافية", 1.0);
 * @endcode
 */
class AnimationEngine {
public:
    AnimationEngine() = default;

    /**
     * @brief بدء تحريكة جديدة
     */
    void animate(const Animation& anim, uint32_t currentTimeMs);

    /**
     * @brief بدء تحريكة لون
     */
    void animateColor(const ColorAnimation& anim, uint32_t currentTimeMs);

    /**
     * @brief تحديث جميع التحريكات النشطة
     * @param currentTimeMs الوقت الحالي (SDL_GetTicks)
     * @return true إذا كانت هناك تحريكات نشطة (تحتاج إعادة رسم)
     */
    bool update(uint32_t currentTimeMs);

    /**
     * @brief الحصول على القيمة المتحركة لخاصية عقدة
     * @param nodeId معرّف العقدة
     * @param propertyKey اسم الخاصية
     * @param defaultValue القيمة الافتراضية إذا لم تكن هناك تحريكة
     * @return القيمة الحالية
     */
    double getValue(const std::string& nodeId, 
                    const std::string& propertyKey,
                    double defaultValue) const;

    /**
     * @brief هل توجد تحريكة نشطة لهذه الخاصية؟
     */
    bool hasAnimation(const std::string& nodeId, 
                      const std::string& propertyKey) const;

    /**
     * @brief إيقاف جميع تحريكات عقدة
     */
    void cancelAnimations(const std::string& nodeId);

    /**
     * @brief إيقاف جميع التحريكات
     */
    void cancelAll();

    /**
     * @brief هل توجد أي تحريكات نشطة؟
     */
    bool hasActiveAnimations() const { return !activeAnimations_.empty() || !activeColorAnimations_.empty(); }

private:
    /// مفتاح فريد لتحريكة: nodeId + ":" + propertyKey
    std::string makeKey(const std::string& nodeId, const std::string& prop) const {
        return nodeId + ":" + prop;
    }

    std::unordered_map<std::string, ActiveAnimation> activeAnimations_;
    std::unordered_map<std::string, ActiveColorAnimation> activeColorAnimations_;
};

} // namespace ui
} // namespace sad

#endif // SAD_UI_ANIMATION_H
