/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: state.h
 * المسار: features/graphics/core/include/sad_ui/state.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * الوصف:
 * ------
 * نظام إدارة الحالة التفاعلي (Reactive State Management).
 *
 * هذا الملف يُعرّف نظام الحالة الذي يُدير البيانات التفاعلية
 * في واجهات لغة ص. النظام مستوحى من SwiftUI @State و React useState.
 *
 * المفاهيم الأساسية:
 * ──────────────────────
 * 1. **الحالة (State)**: متغير تفاعلي — عندما يتغير، يُعاد بناء
 *    العناصر التي تعتمد عليه تلقائياً.
 *
 * 2. **الربط (Binding)**: إشارة لحالة في واجهة أخرى.
 *    يسمح للواجهات الفرعية بتعديل حالة الأب.
 *
 * 3. **المُراقب (Observer)**: دالة تُستدعى عند تغيّر الحالة.
 *    العناصر تُسجّل نفسها كمراقبين للحالات التي تعتمد عليها.
 *
 * 4. **الحالة المحسوبة (Computed)**: قيمة مشتقة من حالات أخرى.
 *    تُعاد حسابها تلقائياً عند تغيّر أي من مدخلاتها.
 *
 * مثال في كود ص:
 * ─────────────────
 * @code
 *   واجهة عداد {
 *       @حالة عدد: رقم = 0              ← State
 *       @محسوب مضاعف = عدد * 2          ← Computed
 *
 *       عرض() {
 *           عمود {
 *               نص("العدد: \(عدد)")       ← يعتمد على "عدد"
 *               نص("المضاعف: \(مضاعف)")  ← يعتمد على "مضاعف"
 *               زر("زد").عند_النقر {      ← يُعدّل "عدد"
 *                   عدد += 1
 *               }
 *           }
 *       }
 *   }
 * @endcode
 *
 * عند الضغط على الزر:
 * 1. عدد يتغير من 0 إلى 1
 * 2. النظام يُخطر المراقبين (نص الأول + مضاعف)
 * 3. مضاعف يُعاد حسابه (1 * 2 = 2)
 * 4. النظام يُخطر مراقبي مضاعف (نص الثاني)
 * 5. العناصر المتأثرة تُعاد رسمها فقط (لا إعادة رسم كاملة)
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_STATE_H
#define SAD_UI_STATE_H

#include "types.h"

#include <any>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// واجهة المُراقب (Observer Interface)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief واجهة المُراقب — يُستدعى عند تغيّر الحالة
 *
 * أي كائن يريد مراقبة حالة يجب أن يرث من هذه الواجهة.
 * عادة العناصر (UINode) هي المراقبون.
 */
class StateObserver {
public:
    virtual ~StateObserver() = default;

    /**
     * @brief يُستدعى عند تغيّر قيمة الحالة
     *
     * @param stateName اسم المتغير الذي تغيّر
     * @param newValue القيمة الجديدة
     */
    virtual void onStateChanged(const std::string& stateName,
                                 const std::any& newValue) = 0;
};

// ═══════════════════════════════════════════════════════════════════════════════
// مدخل الحالة (State Entry) — تسجيل متغير حالة واحد
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief تسجيل لمتغير حالة واحد في المخزن
 *
 * يحتوي على:
 * - القيمة الحالية (std::any — يدعم أي نوع)
 * - نوع الربط (حالة، ربط، محسوب...)
 * - قائمة المراقبين
 * - (اختياري) دالة الحساب للحالة المحسوبة
 * - (اختياري) الحالات التي تعتمد عليها
 */
struct StateEntry {
    std::string name;                               ///< اسم المتغير
    std::any value;                                 ///< القيمة الحالية
    std::string valueType;                          ///< نوع القيمة ("رقم"، "نص"...)
    StateBindingType bindingType;                   ///< نوع الربط

    /// المراقبون — يُخطرون عند التغيير
    std::vector<std::weak_ptr<StateObserver>> observers;

    /// للحالة المحسوبة: دالة إعادة الحساب
    std::function<std::any()> computeFunction;

    /// للحالة المحسوبة: الحالات التي تعتمد عليها
    std::unordered_set<std::string> dependencies;
};

