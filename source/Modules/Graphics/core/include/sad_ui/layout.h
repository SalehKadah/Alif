/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: layout.h
 * المسار: features/graphics/core/include/sad_ui/layout.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * الوصف:
 * ------
 * محرك التخطيط (Layout Engine) — يحسب الموقع والأبعاد النهائية لكل عنصر.
 *
 * بعد أن تكون لدينا شجرة IR مع أنماطها، يحسب محرك التخطيط
 * أين يظهر كل عنصر على الشاشة وما حجمه.
 *
 * خوارزمية التخطيط:
 * ──────────────────
 * نستخدم خوارزمية Flexbox مبسطة بمرحلتين:
 *
 * 1. **مرحلة القياس (Measure)**: من الأسفل للأعلى
 *    - كل عنصر ورقي (نص، صورة) يعرف حجمه الطبيعي
 *    - كل حاوي (عمود، صف) يحسب حجمه من أبنائه
 *    - القيود (constraints) تُمرر من الأعلى
 *
 * 2. **مرحلة التوزيع (Arrange)**: من الأعلى للأسفل
 *    - كل حاوي يوزع المساحة على أبنائه
 *    - المحاذاة والحشوة والهوامش تُطبّق هنا
 *    - النتيجة: مستطيل (x, y, width, height) لكل عنصر
 *
 * دعم RTL:
 * ─────────
 * التخطيط يدعم الاتجاه من اليمين لليسار تلقائياً:
 * - صف (Row): الأبناء يظهرون من اليمين لليسار
 * - حشوة/هامش: start = يمين، end = يسار
 * - محاذاة: Start = يمين
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_LAYOUT_H
#define SAD_UI_LAYOUT_H

#include "types.h"
#include "style.h"

#include <vector>
#include <memory>

namespace sad {
namespace ui {

// تصريح مسبق
class IRNode;

// ═══════════════════════════════════════════════════════════════════════════════
// اتجاه التخطيط (Layout Direction)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief اتجاه التخطيط الأساسي
 *
 * يحدد اتجاه الكتابة والتخطيط:
 * - RTL (من اليمين لليسار) — الافتراضي للعربية
 * - LTR (من اليسار لليمين) — للاتينية
 */
enum class LayoutDirection : uint8_t {
    RTL,    ///< من اليمين لليسار (الافتراضي)
    LTR     ///< من اليسار لليمين
};

// ═══════════════════════════════════════════════════════════════════════════════
// القيود (Layout Constraints)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief قيود الحجم لعنصر واحد
 *
 * تُمرر من العنصر الأب لتحديد الحد الأدنى والأقصى لحجم الابن.
 * مشابهة لـ BoxConstraints في Flutter أو ProposedSize في SwiftUI.
 */
struct LayoutConstraints {
    float minWidth  = 0.0f;         ///< أدنى عرض مسموح
    float minHeight = 0.0f;         ///< أدنى ارتفاع مسموح
    float maxWidth  = 1e6f;         ///< أقصى عرض مسموح
    float maxHeight = 1e6f;         ///< أقصى ارتفاع مسموح

    /// هل العرض ثابت (min == max)؟
    bool isWidthFixed() const { return minWidth == maxWidth; }

    /// هل الارتفاع ثابت؟
    bool isHeightFixed() const { return minHeight == maxHeight; }

    /// تطبيق حد على قيمة (clamp)
    float clampWidth(float w) const;
    float clampHeight(float h) const;
};

// ═══════════════════════════════════════════════════════════════════════════════
// حجم مقاس (Measured Size)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief نتيجة القياس — الحجم الذي يحتاجه العنصر
 */
struct MeasuredSize {
    float width  = 0.0f;    ///< العرض المقاس
    float height = 0.0f;    ///< الارتفاع المقاس
};

// ═══════════════════════════════════════════════════════════════════════════════
// مستطيل التخطيط (Layout Rect)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief الموقع والحجم النهائي لعنصر على الشاشة
 *
 * بعد مرحلة التوزيع، كل عنصر يحصل على LayoutRect يحدد
 * أين يُرسم بالضبط (بالنسبة للعنصر الأب).
 */
struct LayoutRect {
    float x = 0.0f;         ///< الموقع الأفقي (من يمين الأب في RTL)
    float y = 0.0f;         ///< الموقع العمودي (من أعلى الأب)
    float width  = 0.0f;    ///< العرض النهائي
    float height = 0.0f;    ///< الارتفاع النهائي

