/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: notification.h
 * المسار: features/graphics/core/include/sad_ui/notification.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * نظام الإشعارات (Notification System).
 *
 * يدعم:
 * - إشعارات Toast (مؤقتة)
 * - إشعارات Snackbar (مع إجراء)
 * - إشعارات Banner (ثابتة)
 * - إشعارات Push (نظامية)
 * - أولويات وطوابير
 * - إخفاء تلقائي مع مدة قابلة للتخصيص
 * - رسوم متحركة (ظهور/اختفاء)
 * - تجميع الإشعارات
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_NOTIFICATION_H
#define SAD_UI_NOTIFICATION_H

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <deque>
#include <cstdint>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// أنواع الإشعارات
// ═══════════════════════════════════════════════════════════════════════════════

enum class NotificationType : uint8_t {
    Toast,          ///< رسالة مؤقتة تختفي ذاتياً
    Snackbar,       ///< رسالة مع زر إجراء
    Banner,         ///< شريط ثابت أعلى/أسفل الشاشة
    Dialog,         ///< حوار إشعار (يحتاج تأكيد)
    Push            ///< إشعار نظامي
};

enum class NotificationPriority : uint8_t {
    Low,
    Normal,
    High,
    Urgent
};

enum class NotificationPosition : uint8_t {
    TopLeft,
    TopCenter,
    TopRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
    Center
};

enum class NotificationState : uint8_t {
    Queued,         ///< في الطابور
    Showing,        ///< يظهر حالياً
    Hiding,         ///< يختفي (رسوم متحركة)
    Dismissed       ///< تم إخفاؤه
};

// ═══════════════════════════════════════════════════════════════════════════════
// إجراء الإشعار
// ═══════════════════════════════════════════════════════════════════════════════

struct NotificationAction {
    std::string label;
    std::function<void()> onAction;
    bool dismissOnAction = true;
};

// ═══════════════════════════════════════════════════════════════════════════════
// بيانات الإشعار
// ═══════════════════════════════════════════════════════════════════════════════

struct NotificationData {
    int id = -1;
    NotificationType type = NotificationType::Toast;
    NotificationPriority priority = NotificationPriority::Normal;
    NotificationPosition position = NotificationPosition::BottomCenter;

    // ─── محتوى ───
    std::string title;
    std::string message;
    std::string icon;           ///< اسم الأيقونة أو مسار
    std::string groupKey;       ///< مفتاح التجميع

    // ─── إجراء ───
    std::vector<NotificationAction> actions;

    // ─── مدة ───
    uint32_t durationMs = 3000; ///< مدة العرض (0 = دائم)
    bool autoDismiss = true;

    // ─── رسوم متحركة ───
    uint32_t animationDurationMs = 300;

    // ─── حالة ───
    NotificationState state = NotificationState::Queued;
    uint64_t showTime = 0;      ///< وقت ظهور
    float animationProgress = 0; ///< 0-1

    // ─── مستمعون ───
    std::function<void()> onShow;
    std::function<void()> onDismiss;
    std::function<void()> onTap;
};

// ═══════════════════════════════════════════════════════════════════════════════
// إعدادات الإشعارات
// ═══════════════════════════════════════════════════════════════════════════════

struct NotificationConfig {
    int maxVisible = 3;                 ///< أقصى عدد ظاهر
    int maxQueue = 50;                  ///< أقصى حجم الطابور
    bool stackNotifications = true;     ///< تكديس الإشعارات
    float spacing = 8.0f;              ///< المسافة بين الإشعارات
    float margin = 16.0f;             ///< الهامش من الحافة
    bool groupSimilar = true;          ///< تجميع المتشابهة
    NotificationPosition defaultPosition = NotificationPosition::BottomCenter;
    uint32_t defaultDuration = 3000;
};

// ═══════════════════════════════════════════════════════════════════════════════
// مدير الإشعارات (Notification Manager)
// ═══════════════════════════════════════════════════════════════════════════════

class NotificationManager {
public:
    static NotificationManager& instance();

    /// الإعدادات
    void configure(const NotificationConfig& config) { config_ = config; }
    const NotificationConfig& config() const { return config_; }

    /// ─── إظهار إشعارات ───

    /// إشعار toast بسيط
    int showToast(const std::string& message, uint32_t durationMs = 3000);

    /// إشعار snackbar مع إجراء
    int showSnackbar(const std::string& message, const std::string& actionLabel,
                     std::function<void()> action);

    /// إشعار banner
    int showBanner(const std::string& title, const std::string& message);

    /// إشعار مخصص كامل
    int show(const NotificationData& data);

    /// ─── إدارة ───

    /// إخفاء إشعار
    void dismiss(int notificationId);

    /// إخفاء الكل
    void dismissAll();

    /// إخفاء بمجموعة
    void dismissGroup(const std::string& groupKey);

    /// تحديث (يُستدعى كل إطار)
    void update(uint64_t currentTimeMs);

    /// ─── استعلام ───

    /// الإشعارات الظاهرة
    std::vector<NotificationData*> visibleNotifications();

    /// الإشعارات في الطابور
    size_t queueSize() const { return queue_.size(); }

    /// عدد الظاهر
    size_t visibleCount() const;

    /// ─── مستمعون ───

    using NotificationCallback = std::function<void(const NotificationData&)>;
    void onNotificationShown(NotificationCallback cb) { shownCb_ = cb; }
    void onNotificationDismissed(NotificationCallback cb) { dismissedCb_ = cb; }

private:
    NotificationManager() = default;
    NotificationConfig config_;
    std::vector<std::shared_ptr<NotificationData>> active_;
    std::deque<std::shared_ptr<NotificationData>> queue_;
    int nextId_ = 1;

    NotificationCallback shownCb_;
    NotificationCallback dismissedCb_;

    void processQueue(uint64_t currentTimeMs);
    void showNext(uint64_t currentTimeMs);
};

} // namespace ui
} // namespace sad

#endif // SAD_UI_NOTIFICATION_H
