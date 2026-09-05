/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: focus.h
 * المسار: features/graphics/core/include/sad_ui/focus.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * نظام إدارة التركيز (Focus Management System).
 *
 * يدعم:
 * - شجرة تركيز هرمية (Focus Tree)
 * - نطاقات تركيز (Focus Scopes)
 * - مصائد التركيز (Focus Traps) — للحوارات/القوائم
 * - تنقل Tab / Shift+Tab
 * - سياسات التنقل (Traversal Policies)
 * - تركيز تلقائي (Autofocus)
 * - حلقة مؤشرات بصرية (Focus Ring)
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_FOCUS_H
#define SAD_UI_FOCUS_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <cstdint>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// سياسة التنقل
// ═══════════════════════════════════════════════════════════════════════════════

enum class TraversalPolicy : uint8_t {
    ReadingOrder,       ///< ترتيب القراءة (DOM order)
    DirectionalOrder,   ///< اتجاهي (أعلى/أسفل/يمين/يسار)
    ExplicitOrder,      ///< ترتيب يدوي (tabIndex)
    WidgetOrder         ///< ترتيب الويدجت
};

enum class FocusDirection : uint8_t {
    Next,           ///< التالي (Tab)
    Previous,       ///< السابق (Shift+Tab)
    Up,
    Down,
    Left,
    Right
};

// ═══════════════════════════════════════════════════════════════════════════════
// عقدة تركيز (Focus Node)
// ═══════════════════════════════════════════════════════════════════════════════

class FocusScope;

class FocusNode {
public:
    explicit FocusNode(int id, const std::string& debugLabel = "");
    virtual ~FocusNode() = default;

    int id() const { return id_; }
    const std::string& debugLabel() const { return debugLabel_; }

    // ─── حالة ───
    bool hasFocus() const { return hasFocus_; }
    bool canRequestFocus() const { return canRequestFocus_ && isEnabled_; }
    bool isEnabled() const { return isEnabled_; }

    void setCanRequestFocus(bool can) { canRequestFocus_ = can; }
    void setEnabled(bool enabled) { isEnabled_ = enabled; }

    // ─── ترتيب Tab ───
    int tabIndex() const { return tabIndex_; }
    void setTabIndex(int idx) { tabIndex_ = idx; }

    // ─── تركيز تلقائي ───
    bool autofocus() const { return autofocus_; }
    void setAutofocus(bool af) { autofocus_ = af; }

    // ─── الأب ───
    FocusScope* parentScope() const { return parentScope_; }
    void setParentScope(FocusScope* scope) { parentScope_ = scope; }

    // ─── مُستمعون ───
    using FocusCallback = std::function<void(bool hasFocus)>;
    void onFocusChange(FocusCallback cb) { focusCb_ = cb; }

    // ─── موقع (للتنقل الاتجاهي) ───
    float x = 0, y = 0, width = 0, height = 0;

    // ─── صلاحية التركيز ───
    bool skipTraversal = false; ///< تجاوز عند التنقل Tab

protected:
    friend class FocusScope;
    friend class FocusManager;
    void setFocus(bool focus);

private:
    int id_;
    std::string debugLabel_;
    bool hasFocus_ = false;
    bool canRequestFocus_ = true;
    bool isEnabled_ = true;
    int tabIndex_ = 0;
    bool autofocus_ = false;
    FocusScope* parentScope_ = nullptr;
    FocusCallback focusCb_;
};

// ═══════════════════════════════════════════════════════════════════════════════
// نطاق تركيز (Focus Scope)
// ═══════════════════════════════════════════════════════════════════════════════

class FocusScope {
public:
    explicit FocusScope(int id, const std::string& debugLabel = "");

    int id() const { return id_; }
    const std::string& debugLabel() const { return debugLabel_; }

    // ─── عقد ───
    void addNode(std::shared_ptr<FocusNode> node);
    void removeNode(int nodeId);
    FocusNode* findNode(int nodeId) const;
    const std::vector<std::shared_ptr<FocusNode>>& nodes() const { return nodes_; }

    // ─── نطاقات فرعية ───
    void addChildScope(std::shared_ptr<FocusScope> scope);
    void removeChildScope(int scopeId);
    const std::vector<std::shared_ptr<FocusScope>>& childScopes() const { return childScopes_; }

    // ─── مصيدة التركيز ───
    bool isFocusTrap() const { return isTrap_; }
    void setFocusTrap(bool trap) { isTrap_ = trap; }

    // ─── العقدة المركّزة ───
    FocusNode* focusedNode() const { return focusedNode_; }
    void setFocusedNode(FocusNode* node);

    // ─── سياسة التنقل ───
    TraversalPolicy traversalPolicy() const { return policy_; }
    void setTraversalPolicy(TraversalPolicy p) { policy_ = p; }

    // ─── ترتيب التنقل ───
    std::vector<FocusNode*> traversalOrder() const;

private:
    int id_;
    std::string debugLabel_;
    std::vector<std::shared_ptr<FocusNode>> nodes_;
    std::vector<std::shared_ptr<FocusScope>> childScopes_;
    FocusNode* focusedNode_ = nullptr;
    bool isTrap_ = false;
    TraversalPolicy policy_ = TraversalPolicy::ReadingOrder;
};

// ═══════════════════════════════════════════════════════════════════════════════
// مدير التركيز (Focus Manager)
// ═══════════════════════════════════════════════════════════════════════════════

class FocusManager {
public:
    static FocusManager& instance();

    /// النطاق الجذر
    FocusScope& rootScope() { return rootScope_; }

    /// التركيز على عقدة
    bool requestFocus(int nodeId);

    /// إلغاء التركيز
    void unfocus();

    /// التنقل
    bool moveFocus(FocusDirection direction);
    bool nextFocus();     ///< Tab
    bool previousFocus(); ///< Shift+Tab

    /// العقدة المركّزة حالياً
    FocusNode* focusedNode() const { return focusedNode_; }
    int focusedNodeId() const { return focusedNode_ ? focusedNode_->id() : -1; }

    /// دفع/سحب مصيدة تركيز
    void pushFocusTrap(FocusScope* scope);
    void popFocusTrap();
    FocusScope* activeTrap() const;

    /// تطبيق التركيز التلقائي
    void applyAutofocus();

    /// مستمعي تغيير التركيز
    using FocusChangeCallback = std::function<void(FocusNode* oldNode, FocusNode* newNode)>;
    void onFocusChange(FocusChangeCallback cb) { focusChangeCb_ = cb; }

    /// إعدادات بصرية
    bool showFocusRing = true;
    float focusRingWidth = 2.0f;
    float focusRingOffset = 2.0f;

private:
    FocusManager();
    FocusScope rootScope_;
    FocusNode* focusedNode_ = nullptr;
    std::vector<FocusScope*> trapStack_;
    FocusChangeCallback focusChangeCb_;

    FocusNode* findNodeGlobal(int nodeId) const;
    FocusNode* findInScope(FocusScope* scope, int nodeId) const;
    std::vector<FocusNode*> collectAllNodes(FocusScope* scope) const;
    FocusScope* activeScopeForNavigation() const;
};

} // namespace ui
} // namespace sad

#endif // SAD_UI_FOCUS_H