    /// هل نقطة (px, py) داخل هذا المستطيل؟
    bool contains(float px, float py) const {
        return px >= x && px <= x + width &&
               py >= y && py <= y + height;
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// نتيجة التخطيط (Layout Result)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief نتيجة التخطيط لعقدة واحدة وكل أبنائها
 *
 * شجرة متوازية مع شجرة IR تحتوي على الموقع والحجم لكل عنصر.
 */
struct LayoutResult {
    LayoutRect rect;                                    ///< موقع وحجم هذا العنصر
    float contentHeight = 0.0f;                         ///< ارتفاع المحتوى الكامل (للعناصر القابلة للتمرير)
    std::vector<std::shared_ptr<LayoutResult>> children; ///< نتائج الأبناء
};

// ═══════════════════════════════════════════════════════════════════════════════
// محرك التخطيط (Layout Engine)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief محرك حساب التخطيط — يحدد موقع وحجم كل عنصر
 *
 * الاستخدام:
 * @code
 *   LayoutEngine engine;
 *   engine.setDirection(LayoutDirection::RTL); // العربية
 *   engine.setViewportSize(1920, 1080);
 *
 *   auto result = engine.layout(irRoot, constraints);
 *   // result->rect يحتوي على موقع وحجم الجذر
 *   // result->children[0]->rect يحتوي على الابن الأول
 * @endcode
 */
class LayoutEngine {
public:
    LayoutEngine();

    /// تعيين اتجاه التخطيط (RTL للعربية)
    void setDirection(LayoutDirection dir) { direction_ = dir; }
    LayoutDirection getDirection() const { return direction_; }

    /// تعيين حجم نافذة العرض (viewport)
    void setViewportSize(float width, float height) {
        viewportWidth_ = width;
        viewportHeight_ = height;
    }

    /// تعيين كثافة الشاشة (لتحويل dp إلى px)
    void setScreenDensity(float density) { screenDensity_ = density; }

    /**
     * @brief حساب التخطيط لشجرة IR كاملة
     *
     * يُجري مرحلتي القياس والتوزيع ويعيد شجرة LayoutResult.
     *
     * @param root جذر شجرة IR
     * @param constraints قيود الحجم (عادة حجم الشاشة)
     * @return شجرة نتائج التخطيط
     */
    std::shared_ptr<LayoutResult> layout(
        const std::shared_ptr<IRNode>& root,
        const LayoutConstraints& constraints
    );

private:
    // ─── المرحلة الأولى: القياس ──────────

    /// قياس حجم عقدة واحدة (تكرار من الأسفل للأعلى)
    MeasuredSize measure(const IRNode& node, const LayoutConstraints& constraints);

    /// قياس عمود (Column) — الأبناء فوق بعض عمودياً
    MeasuredSize measureColumn(const IRNode& node, const LayoutConstraints& constraints);

    /// قياس صف (Row) — الأبناء جنب بعض أفقياً
    MeasuredSize measureRow(const IRNode& node, const LayoutConstraints& constraints);

    /// قياس عنصر ورقي (نص، صورة...)
    MeasuredSize measureLeaf(const IRNode& node, const LayoutConstraints& constraints);

    /// قياس شبكة (Grid) — الأبناء في صفوف وأعمدة
    MeasuredSize measureGrid(const IRNode& node, const LayoutConstraints& constraints);

    /// قياس مكدس (Stack) — الأبناء متراكبين
    MeasuredSize measureStack(const IRNode& node, const LayoutConstraints& constraints);

    /// قياس التفاف (Wrap) — الأبناء يلتفون للسطر التالي
    MeasuredSize measureWrap(const IRNode& node, const LayoutConstraints& constraints);

    /// قياس عنصر قابل للتمرير (ScrollView/LazyColumn/List) — أبناء بارتفاع غير محدود
    MeasuredSize measureScrollView(const IRNode& node, const LayoutConstraints& constraints);

    // ─── المرحلة الثانية: التوزيع ────────

    /// توزيع المساحة على الأبناء وتحديد مواقعهم
    std::shared_ptr<LayoutResult> arrange(
        const IRNode& node,
        const LayoutConstraints& constraints,
        float offsetX, float offsetY
    );

    // ─── أدوات مساعدة ────────────────────

    /// تحويل أبعاد dp إلى بكسل
    float dpToPx(float dp) const { return dp * screenDensity_; }

    /// تحويل بكسل إلى dp
    float pxToDp(float px) const { return px / screenDensity_; }

    // ─── البيانات ────────────────────────

    LayoutDirection direction_ = LayoutDirection::RTL;  ///< اتجاه التخطيط
    float viewportWidth_  = 0.0f;   ///< عرض نافذة العرض
    float viewportHeight_ = 0.0f;   ///< ارتفاع نافذة العرض
    float screenDensity_  = 1.0f;   ///< كثافة الشاشة (px/dp)
};

} // namespace ui
} // namespace sad

#endif // SAD_UI_LAYOUT_H
