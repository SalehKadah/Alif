/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * ملف: reconciler.cpp
 * المسار: features/graphics/core/src/reconciler.cpp
 * ═══════════════════════════════════════════════════════════════════════════════
 *
 * الوصف بالعربية:
 * ───────────────
 * تنفيذ نظام المطابقة الذكية (Reconciler) — Virtual DOM على غرار React.
 *
 * يحتوي على:
 * 1. diff() — المقارنة الذكية بين شجرتي IR
 * 2. patch() — تطبيق الرقع على الشجرة الحية
 * 3. خوارزمية مقارنة الأبناء بالمفاتيح (keyed diffing)
 *
 * الخوارزمية:
 * ───────────
 * diff(old, new) يعمل تكرارياً:
 *   - إذا اختلف النوع → REPLACE (لا مقارنة فرعية)
 *   - إذا تساوى النوع → مقارنة الخصائص والأحداث والأبناء والتحريكات
 *   - الأبناء يُقارنون بالمفاتيح أولاً (id)، ثم بالفهرس
 *
 * patch(tree, patches) يعمل من الأعمق للأقرب:
 *   - يتنقل في الشجرة عبر المسار (path)
 *   - يُطبّق التغيير الموضعي فقط
 *
 * English Description:
 * ────────────────────
 * Implementation of the Reconciler (React-style Virtual DOM).
 * Contains diff(), patch(), and keyed children diffing algorithm.
 *
 * حقوق النشر (c) 2024-2026 فريق لغة ص
 * مرخص تحت رخصة MIT
 * ═══════════════════════════════════════════════════════════════════════════════
 */

#include "sad_ui/reconciler.h"
#include <algorithm>
#include <unordered_set>
#include <cassert>

namespace sad
{
    namespace ui
    {

        // ═══════════════════════════════════════════════════════════════════════════════
        // cloneSubtree — نسخ عميق لشجرة فرعية (دالة مساعدة داخلية)
        // ═══════════════════════════════════════════════════════════════════════════════

        static std::shared_ptr<IRNode> cloneSubtreeHelper(const IRNode *source)
        {
            if (!source)
                return nullptr;

            auto clone = IRNode::create(source->getType());
            clone->setId(source->getId());
            clone->setRenderingHint(source->getRenderingHint());
            clone->setTargetPlatform(source->getTargetPlatform());

            // (AR) نسخ الخصائص
            for (const auto &prop : source->getProperties())
            {
                clone->addProperty(prop);
            }

            // (AR) نسخ الأحداث
            for (const auto &event : source->getEvents())
            {
                clone->addEvent(event);
            }

            // (AR) نسخ التحريكات
            for (const auto &anim : source->getAnimations())
            {
                clone->addAnimation(anim);
            }

            // (AR) نسخ إشارات الحالة
            for (const auto &ref : source->getStateRefs())
            {
                clone->addStateRef(ref);
            }

            // (AR) نسخ الأبناء تكرارياً
            for (const auto &child : source->getChildren())
            {
                auto childClone = cloneSubtreeHelper(child.get());
                if (childClone)
                {
                    clone->addChild(std::move(childClone));
                }
            }

            return clone;
        }

        // ═══════════════════════════════════════════════════════════════════════════════
        // diff — المقارنة الرئيسية
        // ═══════════════════════════════════════════════════════════════════════════════

