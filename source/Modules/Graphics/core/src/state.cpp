/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: state.cpp
 * المسار: features/graphics/core/src/state.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * تنفيذ نظام إدارة الحالة التفاعلي (StateStore).
 *
 * المنطق الأساسي:
 * - set() → تحديث القيمة → إخطار المراقبين → إعادة حساب المحسوبة
 * - beginBatch()/endBatch() → تجميع التحديثات ثم إخطار مرة واحدة
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/state.h"

#include <sstream>
#include <algorithm>
#include <stdexcept>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// تنفيذ StateStore
// ═══════════════════════════════════════════════════════════════════════════════

StateStore::StateStore() = default;
StateStore::~StateStore() = default;

// ─── تعريف الحالات ──────────────────────────

void StateStore::define(
    const std::string& name,
    const std::string& valueType,
    StateBindingType bindingType,
    std::any initialValue
) {
    StateEntry entry;
    entry.name = name;
    entry.valueType = valueType;
    entry.bindingType = bindingType;
    entry.value = std::move(initialValue);
    entries_[name] = std::move(entry);
}

void StateStore::defineComputed(
    const std::string& name,
    const std::unordered_set<std::string>& dependencies,
    std::function<std::any()> computeFn
) {
    StateEntry entry;
    entry.name = name;
    entry.valueType = "محسوب";
    entry.bindingType = StateBindingType::Computed;
    entry.computeFunction = std::move(computeFn);
    entry.dependencies = dependencies;

    // حساب القيمة الأولية
    if (entry.computeFunction) {
        entry.value = entry.computeFunction();
    }

    entries_[name] = std::move(entry);
}

// ─── القراءة والكتابة ───────────────────────

std::any StateStore::get(const std::string& name) const {
    auto it = entries_.find(name);
    if (it == entries_.end()) {
        return {};
    }
    return it->second.value;
}

void StateStore::set(const std::string& name, std::any value) {
    auto it = entries_.find(name);
    if (it == entries_.end()) {
        return; // حالة غير معرّفة — تتجاهل
    }

    // تحديث القيمة
    it->second.value = std::move(value);

    if (batching_) {
        // في وضع التجميع: تأجيل الإخطار
        pendingNotifications_.insert(name);
    } else {
        // إخطار المراقبين فوراً
        notifyObservers(name);
        // إعادة حساب الحالات المحسوبة المعتمدة
        recomputeDependents(name);
    }
}

bool StateStore::has(const std::string& name) const {
    return entries_.count(name) > 0;
}

// ─── المراقبة ───────────────────────────────

void StateStore::addObserver(
    const std::string& name,
    std::shared_ptr<StateObserver> observer
) {
    auto it = entries_.find(name);
    if (it == entries_.end()) return;

    it->second.observers.push_back(observer);
}

void StateStore::removeObserver(
    const std::string& name,
    const std::shared_ptr<StateObserver>& observer
) {
    auto it = entries_.find(name);
    if (it == entries_.end()) return;

    auto& observers = it->second.observers;
    observers.erase(
        std::remove_if(observers.begin(), observers.end(),
            [&observer](const std::weak_ptr<StateObserver>& weak) {
                auto locked = weak.lock();
                return !locked || locked == observer;
            }),
        observers.end()
    );
}

// ─── التجميع ────────────────────────────────

void StateStore::beginBatch() {
    batching_ = true;
    pendingNotifications_.clear();
}

void StateStore::endBatch() {
    batching_ = false;

    // إخطار جميع المراقبين المعلقين
    for (const auto& name : pendingNotifications_) {
        notifyObservers(name);
        recomputeDependents(name);
    }
    pendingNotifications_.clear();
}

// ─── التصحيح ────────────────────────────────

std::string StateStore::dump() const {
    std::ostringstream ss;
    ss << "=== مخزن الحالة (" << entries_.size() << " حالة) ===\n";

    for (const auto& [name, entry] : entries_) {
        ss << "  " << name
           << " [" << entry.valueType << "]"
           << " (مراقبون: " << entry.observers.size() << ")";

        if (entry.bindingType == StateBindingType::Computed) {
            ss << " [محسوب]";
        }
        ss << "\n";
    }

    return ss.str();
}

// ─── دوال داخلية ────────────────────────────

void StateStore::notifyObservers(const std::string& name) {
    auto it = entries_.find(name);
    if (it == entries_.end()) return;

    auto& entry = it->second;

    // تنظيف المراقبين المنتهيين
    cleanupExpiredObservers(name);

    // إخطار المراقبين النشطين
    for (auto& weak : entry.observers) {
        if (auto observer = weak.lock()) {
            observer->onStateChanged(name, entry.value);
        }
    }
}

void StateStore::recomputeDependents(const std::string& changedState) {
    // البحث عن حالات محسوبة تعتمد على الحالة التي تغيرت
    for (auto& [name, entry] : entries_) {
        if (entry.bindingType == StateBindingType::Computed &&
            entry.dependencies.count(changedState) > 0 &&
            entry.computeFunction) {

            // إعادة حساب القيمة
            entry.value = entry.computeFunction();

            // إخطار مراقبي الحالة المحسوبة
            notifyObservers(name);

            // تكرار: الحالات المحسوبة قد تعتمد على محسوبة أخرى
            // (ملاحظة: في الإصدار الحالي، ندعم مستوى واحد فقط)
        }
    }
}

void StateStore::cleanupExpiredObservers(const std::string& name) {
    auto it = entries_.find(name);
    if (it == entries_.end()) return;

    auto& observers = it->second.observers;
    observers.erase(
        std::remove_if(observers.begin(), observers.end(),
            [](const std::weak_ptr<StateObserver>& weak) {
                return weak.expired();
            }),
        observers.end()
    );
}

} // namespace ui
} // namespace sad
