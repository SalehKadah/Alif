/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: window_control.cpp
 * المسار: features/graphics/core/src/window_control.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * @brief (AR) تعريف accessor المتحكّم النشط للنافذة (window_control.h).
 * @brief (EN) Defines the active window controller accessor (window_control.h).
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص — مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/window_control.h"

namespace sad
{
    namespace ui
    {
        WindowController &windowController()
        {
            // (AR) مُسرَّبٌ عمدًا (never-destroyed) نظير nav(): يتفادى اعتماديّة ترتيب
            //      إتلاف statics عبر الوحدات (حلقة النافذة قد تستهلكه عند الخروج).
            static WindowController *c = new WindowController();
            return *c;
        }
    } // namespace ui
} // namespace sad
