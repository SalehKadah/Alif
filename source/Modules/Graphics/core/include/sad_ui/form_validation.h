/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: form_validation.h
 * المسار: features/graphics/core/include/sad_ui/form_validation.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * نظام التحقق من النماذج (Form Validation System).
 *
 * يدعم:
 * - مُحققات جاهزة (مطلوب، بريد، طول، نمط regex...)
 * - مُحققات مخصصة
 * - حالة حقل ونموذج كامل
 * - رسائل خطأ مخصصة (عربي/إنجليزي)
 * - تحقق فوري (onChange) أو عند الإرسال (onSubmit)
 * - ربط تفاعلي مع الحالة
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_FORM_VALIDATION_H
#define SAD_UI_FORM_VALIDATION_H

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>
#include <optional>
#include <regex>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// نتيجة التحقق
// ═══════════════════════════════════════════════════════════════════════════════

struct ValidationResult {
    bool isValid = true;
    std::string errorMessage;

    static ValidationResult valid() { return {true, ""}; }
    static ValidationResult invalid(const std::string& msg) { return {false, msg}; }
};

// ═══════════════════════════════════════════════════════════════════════════════
// مُحقق (Validator)
// ═══════════════════════════════════════════════════════════════════════════════

using ValidatorFunc = std::function<ValidationResult(const std::string& value)>;

class Validator {
public:
    virtual ~Validator() = default;
    virtual ValidationResult validate(const std::string& value) const = 0;
};

// ─── مُحققات جاهزة ───

class RequiredValidator : public Validator {
public:
    explicit RequiredValidator(const std::string& msg = "هذا الحقل مطلوب")
        : message_(msg) {}
    ValidationResult validate(const std::string& value) const override;
private:
    std::string message_;
};

class MinLengthValidator : public Validator {
public:
    MinLengthValidator(size_t min, const std::string& msg = "")
        : minLength_(min), message_(msg) {}
    ValidationResult validate(const std::string& value) const override;
private:
    size_t minLength_;
    std::string message_;
};

class MaxLengthValidator : public Validator {
public:
    MaxLengthValidator(size_t max, const std::string& msg = "")
        : maxLength_(max), message_(msg) {}
    ValidationResult validate(const std::string& value) const override;
private:
    size_t maxLength_;
    std::string message_;
};

class EmailValidator : public Validator {
public:
    explicit EmailValidator(const std::string& msg = "بريد إلكتروني غير صالح")
        : message_(msg) {}
    ValidationResult validate(const std::string& value) const override;
private:
    std::string message_;
};

class PatternValidator : public Validator {
public:
    PatternValidator(const std::string& pattern, const std::string& msg = "قيمة غير صالحة")
        : pattern_(pattern), message_(msg) {}
    ValidationResult validate(const std::string& value) const override;
private:
    std::string pattern_;
    std::string message_;
};

class NumericValidator : public Validator {
public:
    explicit NumericValidator(const std::string& msg = "يجب أن يكون رقمًا")
        : message_(msg) {}
    ValidationResult validate(const std::string& value) const override;
private:
    std::string message_;
};

class RangeValidator : public Validator {
public:
    RangeValidator(double min, double max, const std::string& msg = "")
        : min_(min), max_(max), message_(msg) {}
    ValidationResult validate(const std::string& value) const override;
private:
    double min_, max_;
    std::string message_;
};

class CustomValidator : public Validator {
public:
    explicit CustomValidator(ValidatorFunc func) : func_(std::move(func)) {}
    ValidationResult validate(const std::string& value) const override;
private:
    ValidatorFunc func_;
};

// ═══════════════════════════════════════════════════════════════════════════════
// وضع التحقق
// ═══════════════════════════════════════════════════════════════════════════════

enum class ValidationMode : uint8_t {
    OnChange,       ///< تحقق فوري عند كل تغيير
    OnBlur,         ///< تحقق عند مغادرة الحقل
    OnSubmit,       ///< تحقق عند الإرسال فقط
    OnChangeAfterSubmit ///< فوري بعد أول إرسال
};

// ═══════════════════════════════════════════════════════════════════════════════
// حالة حقل (Field State)
// ═══════════════════════════════════════════════════════════════════════════════

struct FieldState {
    std::string name;
    std::string value;
    std::string errorMessage;
    bool isValid = true;
    bool isDirty = false;       ///< تم تغيير القيمة
    bool isTouched = false;     ///< تم التركيز عليه
    bool hasBeenValidated = false;

    bool hasError() const { return !isValid && !errorMessage.empty(); }
};

// ═══════════════════════════════════════════════════════════════════════════════
// حقل نموذج (Form Field)
// ═══════════════════════════════════════════════════════════════════════════════

class FormField {
public:
    FormField(const std::string& name, const std::string& initialValue = "");

    /// إضافة مُحقق
    FormField& required(const std::string& msg = "هذا الحقل مطلوب");
    FormField& minLength(size_t min, const std::string& msg = "");
    FormField& maxLength(size_t max, const std::string& msg = "");
    FormField& email(const std::string& msg = "بريد إلكتروني غير صالح");
    FormField& pattern(const std::string& pat, const std::string& msg = "قيمة غير صالحة");
    FormField& numeric(const std::string& msg = "يجب أن يكون رقمًا");
    FormField& range(double min, double max, const std::string& msg = "");
    FormField& custom(ValidatorFunc func);

    /// تحديث القيمة
    void setValue(const std::string& value);
    void markTouched();
    void reset();

    /// تحقق
    ValidationResult validate() const;
    bool isValid() const;

    /// الحالة
    const FieldState& state() const { return state_; }
    const std::string& name() const { return state_.name; }
    const std::string& value() const { return state_.value; }

    /// مستمع التغيير
    using ChangeCallback = std::function<void(const FieldState&)>;
    void onChange(ChangeCallback cb) { changeCb_ = cb; }

private:
    FieldState state_;
    std::vector<std::shared_ptr<Validator>> validators_;
    ChangeCallback changeCb_;
    void notifyChange();
};

// ═══════════════════════════════════════════════════════════════════════════════
// حالة النموذج (Form State)
// ═══════════════════════════════════════════════════════════════════════════════

class FormState {
public:
    explicit FormState(ValidationMode mode = ValidationMode::OnChange);

    /// إضافة حقل
    FormField& addField(const std::string& name, const std::string& initialValue = "");

    /// الوصول للحقل
    FormField* field(const std::string& name);
    const FormField* field(const std::string& name) const;

    /// تحقق من كل الحقول
    bool validate();
    bool isValid() const;

    /// إرسال
    bool submit();

    /// إعادة تعيين
    void reset();

    /// جمع القيم
    std::unordered_map<std::string, std::string> values() const;

    /// جمع الأخطاء
    std::unordered_map<std::string, std::string> errors() const;

    /// الوضع
    ValidationMode mode() const { return mode_; }
    void setMode(ValidationMode mode) { mode_ = mode; }

    /// مستمعي الإرسال
    using SubmitCallback = std::function<void(const std::unordered_map<std::string, std::string>&)>;
    void onSubmit(SubmitCallback cb) { submitCb_ = cb; }

    /// مستمعي تغير الصلاحية
    using ValidityCallback = std::function<void(bool isValid)>;
    void onValidityChange(ValidityCallback cb) { validityCb_ = cb; }

private:
    std::vector<std::unique_ptr<FormField>> fields_;
    ValidationMode mode_;
    bool hasSubmitted_ = false;

    SubmitCallback submitCb_;
    ValidityCallback validityCb_;
};

} // namespace ui
} // namespace sad

#endif // SAD_UI_FORM_VALIDATION_H
