/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: animation.cpp
 * المسار: features/graphics/core/src/animation.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * تنفيذ محرك الانيميشن.
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/animation.h"
#include <algorithm>

namespace sad {
namespace ui {

void AnimationEngine::animate(const Animation& anim, uint32_t currentTimeMs) {
    auto key = makeKey(anim.targetNodeId, anim.propertyKey);
    
    ActiveAnimation active;
    active.spec = anim;
    active.startTimeMs = currentTimeMs + anim.delayMs;
    active.currentValue = anim.fromValue;
    active.currentRepeat = 0;
    active.isReversed = false;
    active.isFinished = false;
    
    activeAnimations_[key] = std::move(active);
}

void AnimationEngine::animateColor(const ColorAnimation& anim, uint32_t currentTimeMs) {
    auto key = makeKey(anim.targetNodeId, anim.propertyKey);
    
    ActiveColorAnimation active;
    active.spec = anim;
    active.startTimeMs = currentTimeMs + anim.delayMs;
    active.currentR = anim.fromR;
    active.currentG = anim.fromG;
    active.currentB = anim.fromB;
    active.currentA = anim.fromA;
    active.isFinished = false;
    
    activeColorAnimations_[key] = std::move(active);
}

bool AnimationEngine::update(uint32_t currentTimeMs) {
    bool anyActive = false;
    
    // تحديث التحريكات الرقمية
    for (auto it = activeAnimations_.begin(); it != activeAnimations_.end(); ) {
        auto& active = it->second;
        
        if (active.isFinished) {
            if (active.spec.onComplete) {
                active.spec.onComplete();
            }
            it = activeAnimations_.erase(it);
            continue;
        }
        
        // لم يبدأ بعد (تأخير)
        if (currentTimeMs < active.startTimeMs) {
            anyActive = true;
            ++it;
            continue;
        }
        
        uint32_t elapsed = currentTimeMs - active.startTimeMs;
        double progress = static_cast<double>(elapsed) / static_cast<double>(active.spec.durationMs);
        
        if (progress >= 1.0) {
            // التحقق من التكرار
            if (active.spec.repeatCount == -1 || active.currentRepeat < active.spec.repeatCount) {
                active.currentRepeat++;
                active.startTimeMs = currentTimeMs;
                if (active.spec.autoReverse) {
                    active.isReversed = !active.isReversed;
                }
                progress = 0.0;
            } else {
                progress = 1.0;
                active.isFinished = true;
            }
        }
        
        // تطبيق التسهيل
        double easedProgress = applyEasing(active.spec.easing, progress);
        
        // حساب القيمة
        double from = active.isReversed ? active.spec.toValue : active.spec.fromValue;
        double to = active.isReversed ? active.spec.fromValue : active.spec.toValue;
        active.currentValue = from + (to - from) * easedProgress;
        
        anyActive = true;
        ++it;
    }
    
    // تحديث تحريكات الألوان
    for (auto it = activeColorAnimations_.begin(); it != activeColorAnimations_.end(); ) {
        auto& active = it->second;
        
        if (active.isFinished) {
            if (active.spec.onComplete) {
                active.spec.onComplete();
            }
            it = activeColorAnimations_.erase(it);
            continue;
        }
        
        if (currentTimeMs < active.startTimeMs) {
            anyActive = true;
            ++it;
            continue;
        }
        
        uint32_t elapsed = currentTimeMs - active.startTimeMs;
        double progress = static_cast<double>(elapsed) / static_cast<double>(active.spec.durationMs);
        
        if (progress >= 1.0) {
            progress = 1.0;
            active.isFinished = true;
        }
        
        double ep = applyEasing(active.spec.easing, progress);
        float ef = static_cast<float>(ep);
        
        active.currentR = active.spec.fromR + (active.spec.toR - active.spec.fromR) * ef;
        active.currentG = active.spec.fromG + (active.spec.toG - active.spec.fromG) * ef;
        active.currentB = active.spec.fromB + (active.spec.toB - active.spec.fromB) * ef;
        active.currentA = active.spec.fromA + (active.spec.toA - active.spec.fromA) * ef;
        
        anyActive = true;
        ++it;
    }
    
    return anyActive;
}

double AnimationEngine::getValue(const std::string& nodeId, 
                                  const std::string& propertyKey,
                                  double defaultValue) const {
    auto key = makeKey(nodeId, propertyKey);
    auto it = activeAnimations_.find(key);
    if (it != activeAnimations_.end()) {
        return it->second.currentValue;
    }
    return defaultValue;
}

bool AnimationEngine::hasAnimation(const std::string& nodeId, 
                                    const std::string& propertyKey) const {
    auto key = makeKey(nodeId, propertyKey);
    return activeAnimations_.count(key) > 0 || activeColorAnimations_.count(key) > 0;
}

void AnimationEngine::cancelAnimations(const std::string& nodeId) {
    for (auto it = activeAnimations_.begin(); it != activeAnimations_.end(); ) {
        if (it->second.spec.targetNodeId == nodeId) {
            it = activeAnimations_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = activeColorAnimations_.begin(); it != activeColorAnimations_.end(); ) {
        if (it->second.spec.targetNodeId == nodeId) {
            it = activeColorAnimations_.erase(it);
        } else {
            ++it;
        }
    }
}

void AnimationEngine::cancelAll() {
    activeAnimations_.clear();
    activeColorAnimations_.clear();
}

} // namespace ui
} // namespace sad