        DiffResult Reconciler::diff(const std::shared_ptr<IRNode> &oldTree,
                                    const std::shared_ptr<IRNode> &newTree)
        {
            DiffResult result;

            // (AR) حالة خاصة: كلتاهما فارغتان
            if (!oldTree && !newTree)
            {
                return result;
            }

            // (AR) حالة خاصة: الشجرة القديمة فارغة → إدراج الجذر الجديد بالكامل
            if (!oldTree && newTree)
            {
                Patch p;
                p.path = {};
                p.type = PatchType::REPLACE;
                p.replacementNode = newTree;
                result.patches.push_back(std::move(p));
                result.stats.nodesReplaced = 1;
                return result;
            }

            // (AR) حالة خاصة: الشجرة الجديدة فارغة → حذف الجذر
            if (oldTree && !newTree)
            {
                Patch p;
                p.path = {};
                p.type = PatchType::REPLACE;
                p.replacementNode = nullptr;
                result.patches.push_back(std::move(p));
                result.stats.nodesReplaced = 1;
                return result;
            }

            // (AR) المقارنة التكرارية
            std::vector<size_t> rootPath;
            diffNodes(oldTree.get(), newTree.get(), rootPath, result);

            // (AR) ترتيب الرقع: الأعمق أولاً لتجنب تعارض الأفهرسة عند التطبيق
            // (EN) Sort patches: deepest first to avoid index conflicts during patching
            std::sort(result.patches.begin(), result.patches.end(),
                      [](const Patch &a, const Patch &b)
                      {
                          // (AR) الأعمق أولاً (path أطول)
                          if (a.path.size() != b.path.size())
                              return a.path.size() > b.path.size();
                          // (AR) نفس العمق → الأفهرسة الأكبر أولاً (لتجنب إزاحة)
                          return a.path > b.path;
                      });

            return result;
        }

        // ═══════════════════════════════════════════════════════════════════════════════
        // diffNodes — المقارنة التكرارية لعقدتين
        // ═══════════════════════════════════════════════════════════════════════════════

        void Reconciler::diffNodes(const IRNode *oldNode,
                                   const IRNode *newNode,
                                   std::vector<size_t> &currentPath,
                                   DiffResult &result)
        {
            result.stats.nodesCompared++;

            // ─── (AR) إذا اختلف النوع → استبدال كامل ───
            if (oldNode->getType() != newNode->getType())
            {
                Patch p;
                p.path = currentPath;
                p.type = PatchType::REPLACE;
                // (AR) ننسخ الشجرة الفرعية الجديدة بالكامل
                p.replacementNode = cloneSubtreeHelper(newNode);
                result.patches.push_back(std::move(p));
                result.stats.nodesReplaced++;
                return;
            }

            // ─── (AR) النوع متطابق → مقارنة تفصيلية ───

            // (AR) مقارنة الخصائص
            diffProperties(oldNode, newNode, currentPath, result);

            // (AR) مقارنة الأحداث
            diffEvents(oldNode, newNode, currentPath, result);

            // (AR) مقارنة التحريكات
            diffAnimations(oldNode, newNode, currentPath, result);

            // (AR) مقارنة الأبناء (الأكثر تعقيداً)
            diffChildren(oldNode, newNode, currentPath, result);

            // (AR) إذا لم تُولّد أي رقعة → العقدة لم تتغير
            // (EN) If no patches generated → node unchanged
            // (AR) ملاحظة: هذا الإحصاء تقريبي — الخصائص قد تُنتج رقعة لنفس العقدة
        }

        // ═══════════════════════════════════════════════════════════════════════════════
        // diffProperties — مقارنة الخصائص
        // ═══════════════════════════════════════════════════════════════════════════════

        void Reconciler::diffProperties(const IRNode *oldNode,
                                        const IRNode *newNode,
                                        const std::vector<size_t> &path,
                                        DiffResult &result)
        {
            const auto &oldProps = oldNode->getProperties();
            const auto &newProps = newNode->getProperties();

            // (AR) بناء خريطة سريعة للخصائص القديمة
            std::unordered_map<std::string, const IRProperty *> oldMap;
            for (const auto &prop : oldProps)
            {
                oldMap[prop.key] = &prop;
            }

            // (AR) بناء خريطة سريعة للخصائص الجديدة
            std::unordered_map<std::string, const IRProperty *> newMap;
            for (const auto &prop : newProps)
            {
                newMap[prop.key] = &prop;
            }

            std::vector<IRProperty> updated;
            std::vector<std::string> removed;

            // (AR) البحث عن خصائص متغيرة أو جديدة
            for (const auto &newProp : newProps)
            {
                auto it = oldMap.find(newProp.key);
                if (it == oldMap.end())
                {
                    // (AR) خاصية جديدة
                    updated.push_back(newProp);
                }
                else if (it->second->value != newProp.value)
                {
                    // (AR) خاصية تغيرت قيمتها
                    updated.push_back(newProp);
                }
            }

            // (AR) البحث عن خصائص محذوفة
            for (const auto &oldProp : oldProps)
            {
                if (newMap.find(oldProp.key) == newMap.end())
                {
                    removed.push_back(oldProp.key);
                }
            }

            // (AR) إنشاء رقعة فقط إذا كان هناك تغيير
            if (!updated.empty() || !removed.empty())
            {
                Patch p;
                p.path = path;
                p.type = PatchType::UPDATE_PROPS;
                p.updatedProps = std::move(updated);
                p.removedProps = std::move(removed);
                result.patches.push_back(std::move(p));
                result.stats.propsUpdated++;
            }
        }

