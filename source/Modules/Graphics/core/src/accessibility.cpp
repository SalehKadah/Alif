/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: accessibility.cpp
 * المسار: features/graphics/core/src/accessibility.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * تنفيذ نظام إمكانية الوصول.
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/accessibility.h"
#include <sstream>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// SemanticNode
// ═══════════════════════════════════════════════════════════════════════════════

static const char* roleToString(SemanticRole role) {
    switch (role) {
        case SemanticRole::Button:       return "button";
        case SemanticRole::Link:         return "link";
        case SemanticRole::Image:        return "img";
        case SemanticRole::Text:         return "text";
        case SemanticRole::Heading:      return "heading";
        case SemanticRole::TextField:    return "textbox";
        case SemanticRole::Checkbox:     return "checkbox";
        case SemanticRole::RadioButton:  return "radio";
        case SemanticRole::Switch:       return "switch";
        case SemanticRole::Slider:       return "slider";
        case SemanticRole::ProgressBar:  return "progressbar";
        case SemanticRole::Tab:          return "tab";
        case SemanticRole::TabList:      return "tablist";
        case SemanticRole::TabPanel:     return "tabpanel";
        case SemanticRole::Menu:         return "menu";
        case SemanticRole::MenuItem:     return "menuitem";
        case SemanticRole::Dialog:       return "dialog";
        case SemanticRole::Alert:        return "alert";
        case SemanticRole::AlertDialog:  return "alertdialog";
        case SemanticRole::List:         return "list";
        case SemanticRole::ListItem:     return "listitem";
        case SemanticRole::Table:        return "table";
        case SemanticRole::Row:          return "row";
        case SemanticRole::Cell:         return "cell";
        case SemanticRole::Navigation:   return "navigation";
        case SemanticRole::Main:         return "main";
        case SemanticRole::Banner:       return "banner";
        case SemanticRole::Footer:       return "contentinfo";
        case SemanticRole::Form:         return "form";
        case SemanticRole::Search:       return "search";
        case SemanticRole::Complementary:return "complementary";
        case SemanticRole::Scrollable:   return "scrollbar";
        case SemanticRole::Tooltip:      return "tooltip";
        case SemanticRole::TreeItem:     return "treeitem";
        case SemanticRole::Group:        return "group";
        default: return "";
    }
}

std::string SemanticNode::describe() const {
    std::ostringstream ss;

    const char* roleName = roleToString(role);
    if (roleName[0]) ss << roleName;

    if (!label.empty()) {
        if (ss.tellp() > 0) ss << ", ";
        ss << label;
    }
    if (!value.empty()) {
        if (ss.tellp() > 0) ss << ": ";
        ss << value;
    }
    if (!hint.empty()) {
        if (ss.tellp() > 0) ss << ". ";
        ss << hint;
    }

    // حالات
    if (!isEnabled) ss << ", disabled";
    if (isSelected) ss << ", selected";
    if (isChecked) ss << ", checked";
    if (isExpanded) ss << ", expanded";
    if (isRequired) ss << ", required";

    return ss.str();
}

std::unordered_map<std::string, std::string> SemanticNode::ariaAttributes() const {
    std::unordered_map<std::string, std::string> attrs;

    const char* roleName = roleToString(role);
    if (roleName[0]) attrs["role"] = roleName;

    if (!label.empty()) attrs["aria-label"] = label;
    if (!hint.empty()) attrs["aria-description"] = hint;
    if (!isEnabled) attrs["aria-disabled"] = "true";
    if (isFocusable) attrs["tabindex"] = std::to_string(tabIndex >= 0 ? tabIndex : 0);
    if (isSelected) attrs["aria-selected"] = "true";
    if (isHidden) attrs["aria-hidden"] = "true";
    if (isReadOnly) attrs["aria-readonly"] = "true";
    if (isRequired) attrs["aria-required"] = "true";

    if (role == SemanticRole::Checkbox || role == SemanticRole::Switch) {
        attrs["aria-checked"] = isChecked ? "true" : "false";
    }
    if (role == SemanticRole::Slider || role == SemanticRole::ProgressBar) {
        attrs["aria-valuemin"] = std::to_string(minValue);
        attrs["aria-valuemax"] = std::to_string(maxValue);
        attrs["aria-valuenow"] = std::to_string(currentValue);
    }
    if (headingLevel > 0 && headingLevel <= 6) {
        attrs["aria-level"] = std::to_string(headingLevel);
    }
    if (isExpanded) attrs["aria-expanded"] = "true";

    switch (liveRegion) {
        case LiveRegion::Polite: attrs["aria-live"] = "polite"; break;
        case LiveRegion::Assertive: attrs["aria-live"] = "assertive"; break;
        default: break;
    }

    if (labelledBy >= 0) attrs["aria-labelledby"] = "node-" + std::to_string(labelledBy);
    if (describedBy >= 0) attrs["aria-describedby"] = "node-" + std::to_string(describedBy);

    return attrs;
}

