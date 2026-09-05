/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: focus.cpp
 * المسار: features/graphics/core/src/focus.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * تنفيذ نظام إدارة التركيز.
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/focus.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// FocusNode
// ═══════════════════════════════════════════════════════════════════════════════

FocusNode::FocusNode(int id, const std::string& debugLabel)
    : id_(id), debugLabel_(debugLabel) {}

void FocusNode::setFocus(bool focus) {
    if (hasFocus_ == focus) return;
    hasFocus_ = focus;
    if (focusCb_) focusCb_(focus);
}

// ═══════════════════════════════════════════════════════════════════════════════
// FocusScope
// ═══════════════════════════════════════════════════════════════════════════════

FocusScope::FocusScope(int id, const std::string& debugLabel)
    : id_(id), debugLabel_(debugLabel) {}

void FocusScope::addNode(std::shared_ptr<FocusNode> node) {
    node->setParentScope(this);
    nodes_.push_back(std::move(node));
}

void FocusScope::removeNode(int nodeId) {
    nodes_.erase(
        std::remove_if(nodes_.begin(), nodes_.end(),
            [nodeId](const auto& n) { return n->id() == nodeId; }),
        nodes_.end());
    if (focusedNode_ && focusedNode_->id() == nodeId)
        focusedNode_ = nullptr;
}

FocusNode* FocusScope::findNode(int nodeId) const {
    for (const auto& n : nodes_) {
        if (n->id() == nodeId) return n.get();
    }
    for (const auto& child : childScopes_) {
        auto* found = child->findNode(nodeId);
        if (found) return found;
    }
    return nullptr;
}

void FocusScope::addChildScope(std::shared_ptr<FocusScope> scope) {
    childScopes_.push_back(std::move(scope));
}

void FocusScope::removeChildScope(int scopeId) {
    childScopes_.erase(
        std::remove_if(childScopes_.begin(), childScopes_.end(),
            [scopeId](const auto& s) { return s->id() == scopeId; }),
        childScopes_.end());
}

void FocusScope::setFocusedNode(FocusNode* node) {
    if (focusedNode_ == node) return;
    if (focusedNode_) focusedNode_->setFocus(false);
    focusedNode_ = node;
    if (focusedNode_) focusedNode_->setFocus(true);
}

std::vector<FocusNode*> FocusScope::traversalOrder() const {
    std::vector<FocusNode*> result;
    for (const auto& n : nodes_) {
        if (n->canRequestFocus() && !n->skipTraversal)
            result.push_back(n.get());
    }

    if (policy_ == TraversalPolicy::ExplicitOrder) {
        std::sort(result.begin(), result.end(),
            [](FocusNode* a, FocusNode* b) { return a->tabIndex() < b->tabIndex(); });
    }
    // ReadingOrder: ترتيب الإضافة الافتراضي

    return result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// FocusManager
// ═══════════════════════════════════════════════════════════════════════════════

FocusManager::FocusManager() : rootScope_(0, "root") {}

FocusManager& FocusManager::instance() {
    static FocusManager inst;
    return inst;
}

bool FocusManager::requestFocus(int nodeId) {
    FocusNode* node = findNodeGlobal(nodeId);
    if (!node || !node->canRequestFocus()) return false;

    // تحقق من المصيدة
    auto* trap = activeTrap();
    if (trap) {
        auto* inTrap = findInScope(trap, nodeId);
        if (!inTrap) return false; // خارج المصيدة
    }

    FocusNode* oldNode = focusedNode_;
    if (oldNode) oldNode->setFocus(false);

    focusedNode_ = node;
    node->setFocus(true);

    // تحديث النطاق الأب
    if (node->parentScope()) {
        node->parentScope()->setFocusedNode(node);
    }

    if (focusChangeCb_) focusChangeCb_(oldNode, node);
    return true;
}

void FocusManager::unfocus() {
    if (focusedNode_) {
        FocusNode* old = focusedNode_;
        focusedNode_->setFocus(false);
        focusedNode_ = nullptr;
        if (focusChangeCb_) focusChangeCb_(old, nullptr);
    }
}

bool FocusManager::moveFocus(FocusDirection direction) {
    if (direction == FocusDirection::Next) return nextFocus();
    if (direction == FocusDirection::Previous) return previousFocus();

    // تنقل اتجاهي
    if (!focusedNode_) return nextFocus();

    auto* scope = activeScopeForNavigation();
    auto all = collectAllNodes(scope);
    if (all.empty()) return false;

    FocusNode* best = nullptr;
    float bestDist = std::numeric_limits<float>::max();
    float cx = focusedNode_->x + focusedNode_->width / 2.0f;
    float cy = focusedNode_->y + focusedNode_->height / 2.0f;

    for (auto* node : all) {
        if (node == focusedNode_ || !node->canRequestFocus()) continue;

        float nx = node->x + node->width / 2.0f;
        float ny = node->y + node->height / 2.0f;
        float dx = nx - cx;
        float dy = ny - cy;

        bool validDir = false;
        switch (direction) {
            case FocusDirection::Up:    validDir = (dy < 0); break;
            case FocusDirection::Down:  validDir = (dy > 0); break;
            case FocusDirection::Left:  validDir = (dx < 0); break;
            case FocusDirection::Right: validDir = (dx > 0); break;
            default: break;
        }
        if (!validDir) continue;

        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < bestDist) {
            bestDist = dist;
            best = node;
        }
    }

    if (best) return requestFocus(best->id());
    return false;
}

