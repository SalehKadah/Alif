/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: form_validation.cpp
 * المسار: features/graphics/core/src/form_validation.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * تنفيذ نظام التحقق من النماذج.
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/form_validation.h"
#include <algorithm>
#include <cctype>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// مُحققات جاهزة
// ═══════════════════════════════════════════════════════════════════════════════

ValidationResult RequiredValidator::validate(const std::string& value) const {
    if (value.empty()) return ValidationResult::invalid(message_);
    // تحقق من وجود محتوى غير فارغ
    bool allWhitespace = std::all_of(value.begin(), value.end(),
        [](unsigned char c) { return std::isspace(c); });
    if (allWhitespace) return ValidationResult::invalid(message_);
    return ValidationResult::valid();
}

ValidationResult MinLengthValidator::validate(const std::string& value) const {
    if (value.empty()) return ValidationResult::valid(); // required يتعامل مع الفراغ
    if (value.length() < minLength_) {
        std::string msg = message_;
        if (msg.empty())
            msg = "يجب أن يكون " + std::to_string(minLength_) + " حرف على الأقل";
        return ValidationResult::invalid(msg);
    }
    return ValidationResult::valid();
}

ValidationResult MaxLengthValidator::validate(const std::string& value) const {
    if (value.length() > maxLength_) {
        std::string msg = message_;
        if (msg.empty())
            msg = "يجب ألا يتجاوز " + std::to_string(maxLength_) + " حرف";
        return ValidationResult::invalid(msg);
    }
    return ValidationResult::valid();
}

ValidationResult EmailValidator::validate(const std::string& value) const {
    if (value.empty()) return ValidationResult::valid();
    // تحقق بسيط: يحتوي @ و. بعده
    auto at = value.find('@');
    if (at == std::string::npos || at == 0) return ValidationResult::invalid(message_);
    auto dot = value.find('.', at + 1);
    if (dot == std::string::npos || dot == value.length() - 1)
        return ValidationResult::invalid(message_);
    // لا مسافات
    if (value.find(' ') != std::string::npos) return ValidationResult::invalid(message_);
    return ValidationResult::valid();
}

ValidationResult PatternValidator::validate(const std::string& value) const {
    if (value.empty()) return ValidationResult::valid();
    try {
        std::regex re(pattern_);
        if (!std::regex_match(value, re))
            return ValidationResult::invalid(message_);
    } catch (...) {
        return ValidationResult::invalid("نمط regex غير صالح");
    }
    return ValidationResult::valid();
}

ValidationResult NumericValidator::validate(const std::string& value) const {
    if (value.empty()) return ValidationResult::valid();
    try {
        size_t pos = 0;
        std::stod(value, &pos);
        if (pos != value.length()) return ValidationResult::invalid(message_);
    } catch (...) {
        return ValidationResult::invalid(message_);
    }
    return ValidationResult::valid();
}

ValidationResult RangeValidator::validate(const std::string& value) const {
    if (value.empty()) return ValidationResult::valid();
    try {
        double num = std::stod(value);
        if (num < min_ || num > max_) {
            std::string msg = message_;
            if (msg.empty())
                msg = "يجب أن يكون بين " + std::to_string(min_) + " و " + std::to_string(max_);
            return ValidationResult::invalid(msg);
        }
    } catch (...) {
        return ValidationResult::invalid("يجب أن يكون رقمًا");
    }
    return ValidationResult::valid();
}

ValidationResult CustomValidator::validate(const std::string& value) const {
    if (func_) return func_(value);
    return ValidationResult::valid();
}

// ═══════════════════════════════════════════════════════════════════════════════
// FormField
// ═══════════════════════════════════════════════════════════════════════════════

FormField::FormField(const std::string& name, const std::string& initialValue) {
    state_.name = name;
    state_.value = initialValue;
}

FormField& FormField::required(const std::string& msg) {
    validators_.push_back(std::make_shared<RequiredValidator>(msg));
    return *this;
}

