/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: layout.cpp
 * المسار: features/graphics/core/src/layout.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * تنفيذ محرك التخطيط (Layout Engine) — خوارزمية Flexbox المبسطة.
 *
 * خوارزمية التخطيط تعمل بمرحلتين:
 * ──────────────────────────────────
 * المرحلة 1 — القياس (Measure): من الأوراق إلى الجذر
 *   - كل عنصر ورقي يُحسب حجمه الطبيعي
 *   - كل حاوي يجمع أحجام أبنائه
 *
 * المرحلة 2 — التوزيع (Arrange): من الجذر إلى الأوراق
 *   - كل حاوي يوزع المساحة المتاحة على أبنائه
 *   - تُطبّق المحاذاة والحشوة
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/layout.h"
#include "sad_ui/ir.h"
#include "sad_ui/prop_keys.h" // (AR) SoT مفتاح الحشو (props::PADDING = «حشوة»)

#include <algorithm>
#include <numeric>

// (FR-009) قراءة خاصّيّة عدديّة (double أو int64) من عقدة، بقيمة افتراضيّة.
//   تُستعمل لإزاحة الموضع الحرّ (إزاحة_س/إزاحة_ص) في arrange.
static float readNumericProp(const sad::ui::IRNode& node,
                             const std::string& key, float fallback) {
    const auto* p = node.findProperty(key);
    if (!p) return fallback;
    if (auto* v = std::get_if<double>(&p->value)) return static_cast<float>(*v);
    if (auto* vi = std::get_if<int64_t>(&p->value)) return static_cast<float>(*vi);
    return fallback;
}

// (FR-009) الحاويات القابلة للتمرير تُفسّر إزاحة_س/إزاحة_ص كإزاحة تمرير (تُطبَّق على
//   الأبناء وقت الرسم)، لا كموضع حرّ — فتُستثنى من إزاحة الموضع الحرّ في arrange.
//   التصنيف نفسه في types.cpp — مصدر واحد لتسعة مواضع كانت تكتبه حرفيًّا.
using sad::ui::isScrollableType;

// حساب عدد أحرف UTF-8 الفعلية (ليس البايتات)
static size_t utf8CharCount(const std::string& text) {
    size_t count = 0;
    for (size_t i = 0; i < text.size(); ++count) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) { i += 1; }
        else if ((c & 0xE0) == 0xC0) { i += 2; }
        else if ((c & 0xF0) == 0xE0) { i += 3; }
        else if ((c & 0xF8) == 0xF0) { i += 4; }
        else { i += 1; }
    }
    return count;
}

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// تنفيذ LayoutConstraints
// ═══════════════════════════════════════════════════════════════════════════════

float LayoutConstraints::clampWidth(float w) const {
    return std::max(minWidth, std::min(w, maxWidth));
}

float LayoutConstraints::clampHeight(float h) const {
    return std::max(minHeight, std::min(h, maxHeight));
}

// ═══════════════════════════════════════════════════════════════════════════════
// تنفيذ LayoutEngine
// ═══════════════════════════════════════════════════════════════════════════════

LayoutEngine::LayoutEngine() = default;

std::shared_ptr<LayoutResult> LayoutEngine::layout(
    const std::shared_ptr<IRNode>& root,
    const LayoutConstraints& constraints
) {
    if (!root) return nullptr;

    // مرحلة واحدة مدمجة: قياس + توزيع
    return arrange(*root, constraints, 0.0f, 0.0f);
}

// ─── القياس ──────────────────────────────────

MeasuredSize LayoutEngine::measure(
    const IRNode& node,
    const LayoutConstraints& constraints
) {
    // تحقق من وجود عرض/ارتفاع ثابت
    const auto* widthProp  = node.findProperty(props::WIDTH);
    const auto* heightProp = node.findProperty(props::HEIGHT);

    LayoutConstraints adjusted = constraints;

    if (widthProp) {
        if (auto* v = std::get_if<double>(&widthProp->value)) {
            float w = static_cast<float>(*v);
            adjusted.minWidth = adjusted.maxWidth = w;
        } else if (auto* vi = std::get_if<int64_t>(&widthProp->value)) {
            float w = static_cast<float>(*vi);
            adjusted.minWidth = adjusted.maxWidth = w;
        }
    }
    if (heightProp) {
        if (auto* v = std::get_if<double>(&heightProp->value)) {
            float h = static_cast<float>(*v);
            adjusted.minHeight = adjusted.maxHeight = h;
        } else if (auto* vi = std::get_if<int64_t>(&heightProp->value)) {
            float h = static_cast<float>(*vi);
            adjusted.minHeight = adjusted.maxHeight = h;
        }
    }

    // حسب نوع العنصر
    switch (node.getType()) {
        case UINodeType::Column:
            return measureColumn(node, adjusted);

        case UINodeType::Row:
            return measureRow(node, adjusted);

        case UINodeType::Grid:
            return measureGrid(node, adjusted);

        case UINodeType::Stack:
            return measureStack(node, adjusted);

        case UINodeType::Wrap:
            return measureWrap(node, adjusted);

        case UINodeType::ScrollView:
        case UINodeType::LazyColumn:
        case UINodeType::List:
        case UINodeType::ListView:
            return measureScrollView(node, adjusted);

        default:
            // عنصر ورقي أو حاوي بسيط
            if (node.childCount() == 0) {
                return measureLeaf(node, adjusted);
            }
            // حاوي بأبناء — يتصرف كعمود افتراضياً
            return measureColumn(node, adjusted);
    }
}

