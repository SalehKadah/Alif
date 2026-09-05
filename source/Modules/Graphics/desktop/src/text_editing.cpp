/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: text_editing.cpp
 * المسار: features/graphics/backends/desktop/src/text_editing.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * تنفيذ نظام تحرير النصوص المتقدم.
 *
 * يشمل:
 * - TextEditState: المؤشر والتحديد
 * - UndoRedoManager: التراجع/الإعادة
 * - SadSyntaxHighlighter: التلوين النحوي للغة ص
 * - TextEditStateMap: ربط الحالات بالعقد
 *
 * ═══════════════════════════════════════════════════════════════════════════════
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "text_editing.h"
#include <algorithm>
#include <unordered_set>
#include <cstring>

namespace sad {
namespace ui {
namespace desktop {

// ═══════════════════════════════════════════════════════════════════════════════
// TextEditState — المؤشر والتحديد
// ═══════════════════════════════════════════════════════════════════════════════

size_t TextEditState::nextCharPos(size_t pos) const {
    if (pos >= text.size()) return text.size();
    unsigned char c = static_cast<unsigned char>(text[pos]);
    // تحديد طول الحرف UTF-8
    size_t len = 1;
    if ((c & 0x80) == 0)       len = 1;      // ASCII
    else if ((c & 0xE0) == 0xC0) len = 2;    // 2-byte
    else if ((c & 0xF0) == 0xE0) len = 3;    // 3-byte (عربي)
    else if ((c & 0xF8) == 0xF0) len = 4;    // 4-byte (إيموجي)
    size_t next = pos + len;
    return (next > text.size()) ? text.size() : next;
}

size_t TextEditState::prevCharPos(size_t pos) const {
    if (pos == 0) return 0;
    size_t p = pos - 1;
    // تراجع عبر continuation bytes
    while (p > 0 && (static_cast<unsigned char>(text[p]) & 0xC0) == 0x80) {
        --p;
    }
    return p;
}

std::string TextEditState::getSelectedText() const {
    if (!hasSelection) return "";
    size_t s = std::min(selectionStart, selectionEnd);
    size_t e = std::max(selectionStart, selectionEnd);
    if (s >= text.size()) return "";
    if (e > text.size()) e = text.size();
    return text.substr(s, e - s);
}

void TextEditState::deleteSelection() {
    if (!hasSelection) return;
    size_t s = std::min(selectionStart, selectionEnd);
    size_t e = std::max(selectionStart, selectionEnd);
    if (s >= text.size()) { hasSelection = false; return; }
    if (e > text.size()) e = text.size();
    text.erase(s, e - s);
    cursorPos = s;
    hasSelection = false;
}

void TextEditState::insertText(const std::string& newText) {
    if (hasSelection) {
        deleteSelection();
    }
    if (cursorPos > text.size()) cursorPos = text.size();
    text.insert(cursorPos, newText);
    cursorPos += newText.size();
}

void TextEditState::moveCursorRight(bool extendSelection) {
    size_t oldPos = cursorPos;
    if (hasSelection && !extendSelection) {
        // القفز إلى نهاية التحديد
        cursorPos = std::max(selectionStart, selectionEnd);
        hasSelection = false;
        return;
    }
    cursorPos = nextCharPos(cursorPos);
    if (extendSelection) {
        if (!hasSelection) {
            selectionStart = oldPos;
            hasSelection = true;
        }
        selectionEnd = cursorPos;
        if (selectionStart == selectionEnd) hasSelection = false;
    } else {
        hasSelection = false;
    }
}

void TextEditState::moveCursorLeft(bool extendSelection) {
    size_t oldPos = cursorPos;
    if (hasSelection && !extendSelection) {
        // القفز إلى بداية التحديد
        cursorPos = std::min(selectionStart, selectionEnd);
        hasSelection = false;
        return;
    }
    cursorPos = prevCharPos(cursorPos);
    if (extendSelection) {
        if (!hasSelection) {
            selectionStart = oldPos;
            hasSelection = true;
        }
        selectionEnd = cursorPos;
        if (selectionStart == selectionEnd) hasSelection = false;
    } else {
        hasSelection = false;
    }
}

void TextEditState::moveCursorHome(bool extendSelection) {
    size_t oldPos = cursorPos;
    cursorPos = 0;
    if (extendSelection) {
        if (!hasSelection) {
            selectionStart = oldPos;
            hasSelection = true;
        }
        selectionEnd = 0;
        if (selectionStart == selectionEnd) hasSelection = false;
    } else {
        hasSelection = false;
    }
}

void TextEditState::moveCursorEnd(bool extendSelection) {
    size_t oldPos = cursorPos;
    cursorPos = text.size();
    if (extendSelection) {
        if (!hasSelection) {
            selectionStart = oldPos;
            hasSelection = true;
        }
        selectionEnd = text.size();
        if (selectionStart == selectionEnd) hasSelection = false;
    } else {
        hasSelection = false;
    }
}

void TextEditState::selectAll() {
    selectionStart = 0;
    selectionEnd = text.size();
    cursorPos = text.size();
    hasSelection = (text.size() > 0);
}

void TextEditState::clearSelection() {
    hasSelection = false;
    selectionStart = selectionEnd = cursorPos;
}

void TextEditState::setCursorFromX(float x, float textStartX,
    std::function<float(const std::string&, size_t)> measureFunc) {
    if (text.empty()) {
        cursorPos = 0;
        return;
    }
    float relX = x - textStartX;
    if (relX <= 0) { cursorPos = 0; return; }

    // بحث ثنائي (binary search) على موقع الحرف
    size_t best = 0;
    float bestDist = relX;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t next = nextCharPos(pos);
        float w = measureFunc(text, next);
        float dist = std::abs(w - relX);
        if (dist < bestDist) {
            bestDist = dist;
            best = next;
        }
        if (w > relX) break;
        pos = next;
    }
    cursorPos = best;
}

// ═══════════════════════════════════════════════════════════════════════════════
// UndoRedoManager — التراجع/الإعادة
// ═══════════════════════════════════════════════════════════════════════════════

void UndoRedoManager::pushAction(const TextEditAction& action) {
    // مسح redo عند أي عملية جديدة
    redoStack_.clear();
    undoStack_.push_back(action);
    // تحديد حجم المكدس
    if (undoStack_.size() > MAX_UNDO_SIZE) {
        undoStack_.erase(undoStack_.begin());
    }
}

bool UndoRedoManager::undo(std::string& outText, size_t& outCursor) {
    if (undoStack_.empty()) return false;
    auto action = undoStack_.back();
    undoStack_.pop_back();
    redoStack_.push_back(action);
    outText = action.textBefore;
    outCursor = action.cursorBefore;
    return true;
}

bool UndoRedoManager::redo(std::string& outText, size_t& outCursor) {
    if (redoStack_.empty()) return false;
    auto action = redoStack_.back();
    redoStack_.pop_back();
    undoStack_.push_back(action);
    outText = action.textAfter;
    outCursor = action.cursorAfter;
    return true;
}

void UndoRedoManager::clear() {
    undoStack_.clear();
    redoStack_.clear();
}

// ═══════════════════════════════════════════════════════════════════════════════
// (AR) SadSyntaxHighlighter انتقل إلى القلب: core/src/syntax_highlighter.cpp
//      منطقٌ خالصٌ لا يخصُّ SDL، وحبسُه هنا كان يحبس رسمَ «كتلة_كود» معه.
// ═══════════════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════════════
// TextEditStateMap — خريطة ربط الحالات
// ═══════════════════════════════════════════════════════════════════════════════

TextEditState& TextEditStateMap::getState(const ::sad::ui::IRNode* node) {
    return states_[node].state;
}

UndoRedoManager& TextEditStateMap::getUndoManager(const ::sad::ui::IRNode* node) {
    return states_[node].undoManager;
}

void TextEditStateMap::clearState(const ::sad::ui::IRNode* node) {
    states_.erase(node);
}

void TextEditStateMap::clearAll() {
    states_.clear();
}

} // namespace desktop
} // namespace ui
} // namespace sad
