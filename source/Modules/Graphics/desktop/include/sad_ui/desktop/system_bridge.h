/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: system_bridge.h
 * المسار: features/graphics/backends/desktop/include/sad_ui/desktop/system_bridge.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * جسر سطح المكتب (Desktop System Bridge).
 *
 * يربط الأنظمة الثمانية بـ SDL2 + OpenGL:
 * - Theme → SDL_SetRenderDrawColor / ألوان SDL
 * - Gesture → SDL_Event (ماوس + لوحة مفاتيح)
 * - ScrollPhysics → SDL_MOUSEWHEEL events
 * - Accessibility → OS accessibility APIs (AT-SPI/MSAA/NSAccessibility)
 * - FormValidation → SDL عناصر الإدخال
 * - Focus → SDL keyboard focus + tab navigation
 * - ImageCache → SDL_Surface / SDL_Texture
 * - Notification → SDL overlay / OS notifications
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_DESKTOP_SYSTEM_BRIDGE_H
#define SAD_UI_DESKTOP_SYSTEM_BRIDGE_H

#include "sad_ui/system_bridge.h"

#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// تصريحات أمامية لـ SDL2
struct SDL_Renderer;

namespace sad {
namespace ui {
namespace desktop {

// ═══════════════════════════════════════════════════════════════════════════════
// جسر السمات — Desktop
// ═══════════════════════════════════════════════════════════════════════════════

class DesktopThemeBridge : public ThemeBridge {
public:
    void applyColorScheme(const ColorScheme& scheme) override;
    void applyTextTheme(const TextTheme& textTheme) override;
    void setDarkMode(bool dark) override;
    bool isSystemDarkMode() const override;
    uint32_t toNativeColor(const ThemeColor& color) const override;
    void applyTheme(const ThemeProvider& provider) override;

    /// SDL-specific: الحصول على اللون كـ RGBA bytes
    void toSDLColor(const ThemeColor& color,
                    uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a) const;

    const ColorScheme& getCurrentScheme() const { return currentScheme_; }

private:
    ColorScheme currentScheme_;
    TextTheme currentTextTheme_;
    bool darkMode_ = false;
};

// ═══════════════════════════════════════════════════════════════════════════════
// جسر الإيماءات — Desktop
// ═══════════════════════════════════════════════════════════════════════════════

class DesktopGestureBridge : public GestureBridge {
public:
    bool translateNativeEvent(void* nativeEvent, GestureEvent& outEvent) override;
    void registerDetector(const std::string& nodeId,
                          std::shared_ptr<GestureDetector> detector) override;
    void unregisterDetector(const std::string& nodeId) override;
    void setGestureEnabled(GestureType type, bool enabled) override;
    bool supportsGesture(GestureType type) const override;

    /// SDL-specific: معالجة SDL_Event مباشرة
    bool processSDLEvent(const void* sdlEvent, GestureEvent& outEvent);

private:
    std::unordered_map<std::string, std::shared_ptr<GestureDetector>> detectors_;
    std::unordered_set<GestureType> disabledGestures_;

    // تتبع حالة الماوس لتحويل أحداث SDL
    bool mouseDown_ = false;
    GesturePoint lastMousePos_;
    std::chrono::steady_clock::time_point lastClickTime_;
    GesturePoint lastClickPos_;
    int clickCount_ = 0;
};

// ═══════════════════════════════════════════════════════════════════════════════
// جسر فيزياء التمرير — Desktop
// ═══════════════════════════════════════════════════════════════════════════════

class DesktopScrollPhysicsBridge : public ScrollPhysicsBridge {
public:
    std::shared_ptr<ScrollPhysics> getPlatformScrollPhysics() const override;
    void applyNativeScroll(const std::string& nodeId,
                           const ScrollMetrics& metrics) override;
    void animateScrollTo(const std::string& nodeId,
                         float targetOffset, float durationMs) override;
    bool translateScrollEvent(void* nativeEvent,
                              ScrollMetrics& outMetrics) override;
private:
    std::unordered_map<std::string, ScrollMetrics> scrollStates_;
};

// ═══════════════════════════════════════════════════════════════════════════════
// جسر إمكانية الوصول — Desktop
// ═══════════════════════════════════════════════════════════════════════════════

class DesktopAccessibilityBridge : public AccessibilityBridge {
public:
    void applySemanticTree(const SemanticTree& tree) override;
    void updateSemanticNode(const SemanticNode& node) override;
    void announce(const std::string& message,
                  LiveRegion priority = LiveRegion::Polite) override;
    void setNodeLabel(const std::string& nodeId,
                      const std::string& label) override;
    void setNodeRole(const std::string& nodeId,
                     SemanticRole role) override;
    bool isScreenReaderActive() const override;

private:
    std::unordered_map<std::string, SemanticNode> nodeCache_;
};

// ═══════════════════════════════════════════════════════════════════════════════
// جسر التحقق من النماذج — Desktop
// ═══════════════════════════════════════════════════════════════════════════════

class DesktopFormValidationBridge : public FormValidationBridge {
public:
    void bindFormField(const std::string& nodeId,
                       const FormField& field) override;
    void showValidationError(const std::string& nodeId,
                             const std::string& errorMessage) override;
    void clearValidationError(const std::string& nodeId) override;
    void applyFormState(const FormState& state) override;
    void enableNativeValidation(const std::string& nodeId, bool enable) override;

