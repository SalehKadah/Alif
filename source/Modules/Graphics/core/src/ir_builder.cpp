/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: ir_builder.cpp
 * المسار: features/graphics/core/src/ir_builder.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * تنفيذ باني التمثيل الوسيط (IR Builder).
 *
 * هذا الملف يحتوي على المنطق الأساسي لتحويل شجرة UINode إلى شجرة IRNode
 * مع التوجيه الذكي (Smart Routing) والتحسينات.
 *
 * المسار:
 *   UINode Tree ──→ IRBuilder::build() ──→ IRModule
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/ir_builder.h"
#include "sad_ui/hybrid_routing.h"
#include "sad_ui/types.h"

#include <algorithm>
#include <sstream>

namespace sad
{
    namespace ui
    {

        // ═══════════════════════════════════════════════════════════════════════════════
        // تنفيذ IRBuilder
        // ═══════════════════════════════════════════════════════════════════════════════

        IRBuilder::IRBuilder(const IRBuildOptions &options)
            : options_(options), router_(std::make_unique<HybridRouter>(options.targetPlatform))
        {
            // تسجيل العناصر الأصلية الافتراضية لكل منصة
            // هذه العناصر موجودة في معظم المنصات كعناصر أصلية
            registerNativeWidget(UINodeType::Text);
            registerNativeWidget(UINodeType::Button);
            registerNativeWidget(UINodeType::Image);
            registerNativeWidget(UINodeType::TextField);
            registerNativeWidget(UINodeType::Toggle);
            registerNativeWidget(UINodeType::Slider);
            registerNativeWidget(UINodeType::ScrollView);
            registerNativeWidget(UINodeType::List);
        }

        IRModule IRBuilder::build(const ViewDefinition &view)
        {
            IRModule module;
            module.name = view.name;
            module.sourceFile = view.sourceFile;

            // تحويل متغيرات الحالة
            for (const auto &binding : view.stateBindings)
            {
                module.stateDefinitions[binding.name] = binding.valueType;
            }

            // تحويل شجرة العناصر
            if (view.rootNode)
            {
                module.root = buildNode(view.rootNode);
            }

            // تحسين (اختياري)
            if (options_.optimize && module.root)
            {
                optimizeTree(module.root);
            }

            return module;
        }

        std::shared_ptr<IRNode> IRBuilder::buildNode(const std::shared_ptr<UINode> &node)
        {
            if (!node)
                return nullptr;

            // إنشاء عقدة IR مقابلة
            auto irNode = IRNode::create(node->getType());
            irNode->setId(node->getId());

            // تحديد تلميح العرض (التوجيه الذكي الهجين)
            if (options_.enableSmartRouting && router_)
            {
                irNode->setRenderingHint(router_->route(node->getType()));
            }
            else
            {
                irNode->setRenderingHint(determineRenderingHint(node->getType()));
            }

            irNode->setTargetPlatform(options_.targetPlatform);

            // تحويل الوسيطة الأساسية إلى خاصية
            if (!node->getPrimaryArg().empty())
            {
                IRProperty textProp;
                textProp.key = "text";
                textProp.value = node->getPrimaryArg();
                irNode->addProperty(textProp);
            }

            // تحويل الوسائط المسمّاة
            for (const auto &[key, value] : node->getNamedArgs())
            {
                IRProperty prop;
                prop.key = key;
                prop.value = value;
                irNode->addProperty(prop);
            }

            // تحويل المعدّلات إلى خصائص وأحداث
            processModifiers(*node, *irNode);

            // تحويل روابط الحالة
            processStateBindings(*node, *irNode);

            // تحويل الأبناء (تكرار)
            for (const auto &child : node->getChildren())
            {
                auto irChild = buildNode(child);
                if (irChild)
                {
                    irNode->addChild(irChild);
                }
            }

            return irNode;
        }

        // ─── تحويل المعدّلات إلى خصائص وأحداث ──────

