/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: print_tree.cpp
 * المسار: features/graphics/core/src/print_tree.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * @brief (AR) طباعة شجرة عناصر IR (تصحيح) — تعريفٌ في المكتبة
 * @brief (EN) IR widget-tree pretty printer — library definition
 *
 * (AR) تعريفُ منطق طباعة شجرة `IRNode` (مصدر الحقيقة الوحيد) يعيش هنا لا في
 *      الرأس، فيُترجَم مرّةً واحدة ويُربَط به المحرّكان (المفسّر عبر `طباعة_شجرة`،
 *      المترجم عبر `sad_print_tree`) ⇒ **تطابقُ مخرَجٍ بالبناء**. الرأس يحمل
 *      التصريح فقط.
 * (EN) The IRNode tree printer definition lives here (single source of truth),
 *      compiled once and linked by both engines ⇒ byte-identical output. The
 *      header holds only the declaration.
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/print_tree.h"

#include <string>
#include <variant>
#include <type_traits>

namespace sad
{
    namespace ui
    {
        void printIRNodeTree(const std::shared_ptr<IRNode> &node, int depth,
                             std::ostream &os, int maxDepth)
        {
            if (!node || depth > maxDepth)
                return;

            std::string indent(depth * 2, ' ');

            // (AR) نوع العنصر
            os << indent << nodeTypeToArabicName(node->getType());

            // (AR) الخصائص
            const auto &props = node->getProperties();
            if (!props.empty())
            {
                os << "(";
                bool first = true;
                for (const auto &prop : props)
                {
                    if (!first)
                        os << ", ";
                    first = false;
                    os << prop.key << ": ";
                    std::visit([&os](const auto &v)
                               {
                        using T = std::decay_t<decltype(v)>;
                        if constexpr (std::is_same_v<T, std::string>) {
                            std::string s = v;
                            constexpr size_t kMaxStrBytes = 40; // حدّ الاقتطاع بالبايتات (مشترك بين المحرّكين)
                            bool truncated = false;
                            if (s.size() > kMaxStrBytes) {
                                // (AR) لا تقطع منتصف محرف UTF-8: تراجع عن بايتات المتابعة (0x80–0xBF)
                                //      إلى بداية المحرف. cut ∈ [0,kMaxStrBytes] ⊂ [0,size) — size>kMaxStrBytes
                                //      مضمون هنا، فلا قراءة خارج الحدود.
                                size_t cut = kMaxStrBytes;
                                while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80)
                                    --cut;
                                // (AR) إن كانت أوّل kMaxStrBytes بايتات كلّها متابعة (سلسلة تالفة سلفًا)
                                //      نعود للقطع الخام تفاديًا لفقدان المحتوى بالكامل.
                                if (cut == 0)
                                    cut = kMaxStrBytes;
                                s.resize(cut);
                                truncated = true;
                            }
                            // (AR) تهريب المحارف الخاصّة كي لا يلتبس المخرَج أو يُكسَر
                            //      (شرطة خلفيّة/اقتباس/سطر/إرجاع/جدولة). يُطبَّق بعد الاقتطاع
                            //      فلا يُقصُّ تسلسلَ هروبٍ في منتصفه.
                            // (EN) Escape special chars so the output stays unambiguous and
                            //      unbreakable (backslash/quote/newline/CR/tab). Applied after
                            //      truncation so no escape sequence is cut mid-way.
                            os << '"';
                            for (char c : s) {
                                switch (c) {
                                case '\\': os << "\\\\"; break;
                                case '"':  os << "\\\""; break;
                                case '\n': os << "\\n"; break;
                                case '\r': os << "\\r"; break;
                                case '\t': os << "\\t"; break;
                                default:   os << c; break;
                                }
                            }
                            if (truncated)
                                os << "...";
                            os << '"';
                        } else if constexpr (std::is_same_v<T, bool>) {
                            os << (v ? "\xd8\xb5\xd8\xad\xd9\x8a\xd8\xad" : "\xd8\xae\xd8\xb7\xd8\xa3"); // صحيح/خطأ
                        } else {
                            os << v;
                        } }, prop.value);
                }
                os << ")";
            }

            // (AR) الأحداث
            const auto &events = node->getEvents();
            if (!events.empty())
            {
                os << " [";
                bool first = true;
                for (const auto &event : events)
                {
                    if (!first)
                        os << ", ";
                    first = false;
                    os << event.getEventName();
                }
                os << "]";
            }

            // (AR) التحريكات
            const auto &anims = node->getAnimations();
            if (!anims.empty())
            {
                os << " {";
                bool first = true;
                for (const auto &anim : anims)
                {
                    if (!first)
                        os << ", ";
                    first = false;
                    os << "\xd8\xad\xd8\xb1\xd9\x83\xd8\xa9:" // حركة:
                       << animationTypeToString(anim.type)
                       << "/" << anim.duration << "\xd8\xab"; // ث
                    if (anim.easing != EasingCurve::EaseInOut)
                        os << "/" << easingCurveToString(anim.easing);
                    if (anim.delay > 0.0f)
                        os << " \xd8\xaa\xd8\xa3\xd8\xae\xd9\x8a\xd8\xb1:" << anim.delay << "\xd8\xab"; // تأخير:Xث
                    if (anim.repeatCount != 1)
                        os << " \xd8\xaa\xd9\x83\xd8\xb1\xd8\xa7\xd8\xb1:" // تكرار:
                           << (anim.repeatCount == 0 ? "\xe2\x88\x9e" : std::to_string(anim.repeatCount)); // ∞
                    if (anim.autoReverse)
                        os << " \xd8\xb9\xd9\x83\xd8\xb3_\xd8\xaa\xd9\x84\xd9\x82\xd8\xa7\xd8\xa6\xd9\x8a"; // عكس_تلقائي
                }
                os << "}";
            }

            os << "\n";

            // (AR) الأبناء تكراريًّا (نلتقط المتّجه بمرجعٍ ثابت مرّةً بدل getChildren()/
            //      childCount() لكلّ تكرار؛ نفس الترتيب ⇒ مخرَجٌ متطابق بالبناء).
            // (EN) Recurse into children (cache vector by const-ref; identical order/output).
            const auto &children = node->getChildren();
            for (const auto &child : children)
                printIRNodeTree(child, depth + 1, os, maxDepth);
        }

    } // namespace ui
} // namespace sad