MeasuredSize LayoutEngine::measureColumn(
    const IRNode& node,
    const LayoutConstraints& constraints
) {
    // عمود: الأبناء فوق بعض عمودياً
    // العرض = أكبر عرض من الأبناء
    // الارتفاع = مجموع ارتفاعات الأبناء + تباعد

    float totalHeight = 0.0f;
    float maxWidth = 0.0f;

    // التباعد بين الأبناء
    const auto* spacingProp = node.findProperty(props::SPACING);
    float spacing = 0.0f;
    if (spacingProp) {
        if (auto* v = std::get_if<double>(&spacingProp->value))
            spacing = static_cast<float>(*v);
        else if (auto* vi = std::get_if<int64_t>(&spacingProp->value))
            spacing = static_cast<float>(*vi);
    }

    // قراءة الحشو (padding)
    const auto* paddingProp = node.findProperty(props::PADDING); // SoT: «حشوة»
    float padding = 0.0f;
    if (paddingProp) {
        if (auto* v = std::get_if<double>(&paddingProp->value))
            padding = static_cast<float>(*v);
        else if (auto* vi = std::get_if<int64_t>(&paddingProp->value))
            padding = static_cast<float>(*vi);
    }

    // قراءة الهامش (margin)
    const auto* marginProp = node.findProperty(props::MARGIN);
    float margin = 0.0f;
    if (marginProp) {
        if (auto* v = std::get_if<double>(&marginProp->value))
            margin = static_cast<float>(*v);
        else if (auto* vi = std::get_if<int64_t>(&marginProp->value))
            margin = static_cast<float>(*vi);
    }

    for (size_t i = 0; i < node.childCount(); ++i) {
        const auto& child = node.getChildren()[i];
        auto childSize = measure(*child, constraints);
        maxWidth = std::max(maxWidth, childSize.width);
        totalHeight += childSize.height;
        if (i > 0) totalHeight += spacing;
    }

    // إضافة الحشو والهامش
    totalHeight += padding * 2;
    maxWidth += padding * 2;

    return {
        constraints.clampWidth(maxWidth + margin * 2),
        constraints.clampHeight(totalHeight + margin * 2)
    };
}

MeasuredSize LayoutEngine::measureRow(
    const IRNode& node,
    const LayoutConstraints& constraints
) {
    // صف: الأبناء جنب بعض أفقياً
    // العرض = مجموع عروض الأبناء + تباعد
    // الارتفاع = أكبر ارتفاع من الأبناء

    float totalWidth = 0.0f;
    float maxHeight = 0.0f;

    const auto* spacingProp = node.findProperty(props::SPACING);
    float spacing = 0.0f;
    if (spacingProp) {
        if (auto* v = std::get_if<double>(&spacingProp->value))
            spacing = static_cast<float>(*v);
        else if (auto* vi = std::get_if<int64_t>(&spacingProp->value))
            spacing = static_cast<float>(*vi);
    }

    // قراءة الحشو (padding)
    const auto* paddingProp = node.findProperty(props::PADDING); // SoT: «حشوة»
    float padding = 0.0f;
    if (paddingProp) {
        if (auto* v = std::get_if<double>(&paddingProp->value))
            padding = static_cast<float>(*v);
        else if (auto* vi = std::get_if<int64_t>(&paddingProp->value))
            padding = static_cast<float>(*vi);
    }

    // قراءة الهامش (margin)
    const auto* marginProp = node.findProperty(props::MARGIN);
    float margin = 0.0f;
    if (marginProp) {
        if (auto* v = std::get_if<double>(&marginProp->value))
            margin = static_cast<float>(*v);
        else if (auto* vi = std::get_if<int64_t>(&marginProp->value))
            margin = static_cast<float>(*vi);
    }

    for (size_t i = 0; i < node.childCount(); ++i) {
        const auto& child = node.getChildren()[i];
        auto childSize = measure(*child, constraints);
        totalWidth += childSize.width;
        maxHeight = std::max(maxHeight, childSize.height);
        if (i > 0) totalWidth += spacing;
    }

    // إضافة الحشو والهامش
    totalWidth += padding * 2;
    maxHeight += padding * 2;

    return {
        constraints.clampWidth(totalWidth + margin * 2),
        constraints.clampHeight(maxHeight + margin * 2)
    };
}