// ═══════════════════════════════════════════════════════════════════════════════
// SemanticTree
// ═══════════════════════════════════════════════════════════════════════════════

std::shared_ptr<SemanticNode> SemanticTree::findById(int id) const {
    if (!root_) return nullptr;
    return findByIdImpl(root_, id);
}

std::shared_ptr<SemanticNode> SemanticTree::findByIdImpl(
    const std::shared_ptr<SemanticNode>& node, int id) const {
    if (!node) return nullptr;
    if (node->id == id) return node;
    for (const auto& child : node->children) {
        auto result = findByIdImpl(child, id);
        if (result) return result;
    }
    return nullptr;
}

std::vector<SemanticNode*> SemanticTree::focusableNodes() const {
    std::vector<SemanticNode*> result;
    if (root_) collectFocusable(root_.get(), result);
    return result;
}

void SemanticTree::collectFocusable(SemanticNode* node, std::vector<SemanticNode*>& out) const {
    if (!node || node->isHidden) return;
    if (node->isFocusable && node->isEnabled) out.push_back(node);
    for (const auto& child : node->children)
        collectFocusable(child.get(), out);
}

std::vector<SemanticNode*> SemanticTree::readingOrder() const {
    std::vector<SemanticNode*> result;
    if (root_) collectDFS(root_.get(), result);
    return result;
}

void SemanticTree::collectDFS(SemanticNode* node, std::vector<SemanticNode*>& out) const {
    if (!node || node->isHidden) return;
    if (node->isAccessible()) out.push_back(node);
    for (const auto& child : node->children)
        collectDFS(child.get(), out);
}

bool SemanticTree::updateNode(int id, const SemanticNode& updated) {
    auto node = findById(id);
    if (!node) return false;
    node->label = updated.label;
    node->value = updated.value;
    node->hint = updated.hint;
    node->isEnabled = updated.isEnabled;
    node->isFocusable = updated.isFocusable;
    node->isSelected = updated.isSelected;
    node->isChecked = updated.isChecked;
    node->isExpanded = updated.isExpanded;
    node->isHidden = updated.isHidden;
    node->currentValue = updated.currentValue;
    return true;
}

bool SemanticTree::removeNode(int id) {
    if (!root_) return false;
    if (root_->id == id) {
        root_ = nullptr;
        return true;
    }
    // بحث عن الأب وحذف الطفل
    auto order = readingOrder();
    for (auto* node : order) {
        auto& children = node->children;
        for (auto it = children.begin(); it != children.end(); ++it) {
            if ((*it)->id == id) {
                children.erase(it);
                return true;
            }
        }
    }
    return false;
}

int SemanticTree::nodeCount() const {
    if (!root_) return 0;
    return countNodes(root_.get());
}

int SemanticTree::countNodes(const SemanticNode* node) const {
    if (!node) return 0;
    int count = 1;
    for (const auto& child : node->children)
        count += countNodes(child.get());
    return count;
}

// ═══════════════════════════════════════════════════════════════════════════════
// AccessibilityManager
// ═══════════════════════════════════════════════════════════════════════════════

AccessibilityManager& AccessibilityManager::instance() {
    static AccessibilityManager inst;
    return inst;
}

void AccessibilityManager::announce(const std::string& message, LiveRegion priority) {
    if (announcementCb_) announcementCb_(message, priority);
}

void AccessibilityManager::focusNode(int nodeId) {
    focusedNodeId_ = nodeId;
    if (focusCb_) focusCb_(nodeId);

    // إعلان تلقائي عند التركيز
    auto node = tree_.findById(nodeId);
    if (node && settings_.screenReaderEnabled) {
        announce(node->describe(), LiveRegion::Polite);
    }
}

void AccessibilityManager::moveToNext() {
    auto nodes = tree_.focusableNodes();
    if (nodes.empty()) return;

    int idx = -1;
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i]->id == focusedNodeId_) {
            idx = static_cast<int>(i);
            break;
        }
    }
    idx = (idx + 1) % static_cast<int>(nodes.size());
    focusNode(nodes[idx]->id);
}

void AccessibilityManager::moveToPrevious() {
    auto nodes = tree_.focusableNodes();
    if (nodes.empty()) return;

    int idx = 0;
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i]->id == focusedNodeId_) {
            idx = static_cast<int>(i);
            break;
        }
    }
    idx = (idx - 1 + static_cast<int>(nodes.size())) % static_cast<int>(nodes.size());
    focusNode(nodes[idx]->id);
}

} // namespace ui
} // namespace sad
