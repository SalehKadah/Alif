/* وحدةُ الرسومات — جسرُ ألف إلى مكتبة sad_ui
 *
 * تُحوِّل كائناتِ ألف إلى شجرة IRNode مرّةً واحدة، ثمّ تُسلِّمها لخلفيّة
 * سطح المكتب (SDL2 + SDL2_ttf). بعد التحويل لا تعرف المكتبةُ شيئاً عن ألف.
 */

#include "alif.h"

#include "sad_ui/ir.h"
#include "sad_ui/types.h"
#include "sad_ui/prop_keys.h"
#include "sad_ui/desktop/window.h"

#include <memory>
#include <string>
#include <vector>

using sad::ui::IRNode;
using sad::ui::UINodeType;
namespace props = sad::ui::props;

static const char* ALIF_GRAPHICS_CAPSULE = "رسومات.عنصر";

/* ═══ الكبسولة: تملك مؤشّراً ذكيّاً إلى عقدةِ IR ═══ */

using NodePtr = std::shared_ptr<IRNode>;

static void graphics_capsuleFree(AlifObject* _capsule) {
	void* p = alifCapsule_getPointer(_capsule, ALIF_GRAPHICS_CAPSULE);
	delete static_cast<NodePtr*>(p);
}

static AlifObject* wrapNode(NodePtr _node) {
	NodePtr* held = new NodePtr(std::move(_node));
	AlifObject* cap = alifCapsule_new(held, ALIF_GRAPHICS_CAPSULE, graphics_capsuleFree);
	if (cap == nullptr) { delete held; return nullptr; }
	return cap;
}

static NodePtr unwrapNode(AlifObject* _obj) {
	void* p = alifCapsule_getPointer(_obj, ALIF_GRAPHICS_CAPSULE);
	if (p == nullptr) return nullptr;
	return *static_cast<NodePtr*>(p);
}

/* ═══ أدواتٌ صغيرة ═══ */

static bool asUTF8(AlifObject* _obj, std::string* _out) {
	AlifSizeT len = 0;
	const char* s = alifUStr_asUTF8AndSize(_obj, &len);
	if (s == nullptr) return false;
	_out->assign(s, static_cast<size_t>(len));
	return true;
}

/* ═══ بنّاؤو العناصر ═══ */

/* نص_عنصر(النص، حجم=،  لون=) */
static AlifObject* graphics_text(AlifObject* /*_module*/, AlifObject* _args) {
	AlifObject* textObj = nullptr;
	AlifObject* sizeObj = nullptr;
	AlifObject* colorObj = nullptr;
	if (!alifArg_parseTuple(_args, "O|OO", &textObj, &sizeObj, &colorObj)) return nullptr;

	std::string text;
	if (!asUTF8(textObj, &text)) return nullptr;

	NodePtr node = IRNode::create(UINodeType::Text);
	node->setProperty(props::TEXT, text);

	if (sizeObj != nullptr and sizeObj != ALIF_NONE) {
		double sz = alifFloat_asDouble(sizeObj);
		if (sz == -1.0 and alifErr_occurred()) return nullptr;
		node->setProperty(props::FONT_SIZE, sz);
	}
	if (colorObj != nullptr and colorObj != ALIF_NONE) {
		std::string color;
		if (!asUTF8(colorObj, &color)) return nullptr;
		node->setProperty(props::COLOR, color);
	}
	return wrapNode(std::move(node));
}

/* بنّاءٌ عامٌّ للحاويات: عمود(...) و صف(...) */
static AlifObject* buildContainer(UINodeType _type, AlifObject* _args) {
	NodePtr node = IRNode::create(_type);
	AlifSizeT count = alifTuple_size(_args);
	for (AlifSizeT i = 0; i < count; i++) {
		AlifObject* item = alifTuple_getItem(_args, i);
		NodePtr child = unwrapNode(item);
		if (child == nullptr) {
			alifErr_setString(_alifExcTypeError_, "عمود/صف: يُتوقّع عنصرُ رسوماتٍ في كلّ وسيط");
			return nullptr;
		}
		node->addChild(child);
	}
	return wrapNode(std::move(node));
}

static AlifObject* graphics_column(AlifObject* /*_module*/, AlifObject* _args) {
	return buildContainer(UINodeType::Column, _args);
}

static AlifObject* graphics_row(AlifObject* /*_module*/, AlifObject* _args) {
	return buildContainer(UINodeType::Row, _args);
}

