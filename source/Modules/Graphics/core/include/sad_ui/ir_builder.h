/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: ir_builder.h
 * المسار: features/graphics/core/include/sad_ui/ir_builder.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * الوصف:
 * ------
 * باني التمثيل الوسيط (IR Builder) — يحوّل شجرة UINode إلى شجرة IRNode.
 *
 * هذه هي الخطوة الحاسمة في خط الأنابيب:
 *
 *     UINode Tree  ──→  IRBuilder  ──→  IR Tree
 *                    ↑ هنا يحدث التحويل
 *
 * مسؤوليات IRBuilder:
 * ──────────────────────
 * 1. **تحليل المعدّلات**: تحويل Modifier إلى IRProperty
 *    - Modifier(FontSize, 24) → IRProperty("fontSize", 24.0)
 *    - Modifier(OnTap, expr) → IREvent("onTap", expr)
 *
 * 2. **التوجيه الذكي**: تحديد RenderingHint لكل عقدة
 *    - هل هذا العنصر موجود أصلياً في المنصة؟ → RenderingHint::Native
 *    - هل هذا عنصر مخصص؟ → RenderingHint::Canvas
 *    - هل فيه خلفية مخصصة لكن النص أصلي؟ → RenderingHint::Hybrid
 *
 * 3. **تحليل الحالة**: ربط IRStateRef بالعقد التي تعتمد على حالة
 *
 * 4. **التحسين المبدئي**: حذف عقد فارغة، دمج معدّلات متكررة
 *
 * مثال:
 * ──────
 *   UINode(Text, "مرحباً", [FontSize=24, Color=Red])
 *   ↓ IRBuilder
 *   IRNode(Text, props={text:"مرحباً", fontSize:24, color:"#FF0000"},
 *          hint=Native)
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_IR_BUILDER_H
#define SAD_UI_IR_BUILDER_H

#include "node.h"
#include "ir.h"
#include "hybrid_routing.h"

#include <memory>
#include <string>
#include <unordered_set>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// خيارات البناء (Build Options)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief خيارات التحكم في عملية بناء IR
 *
 * تُحدد كيف يتعامل IRBuilder مع التحويل:
 * - المنصة المستهدفة (تؤثر على التوجيه الذكي)
 * - هل نُحسّن IR (حذف عقد فارغة، دمج...)
 * - هل نضيف معلومات تصحيح
 */
struct IRBuildOptions {
    /// المنصة المستهدفة — تؤثر على قرارات التوجيه الذكي
    TargetPlatform targetPlatform = TargetPlatform::Desktop;

    /// تمكين التحسينات (حذف عقد فارغة، دمج معدّلات)
    bool optimize = true;

    /// تضمين معلومات التصحيح (أرقام الأسطر، أسماء مصدرية)
    bool includeDebugInfo = true;

    /// تمكين التوجيه الذكي (Native/Canvas)
    bool enableSmartRouting = true;
};

// ═══════════════════════════════════════════════════════════════════════════════
// باني IR (IR Builder)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief يحوّل شجرة UINode إلى شجرة IRNode
 *
 * الاستخدام:
 * @code
 *   IRBuilder builder(options);
 *   auto irModule = builder.build(viewDef);
 *   // أو من عقدة مباشرة:
 *   auto irRoot = builder.buildNode(uiNode);
 * @endcode
 */
class IRBuilder {
public:
    /// إنشاء الباني بخيارات محددة
    explicit IRBuilder(const IRBuildOptions& options = {});

    /**
     * @brief بناء وحدة IR كاملة من تعريف واجهة
     *
     * هذه هي الدالة الرئيسية — تأخذ ViewDefinition كامل وتُعيد IRModule.
     *
     * @param view تعريف الواجهة (اسم + حالة + شجرة)
     * @return وحدة IR جاهزة للتمرير لمولّد الكود
     */
    IRModule build(const ViewDefinition& view);

    /**
     * @brief بناء عقدة IR من عقدة UINode واحدة
     *
     * تُستخدم للتحويل التدريجي أو الاختبار.
     *
     * @param node عقدة UINode المدخلة
     * @return عقدة IRNode المقابلة
     */
    std::shared_ptr<IRNode> buildNode(const std::shared_ptr<UINode>& node);

    /**
     * @brief تسجيل عناصر أصلية للمنصة
     *
     * يُخبر التوجيه الذكي أن هذا النوع موجود أصلياً في المنصة.
     * مثال: على Android، Button و Text أصليان لكن Canvas لا.
     *
     * @param type نوع العنصر
     */
    void registerNativeWidget(UINodeType type) {
        nativeWidgets_.insert(type);
    }

    /// الحصول على الخيارات الحالية
    const IRBuildOptions& getOptions() const { return options_; }

    /// الحصول على جدول التوجيه الهجين (للتخصيص)
    HybridRouter* getRouter() { return router_.get(); }
    const HybridRouter* getRouter() const { return router_.get(); }

private:
    // ─── دوال داخلية ────────────────────────────

    /// تحويل معدّلات UINode إلى خصائص وأحداث IR
    void processModifiers(const UINode& source, IRNode& target);

    /// تحديد تلميح العرض بناءً على النوع والمنصة
    RenderingHint determineRenderingHint(UINodeType type) const;

    /// تحويل روابط الحالة إلى إشارات IR
    void processStateBindings(const UINode& source, IRNode& target);

    /// تحسين شجرة IR (اختياري)
    void optimizeTree(std::shared_ptr<IRNode>& root);

    // ─── البيانات ────────────────────────────────

    IRBuildOptions options_;                              ///< خيارات البناء
    std::unordered_set<UINodeType> nativeWidgets_;       ///< عناصر أصلية مسجلة
    std::unique_ptr<HybridRouter> router_;                ///< الموجّه الهجين
};

} // namespace ui
} // namespace sad

#endif // SAD_UI_IR_BUILDER_H