// ═══════════════════════════════════════════════════════════════════════════════
// مخزن الحالة (State Store)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief مخزن مركزي لجميع متغيرات الحالة في واجهة واحدة
 *
 * كل واجهة (View) تملك StateStore واحد يُدير كل حالاتها.
 *
 * الاستخدام:
 * @code
 *   StateStore store;
 *
 *   // تسجيل حالة جديدة
 *   store.define("عداد", "رقم", StateBindingType::State, 0);
 *
 *   // قراءة القيمة
 *   int count = std::any_cast<int>(store.get("عداد"));
 *
 *   // تعديل القيمة (يُخطر المراقبين تلقائياً)
 *   store.set("عداد", count + 1);
 *
 *   // إضافة مراقب
 *   store.addObserver("عداد", myObserver);
 * @endcode
 */
class StateStore {
public:
    StateStore();
    ~StateStore();

    // ─── تعريف الحالات ──────────────────────

    /**
     * @brief تعريف متغير حالة جديد
     *
     * @param name اسم المتغير (بالعربية)
     * @param valueType نوع القيمة ("رقم"، "نص"، "منطقي"...)
     * @param bindingType نوع الربط (حالة، ربط، محسوب...)
     * @param initialValue القيمة الأولية
     */
    void define(const std::string& name,
                const std::string& valueType,
                StateBindingType bindingType,
                std::any initialValue = {});

    /**
     * @brief تعريف حالة محسوبة
     *
     * الحالة المحسوبة تُشتق تلقائياً من حالات أخرى.
     *
     * @param name اسم المتغير المحسوب
     * @param dependencies الحالات التي يعتمد عليها
     * @param computeFn دالة إعادة الحساب
     */
    void defineComputed(const std::string& name,
                        const std::unordered_set<std::string>& dependencies,
                        std::function<std::any()> computeFn);

    // ─── القراءة والكتابة ───────────────────

    /// قراءة قيمة الحالة
    std::any get(const std::string& name) const;

    /**
     * @brief تعيين قيمة جديدة للحالة
     *
     * عند التعيين:
     * 1. تُحدّث القيمة في المخزن
     * 2. تُخطر جميع المراقبين
     * 3. تُعاد حساب الحالات المحسوبة المعتمدة
     *
     * @param name اسم المتغير
     * @param value القيمة الجديدة
     */
    void set(const std::string& name, std::any value);

    /// هل يوجد متغير بهذا الاسم؟
    bool has(const std::string& name) const;

    // ─── المراقبة ───────────────────────────

    /// إضافة مراقب لمتغير محدد
    void addObserver(const std::string& name,
                     std::shared_ptr<StateObserver> observer);

    /// إزالة مراقب
    void removeObserver(const std::string& name,
                        const std::shared_ptr<StateObserver>& observer);

    // ─── التجميع (Batching) ─────────────────

    /**
     * @brief بدء تجميع التحديثات
     *
     * عند تجميع التحديثات، لا يُخطر المراقبون حتى انتهاء التجميع.
     * هذا مفيد عند تعديل عدة حالات معاً (يمنع إعادة الرسم المتكررة).
     *
     * @code
     *   store.beginBatch();
     *   store.set("اسم", "أحمد");
     *   store.set("عمر", 25);
     *   store.endBatch();  // ← إخطار واحد لكل المراقبين
     * @endcode
     */
    void beginBatch();

    /// إنهاء التجميع وإخطار المراقبين
    void endBatch();

    // ─── التصحيح ────────────────────────────

    /// طباعة كل الحالات وقيمها (للتصحيح)
    std::string dump() const;

    /// عدد الحالات المُسجّلة
    size_t count() const { return entries_.size(); }

private:
    /// إخطار المراقبين بتغيّر حالة
    void notifyObservers(const std::string& name);

    /// إعادة حساب الحالات المحسوبة المعتمدة على حالة متغيرة
    void recomputeDependents(const std::string& changedState);

    /// تنظيف المراقبين المنتهيين (weak_ptr expired)
    void cleanupExpiredObservers(const std::string& name);

    std::unordered_map<std::string, StateEntry> entries_;   ///< مخزن الحالات
    bool batching_ = false;                                  ///< هل نحن في وضع التجميع؟
    std::unordered_set<std::string> pendingNotifications_;   ///< إخطارات مؤجلة
};

} // namespace ui
} // namespace sad

#endif // SAD_UI_STATE_H
