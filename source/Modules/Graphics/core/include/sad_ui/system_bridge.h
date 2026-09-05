/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: system_bridge.h
 * المسار: features/graphics/core/include/sad_ui/system_bridge.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * واجهة الجسر المجردة (Abstract System Bridge).
 *
 * تربط الأنظمة الثمانية الأساسية بكل backend:
 * 1. السمات (Theme)
 * 2. الإيماءات (Gesture)
 * 3. فيزياء التمرير (ScrollPhysics)
 * 4. إمكانية الوصول (Accessibility)
 * 5. التحقق من النماذج (FormValidation)
 * 6. إدارة التركيز (Focus)
 * 7. ذاكرة الصور (ImageCache)
 * 8. الإشعارات (Notification)
 *
 * كل backend ينفّذ هذه الواجهة لترجمة الأنظمة الأساسية
 * إلى واجهات المنصة الأصلية.
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_SYSTEM_BRIDGE_H
#define SAD_UI_SYSTEM_BRIDGE_H

#include "sad_ui/theme.h"
#include "sad_ui/gesture.h"
#include "sad_ui/scroll_physics.h"
#include "sad_ui/accessibility.h"
#include "sad_ui/form_validation.h"
#include "sad_ui/focus.h"
#include "sad_ui/image_cache.h"
#include "sad_ui/notification.h"

#include <string>
#include <memory>
#include <functional>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// معلومات المنصة
// ═══════════════════════════════════════════════════════════════════════════════

enum class PlatformType : uint8_t {
    Desktop,    ///< سطح المكتب (SDL2 + OpenGL)
    Web,        ///< ويب (HTML5/CSS/JS)
    Android,    ///< أندرويد (Compose / OpenGL ES)
    iOS,        ///< آي أو إس (SwiftUI / Metal)
    macOS       ///< ماك (AppKit / CoreGraphics)
};

struct PlatformInfo {
    PlatformType type;
    std::string name;
    std::string version;
    float scaleFactor = 1.0f;
    bool supportsTouch = false;
    bool supportsMouse = true;
    bool supportsKeyboard = true;
    bool supportsHaptic = false;
    bool supportsForceTap = false;
    bool isRTL = true;            ///< دعم اتجاه RTL
};

// ═══════════════════════════════════════════════════════════════════════════════
// واجهة جسر السمات (Theme Bridge)
// ═══════════════════════════════════════════════════════════════════════════════

class ThemeBridge {
public:
    virtual ~ThemeBridge() = default;

    /// تطبيق مخطط ألوان على المنصة
    virtual void applyColorScheme(const ColorScheme& scheme) = 0;

    /// تطبيق نمط نصوص
    virtual void applyTextTheme(const TextTheme& textTheme) = 0;

    /// التبديل بين الوضع الداكن والفاتح
    virtual void setDarkMode(bool dark) = 0;

    /// الحصول على وضع النظام الحالي
    virtual bool isSystemDarkMode() const = 0;

    /// تحويل ThemeColor إلى تمثيل المنصة
    virtual uint32_t toNativeColor(const ThemeColor& color) const = 0;

    /// تطبيق سمة كاملة
    virtual void applyTheme(const ThemeProvider& provider) = 0;
};

// ═══════════════════════════════════════════════════════════════════════════════
// واجهة جسر الإيماءات (Gesture Bridge)
// ═══════════════════════════════════════════════════════════════════════════════

class GestureBridge {
public:
    virtual ~GestureBridge() = default;

    /// تحويل حدث منصة أصلي إلى GestureEvent
    virtual bool translateNativeEvent(void* nativeEvent, GestureEvent& outEvent) = 0;

    /// تسجيل كاشف إيماءات للمنصة
    virtual void registerDetector(const std::string& nodeId,
                                  std::shared_ptr<GestureDetector> detector) = 0;

    /// إلغاء تسجيل كاشف
    virtual void unregisterDetector(const std::string& nodeId) = 0;

