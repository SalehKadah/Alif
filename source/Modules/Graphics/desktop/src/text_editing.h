/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: text_editing.h
 * المسار: features/graphics/backends/desktop/src/text_editing.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * نظام تحرير النصوص المتقدم لـ Desktop Backend.
 *
 * يشمل:
 * - حالة المؤشر والتحديد (Cursor & Selection)
 * - نظام التراجع/الإعادة (Undo/Redo)
 * - دعم الحافظة (Clipboard via SDL2)
 * - تلوين نحوي للغة ص (Syntax Highlighting)
 * - أرقام الأسطر (Line Numbers)
 *
 * ═══════════════════════════════════════════════════════════════════════════════
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_TEXT_EDITING_H
#define SAD_UI_TEXT_EDITING_H

#include <string>
#include <vector>
#include <stack>
#include <functional>
#include <cstdint>

// التلوين النحوي — انتقل إلى المكتبة (sad::ui)
#include "sad_ui/syntax_highlighter.h"

// Forward declaration of IRNode in its actual namespace
namespace sad { namespace ui { class IRNode; } }

namespace sad {
namespace ui {
namespace desktop {

// ═══════════════════════════════════════════════════════════════════════════════
// 1. حالة تحرير النص (TextEditState)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief حالة المؤشر والتحديد لحقل نص
 */
struct TextEditState {
    std::string text;               ///< النص الحالي
    size_t cursorPos = 0;           ///< موقع المؤشر (بـ UTF-8 byte offset)
    size_t selectionStart = 0;      ///< بداية التحديد
    size_t selectionEnd = 0;        ///< نهاية التحديد
    bool hasSelection = false;      ///< هل يوجد نص محدد
    size_t scrollOffsetX = 0;       ///< إزاحة التمرير الأفقي

    /// الحصول على النص المحدد
    std::string getSelectedText() const;

    /// حذف النص المحدد
    void deleteSelection();

    /// إدراج نص عند المؤشر (يحذف التحديد أولاً)
    void insertText(const std::string& newText);

    /// تحريك المؤشر يميناً (حرف UTF-8 واحد)
    void moveCursorRight(bool extendSelection = false);

    /// تحريك المؤشر يساراً (حرف UTF-8 واحد)
    void moveCursorLeft(bool extendSelection = false);

    /// تحريك المؤشر إلى بداية النص
    void moveCursorHome(bool extendSelection = false);

    /// تحريك المؤشر إلى نهاية النص
    void moveCursorEnd(bool extendSelection = false);

    /// تحديد كل النص
    void selectAll();

    /// إلغاء التحديد
    void clearSelection();

    /// تحريك المؤشر إلى أقرب حرف عند إحداثي x
    void setCursorFromX(float x, float textStartX,
                        std::function<float(const std::string&, size_t)> measureFunc);

private:
    /// الحصول على بداية الحرف التالي (UTF-8)
    size_t nextCharPos(size_t pos) const;

    /// الحصول على بداية الحرف السابق (UTF-8)
    size_t prevCharPos(size_t pos) const;
};

// ═══════════════════════════════════════════════════════════════════════════════
// 2. نظام التراجع/الإعادة (UndoRedoManager)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief عملية نصية قابلة للتراجع
 */
struct TextEditAction {
    std::string textBefore;     ///< النص قبل العملية
    std::string textAfter;      ///< النص بعد العملية
    size_t cursorBefore;        ///< موقع المؤشر قبل
    size_t cursorAfter;         ///< موقع المؤشر بعد
};

/**
 * @brief مدير التراجع والإعادة
 */
class UndoRedoManager {
public:
    static constexpr size_t MAX_UNDO_SIZE = 100;

    /// تسجيل عملية جديدة
    void pushAction(const TextEditAction& action);

    /// التراجع — يُرجع true إذا نجح
    bool undo(std::string& outText, size_t& outCursor);

    /// الإعادة — يُرجع true إذا نجح
    bool redo(std::string& outText, size_t& outCursor);

    /// هل يمكن التراجع؟
    bool canUndo() const { return !undoStack_.empty(); }

    /// هل يمكن الإعادة؟
    bool canRedo() const { return !redoStack_.empty(); }

    /// مسح كل التاريخ
    void clear();

private:
    std::vector<TextEditAction> undoStack_;
    std::vector<TextEditAction> redoStack_;
};

// ═══════════════════════════════════════════════════════════════════════════════
// 3. تلوين نحوي للغة ص — انتقل إلى المكتبة: sad_ui/syntax_highlighter.h
//    (الأسماء متاحةٌ هنا عبر sad::ui لأنّ desktop فضاءٌ متداخلٌ فيه.)
// ═══════════════════════════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════════════════════════
// 4. خريطة حالات التحرير (TextEditStateMap)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief خريطة تربط كل IRNode بحالة تحرير
 */
class TextEditStateMap {
public:
    /// الحصول على/إنشاء حالة تحرير لعقدة
    TextEditState& getState(const ::sad::ui::IRNode* node);

    /// الحصول على مدير التراجع لعقدة
    UndoRedoManager& getUndoManager(const ::sad::ui::IRNode* node);

    /// مسح حالة عقدة
    void clearState(const ::sad::ui::IRNode* node);

    /// مسح كل الحالات
    void clearAll();

private:
    struct NodeEditData {
        TextEditState state;
        UndoRedoManager undoManager;
    };
    std::unordered_map<const ::sad::ui::IRNode*, NodeEditData> states_;
};

} // namespace desktop
} // namespace ui
} // namespace sad

#endif // SAD_UI_TEXT_EDITING_H