FormField& FormField::minLength(size_t min, const std::string& msg) {
    validators_.push_back(std::make_shared<MinLengthValidator>(min, msg));
    return *this;
}

FormField& FormField::maxLength(size_t max, const std::string& msg) {
    validators_.push_back(std::make_shared<MaxLengthValidator>(max, msg));
    return *this;
}

FormField& FormField::email(const std::string& msg) {
    validators_.push_back(std::make_shared<EmailValidator>(msg));
    return *this;
}

FormField& FormField::pattern(const std::string& pat, const std::string& msg) {
    validators_.push_back(std::make_shared<PatternValidator>(pat, msg));
    return *this;
}

FormField& FormField::numeric(const std::string& msg) {
    validators_.push_back(std::make_shared<NumericValidator>(msg));
    return *this;
}

FormField& FormField::range(double min, double max, const std::string& msg) {
    validators_.push_back(std::make_shared<RangeValidator>(min, max, msg));
    return *this;
}

FormField& FormField::custom(ValidatorFunc func) {
    validators_.push_back(std::make_shared<CustomValidator>(std::move(func)));
    return *this;
}

void FormField::setValue(const std::string& value) {
    state_.value = value;
    state_.isDirty = true;
    auto result = validate();
    state_.isValid = result.isValid;
    state_.errorMessage = result.errorMessage;
    state_.hasBeenValidated = true;
    notifyChange();
}

void FormField::markTouched() {
    state_.isTouched = true;
}

void FormField::reset() {
    state_.value.clear();
    state_.errorMessage.clear();
    state_.isValid = true;
    state_.isDirty = false;
    state_.isTouched = false;
    state_.hasBeenValidated = false;
    notifyChange();
}

ValidationResult FormField::validate() const {
    for (const auto& v : validators_) {
        auto result = v->validate(state_.value);
        if (!result.isValid) return result;
    }
    return ValidationResult::valid();
}

bool FormField::isValid() const {
    return validate().isValid;
}

void FormField::notifyChange() {
    if (changeCb_) changeCb_(state_);
}

// ═══════════════════════════════════════════════════════════════════════════════
// FormState
// ═══════════════════════════════════════════════════════════════════════════════

FormState::FormState(ValidationMode mode) : mode_(mode) {}

FormField& FormState::addField(const std::string& name, const std::string& initialValue) {
    fields_.push_back(std::make_unique<FormField>(name, initialValue));
    return *fields_.back();
}

FormField* FormState::field(const std::string& name) {
    for (auto& f : fields_) {
        if (f->name() == name) return f.get();
    }
    return nullptr;
}

const FormField* FormState::field(const std::string& name) const {
    for (const auto& f : fields_) {
        if (f->name() == name) return f.get();
    }
    return nullptr;
}

bool FormState::validate() {
    bool allValid = true;
    for (auto& f : fields_) {
        auto result = f->validate();
        if (!result.isValid) {
            allValid = false;
            // تحديث الحالة
            FieldState s = f->state();
            f->setValue(f->value()); // يحدّث الحالة
        }
    }
    if (validityCb_) validityCb_(allValid);
    return allValid;
}

bool FormState::isValid() const {
    for (const auto& f : fields_) {
        if (!f->isValid()) return false;
    }
    return true;
}

bool FormState::submit() {
    hasSubmitted_ = true;
    if (!validate()) return false;
    if (submitCb_) submitCb_(values());
    return true;
}

void FormState::reset() {
    hasSubmitted_ = false;
    for (auto& f : fields_) f->reset();
}

std::unordered_map<std::string, std::string> FormState::values() const {
    std::unordered_map<std::string, std::string> result;
    for (const auto& f : fields_)
        result[f->name()] = f->value();
    return result;
}

std::unordered_map<std::string, std::string> FormState::errors() const {
    std::unordered_map<std::string, std::string> result;
    for (const auto& f : fields_) {
        auto r = f->validate();
        if (!r.isValid) result[f->name()] = r.errorMessage;
    }
    return result;
}

} // namespace ui
} // namespace sad
