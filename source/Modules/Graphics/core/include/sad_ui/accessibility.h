/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: accessibility.h
 * المسار: features/graphics/core/include/sad_ui/accessibility.h
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * نظام إمكانية الوصول (Accessibility System).
 *
 * يدعم:
 * - شجرة دلالية (Semantic Tree)
 * - أدوار ARIA (SemanticRole)
 * - قارئات الشاشة (Screen Reader)
 * - إعدادات إمكانية وصول (خطوط كبيرة، تباين عالٍ...)
 * - إعلانات مباشرة (Live Announcements)
 * - ترجمة ARIA لـ Web backend
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#ifndef SAD_UI_ACCESSIBILITY_H
#define SAD_UI_ACCESSIBILITY_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <cstdint>

namespace sad {
namespace ui {

// ═══════════════════════════════════════════════════════════════════════════════
// أدوار دلالية (Semantic Roles)
// ═══════════════════════════════════════════════════════════════════════════════

enum class SemanticRole : uint8_t {
    None,
    Button,
    Link,
    Image,
    Text,
    Heading,
    TextField,
    Checkbox,
    RadioButton,
    Switch,
    Slider,
    ProgressBar,
    Tab,
    TabList,
    TabPanel,
    Menu,
    MenuItem,
    Dialog,
    Alert,
    AlertDialog,
    List,
    ListItem,
    Table,
    Row,
    Cell,
    Navigation,
    Main,
    Banner,
    Footer,
    Form,
    Search,
    Complementary,
    Scrollable,
    Tooltip,
    TreeItem,
    Group
};

// ═══════════════════════════════════════════════════════════════════════════════
// إجراءات دلالية
// ═══════════════════════════════════════════════════════════════════════════════

enum class SemanticAction : uint16_t {
    Tap          = 1 << 0,
    LongPress    = 1 << 1,
    ScrollLeft   = 1 << 2,
    ScrollRight  = 1 << 3,
    ScrollUp     = 1 << 4,
    ScrollDown   = 1 << 5,
    Increase     = 1 << 6,
    Decrease     = 1 << 7,
    Copy         = 1 << 8,
    Cut          = 1 << 9,
    Paste        = 1 << 10,
    Focus        = 1 << 11,
    Dismiss      = 1 << 12,
    CustomAction = 1 << 13
};

inline SemanticAction operator|(SemanticAction a, SemanticAction b) {
    return static_cast<SemanticAction>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}
inline bool hasAction(SemanticAction actions, SemanticAction check) {
    return (static_cast<uint16_t>(actions) & static_cast<uint16_t>(check)) != 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// مستوى الأهمية (Live Region)
// ═══════════════════════════════════════════════════════════════════════════════

enum class LiveRegion : uint8_t {
    Off,        ///< لا يعلن
    Polite,     ///< يعلن عند الفراغ
    Assertive   ///< يعلن فوراً
};

// ═══════════════════════════════════════════════════════════════════════════════
// عقدة دلالية (Semantic Node)
// ═══════════════════════════════════════════════════════════════════════════════

struct SemanticNode {
    int id = -1;
    SemanticRole role = SemanticRole::None;

    // ─── نصوص ───
    std::string label;          ///< التسمية (aria-label)
    std::string value;          ///< القيمة الحالية
    std::string hint;           ///< تلميح (aria-description)
    std::string tooltip;        ///< tooltip

    // ─── حالات ───
    bool isEnabled = true;
    bool isFocusable = false;
    bool isFocused = false;
    bool isSelected = false;
    bool isChecked = false;
    bool isExpanded = false;
    bool isHidden = false;
    bool isReadOnly = false;
    bool isRequired = false;

    // ─── نطاق ───
    float minValue = 0;
    float maxValue = 0;
    float currentValue = 0;

    // ─── heading ───
    int headingLevel = 0;       ///< 1-6 للعناوين

    // ─── إجراءات ───
    SemanticAction actions = static_cast<SemanticAction>(0);

    // ─── منطقة مباشرة ───
    LiveRegion liveRegion = LiveRegion::Off;

    // ─── علاقات ───
    int labelledBy = -1;        ///< معرّف العنصر الذي يُعرّف هذا
    int describedBy = -1;       ///< معرّف العنصر الذي يصف هذا

    // ─── ترتيب ───
    int tabIndex = -1;
    int sortOrder = 0;

    // ─── أطفال ───
    std::vector<std::shared_ptr<SemanticNode>> children;

    // ─── موقع ───
    float x = 0, y = 0, width = 0, height = 0;

    /// هل لديه نص قابل للقراءة؟
    bool isAccessible() const {
        return !label.empty() || !value.empty() || role != SemanticRole::None;
    }

    /// إنتاج نص وصفي للقارئات
    std::string describe() const;

    /// الحصول على سمات ARIA لـ Web
    std::unordered_map<std::string, std::string> ariaAttributes() const;
};

// ═══════════════════════════════════════════════════════════════════════════════
// إعدادات إمكانية الوصول
// ═══════════════════════════════════════════════════════════════════════════════

struct AccessibilitySettings {
    bool screenReaderEnabled = false;
    bool highContrastEnabled = false;
    bool reduceMotionEnabled = false;
    bool reduceTransparencyEnabled = false;
    bool boldTextEnabled = false;
    float textScaleFactor = 1.0f;
    float minimumTouchTarget = 48.0f;   ///< الحد الأدنى لحجم اللمس (dp)
    bool invertColorsEnabled = false;
};

// ═══════════════════════════════════════════════════════════════════════════════
// شجرة دلالية (Semantic Tree)
// ═══════════════════════════════════════════════════════════════════════════════

class SemanticTree {
public:
    void setRoot(std::shared_ptr<SemanticNode> root) { root_ = root; }
    std::shared_ptr<SemanticNode> root() const { return root_; }

    /// البحث بالمعرّف
    std::shared_ptr<SemanticNode> findById(int id) const;

    /// كل العقد المركّزة
    std::vector<SemanticNode*> focusableNodes() const;

    /// ترتيب القراءة (DFS)
    std::vector<SemanticNode*> readingOrder() const;

    /// تحديث عقدة
    bool updateNode(int id, const SemanticNode& updated);

    /// إزالة عقدة
    bool removeNode(int id);

    /// عدد العقد
    int nodeCount() const;

private:
    std::shared_ptr<SemanticNode> root_;
    std::shared_ptr<SemanticNode> findByIdImpl(
        const std::shared_ptr<SemanticNode>& node, int id) const;
    void collectFocusable(SemanticNode* node, std::vector<SemanticNode*>& out) const;
    void collectDFS(SemanticNode* node, std::vector<SemanticNode*>& out) const;
    int countNodes(const SemanticNode* node) const;
};

// ═══════════════════════════════════════════════════════════════════════════════
// مدير إمكانية الوصول (Accessibility Manager)
// ═══════════════════════════════════════════════════════════════════════════════

class AccessibilityManager {
public:
    static AccessibilityManager& instance();

    /// الشجرة الدلالية
    SemanticTree& tree() { return tree_; }

    /// الإعدادات
    AccessibilitySettings& settings() { return settings_; }
    const AccessibilitySettings& settings() const { return settings_; }

    /// إعلان للقارئ
    void announce(const std::string& message, LiveRegion priority = LiveRegion::Polite);

    /// تركيز عنصر
    void focusNode(int nodeId);
    int focusedNodeId() const { return focusedNodeId_; }

    /// التنقل عبر العناصر
    void moveToNext();
    void moveToPrevious();

    /// مستمعي الإعلانات
    using AnnouncementCallback = std::function<void(const std::string&, LiveRegion)>;
    void onAnnouncement(AnnouncementCallback cb) { announcementCb_ = cb; }

    /// مستمعي تغير التركيز
    using FocusCallback = std::function<void(int nodeId)>;
    void onFocusChange(FocusCallback cb) { focusCb_ = cb; }

private:
    AccessibilityManager() = default;
    SemanticTree tree_;
    AccessibilitySettings settings_;
    int focusedNodeId_ = -1;

    AnnouncementCallback announcementCb_;
    FocusCallback focusCb_;
};

} // namespace ui
} // namespace sad

#endif // SAD_UI_ACCESSIBILITY_H