        // ═══════════════════════════════════════════════════════════════════════════════
        // diffEvents — مقارنة الأحداث
        // ═══════════════════════════════════════════════════════════════════════════════

        void Reconciler::diffEvents(const IRNode *oldNode,
                                    const IRNode *newNode,
                                    const std::vector<size_t> &path,
                                    DiffResult &result)
        {
            const auto &oldEvents = oldNode->getEvents();
            const auto &newEvents = newNode->getEvents();

            // (AR) مقارنة سريعة: نفس العدد ونفس المحتوى
            if (oldEvents.size() == newEvents.size())
            {
                bool same = true;
                for (size_t i = 0; i < oldEvents.size(); i++)
                {
                    // (AR) نقارن كلّ ما يؤثّر في الإرسال: النوع والتعبير وطورَ
                    //   الانتشار وبيانات المعالِج — إعادةُ رسمٍ تفاعليّة تغيّر
                    //   طورًا أو بيانات دون تغيير التعبير كانت تُعدّ مطابِقة فتبقى
                    //   القيمة قديمة (لا تُنبعث UPDATE_EVENTS).
                    if (oldEvents[i].type != newEvents[i].type ||
                        oldEvents[i].expression != newEvents[i].expression ||
                        oldEvents[i].propagation != newEvents[i].propagation ||
                        oldEvents[i].userData != newEvents[i].userData)
                    {
                        same = false;
                        break;
                    }
                }
                if (same)
                    return;
            }

            // (AR) الأحداث تغيرت → رقعة تحديث كاملة
            Patch p;
            p.path = path;
            p.type = PatchType::UPDATE_EVENTS;
            p.updatedEvents = newEvents;
            result.patches.push_back(std::move(p));
        }

        // ═══════════════════════════════════════════════════════════════════════════════
        // diffAnimations — مقارنة التحريكات
        // ═══════════════════════════════════════════════════════════════════════════════

        void Reconciler::diffAnimations(const IRNode *oldNode,
                                        const IRNode *newNode,
                                        const std::vector<size_t> &path,
                                        DiffResult &result)
        {
            const auto &oldAnims = oldNode->getAnimations();
            const auto &newAnims = newNode->getAnimations();

            // (AR) مقارنة سريعة: نفس العدد ونفس المحتوى
            if (oldAnims.size() == newAnims.size())
            {
                bool same = true;
                for (size_t i = 0; i < oldAnims.size(); i++)
                {
                    if (oldAnims[i].type != newAnims[i].type ||
                        oldAnims[i].duration != newAnims[i].duration ||
                        oldAnims[i].easing != newAnims[i].easing ||
                        oldAnims[i].delay != newAnims[i].delay ||
                        oldAnims[i].repeatCount != newAnims[i].repeatCount)
                    {
                        same = false;
                        break;
                    }
                }
                if (same)
                    return;
            }

            // (AR) التحريكات تغيرت → رقعة تحديث
            Patch p;
            p.path = path;
            p.type = PatchType::UPDATE_ANIMATIONS;
            p.updatedAnimations = newAnims;
            result.patches.push_back(std::move(p));
        }

        // ═══════════════════════════════════════════════════════════════════════════════
        // diffChildren — مقارنة الأبناء بالمفاتيح (Keyed Diffing)
        // ═══════════════════════════════════════════════════════════════════════════════