MeasuredSize LayoutEngine::measureLeaf(
    const IRNode& node,
    const LayoutConstraints& constraints
) {
    // عنصر ورقي (نص، صورة، فاصل...)
    // الأحجام الافتراضية تعتمد على النوع

    float defaultWidth = 0.0f;
    float defaultHeight = 0.0f;

    switch (node.getType()) {
        case UINodeType::Text: {
            // النص: حجم تقريبي بناءً على طول النص
            const auto* textProp = node.findProperty(props::TEXT_LATIN);
            if (!textProp) textProp = node.findProperty(props::CONTENT);
            if (!textProp) textProp = node.findProperty(props::TEXT);
            if (textProp) {
                if (auto* text = std::get_if<std::string>(&textProp->value)) {
                    // حجم الخط يؤثّر على العرض والارتفاع معًا (كان العرض ثابتًا
                    // 10px/حرف بلا مراعاة الحجم ⇒ عنوان كبير يُقاس أضيق من رسمه
                    // فتنحرف محاذاة RTL؛ العامل 0.625 يُبقي الخطّ الافتراضيّ 16px
                    // عند 10px/حرف — راجع layout-engine-rtl-cross-axis).
                    float fontSize = 16.0f;
                    const auto* sizeProp = node.findProperty(props::FONT_SIZE);
                    if (!sizeProp) sizeProp = node.findProperty(props::FONT_SIZE_ALT);
                    if (!sizeProp) sizeProp = node.findProperty(props::SIZE);
                    if (sizeProp) {
                        if (auto* v = std::get_if<double>(&sizeProp->value))
                            fontSize = static_cast<float>(*v);
                        else if (auto* vi = std::get_if<int64_t>(&sizeProp->value))
                            fontSize = static_cast<float>(*vi);
                    }
                    // عرض ≈ عدد الأحرف الفعليّ (لا البايتات) × الحجم × 0.625
                    defaultWidth = static_cast<float>(utf8CharCount(*text)) * fontSize * 0.625f;
                    defaultHeight = fontSize * 1.5f;
                }
            }
            if (defaultWidth == 0.0f) defaultWidth = 50.0f;
            if (defaultHeight == 0.0f) defaultHeight = 20.0f;
            break;
        }

        case UINodeType::Button:
            defaultWidth = 120.0f;
            defaultHeight = 48.0f;
            break;

        case UINodeType::Image:
            defaultWidth = 100.0f;
            defaultHeight = 100.0f;
            break;

        case UINodeType::Spacer:
            defaultWidth = 0.0f;
            defaultHeight = 8.0f;
            break;

        case UINodeType::Divider:
            defaultWidth = constraints.maxWidth;
            defaultHeight = 1.0f;
            break;

        case UINodeType::ProgressBar:
            defaultWidth = constraints.maxWidth > 0 ? constraints.maxWidth : 200.0f;
            defaultHeight = 24.0f;
            break;

        case UINodeType::TitleBar:
            // شريط عنوان يملأ عرض حاويته بارتفاع ثابت (م١ قشرة سطح المكتب)
            defaultWidth = constraints.maxWidth < 1e6f ? constraints.maxWidth : 320.0f;
            defaultHeight = 36.0f;
            break;

        case UINodeType::ScrollBar:
            // شريط تمرير رأسيّ نحيل يملأ ارتفاع حاويته
            defaultWidth = 12.0f;
            defaultHeight = constraints.maxHeight < 1e6f ? constraints.maxHeight : 200.0f;
            break;

        case UINodeType::Taskbar:
            // شريط مهامّ يملأ عرض الشاشة بارتفاع ثابت (م٢ صدَفة سطح المكتب)
            defaultWidth = constraints.maxWidth < 1e6f ? constraints.maxWidth : 320.0f;
            defaultHeight = 48.0f;
            break;

        case UINodeType::SystemTray:
            // لوحة نظام مدمجة (ساعة + أيقونات)
            defaultWidth = 140.0f;
            defaultHeight = constraints.maxHeight < 1e6f ? constraints.maxHeight : 40.0f;
            break;

        case UINodeType::StartMenu:
            // لوح قائمة ابدأ منبثق (حجم معقول حين لا أبناء)
            defaultWidth = 320.0f;
            defaultHeight = 420.0f;
            break;

        case UINodeType::Toolbar:
            // شريط أدوات يملأ عرض حاويته بارتفاع مضغوط (م٣ قوائم وأوامر)
            defaultWidth = constraints.maxWidth < 1e6f ? constraints.maxWidth : 320.0f;
            defaultHeight = 44.0f;
            break;

        case UINodeType::SegmentedControl:
            // تحكّم مقسّم مدمج
            defaultWidth = 240.0f;
            defaultHeight = 34.0f;
            break;

        case UINodeType::Menu:
            // قائمة منبثقة (حجم معقول حين لا أبناء)
            defaultWidth = 200.0f;
            defaultHeight = 180.0f;
            break;

        case UINodeType::TreeView:
            // لوح تصفّح شجريّ جانبيّ (م٤ عروض المستكشف)
            defaultWidth = 260.0f;
            defaultHeight = constraints.maxHeight < 1e6f ? constraints.maxHeight : 400.0f;
            break;

        case UINodeType::SplitView:
            // لوح سيّد/تفصيل يملأ حاويته (م٤ عروض المستكشف)
            defaultWidth = constraints.maxWidth < 1e6f ? constraints.maxWidth : 480.0f;
            defaultHeight = constraints.maxHeight < 1e6f ? constraints.maxHeight : 360.0f;
            break;

        case UINodeType::DataTable:
            // جدول بيانات يملأ عرض حاويته بارتفاع معقول (م٤ عروض المستكشف)
            defaultWidth = constraints.maxWidth < 1e6f ? constraints.maxWidth : 480.0f;
            defaultHeight = constraints.maxHeight < 1e6f ? constraints.maxHeight : 300.0f;
            break;

        case UINodeType::Breadcrumb:
            // مسار تنقّل هرميّ يملأ العرض بارتفاع سطر واحد (م٤ عروض المستكشف)
            defaultWidth = constraints.maxWidth < 1e6f ? constraints.maxWidth : 320.0f;
            defaultHeight = 34.0f;
            break;

        case UINodeType::Picker:
            // قائمة منسدلة (م٥ إدخال متقدّم)
            defaultWidth = 200.0f;
            defaultHeight = 40.0f;
            break;

        case UINodeType::DatePicker:
            // منتقي تاريخ (م٥ إدخال متقدّم)
            defaultWidth = 200.0f;
            defaultHeight = 40.0f;
            break;

        case UINodeType::Stepper:
            // خطوات [−] قيمة [+] (م٥ إدخال متقدّم)
            defaultWidth = 140.0f;
            defaultHeight = 40.0f;
            break;

        case UINodeType::RatingBar:
            // شريط تقييم بنجوم (م٥ إدخال متقدّم)
            defaultWidth = 160.0f;
            defaultHeight = 32.0f;
            break;

        case UINodeType::Alert:
            // لوح تنبيه/تغذية راجعة (م٦)
            defaultWidth = 360.0f;
            defaultHeight = 120.0f;
            break;

        case UINodeType::Badge:
            // شارة إشعار صغيرة (م٦)
            defaultWidth = 24.0f;
            defaultHeight = 24.0f;
            break;

        case UINodeType::Chip:
            // رقاقة اختيار (م٦)
            defaultWidth = 96.0f;
            defaultHeight = 32.0f;
            break;

        case UINodeType::Avatar:
            // صورة رمزيّة دائريّة (م٦)
            defaultWidth = 44.0f;
            defaultHeight = 44.0f;
            break;

        case UINodeType::Radio:
            // زرّ راديو (م٦)
            defaultWidth = 24.0f;
            defaultHeight = 24.0f;
            break;

        case UINodeType::SpinBox:
            // حقل رقميّ بأزرار زيادة/نقصان (م٥-ب تحكّم متقدّم)
            defaultWidth = 120.0f;
            defaultHeight = 40.0f;
            break;

        case UINodeType::GroupBox:
            // إطار مجموعة بعنوان يملأ حاويته (م٦-ب تغذية راجعة)
            defaultWidth = constraints.maxWidth < 1e6f ? constraints.maxWidth : 280.0f;
            defaultHeight = constraints.maxHeight < 1e6f ? constraints.maxHeight : 160.0f;
            break;

        case UINodeType::Spinner:
            // مؤشّر انشغال دائريّ مربّع الأبعاد (م٦-ب تغذية راجعة)
            defaultWidth = 48.0f;
            defaultHeight = 48.0f;
            break;

        case UINodeType::StatusBar:
            // شريط حالة سفليّ يملأ عرض النافذة بارتفاع مضغوط (م٦-ب تغذية راجعة)
            defaultWidth = constraints.maxWidth < 1e6f ? constraints.maxWidth : 320.0f;
            defaultHeight = 26.0f;
            break;

        // ملاحظة: Skeleton يبقى على الافتراضيّ ٥٠×٥٠ عمدًا — يقنّنه الاختبار الذهبيّ
        // skeleton_hgradient (test_ui_render_displaylist.cpp)؛ حجمه الفعليّ يُضبَط
        // بخصائص عرض/ارتفاع صريحة عند الاستعمال (كهيكل سطر تحميل يملأ حاويته).

        default:
            defaultWidth = 50.0f;
            defaultHeight = 50.0f;
            break;
    }

    return {
        constraints.clampWidth(defaultWidth),
        constraints.clampHeight(defaultHeight)
    };
}

