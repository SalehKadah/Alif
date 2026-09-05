/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: notification.cpp
 * المسار: features/graphics/core/src/notification.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * تنفيذ نظام الإشعارات.
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/notification.h"
#include <algorithm>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// NotificationManager
// ═══════════════════════════════════════════════════════════════════════════════

NotificationManager& NotificationManager::instance() {
    static NotificationManager inst;
    return inst;
}

int NotificationManager::showToast(const std::string& message, uint32_t durationMs) {
    NotificationData data;
    data.type = NotificationType::Toast;
    data.message = message;
    data.durationMs = durationMs;
    data.position = config_.defaultPosition;
    return show(data);
}

int NotificationManager::showSnackbar(const std::string& message,
                                        const std::string& actionLabel,
                                        std::function<void()> action) {
    NotificationData data;
    data.type = NotificationType::Snackbar;
    data.message = message;
    data.durationMs = 5000;
    data.position = NotificationPosition::BottomCenter;

    NotificationAction act;
    act.label = actionLabel;
    act.onAction = std::move(action);
    act.dismissOnAction = true;
    data.actions.push_back(std::move(act));

    return show(data);
}

int NotificationManager::showBanner(const std::string& title, const std::string& message) {
    NotificationData data;
    data.type = NotificationType::Banner;
    data.title = title;
    data.message = message;
    data.durationMs = 0; // دائم
    data.autoDismiss = false;
    data.position = NotificationPosition::TopCenter;
    return show(data);
}

int NotificationManager::show(const NotificationData& data) {
    auto notif = std::make_shared<NotificationData>(data);
    notif->id = nextId_++;
    notif->state = NotificationState::Queued;

    // تحقق من التجميع
    if (config_.groupSimilar && !notif->groupKey.empty()) {
        for (auto& existing : active_) {
            if (existing->groupKey == notif->groupKey &&
                existing->state == NotificationState::Showing) {
                // تحديث القائم بدلاً من إضافة جديد
                existing->message = notif->message;
                existing->title = notif->title;
                return existing->id;
            }
        }
    }

    // تحقق من حد الطابور
    if (queue_.size() >= static_cast<size_t>(config_.maxQueue)) {
        queue_.pop_front();
    }

    // إدخال بالأولوية
    if (notif->priority >= NotificationPriority::High) {
        queue_.push_front(notif);
    } else {
        queue_.push_back(notif);
    }

    return notif->id;
}

void NotificationManager::dismiss(int notificationId) {
    for (auto it = active_.begin(); it != active_.end(); ++it) {
        if ((*it)->id == notificationId) {
            (*it)->state = NotificationState::Dismissed;
            if ((*it)->onDismiss) (*it)->onDismiss();
            if (dismissedCb_) dismissedCb_(**it);
            active_.erase(it);
            return;
        }
    }
    // أيضاً حذف من الطابور
    queue_.erase(
        std::remove_if(queue_.begin(), queue_.end(),
            [notificationId](const auto& n) { return n->id == notificationId; }),
        queue_.end());
}

void NotificationManager::dismissAll() {
    for (auto& notif : active_) {
        notif->state = NotificationState::Dismissed;
        if (notif->onDismiss) notif->onDismiss();
        if (dismissedCb_) dismissedCb_(*notif);
    }
    active_.clear();
    queue_.clear();
}

void NotificationManager::dismissGroup(const std::string& groupKey) {
    active_.erase(
        std::remove_if(active_.begin(), active_.end(),
            [&](const auto& n) {
                if (n->groupKey == groupKey) {
                    n->state = NotificationState::Dismissed;
                    if (n->onDismiss) n->onDismiss();
                    if (dismissedCb_) dismissedCb_(*n);
                    return true;
                }
                return false;
            }),
        active_.end());

    queue_.erase(
        std::remove_if(queue_.begin(), queue_.end(),
            [&](const auto& n) { return n->groupKey == groupKey; }),
        queue_.end());
}

void NotificationManager::update(uint64_t currentTimeMs) {
    // تحديث الرسوم المتحركة والمدة
    for (auto it = active_.begin(); it != active_.end();) {
        auto& notif = *it;

        if (notif->state == NotificationState::Showing) {
            // رسوم ظهور
            if (notif->animationProgress < 1.0f) {
                float elapsed = static_cast<float>(currentTimeMs - notif->showTime);
                notif->animationProgress =
                    std::min(1.0f, elapsed / notif->animationDurationMs);
            }

            // إخفاء تلقائي
            if (notif->autoDismiss && notif->durationMs > 0) {
                uint64_t elapsed = currentTimeMs - notif->showTime;
                if (elapsed >= notif->durationMs) {
                    notif->state = NotificationState::Hiding;
                }
            }
        }

        if (notif->state == NotificationState::Hiding) {
            // رسوم اختفاء
            notif->animationProgress =
                std::max(0.0f, notif->animationProgress - 0.05f);
            if (notif->animationProgress <= 0) {
                notif->state = NotificationState::Dismissed;
                if (notif->onDismiss) notif->onDismiss();
                if (dismissedCb_) dismissedCb_(*notif);
                it = active_.erase(it);
                continue;
            }
        }
        ++it;
    }

    // معالجة الطابور
    processQueue(currentTimeMs);
}

std::vector<NotificationData*> NotificationManager::visibleNotifications() {
    std::vector<NotificationData*> result;
    for (auto& notif : active_) {
        if (notif->state == NotificationState::Showing ||
            notif->state == NotificationState::Hiding) {
            result.push_back(notif.get());
        }
    }
    return result;
}

size_t NotificationManager::visibleCount() const {
    size_t count = 0;
    for (const auto& n : active_) {
        if (n->state == NotificationState::Showing ||
            n->state == NotificationState::Hiding)
            ++count;
    }
    return count;
}

void NotificationManager::processQueue(uint64_t currentTimeMs) {
    while (visibleCount() < static_cast<size_t>(config_.maxVisible) && !queue_.empty()) {
        showNext(currentTimeMs);
    }
}

void NotificationManager::showNext(uint64_t currentTimeMs) {
    if (queue_.empty()) return;
    auto notif = queue_.front();
    queue_.pop_front();

    notif->state = NotificationState::Showing;
    notif->showTime = currentTimeMs;
    notif->animationProgress = 0;

    if (notif->onShow) notif->onShow();
    if (shownCb_) shownCb_(*notif);

    active_.push_back(notif);
}

} // namespace ui
} // namespace sad