    /// تفعيل/تعطيل نوع إيماءة
    virtual void setGestureEnabled(GestureType type, bool enabled) = 0;

    /// هل المنصة تدعم هذا النوع من الإيماءات؟
    virtual bool supportsGesture(GestureType type) const = 0;
};

// ═══════════════════════════════════════════════════════════════════════════════
// واجهة جسر فيزياء التمرير (ScrollPhysics Bridge)
// ═══════════════════════════════════════════════════════════════════════════════

class ScrollPhysicsBridge {
public:
    virtual ~ScrollPhysicsBridge() = default;

    /// الحصول على فيزياء التمرير المناسبة للمنصة
    virtual std::shared_ptr<ScrollPhysics> getPlatformScrollPhysics() const = 0;

    /// تطبيق تمرير أصلي على عنصر
    virtual void applyNativeScroll(const std::string& nodeId,
                                   const ScrollMetrics& metrics) = 0;

    /// تحديث موقع التمرير بتحريك أصلي
    virtual void animateScrollTo(const std::string& nodeId,
                                 float targetOffset,
                                 float durationMs) = 0;

    /// تحويل حدث تمرير أصلي إلى ScrollMetrics
    virtual bool translateScrollEvent(void* nativeEvent,
                                      ScrollMetrics& outMetrics) = 0;
};

// ═══════════════════════════════════════════════════════════════════════════════
// واجهة جسر إمكانية الوصول (Accessibility Bridge)
// ═══════════════════════════════════════════════════════════════════════════════

class AccessibilityBridge {
public:
    virtual ~AccessibilityBridge() = default;

    /// تطبيق شجرة دلالية على المنصة
    virtual void applySemanticTree(const SemanticTree& tree) = 0;

    /// تحديث عقدة دلالية واحدة
    virtual void updateSemanticNode(const SemanticNode& node) = 0;

    /// إرسال إشعار إمكانية وصول
    virtual void announce(const std::string& message,
                          LiveRegion priority = LiveRegion::Polite) = 0;

    /// تعيين تسمية عنصر
    virtual void setNodeLabel(const std::string& nodeId,
                              const std::string& label) = 0;

    /// تعيين دور عنصر
    virtual void setNodeRole(const std::string& nodeId,
                             SemanticRole role) = 0;

    /// هل قارئ الشاشة نشط؟
    virtual bool isScreenReaderActive() const = 0;
};

// ═══════════════════════════════════════════════════════════════════════════════
// واجهة جسر التحقق من النماذج (FormValidation Bridge)
// ═══════════════════════════════════════════════════════════════════════════════

class FormValidationBridge {
public:
    virtual ~FormValidationBridge() = default;

    /// ربط حقل نموذج بعنصر أصلي
    virtual void bindFormField(const std::string& nodeId,
                               const FormField& field) = 0;

    /// عرض خطأ تحقق على المنصة
    virtual void showValidationError(const std::string& nodeId,
                                     const std::string& errorMessage) = 0;

    /// مسح أخطاء التحقق
    virtual void clearValidationError(const std::string& nodeId) = 0;

    /// تطبيق حالة نموذج كاملة
    virtual void applyFormState(const FormState& state) = 0;

    /// تفعيل التحقق الأصلي للمنصة (مثل HTML5 validation)
    virtual void enableNativeValidation(const std::string& nodeId, bool enable) = 0;
};

// ═══════════════════════════════════════════════════════════════════════════════
// واجهة جسر التركيز (Focus Bridge)
// ═══════════════════════════════════════════════════════════════════════════════

class FocusBridge {
public:
    virtual ~FocusBridge() = default;

    /// طلب تركيز على عنصر
    virtual void requestFocus(const std::string& nodeId) = 0;

    /// إزالة التركيز
    virtual void clearFocus() = 0;

    /// نقل التركيز في اتجاه
    virtual void moveFocus(FocusDirection direction) = 0;

    /// تعيين ترتيب التنقل
    virtual void setTabIndex(const std::string& nodeId, int tabIndex) = 0;