// ─── قياس الشبكة (Grid) ──────────────────────────────────

MeasuredSize LayoutEngine::measureGrid(
    const IRNode& node,
    const LayoutConstraints& constraints
) {
    // شبكة: الأبناء يوزعون في صفوف وأعمدة
    const auto* colsProp = node.findProperty(props::COLUMNS);
    if (!colsProp) colsProp = node.findProperty(props::COLUMNS_LATIN);
    int cols = 2; // افتراضي
    if (colsProp) {
        if (auto* v = std::get_if<int64_t>(&colsProp->value)) cols = static_cast<int>(*v);
        else if (auto* v = std::get_if<double>(&colsProp->value)) cols = static_cast<int>(*v);
    }
    if (cols < 1) cols = 1;

    const auto* spacingProp = node.findProperty(props::SPACING);
    float spacing = 8.0f;
    if (spacingProp) {
        if (auto* v = std::get_if<double>(&spacingProp->value)) spacing = static_cast<float>(*v);
        else if (auto* vi = std::get_if<int64_t>(&spacingProp->value)) spacing = static_cast<float>(*vi);
    }

    const auto* paddingProp = node.findProperty(props::PADDING); // SoT: «حشوة»
    float padding = 0.0f;
    if (paddingProp) {
        if (auto* v = std::get_if<double>(&paddingProp->value)) padding = static_cast<float>(*v);
        else if (auto* vi = std::get_if<int64_t>(&paddingProp->value)) padding = static_cast<float>(*vi);
    }

    float cellW = (constraints.maxWidth - padding * 2 - spacing * (cols - 1)) / cols;
    LayoutConstraints cellConstraints = constraints;
    cellConstraints.maxWidth = std::max(cellW, 0.0f);

    int rows = (static_cast<int>(node.childCount()) + cols - 1) / cols;
    float maxRowH = 0.0f;
    float totalH = padding * 2;

    for (int r = 0; r < rows; ++r) {
        float rowH = 0.0f;
        for (int c = 0; c < cols; ++c) {
            size_t idx = r * cols + c;
            if (idx >= node.childCount()) break;
            auto childSize = measure(*node.getChildren()[idx], cellConstraints);
            rowH = std::max(rowH, childSize.height);
        }
        totalH += rowH;
        if (r > 0) totalH += spacing;
    }

    return {
        constraints.clampWidth(constraints.maxWidth),
        constraints.clampHeight(totalH)
    };
}

// ─── قياس المكدس (Stack) ──────────────────────────────────

MeasuredSize LayoutEngine::measureStack(
    const IRNode& node,
    const LayoutConstraints& constraints
) {
    // مكدس: الأبناء فوق بعض (متراكبين) — الحجم = أكبر ابن
    float maxW = 0.0f, maxH = 0.0f;
    for (const auto& child : node.getChildren()) {
        auto childSize = measure(*child, constraints);
        maxW = std::max(maxW, childSize.width);
        maxH = std::max(maxH, childSize.height);
    }
    return {
        constraints.clampWidth(maxW),
        constraints.clampHeight(maxH)
    };
}

// ─── قياس الالتفاف (Wrap) ──────────────────────────────────

MeasuredSize LayoutEngine::measureWrap(
    const IRNode& node,
    const LayoutConstraints& constraints
) {
    // التفاف: الأبناء يلتفون للسطر التالي عند نفاد المساحة الأفقية
    const auto* spacingProp = node.findProperty(props::SPACING);
    float spacing = 8.0f;
    if (spacingProp) {
        if (auto* v = std::get_if<double>(&spacingProp->value)) spacing = static_cast<float>(*v);
        else if (auto* vi = std::get_if<int64_t>(&spacingProp->value)) spacing = static_cast<float>(*vi);
    }

    float lineX = 0.0f, lineH = 0.0f, totalH = 0.0f;
    float maxW = constraints.maxWidth;

    for (const auto& child : node.getChildren()) {
        auto childSize = measure(*child, constraints);
        if (lineX + childSize.width > maxW && lineX > 0) {
            // سطر جديد
            totalH += lineH + spacing;
            lineX = 0.0f;
            lineH = 0.0f;
        }
        lineX += childSize.width + spacing;
        lineH = std::max(lineH, childSize.height);
    }
    totalH += lineH; // السطر الأخير

    return {
        constraints.clampWidth(maxW),
        constraints.clampHeight(totalH)
    };
}

// ─── قياس عنصر قابل للتمرير (ScrollView) ──────────────────────────────────