        void IRBuilder::processModifiers(const UINode &source, IRNode &target)
        {
            for (const auto &mod : source.getModifiers())
            {
                // (AR) المعدّلات التي تمثّل أحداثًا — القائمة البيضاء صارت مُوسَّعة
                //      من SAD_UI_EVENT_MODIFIER_MAP نفسها التي يُوسّعها التحويل،
                //      فلا يمكن أن يُضاف معدّل حدثٍ ويسقط هنا صامتًا إلى خاصّيّة.
                if (isEventModifier(mod.type))
                {

                    IREvent event;
                    event.type = modifierTypeToIREventType(mod.type);
                    event.expression = mod.callbackExpr;
                    target.addEvent(event);
                    continue;
                }

                // المعدّلات العادية → خصائص IR
                IRProperty prop;
                prop.key = modifierTypeToArabicName(mod.type);

                // تحويل القيمة حسب نوعها
                std::visit([&prop](auto &&val)
                           {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, float>) {
                prop.value = static_cast<double>(val);
            } else if constexpr (std::is_same_v<T, std::string>) {
                prop.value = val;
            } else if constexpr (std::is_same_v<T, bool>) {
                prop.value = val;
            } else if constexpr (std::is_same_v<T, Color>) {
                // تخزين اللون كنص سداسي عشري
                prop.value = val.toHexString();
            } else if constexpr (std::is_same_v<T, Dimension>) {
                prop.value = static_cast<double>(val.value);
            } else if constexpr (std::is_same_v<T, EdgeInsets>) {
                // تخزين الحواف كنص "top,end,bottom,start"
                std::ostringstream ss;
                ss << val.top << "," << val.end_ << ","
                   << val.bottom << "," << val.start;
                prop.value = ss.str();
            } else if constexpr (std::is_same_v<T, AlignmentType>) {
                prop.value = static_cast<int64_t>(val);
            } }, mod.value);

                target.addProperty(prop);
            }
        }

        // ─── تحويل روابط الحالة ──────

        void IRBuilder::processStateBindings(const UINode &source, IRNode &target)
        {
            for (const auto &binding : source.getStateBindings())
            {
                IRStateRef ref;
                ref.stateName = binding.name;
                ref.bindingType = binding.bindingType;
                ref.propertyBinding = binding.valueType;
                target.addStateRef(ref);
            }
        }

        // ─── التوجيه الذكي: تحديد كيفية العرض ──────

        RenderingHint IRBuilder::determineRenderingHint(UINodeType type) const
        {
            // في حالة سطح المكتب: كل شيء يُرسم بـ SDL2 (Canvas)
            if (options_.targetPlatform == TargetPlatform::Desktop ||
                options_.targetPlatform == TargetPlatform::Web)
            {
                return RenderingHint::Canvas;
            }

            // في حالة الجوال: تحقق هل العنصر موجود أصلياً
            if (nativeWidgets_.count(type) > 0)
            {
                return RenderingHint::Native;
            }

            // عناصر التخطيط دائماً أصلية
            if (type == UINodeType::Column || type == UINodeType::Row ||
                type == UINodeType::Stack || type == UINodeType::Grid ||
                type == UINodeType::Container || type == UINodeType::Box)
            {
                return RenderingHint::Native;
            }

            // Canvas و CustomWidget دائماً Canvas
            if (type == UINodeType::Canvas || type == UINodeType::CustomWidget)
            {
                return RenderingHint::Canvas;
            }

            // افتراضي: Canvas (الخيار الآمن)
            return RenderingHint::Canvas;
        }

        // ─── التحسينات ──────

        void IRBuilder::optimizeTree(std::shared_ptr<IRNode> &root)
        {
            if (!root)
                return;

            // حذف عقد Container الفارغة (بلا خصائص وابن واحد)
            auto &children = const_cast<std::vector<std::shared_ptr<IRNode>> &>(root->getChildren());
            for (auto &child : children)
            {
                optimizeTree(child);
            }

            // يمكن إضافة تحسينات إضافية لاحقاً:
            // - دمج معدّلات متكررة من نفس النوع
            // - حذف عناصر بعتامة 0
            // - تبسيط حاويات متداخلة بدون فائدة
        }

    } // namespace ui
} // namespace sad