    /// إنشاء زنزانة تركيز (Focus Trap)
    virtual void createFocusTrap(const std::string& scopeId) = 0;

    /// إزالة زنزانة تركيز
    virtual void releaseFocusTrap(const std::string& scopeId) = 0;

    /// الحصول على العنصر المُركّز حالياً
    virtual std::string getFocusedNodeId() const = 0;
};

// ═══════════════════════════════════════════════════════════════════════════════
// واجهة جسر ذاكرة الصور (ImageCache Bridge)
// ═══════════════════════════════════════════════════════════════════════════════

class ImageCacheBridge {
public:
    virtual ~ImageCacheBridge() = default;

    /// تحميل صورة من مسار باستخدام آلية المنصة
    virtual bool loadImage(const std::string& path,
                           std::function<void(const ImageData&)> callback) = 0;

    /// تحويل ImageData إلى texture/surface أصلي
    virtual void* toNativeTexture(const ImageData& data) = 0;

    /// تحرير texture أصلي
    virtual void releaseNativeTexture(void* texture) = 0;

    /// تفعيل التخزين المؤقت على القرص
    virtual void enableDiskCache(const std::string& cacheDir, size_t maxSize) = 0;

    /// مسح ذاكرة الصور
    virtual void clearCache() = 0;

    /// تحميل مسبق لقائمة صور
    virtual void prefetchImages(const std::vector<std::string>& urls) = 0;
};

// ═══════════════════════════════════════════════════════════════════════════════
// واجهة جسر الإشعارات (Notification Bridge)
// ═══════════════════════════════════════════════════════════════════════════════

class NotificationBridge {
public:
    virtual ~NotificationBridge() = default;

    /// عرض إشعار باستخدام آلية المنصة
    virtual void showNotification(const NotificationData& data) = 0;

    /// إخفاء إشعار
    virtual void dismissNotification(const std::string& id) = 0;

    /// إخفاء جميع الإشعارات
    virtual void dismissAll() = 0;

    /// طلب إذن الإشعارات (للمنصات التي تتطلبه)
    virtual void requestPermission(std::function<void(bool granted)> callback) = 0;

    /// هل الإشعارات مسموحة؟
    virtual bool hasPermission() const = 0;

    /// تعيين قناة إشعارات (Android)
    virtual void setNotificationChannel(const std::string& channelId,
                                         const std::string& channelName) {}
};

// ═══════════════════════════════════════════════════════════════════════════════
// واجهة الجسر الرئيسية (System Bridge)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief الجسر الرئيسي — يوحّد جميع الجسور الفرعية
 *
 * كل backend ينفذ هذه الواجهة ويعيد الجسور المناسبة لمنصته.
 */
class SystemBridge {
public:
    virtual ~SystemBridge() = default;

    /// معلومات المنصة
    virtual PlatformInfo getPlatformInfo() const = 0;

    /// تهيئة الجسر
    virtual bool initialize() = 0;

    /// إيقاف الجسر
    virtual void shutdown() = 0;

    /// هل الجسر جاهز؟
    virtual bool isReady() const = 0;

    // ─── الجسور الفرعية ───

    virtual ThemeBridge& theme() = 0;
    virtual GestureBridge& gesture() = 0;
    virtual ScrollPhysicsBridge& scrollPhysics() = 0;
    virtual AccessibilityBridge& accessibility() = 0;
    virtual FormValidationBridge& formValidation() = 0;
    virtual FocusBridge& focus() = 0;
    virtual ImageCacheBridge& imageCache() = 0;
    virtual NotificationBridge& notification() = 0;

    // ─── مصنع ───

    /// إنشاء جسر للمنصة الحالية تلقائياً
    static std::unique_ptr<SystemBridge> createForCurrentPlatform();

    /// إنشاء جسر لمنصة محددة
    static std::unique_ptr<SystemBridge> createForPlatform(PlatformType type);
};

} // namespace ui
} // namespace sad

#endif // SAD_UI_SYSTEM_BRIDGE_H