MeasuredSize LayoutEngine::measureScrollView(
    const IRNode& node,
    const LayoutConstraints& constraints
) {
    // ScrollView: الحاوية تحترم القيود، لكن الأبناء يُقاسون بارتفاع غير محدود
    LayoutConstraints unbounded = constraints;
    unbounded.maxHeight = 1e6f;
    unbounded.minHeight = 0.0f;

    float totalHeight = 0.0f;
    float maxWidth = 0.0f;

    const auto* spacingProp = node.findProperty(props::SPACING);
    float spacing = 0.0f;
    if (spacingProp) {
        if (auto* v = std::get_if<double>(&spacingProp->value))
            spacing = static_cast<float>(*v);
        else if (auto* vi = std::get_if<int64_t>(&spacingProp->value))
            spacing = static_cast<float>(*vi);
    }

    const auto* paddingProp = node.findProperty(props::PADDING); // SoT: «حشوة»
    float padding = 0.0f;
    if (paddingProp) {
        if (auto* v = std::get_if<double>(&paddingProp->value))
            padding = static_cast<float>(*v);
        else if (auto* vi = std::get_if<int64_t>(&paddingProp->value))
            padding = static_cast<float>(*vi);
    }

    for (size_t i = 0; i < node.childCount(); ++i) {
        const auto& child = node.getChildren()[i];
        auto childSize = measure(*child, unbounded);
        maxWidth = std::max(maxWidth, childSize.width);
        totalHeight += childSize.height;
        if (i > 0) totalHeight += spacing;
    }

    totalHeight += padding * 2;
    maxWidth += padding * 2;

    // الحاوية تُقيّد بالقيود الأصلية (حجم الشاشة)
    // لكن ارتفاع المحتوى الحقيقي يُحفظ في LayoutResult.contentHeight
    return {
        constraints.clampWidth(maxWidth),
        constraints.clampHeight(totalHeight)
    };
}

// ─── التوزيع ──────────────────────────────────