        void Reconciler::diffChildren(const IRNode *oldNode,
                                      const IRNode *newNode,
                                      std::vector<size_t> &currentPath,
                                      DiffResult &result)
        {
            const auto &oldChildren = oldNode->getChildren();
            const auto &newChildren = newNode->getChildren();

            // (AR) كلاهما بدون أبناء → لا شيء
            if (oldChildren.empty() && newChildren.empty())
                return;

            // ─── الخطوة 1: بناء خرائط المفاتيح ───
            // (AR) كل ابن يُعرّف بمفتاحه (id إذا وُجد، وإلا "فهرس_نوع")
            std::unordered_map<std::string, size_t> oldKeyMap; // مفتاح → فهرس قديم
            std::unordered_map<std::string, size_t> newKeyMap; // مفتاح → فهرس جديد

            std::vector<std::string> oldKeys;
            std::vector<std::string> newKeys;

            for (size_t i = 0; i < oldChildren.size(); i++)
            {
                std::string key = getNodeKey(oldChildren[i].get(), i);
                oldKeys.push_back(key);
                oldKeyMap[key] = i;
            }

            for (size_t i = 0; i < newChildren.size(); i++)
            {
                std::string key = getNodeKey(newChildren[i].get(), i);
                newKeys.push_back(key);
                newKeyMap[key] = i;
            }

            // ─── الخطوة 2: تحديد الأبناء المحذوفين ───
            // (AR) أبناء موجودين في القديمة وغير موجودين في الجديدة
            for (size_t i = oldChildren.size(); i > 0; i--)
            {
                size_t idx = i - 1;
                const std::string &key = oldKeys[idx];
                if (newKeyMap.find(key) == newKeyMap.end())
                {
                    Patch p;
                    p.path = currentPath;
                    p.type = PatchType::REMOVE_CHILD;
                    p.removeIndex = idx;
                    result.patches.push_back(std::move(p));
                    result.stats.childrenRemoved++;
                }
            }

            // ─── الخطوة 3: تحديد الأبناء المُدرجين ───
            // (AR) أبناء موجودين في الجديدة وغير موجودين في القديمة
            for (size_t i = 0; i < newChildren.size(); i++)
            {
                const std::string &key = newKeys[i];
                if (oldKeyMap.find(key) == oldKeyMap.end())
                {
                    Patch p;
                    p.path = currentPath;
                    p.type = PatchType::INSERT_CHILD;
                    p.insertedChild = newChildren[i]; // (AR) نشير للعقدة الجديدة مباشرة
                    p.insertIndex = i;
                    result.patches.push_back(std::move(p));
                    result.stats.childrenInserted++;
                }
            }

            // ─── الخطوة 4: مقارنة الأبناء المتبقين تكرارياً ───
            // (AR) أبناء موجودون في كلتا الشجرتين → نقارنهم تكرارياً
            for (size_t newIdx = 0; newIdx < newChildren.size(); newIdx++)
            {
                const std::string &key = newKeys[newIdx];
                auto oldIt = oldKeyMap.find(key);
                if (oldIt != oldKeyMap.end())
                {
                    size_t oldIdx = oldIt->second;
                    // (AR) المقارنة التكرارية — نستخدم الفهرس الجديد في المسار
                    currentPath.push_back(newIdx);
                    diffNodes(oldChildren[oldIdx].get(), newChildren[newIdx].get(),
                              currentPath, result);
                    currentPath.pop_back();
                }
            }

            // ─── الخطوة 5: فحص إعادة الترتيب ───
            // (AR) نفحص إذا كان ترتيب الأبناء المتبقين قد تغير
            // (EN) Check if the order of remaining children changed
            std::vector<size_t> remainingOldOrder;
            std::vector<size_t> remainingNewOrder;

            for (size_t newIdx = 0; newIdx < newChildren.size(); newIdx++)
            {
                const std::string &key = newKeys[newIdx];
                auto oldIt = oldKeyMap.find(key);
                if (oldIt != oldKeyMap.end())
                {
                    remainingOldOrder.push_back(oldIt->second);
                    remainingNewOrder.push_back(newIdx);
                }
            }

            // (AR) فحص إذا كان الترتيب النسبي للأبناء المتبقين قد تغير
            // (EN) Check if the relative order of remaining children changed
            if (remainingOldOrder.size() > 1)
            {
                bool orderChanged = false;
                for (size_t i = 1; i < remainingOldOrder.size(); i++)
                {
                    if (remainingOldOrder[i] < remainingOldOrder[i - 1])
                    {
                        orderChanged = true;
                        break;
                    }
                }

                if (orderChanged)
                {
                    Patch p;
                    p.path = currentPath;
                    p.type = PatchType::REORDER_CHILDREN;
                    // (AR) الترتيب الجديد = فهارس الأبناء في الشجرة الجديدة
                    for (size_t i = 0; i < newChildren.size(); i++)
                    {
                        p.newOrder.push_back(i);
                    }
                    result.patches.push_back(std::move(p));
                    result.stats.childrenReordered++;
                }
            }
        }