    /// الحصول على رسالة الخطأ لعقدة
    std::string getError(const std::string& nodeId) const;

private:
    std::unordered_map<std::string, FormField> boundFields_;
    std::unordered_map<std::string, std::string> errorMessages_;
};

// ═══════════════════════════════════════════════════════════════════════════════
// جسر التركيز — Desktop
// ═══════════════════════════════════════════════════════════════════════════════

class DesktopFocusBridge : public FocusBridge {
public:
    void requestFocus(const std::string& nodeId) override;
    void clearFocus() override;
    void moveFocus(FocusDirection direction) override;
    void setTabIndex(const std::string& nodeId, int tabIndex) override;
    void createFocusTrap(const std::string& scopeId) override;
    void releaseFocusTrap(const std::string& scopeId) override;
    std::string getFocusedNodeId() const override;

private:
    std::string focusedNodeId_;
    std::unordered_map<std::string, int> tabIndices_;
    std::unordered_set<std::string> focusTraps_;
};

// ═══════════════════════════════════════════════════════════════════════════════
// جسر ذاكرة الصور — Desktop
// ═══════════════════════════════════════════════════════════════════════════════

class DesktopImageCacheBridge : public ImageCacheBridge {
public:
    DesktopImageCacheBridge();
    ~DesktopImageCacheBridge();

    bool loadImage(const std::string& path,
                   std::function<void(const ImageData&)> callback) override;
    void* toNativeTexture(const ImageData& data) override;
    void releaseNativeTexture(void* texture) override;
    void enableDiskCache(const std::string& cacheDir, size_t maxSize) override;
    void clearCache() override;
    void prefetchImages(const std::vector<std::string>& urls) override;

    /// SDL-specific: تعيين SDL_Renderer للتحويل إلى textures
    void setRenderer(SDL_Renderer* renderer) { renderer_ = renderer; }

private:
    SDL_Renderer* renderer_ = nullptr;
    std::unordered_map<std::string, void*> textureCache_;
    std::string diskCacheDir_;
    size_t maxDiskSize_ = 0;
};

// ═══════════════════════════════════════════════════════════════════════════════
// جسر الإشعارات — Desktop
// ═══════════════════════════════════════════════════════════════════════════════

class DesktopNotificationBridge : public NotificationBridge {
public:
    void showNotification(const NotificationData& data) override;
    void dismissNotification(const std::string& id) override;
    void dismissAll() override;
    void requestPermission(std::function<void(bool)> callback) override;
    bool hasPermission() const override;

    /// الحصول على الإشعارات النشطة (للرسم بواسطة المُصيّر)
    const std::vector<NotificationData>& getActiveNotifications() const {
        return activeNotifications_;
    }

private:
    std::vector<NotificationData> activeNotifications_;
    bool permissionGranted_ = true;  // سطح المكتب لا يحتاج إذن
};

// ═══════════════════════════════════════════════════════════════════════════════
// الجسر الرئيسي — Desktop
// ═══════════════════════════════════════════════════════════════════════════════

class DesktopSystemBridge : public SystemBridge {
public:
    DesktopSystemBridge();
    ~DesktopSystemBridge();

    PlatformInfo getPlatformInfo() const override;
    bool initialize() override;
    void shutdown() override;
    bool isReady() const override;

    ThemeBridge& theme() override { return theme_; }
    GestureBridge& gesture() override { return gesture_; }
    ScrollPhysicsBridge& scrollPhysics() override { return scrollPhysics_; }
    AccessibilityBridge& accessibility() override { return accessibility_; }
    FormValidationBridge& formValidation() override { return formValidation_; }
    FocusBridge& focus() override { return focus_; }
    ImageCacheBridge& imageCache() override { return imageCache_; }
    NotificationBridge& notification() override { return notification_; }

    /// SDL-specific: تعيين SDL_Renderer
    void setRenderer(SDL_Renderer* renderer);

private:
    bool ready_ = false;
    DesktopThemeBridge theme_;
    DesktopGestureBridge gesture_;
    DesktopScrollPhysicsBridge scrollPhysics_;
    DesktopAccessibilityBridge accessibility_;
    DesktopFormValidationBridge formValidation_;
    DesktopFocusBridge focus_;
    DesktopImageCacheBridge imageCache_;
    DesktopNotificationBridge notification_;
};

} // namespace desktop
} // namespace ui
} // namespace sad

#endif // SAD_UI_DESKTOP_SYSTEM_BRIDGE_H