/* لون_خلفية(عنصر، "#rrggbb") — يُعيد العنصرَ نفسَه */
static AlifObject* graphics_bgColor(AlifObject* /*_module*/, AlifObject* _args) {
	AlifObject* nodeObj = nullptr;
	AlifObject* colorObj = nullptr;
	if (!alifArg_parseTuple(_args, "OO", &nodeObj, &colorObj)) return nullptr;
	NodePtr node = unwrapNode(nodeObj);
	if (node == nullptr) {
		alifErr_setString(_alifExcTypeError_, "لون_خلفية: الوسيطُ الأوّلُ ليس عنصرَ رسومات");
		return nullptr;
	}
	std::string color;
	if (!asUTF8(colorObj, &color)) return nullptr;
	node->setProperty(props::BG_COLOR, color);
	return ALIF_NEWREF(nodeObj);
}

/* ═══ تشغيل_تطبيق(الجذر، عنوان=، عرض=، ارتفاع=) ═══ */

static AlifObject* graphics_run(AlifObject* /*_module*/, AlifObject* _args, AlifObject* _kwargs) {
	static const char* const kwlist[] = {"", "عنوان", "عرض", "ارتفاع", nullptr};
	AlifObject* rootObj = nullptr;
	const char* title = "ألف";
	AlifIntT width = 800;
	AlifIntT height = 600;
	if (!alifArg_parseTupleAndKeywords(_args, _kwargs, "O|sii", kwlist,
	                                   &rootObj, &title, &width, &height)) return nullptr;

	NodePtr root = unwrapNode(rootObj);
	if (root == nullptr) {
		alifErr_setString(_alifExcTypeError_, "تشغيل_تطبيق: الوسيطُ ليس عنصرَ رسومات");
		return nullptr;
	}

	bool ok = false;
	{
		sad::ui::desktop::DesktopWindow window;
		sad::ui::desktop::WindowOptions options;
		options.title = title;
		options.width = width;
		options.height = height;

		/* حلقةُ SDL حاجزةٌ — نُطلِق القفلَ العامّ طوالها */
		ALIF_BEGIN_ALLOW_THREADS
		if (window.create(options)) {
			window.setContent(root);
			window.run();
			window.destroy();
			ok = true;
		}
		ALIF_END_ALLOW_THREADS
	}

	if (!ok) {
		alifErr_setString(_alifExcRuntimeError_, "تشغيل_تطبيق: تعذّر إنشاءُ النافذة");
		return nullptr;
	}
	return ALIF_NONE;
}

/* رسم_ولقطة(الجذر، المسار، عرض=، ارتفاع=) — للفحص الآليّ بلا تفاعل */
static AlifObject* graphics_snapshot(AlifObject* /*_module*/, AlifObject* _args) {
	AlifObject* rootObj = nullptr;
	AlifObject* pathObj = nullptr;
	AlifIntT width = 800;
	AlifIntT height = 600;
	if (!alifArg_parseTuple(_args, "OO|ii", &rootObj, &pathObj, &width, &height)) return nullptr;

	NodePtr root = unwrapNode(rootObj);
	if (root == nullptr) {
		alifErr_setString(_alifExcTypeError_, "رسم_ولقطة: الوسيطُ الأوّلُ ليس عنصرَ رسومات");
		return nullptr;
	}
	std::string path;
	if (!asUTF8(pathObj, &path)) return nullptr;

	bool ok = false;
	{
		sad::ui::desktop::DesktopWindow window;
		sad::ui::desktop::WindowOptions options;
		options.title = "ألف";
		options.width = width;
		options.height = height;

		ALIF_BEGIN_ALLOW_THREADS
		if (window.create(options)) {
			window.setContent(root);
			for (int i = 0; i < 6; i++) window.runOneFrame();
			ok = window.takeScreenshot(path);
			window.destroy();
		}
		ALIF_END_ALLOW_THREADS
	}
	return alifBool_fromLong(ok ? 1 : 0);
}

/* ═══ التسجيل ═══ */

static AlifMethodDef _alifGraphicsMethods_[] = {
	{"نص_عنصر",       ALIF_CPPFUNCTION_CAST(graphics_text),     METHOD_VARARGS},
	{"عمود",          ALIF_CPPFUNCTION_CAST(graphics_column),   METHOD_VARARGS},
	{"صف",            ALIF_CPPFUNCTION_CAST(graphics_row),      METHOD_VARARGS},
	{"لون_خلفية",     ALIF_CPPFUNCTION_CAST(graphics_bgColor),  METHOD_VARARGS},
	{"تشغيل_تطبيق",   ALIF_CPPFUNCTION_CAST(graphics_run),      METHOD_VARARGS | METHOD_KEYWORDS},
	{"رسم_ولقطة",     ALIF_CPPFUNCTION_CAST(graphics_snapshot), METHOD_VARARGS},
	{nullptr, nullptr}
};

static class AlifModuleDef _alifGraphicsModule_ = {
	.base = ALIFMODULEDEF_HEAD_INIT,
	.name = "رسومات",
	.methods = _alifGraphicsMethods_
};

AlifObject* alifInit_graphics(void) {
	return alifModuleDef_init(&_alifGraphicsModule_);
}