        // ═══════════════════════════════════════════════════════════════════════════════
        // getNodeKey — الحصول على مفتاح العقدة
        // ═══════════════════════════════════════════════════════════════════════════════

        std::string Reconciler::getNodeKey(const IRNode *node, size_t index) const
        {
            // (AR) أولوية المفتاح:
            //   1. id المُعيّن صراحة (من خاصية "مفتاح" أو id)
            //   2. تركيبة من النوع والفهرس (fallback)
            if (node && !node->getId().empty())
            {
                return node->getId();
            }

            // (AR) بدون مفتاح → نستخدم النوع + الفهرس
            // (EN) No key → use type + index
            if (node)
            {
                return std::to_string(static_cast<int>(node->getType())) + "_" + std::to_string(index);
            }

            return "__null_" + std::to_string(index);
        }

        // ═══════════════════════════════════════════════════════════════════════════════
        // patch — تطبيق الرقع على الشجرة الحية
        // ═══════════════════════════════════════════════════════════════════════════════

        bool Reconciler::patch(std::shared_ptr<IRNode> &liveTree,
                               const DiffResult &diffResult)
        {
            if (diffResult.isEmpty())
                return true;

            for (const auto &p : diffResult.patches)
            {
                if (!applySinglePatch(liveTree, p))
                {
                    return false;
                }
            }

            return true;
        }

        // ═══════════════════════════════════════════════════════════════════════════════
        // applySinglePatch — تطبيق رقعة واحدة
        // ═══════════════════════════════════════════════════════════════════════════════