std::shared_ptr<LayoutResult> LayoutEngine::arrange(
    const IRNode& node,
    const LayoutConstraints& constraints,
    float offsetX, float offsetY
) {
    auto result = std::make_shared<LayoutResult>();

    // (FR-009) إزاحة الموضع الحرّ (إزاحة_س/إزاحة_ص) — تُزيح العقدة وكامل شجرتها معًا.
    //   نضيفها إلى offsetX/offsetY فتتدفّق إلى مستطيل العقدة *وإلى* مواضع كلّ أبنائها
    //   (المحسوبة من offsetX/offsetY)، فيبقى hitTest متّسقًا مع الرسم. محصورة بغير
    //   الحاويات القابلة للتمرير (حيث تعني تلك المفاتيح إزاحة تمرير لا موضعًا حرًّا).
    if (!isScrollableType(node.getType())) {
        offsetX += readNumericProp(node, props::OFFSET_X, 0.0f); // إزاحة_س
        offsetY += readNumericProp(node, props::OFFSET_Y, 0.0f); // إزاحة_ص
    }

    // قياس الحجم المطلوب
    auto measured = measure(node, constraints);
    result->rect.x = offsetX;
    result->rect.y = offsetY;
    result->rect.width = measured.width;
    result->rect.height = measured.height;

    // توزيع الأبناء
    if (node.childCount() == 0) return result;

    const auto* spacingProp = node.findProperty(props::SPACING);
    float spacing = 0.0f;
    if (spacingProp) {
        if (auto* v = std::get_if<double>(&spacingProp->value))
            spacing = static_cast<float>(*v);
        else if (auto* vi = std::get_if<int64_t>(&spacingProp->value))
            spacing = static_cast<float>(*vi);
    }

    // قراءة الحشو (padding) — إزاحة داخلية للأبناء
    const auto* paddingProp = node.findProperty(props::PADDING); // SoT: «حشوة»
    float padding = 0.0f;
    if (paddingProp) {
        if (auto* v = std::get_if<double>(&paddingProp->value))
            padding = static_cast<float>(*v);
        else if (auto* vi = std::get_if<int64_t>(&paddingProp->value))
            padding = static_cast<float>(*vi);
    }

    // قراءة الهامش (margin) — إزاحة خارجية
    const auto* marginProp = node.findProperty(props::MARGIN);
    float margin = 0.0f;
    if (marginProp) {
        if (auto* v = std::get_if<double>(&marginProp->value))
            margin = static_cast<float>(*v);
        else if (auto* vi = std::get_if<int64_t>(&marginProp->value))
            margin = static_cast<float>(*vi);
    }

    // تعديل القيود المتاحة للأبناء بعد خصم الحشو **والهامش** (إقحام المحتوى):
    // الهامش إزاحة خارجيّة تُقلّص منطقة المحتوى، فالابن مالئ-المحور/الموزون يجب أن
    // يرى عرض/ارتفاع المحتوى الحقيقيّ لا الحاوية كاملة (وإلّا تجاوز فجوة الهامش).
    LayoutConstraints childConstraints = constraints;

    // (AR) سقفُ عرض الابن هو عرضُ **هذه** العقدة الصريح، لا عرضُ أبيها.
    //   كان `childConstraints = constraints` يمرّر قيدَ الأب كما هو، فيرث الابنُ
    //   المالئُ للعرض عرضَ الجذر لا عرضَ حاويته. مقيسٌ على fb0 (١٠٢٤ عرضًا،
    //   حاوية بـ«عرض(٦٠٠)»):
    //     • «فاصل_خط» (defaultWidth = constraints.maxWidth) رسم خطًّا من س=١٦
    //       إلى س=١٠٢٣ — عبر الشاشة كلّها لا داخل حاويته.
    //     • «شريط_تقدم» (المنطق نفسه) فاض ٩٣٦١ بكسلًا خارج حاويته.
    //     • «التفاف» في RTL يبدأ من `measured.width - padding`، وقد صار ١٠٢٤،
    //       فوُضِع ابنُه عند س≈١٠٠٤ ⇒ خارج الحاوية وخارج الشاشة تقريبًا، فبدا
    //       أنّ الحاوية «ابتلعت» ابنها وهي إنّما دفعته بعيدًا.
    //   القصرُ على وجود «عرض» صريح يُبقي أثرَ التغيير محصورًا: عقدةٌ بلا عرضٍ
    //   صريح قياسُها من قيد أبيها أصلًا فلا جديد. ونستثني القابلَ للتمرير أفقيًّا
    //   لأنّ محتواه يتجاوز عرضَه بحكم التعريف.
    if (!isScrollableType(node.getType())) {
        const auto *wProp = node.findProperty(props::WIDTH);
        if (!wProp) wProp = node.findProperty(props::WIDTH_LATIN);
        if (wProp) {
            float w = -1.0f;
            if (auto *v = std::get_if<double>(&wProp->value)) w = static_cast<float>(*v);
            else if (auto *vi = std::get_if<int64_t>(&wProp->value)) w = static_cast<float>(*vi);
            if (w >= 0.0f)
                childConstraints.maxWidth = std::min(childConstraints.maxWidth, w);
        }
    }

    const float inset = padding + margin;
    if (inset > 0) {
        childConstraints.maxWidth = std::max(0.0f, childConstraints.maxWidth - inset * 2);
        childConstraints.maxHeight = std::max(0.0f, childConstraints.maxHeight - inset * 2);
    }

    // (AR) هذا الفرع توزيعٌ **رأسيّ** (عمود بارتفاعٍ غير محدود)، فيقتصر على
    //   الأنواع القابلة للتمرير رأسيًّا. لا يشمل LazyRow (تمرير أفقيّ) عمدًا —
    //   إدراجه كان سيوزّع أبناءه عمودًا بارتفاعٍ غير محدود، وهو خطأ اتّجاه.
    //   isScrollableType الموحَّد (كلّ قابلٍ للتمرير) يخصّ دلالة الإزاحة والرسم
    //   ثنائيَّي المحور، لا اتّجاه التوزيع هنا.
    const auto arrangeType = node.getType();
    if (arrangeType == UINodeType::ScrollView ||
        arrangeType == UINodeType::LazyColumn ||
        arrangeType == UINodeType::List) {
        // ─── توزيع عنصر قابل للتمرير رأسيًّا (ScrollView/LazyColumn/List) ───
        // الأبناء يُوزعون كعمود لكن بارتفاع غير محدود
        LayoutConstraints scrollChildConstraints = childConstraints;
        scrollChildConstraints.maxHeight = 1e6f;
        scrollChildConstraints.minHeight = 0.0f;

        float childY = padding;
        for (size_t i = 0; i < node.childCount(); ++i) {
            const auto& child = node.getChildren()[i];
            auto childResult = arrange(*child, scrollChildConstraints,
                                        offsetX + padding, offsetY + childY);
            childY += childResult->rect.height;
            if (i < node.childCount() - 1) childY += spacing;
            result->children.push_back(childResult);
        }
        childY += padding;
        // حفظ ارتفاع المحتوى الكامل (قد يتجاوز ارتفاع الحاوية)
        result->contentHeight = childY;
    } else if (node.getType() == UINodeType::Grid) {
        // ─── توزيع شبكي (Grid) ───
        const auto* colsProp = node.findProperty(props::COLUMNS);
        if (!colsProp) colsProp = node.findProperty(props::COLUMNS_LATIN);
        int cols = 2;
        if (colsProp) {
            if (auto* v = std::get_if<int64_t>(&colsProp->value)) cols = static_cast<int>(*v);
            else if (auto* v = std::get_if<double>(&colsProp->value)) cols = static_cast<int>(*v);
        }
        if (cols < 1) cols = 1;

        float cellW = (childConstraints.maxWidth - spacing * (cols - 1)) / cols;
        LayoutConstraints cellConstraints2 = childConstraints;
        cellConstraints2.maxWidth = std::max(cellW, 0.0f);

        int rows = (static_cast<int>(node.childCount()) + cols - 1) / cols;
        float gridY = padding;

        for (int r = 0; r < rows; ++r) {
            float rowH = 0.0f;
            // أولاً: قياس ارتفاع الصف
            for (int c = 0; c < cols; ++c) {
                size_t idx = r * cols + c;
                if (idx >= node.childCount()) break;
                auto childSize = measure(*node.getChildren()[idx], cellConstraints2);
                rowH = std::max(rowH, childSize.height);
            }
            // ثانياً: توزيع الخلايا — في RTL تتدفّق الأعمدة من اليمين لليسار
            // (العمود 0 أقصى اليمين) بمرآة موقع الخليّة حول عرض الشبكة.
            for (int c = 0; c < cols; ++c) {
                size_t idx = r * cols + c;
                if (idx >= node.childCount()) break;
                // عكس ترتيب الأعمدة في RTL (العمود 0 أقصى اليمين) — مرآة دقيقة
                // لمواقع LTR بنفس cellW/التباعد/الحشوة، فلا تتولّد إحداثيّات سالبة.
                int col = (direction_ == LayoutDirection::RTL) ? (cols - 1 - c) : c;
                float cellX = padding + col * (cellW + spacing);
                auto childResult = arrange(*node.getChildren()[idx], cellConstraints2,
                                            offsetX + cellX, offsetY + gridY);
                result->children.push_back(childResult);
            }
            gridY += rowH + spacing;
        }
    } else if (node.getType() == UINodeType::Stack) {
        // ─── توزيع مكدس (Stack) — كل الأبناء في نفس الموقع ───
        // في RTL يُحاذى كلّ ابن أضيق من الأب إلى اليمين (نفس دلالة العمود المتقاطع).
        for (const auto& child : node.getChildren()) {
            float childX = padding;
            if (direction_ == LayoutDirection::RTL) {
                auto cs = measure(*child, childConstraints);
                float rightAligned = measured.width - padding - cs.width;
                if (rightAligned > childX) childX = rightAligned;
            }
            auto childResult = arrange(*child, childConstraints,
                                        offsetX + childX, offsetY + padding);
            result->children.push_back(childResult);
        }
    } else if (node.getType() == UINodeType::Wrap) {
        // ─── توزيع التفاف (Wrap) ───
        // في RTL تتدفّق العناصر من اليمين لليسار وتلتفّ عند بلوغ حشوة اليسار:
        // نتتبّع الحافّة اليمنى المتاحة (rightX) وننزل صفًّا عند عدم الاتّساع.
        if (direction_ == LayoutDirection::RTL) {
            float rightX = measured.width - padding, lineY = padding, lineH = 0.0f;
            for (const auto& child : node.getChildren()) {
                auto childSize = measure(*child, childConstraints);
                if (rightX - childSize.width < padding && rightX < measured.width - padding) {
                    lineY += lineH + spacing;
                    rightX = measured.width - padding;
                    lineH = 0.0f;
                }
                float childX = rightX - childSize.width;
                auto childResult = arrange(*child, childConstraints,
                                            offsetX + childX, offsetY + lineY);
                rightX -= childResult->rect.width + spacing;
                lineH = std::max(lineH, childResult->rect.height);
                result->children.push_back(childResult);
            }
        } else {
            float lineX = padding, lineY = padding, lineH = 0.0f;
            float maxWrapW = childConstraints.maxWidth;
            for (const auto& child : node.getChildren()) {
                auto childSize = measure(*child, childConstraints);
                if (lineX + childSize.width > maxWrapW + padding && lineX > padding) {
                    lineY += lineH + spacing;
                    lineX = padding;
                    lineH = 0.0f;
                }
                auto childResult = arrange(*child, childConstraints,
                                            offsetX + lineX, offsetY + lineY);
                lineX += childResult->rect.width + spacing;
                lineH = std::max(lineH, childResult->rect.height);
                result->children.push_back(childResult);
            }
        }
    } else if (node.getType() == UINodeType::Column ||
        (node.getType() != UINodeType::Row && node.childCount() > 0)) {
        // ─── توزيع عمودي (Column) مع دعم الوزن (flex weight) ───
        // المرحلة 1: قياس الأبناء العاديين وحساب الأوزان
        float fixedTotal = 0.0f;
        float totalWeight = 0.0f;
        struct ChildInfo {
            size_t index;
            float height;
            float weight;
            float width;   ///< العرض المقيس (للمحاذاة المتقاطعة في RTL)
        };
        std::vector<ChildInfo> infos;

        for (size_t i = 0; i < node.childCount(); ++i) {
            const auto& child = node.getChildren()[i];
            float weight = 0.0f;
            const auto* weightProp = child->findProperty(props::WEIGHT);
            if (!weightProp) weightProp = child->findProperty(props::FLEX_LATIN);
            if (weightProp) {
                if (auto* v = std::get_if<double>(&weightProp->value)) weight = static_cast<float>(*v);
                else if (auto* vi = std::get_if<int64_t>(&weightProp->value)) weight = static_cast<float>(*vi);
            }
            auto childSize = measure(*child, childConstraints);
            if (weight > 0) {
                totalWeight += weight;
                infos.push_back({i, 0.0f, weight, childSize.width});
            } else {
                fixedTotal += childSize.height;
                infos.push_back({i, childSize.height, 0.0f, childSize.width});
            }
            if (i > 0) fixedTotal += spacing;
        }

        // المرحلة 2: توزيع المساحة المتبقية حسب الأوزان
        // أساس توزيع الوزن: إن كان للعمود ارتفاع صريح فالتوزيع ضمنه (منقوصًا الحشو
        // والهامش)، وإلّا يتمدّد ليملأ القيد المنفذ (childConstraints المخصوم سلفًا).
        // childConstraints.maxHeight لا يُطرح منه padding ثانيةً (كان خصمًا مزدوجًا).
        float availBaseH = childConstraints.maxHeight;
        if (const auto* hP = node.findProperty(props::HEIGHT)) {
            float h = -1.0f;
            if (auto* v = std::get_if<double>(&hP->value)) h = static_cast<float>(*v);
            else if (auto* vi = std::get_if<int64_t>(&hP->value)) h = static_cast<float>(*vi);
            if (h >= 0.0f) availBaseH = std::max(0.0f, h - inset * 2);
        }
        float availableH = std::max(0.0f, availBaseH - fixedTotal);
        for (auto& info : infos) {
            if (info.weight > 0 && totalWeight > 0) {
                info.height = availableH * (info.weight / totalWeight);
            }
        }

        // المرحلة 3: ترتيب الأبناء
        // المحاذاة المتقاطعة (المحور الأفقيّ للعمود): «محاذاة» تحدّد الوضع —
        // «بداية» (افتراضيّ، اتّجاهيّة: RTL يمينًا، LTR يسارًا) · «وسط» · «نهاية».
        // بلا «محاذاة» يبقى السلوك الاتّجاهيّ (أصل «الشاشة تبدأ من الشمال»). «تمدّد»
        // يملأ الابن عرض المحتوى كاملًا (عبر قيد min=max فيتخطّط داخليًّا بالعرض
        // الكامل). الهامش (margin) يُقحِم المحتوى: [margin+padding، العرض−margin−padding].
        std::string cAlign;
        if (const auto* aP = node.findProperty(props::ALIGN))
            if (auto* s = std::get_if<std::string>(&aP->value)) cAlign = *s;
        const bool alignCenter  = (cAlign == "وسط" || cAlign == "center");
        const bool alignEnd     = (cAlign == "نهاية" || cAlign == "end");
        const bool alignStretch = (cAlign == "تمدّد" || cAlign == "تمدد" || cAlign == "stretch");
        const float contentLeft  = margin + padding;
        const float contentRight = measured.width - margin - padding;
        const float contentW = std::max(0.0f, contentRight - contentLeft);

        float childY = margin + padding;
        for (const auto& info : infos) {
            const auto& child = node.getChildren()[info.index];
            LayoutConstraints cc = childConstraints;
            if (info.weight > 0) {
                cc.maxHeight = info.height;
                cc.minHeight = info.height;
            }
            const bool rtl = (direction_ == LayoutDirection::RTL);
            // العرض الصريح للابن يفوز على التمدّد (اتّساقًا مع align-items:stretch):
            // measure يدهس القيد بالعرض الصريح، ففرض rect.width خارجيًّا يُنتج تنافرًا
            // رسم/تخطيط — فنتخطّى التمدّد لهذا الابن ونعامله كـ«بداية».
            bool stretchThis = alignStretch &&
                !child->findProperty(props::WIDTH) && !child->findProperty(props::WIDTH_LATIN);
            float childX;
            if (stretchThis) {
                // يملأ عرض المحتوى: قيد min=max ⇒ يقيس/يتخطّط الابن بالعرض الكامل.
                cc.minWidth = cc.maxWidth = contentW;
                childX = contentLeft;
            } else if (alignCenter)
                childX = contentLeft + (contentW - info.width) * 0.5f;
            else if (alignEnd)
                childX = rtl ? contentLeft : (contentRight - info.width);
            else // بداية (افتراضيّ، اتّجاهيّة)
                childX = rtl ? (contentRight - info.width) : contentLeft;
            if (childX < contentLeft) childX = contentLeft; // لا يتجاوز يسار المحتوى
            auto childResult = arrange(*child, cc, offsetX + childX, offsetY + childY);
            if (info.weight > 0) {
                childResult->rect.height = info.height;
            }
            if (stretchThis) childResult->rect.width = contentW;
            childY += childResult->rect.height + spacing;
            result->children.push_back(childResult);
        }
    } else if (node.getType() == UINodeType::Row) {
        // ─── توزيع أفقي (Row) مع دعم الوزن (flex weight) ───
        // المرحلة 1: قياس الأبناء العاديين وحساب الأوزان
        float fixedTotal = 0.0f;
        float totalWeight = 0.0f;
        struct RowChildInfo {
            size_t index;
            float width;
            float weight;
        };
        std::vector<RowChildInfo> rowInfos;

        for (size_t i = 0; i < node.childCount(); ++i) {
            const auto& child = node.getChildren()[i];
            float weight = 0.0f;
            const auto* weightProp = child->findProperty(props::WEIGHT);
            if (!weightProp) weightProp = child->findProperty(props::FLEX_LATIN);
            if (weightProp) {
                if (auto* v = std::get_if<double>(&weightProp->value)) weight = static_cast<float>(*v);
                else if (auto* vi = std::get_if<int64_t>(&weightProp->value)) weight = static_cast<float>(*vi);
            }
            auto childSize = measure(*child, childConstraints);
            if (weight > 0) {
                totalWeight += weight;
                rowInfos.push_back({i, 0.0f, weight});
            } else {
                fixedTotal += childSize.width;
                rowInfos.push_back({i, childSize.width, 0.0f});
            }
            if (i > 0) fixedTotal += spacing;
        }

        // أساس توزيع الوزن: إن كان للصفّ عرض صريح فالتوزيع ضمنه (منقوصًا الحشو
        // والهامش)، وإلّا يتمدّد ليملأ القيد المنفذ (childConstraints المخصوم سلفًا).
        float availBaseW = childConstraints.maxWidth;
        if (const auto* wP = node.findProperty(props::WIDTH)) {
            float w = -1.0f;
            if (auto* v = std::get_if<double>(&wP->value)) w = static_cast<float>(*v);
            else if (auto* vi = std::get_if<int64_t>(&wP->value)) w = static_cast<float>(*vi);
            if (w >= 0.0f) availBaseW = std::max(0.0f, w - inset * 2);
        }
        float availableW = std::max(0.0f, availBaseW - fixedTotal);
        for (auto& info : rowInfos) {
            if (info.weight > 0 && totalWeight > 0) {
                info.width = availableW * (info.weight / totalWeight);
            }
        }

        // المحاذاة المتقاطعة للصفّ (عموديّة، غير اتّجاهيّة): «محاذاة» — «بداية»
        // (أعلى، افتراضيّ) · «وسط» · «نهاية» (أسفل) · «تمدّد» (يملأ الارتفاع).
        // الهامش يُقحِم منطقة المحتوى في المحورين.
        std::string rAlign;
        if (const auto* aP = node.findProperty(props::ALIGN))
            if (auto* s = std::get_if<std::string>(&aP->value)) rAlign = *s;
        const bool rCenter  = (rAlign == "وسط" || rAlign == "center");
        const bool rEnd     = (rAlign == "نهاية" || rAlign == "end");
        const bool rStretch = (rAlign == "تمدّد" || rAlign == "تمدد" || rAlign == "stretch");
        const float rTop    = margin + padding;
        const float rBottom = measured.height - margin - padding;
        const float rContentH = std::max(0.0f, rBottom - rTop);

        // يرتّب ابنًا واحدًا عند حافّة أفقيّة معطاة، محاذيًا عموديًّا حسب الوضع؛
        // يعيد عرض الابن المرتَّب (لازم للتقدّم الأفقيّ). التمدّد يُتخطّى لابن بارتفاع صريح.
        auto placeRowChild = [&](const RowChildInfo& info, float childXpos) -> float {
            const auto& child = node.getChildren()[info.index];
            LayoutConstraints cc = childConstraints;
            if (info.weight > 0) { cc.maxWidth = cc.minWidth = info.width; }
            bool stretchThis = rStretch &&
                !child->findProperty(props::HEIGHT) && !child->findProperty(props::HEIGHT_LATIN);
            if (stretchThis) { cc.minHeight = cc.maxHeight = rContentH; }
            auto childSz = measure(*child, cc);
            float ch = stretchThis ? rContentH : childSz.height;
            float childY;
            if (stretchThis)   childY = rTop;
            else if (rCenter)  childY = rTop + (rContentH - ch) * 0.5f;
            else if (rEnd)     childY = rBottom - ch;
            else               childY = rTop; // بداية (أعلى)
            if (childY < rTop) childY = rTop;
            auto childResult = arrange(*child, cc, offsetX + childXpos, offsetY + childY);
            if (info.weight > 0)   childResult->rect.width = info.width;
            if (stretchThis)       childResult->rect.height = rContentH;
            result->children.push_back(childResult);
            return childResult->rect.width;
        };

        if (direction_ == LayoutDirection::RTL) {
            float childX = measured.width - margin - padding;
            for (const auto& info : rowInfos) {
                // info.width محسوب سلفًا (المرحلة 1 للثابت، الوزن للموزون) — لا قياس مكرّر.
                childX -= info.width;
                placeRowChild(info, childX);
                childX -= spacing;
            }
        } else {
            float childX = margin + padding;
            for (const auto& info : rowInfos) {
                float w = placeRowChild(info, childX);
                childX += w + spacing;
            }
        }
    }

    return result;
}

} // namespace ui
} // namespace sad