bool FocusManager::nextFocus() {
    auto* scope = activeScopeForNavigation();
    auto order = collectAllNodes(scope);
    if (order.empty()) return false;

    int idx = -1;
    if (focusedNode_) {
        for (size_t i = 0; i < order.size(); ++i) {
            if (order[i]->id() == focusedNode_->id()) {
                idx = static_cast<int>(i);
                break;
            }
        }
    }

    // حلقة
    int nextIdx = (idx + 1) % static_cast<int>(order.size());
    return requestFocus(order[nextIdx]->id());
}

bool FocusManager::previousFocus() {
    auto* scope = activeScopeForNavigation();
    auto order = collectAllNodes(scope);
    if (order.empty()) return false;

    int idx = 0;
    if (focusedNode_) {
        for (size_t i = 0; i < order.size(); ++i) {
            if (order[i]->id() == focusedNode_->id()) {
                idx = static_cast<int>(i);
                break;
            }
        }
    }

    int prevIdx = (idx - 1 + static_cast<int>(order.size())) % static_cast<int>(order.size());
    return requestFocus(order[prevIdx]->id());
}

void FocusManager::pushFocusTrap(FocusScope* scope) {
    if (!scope) return;
    scope->setFocusTrap(true);
    trapStack_.push_back(scope);
    // ركّز على أول عنصر في المصيدة
    auto order = scope->traversalOrder();
    if (!order.empty()) requestFocus(order[0]->id());
}

void FocusManager::popFocusTrap() {
    if (trapStack_.empty()) return;
    auto* scope = trapStack_.back();
    scope->setFocusTrap(false);
    trapStack_.pop_back();
}

FocusScope* FocusManager::activeTrap() const {
    return trapStack_.empty() ? nullptr : trapStack_.back();
}

void FocusManager::applyAutofocus() {
    auto all = collectAllNodes(&rootScope_);
    for (auto* node : all) {
        if (node->autofocus() && node->canRequestFocus()) {
            requestFocus(node->id());
            return;
        }
    }
}

FocusNode* FocusManager::findNodeGlobal(int nodeId) const {
    return findInScope(const_cast<FocusScope*>(&rootScope_), nodeId);
}

FocusNode* FocusManager::findInScope(FocusScope* scope, int nodeId) const {
    if (!scope) return nullptr;
    return scope->findNode(nodeId);
}

std::vector<FocusNode*> FocusManager::collectAllNodes(FocusScope* scope) const {
    if (!scope) return {};
    std::vector<FocusNode*> result = scope->traversalOrder();
    for (const auto& child : scope->childScopes()) {
        auto childNodes = collectAllNodes(child.get());
        result.insert(result.end(), childNodes.begin(), childNodes.end());
    }
    return result;
}

FocusScope* FocusManager::activeScopeForNavigation() const {
    auto* trap = activeTrap();
    return trap ? trap : const_cast<FocusScope*>(&rootScope_);
}

} // namespace ui
} // namespace sad