        bool Reconciler::applySinglePatch(std::shared_ptr<IRNode> &root, const Patch &p)
        {
            switch (p.type)
            {
            case PatchType::REPLACE:
            {
                if (p.path.empty())
                {
                    // (AR) استبدال الجذر بالكامل
                    root = p.replacementNode;
                    if (onNodeUpdated_ && root)
                        onNodeUpdated_(root.get(), PatchType::REPLACE);
                    return true;
                }

                // (AR) استبدال عقدة فرعية — نحتاج الوصول للأب
                IRNode *parent = navigateToParent(root.get(), p.path);
                if (!parent)
                    return false;

                size_t childIdx = p.path.back();
                auto &children = const_cast<std::vector<std::shared_ptr<IRNode>> &>(parent->getChildren());
                if (childIdx < children.size())
                {
                    children[childIdx] = p.replacementNode;
                    if (onNodeUpdated_ && p.replacementNode)
                        onNodeUpdated_(p.replacementNode.get(), PatchType::REPLACE);
                    return true;
                }
                return false;
            }

            case PatchType::UPDATE_PROPS:
            {
                IRNode *target = navigateToNode(root.get(), p.path);
                if (!target)
                    return false;

                // (AR) تحديث الخصائص المتغيرة
                for (const auto &prop : p.updatedProps)
                {
                    target->setProperty(prop.key, prop.value);
                }

                // (AR) حذف الخصائص المحذوفة
                if (!p.removedProps.empty())
                {
                    auto &props = const_cast<std::vector<IRProperty> &>(target->getProperties());
                    for (const auto &key : p.removedProps)
                    {
                        props.erase(
                            std::remove_if(props.begin(), props.end(),
                                           [&key](const IRProperty &prop)
                                           { return prop.key == key; }),
                            props.end());
                    }
                }

                if (onNodeUpdated_)
                    onNodeUpdated_(target, PatchType::UPDATE_PROPS);
                return true;
            }

            case PatchType::UPDATE_EVENTS:
            {
                IRNode *target = navigateToNode(root.get(), p.path);
                if (!target)
                    return false;

                // (AR) استبدال جميع الأحداث
                auto &events = target->getMutableEvents();
                events = p.updatedEvents;

                if (onNodeUpdated_)
                    onNodeUpdated_(target, PatchType::UPDATE_EVENTS);
                return true;
            }

            case PatchType::INSERT_CHILD:
            {
                IRNode *target = navigateToNode(root.get(), p.path);
                if (!target)
                    return false;

                auto &children = const_cast<std::vector<std::shared_ptr<IRNode>> &>(target->getChildren());
                size_t idx = std::min(p.insertIndex, children.size());
                children.insert(children.begin() + static_cast<ptrdiff_t>(idx), p.insertedChild);

                if (onNodeUpdated_)
                    onNodeUpdated_(target, PatchType::INSERT_CHILD);
                return true;
            }

            case PatchType::REMOVE_CHILD:
            {
                IRNode *target = navigateToNode(root.get(), p.path);
                if (!target)
                    return false;

                auto &children = const_cast<std::vector<std::shared_ptr<IRNode>> &>(target->getChildren());
                if (p.removeIndex < children.size())
                {
                    children.erase(children.begin() + static_cast<ptrdiff_t>(p.removeIndex));
                }

                if (onNodeUpdated_)
                    onNodeUpdated_(target, PatchType::REMOVE_CHILD);
                return true;
            }

            case PatchType::REORDER_CHILDREN:
            {
                IRNode *target = navigateToNode(root.get(), p.path);
                if (!target)
                    return false;

                auto &children = const_cast<std::vector<std::shared_ptr<IRNode>> &>(target->getChildren());

                // (AR) إعادة ترتيب الأبناء حسب p.newOrder
                // (EN) We need to build the reordered list based on the new tree's structure
                // (AR) بما أن الرقعة قد أُنشئت بعد INSERT/REMOVE، نحتاج حذراً
                //      نستخدم الطريقة الآمنة: ننسخ المصفوفة ثم نعيد ترتيبها
                if (p.newOrder.size() == children.size())
                {
                    std::vector<std::shared_ptr<IRNode>> reordered(children.size());
                    for (size_t i = 0; i < p.newOrder.size(); i++)
                    {
                        if (p.newOrder[i] < children.size())
                        {
                            reordered[i] = children[p.newOrder[i]];
                        }
                    }
                    children = std::move(reordered);
                }

                if (onNodeUpdated_)
                    onNodeUpdated_(target, PatchType::REORDER_CHILDREN);
                return true;
            }

            case PatchType::UPDATE_ANIMATIONS:
            {
                IRNode *target = navigateToNode(root.get(), p.path);
                if (!target)
                    return false;

                auto &anims = target->getMutableAnimations();
                anims = p.updatedAnimations;

                if (onNodeUpdated_)
                    onNodeUpdated_(target, PatchType::UPDATE_ANIMATIONS);
                return true;
            }

            case PatchType::UPDATE_STATE_REFS:
            {
                // (AR) لا نُطبّق هذا حالياً — إشارات الحالة تُدار من UIBridge
                return true;
            }

            default:
                return false;
            }
        }

        // ═══════════════════════════════════════════════════════════════════════════════
        // navigateToNode — الوصول لعقدة عبر المسار
        // ═══════════════════════════════════════════════════════════════════════════════

        IRNode *Reconciler::navigateToNode(IRNode *root, const std::vector<size_t> &path)
        {
            if (!root)
                return nullptr;

            IRNode *current = root;
            for (size_t idx : path)
            {
                if (idx >= current->childCount())
                    return nullptr;
                current = current->getChildren()[idx].get();
                if (!current)
                    return nullptr;
            }
            return current;
        }

        // ═══════════════════════════════════════════════════════════════════════════════
        // navigateToParent — الوصول لعقدة الأب
        // ═══════════════════════════════════════════════════════════════════════════════

        IRNode *Reconciler::navigateToParent(IRNode *root, const std::vector<size_t> &path)
        {
            if (!root || path.empty())
                return nullptr;

            // (AR) المسار بدون آخر عنصر = مسار الأب
            std::vector<size_t> parentPath(path.begin(), path.end() - 1);
            return navigateToNode(root, parentPath);
        }

    } // namespace ui
} // namespace sad
