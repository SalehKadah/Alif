/* وحدةُ الرسومات — جسرُ ألف إلى مكتبة sad_ui
 *
 * تُحوِّل كائناتِ ألف إلى شجرة IRNode مرّةً واحدة، ثمّ تُسلِّمها لخلفيّة
 * سطح المكتب (SDL2 + SDL2_ttf). بعد التحويل لا تعرف المكتبةُ شيئاً عن ألف.
 *
 * العنصرُ صنفٌ حقيقيٌّ ذو جدولِ طرائق، فتصحّ السَّلسلةُ التصريحيّة:
 *     نص_عنصر("مرحباً").حجم_خط(32).لون("#ffd479")
 * وهذه صيغةُ لغة ص نفسِها، ولم تُمَسَّ بها قواعدُ نحوِ ألف.
 */

#include "alif.h"
#include "AlifCore_ModuleObject.h"
#include "AlifCore_ModSupport.h"

#include "sad_ui/ir.h"
#include "sad_ui/types.h"
#include "sad_ui/prop_keys.h"
#include "sad_ui/desktop/window.h"
#include "sad_ui/reconciler.h"

#include <memory>
#include <string>
#include <unordered_map>

using sad::ui::IRNode;
using sad::ui::UINodeType;
namespace props = sad::ui::props;

using sad::ui::IREventType;
using NodePtr = std::shared_ptr<IRNode>;

extern AlifModuleDef _alifGraphicsModule_;

/* ═══ حالةُ الوحدة ═══ */

class GraphicsState {
public:
	AlifObject* elementType{};
};

static inline GraphicsState* getGraphicsState(AlifObject* _module) {
	return (GraphicsState*)_alifModule_getState(_module);
}

/* ═══ صنفُ العنصر ═══ */

class ElementObject {
public:
	ALIFOBJECT_HEAD{};
	NodePtr node{};
};

/* ألفُ لا تكشف فتحةَ حارسٍ خاصّة، فنُنظّف في فتحة التحرير:
   نستدعي الهادمَ صراحةً ثمّ نُعيد الذاكرة. */
static void element_free(void* _ptr) {
	ElementObject* self = (ElementObject*)_ptr;
	self->node.~shared_ptr();
	alifMem_objFree(_ptr);
}

/* ═══ أدواتٌ صغيرة ═══ */

static bool asUTF8(AlifObject* _obj, std::string* _out) {
	AlifSizeT len = 0;
	const char* s = alifUStr_asUTF8AndSize(_obj, &len);
	if (s == nullptr) return false;
	_out->assign(s, static_cast<size_t>(len));
	return true;
}

/* ألفُ تنادي دوالَّها بـvectorcall، و يفحص 
   وحدَه فيُخطئ في دوالِّ ألف نفسِها. نفحص الاثنَين. */
static bool isCallable(AlifObject* _obj) {
	if (_obj == nullptr) return false;
	AlifTypeObject* t = ALIF_TYPE(_obj);
	if (t->call != nullptr) return true;
	return (t->flags & ALIF_TPFLAGS_HAVE_VECTORCALL) != 0 and t->vectorCallOffset != 0;
}

static AlifObject* newElement(AlifObject* _module, NodePtr _node) {
	GraphicsState* state = getGraphicsState(_module);
	AlifTypeObject* type = (AlifTypeObject*)state->elementType;
	ElementObject* self = (ElementObject*)type->alloc(type, 0);
	if (self == nullptr) return nullptr;
	/* الذاكرةُ صفريّةٌ من alloc — نبني المؤشّرَ الذكيّ في مكانه */
	new (&self->node) NodePtr(std::move(_node));
	return (AlifObject*)self;
}

/* يقبل عنصراً ويُعيد عقدتَه، أو nullptr مع استثناءٍ مضبوط */
static NodePtr nodeOf(AlifObject* _module, AlifObject* _obj, const char* _where) {
	GraphicsState* state = getGraphicsState(_module);
	if (!ALIF_IS_TYPE(_obj, (AlifTypeObject*)state->elementType)) {
		alifErr_format(_alifExcTypeError_, "%s: يُتوقّع عنصرُ رسومات", _where);
		return nullptr;
	}
	return ((ElementObject*)_obj)->node;
}

/* ═══ سجلُّ المعالِجات ═══
   المعالِجُ كائنُ ألفٍ قابلٌ للنداء. نحفظ مرجعاً له مفتاحُه نصٌّ، ونضع
   المفتاحَ في `IREvent::expression`، فتبقى شجرةُ IR جاهلةً بألفَ تماماً. */

static std::unordered_map<std::string, AlifObject*> _graphicsHandlers_{};
static unsigned long long _graphicsHandlerSeq_ = 0;

static void clearHandlers() {
	for (auto& kv : _graphicsHandlers_) ALIF_XDECREF(kv.second);
	_graphicsHandlers_.clear();
}

/* ═══ طرائقُ السَّلسلة — كلٌّ منها يُعيد العنصرَ نفسَه ═══ */

static AlifObject* setTextProp(ElementObject* _self, AlifObject* _value, const char* _key) {
	std::string v;
	if (!asUTF8(_value, &v)) return nullptr;
	_self->node->setProperty(_key, v);
	return ALIF_NEWREF((AlifObject*)_self);
}

static AlifObject* setNumProp(ElementObject* _self, AlifObject* _value, const char* _key) {
	double v = alifFloat_asDouble(_value);
	if (v == -1.0 and alifErr_occurred()) return nullptr;
	_self->node->setProperty(_key, v);
	return ALIF_NEWREF((AlifObject*)_self);
}

static AlifObject* element_color(ElementObject* s, AlifObject* v)    { return setTextProp(s, v, props::COLOR); }
static AlifObject* element_bgColor(ElementObject* s, AlifObject* v)  { return setTextProp(s, v, props::BG_COLOR); }
static AlifObject* element_text(ElementObject* s, AlifObject* v)     { return setTextProp(s, v, props::TEXT); }
static AlifObject* element_fontSize(ElementObject* s, AlifObject* v) { return setNumProp(s, v, props::FONT_SIZE); }
static AlifObject* element_width(ElementObject* s, AlifObject* v)    { return setNumProp(s, v, props::WIDTH); }
static AlifObject* element_height(ElementObject* s, AlifObject* v)   { return setNumProp(s, v, props::HEIGHT); }
static AlifObject* element_padding(ElementObject* s, AlifObject* v)  { return setNumProp(s, v, props::PADDING); }
static AlifObject* element_spacing(ElementObject* s, AlifObject* v)  { return setNumProp(s, v, props::SPACING); }

/* .عند_النقر(دالّة) — تُسجّل المعالِجَ وتُعيد العنصرَ نفسَه */
static AlifObject* element_onTap(ElementObject* _self, AlifObject* _callable) {
	if (!isCallable(_callable)) {
		alifErr_setString(_alifExcTypeError_, "عند_النقر: يُتوقّع شيءٌ قابلٌ للنداء");
		return nullptr;
	}
	char key[32]{};
	snprintf(key, sizeof(key), "h%llu", ++_graphicsHandlerSeq_);

	AlifObject*& slot = _graphicsHandlers_[key];
	ALIF_XDECREF(slot);
	slot = ALIF_NEWREF(_callable);

	sad::ui::IREvent ev{};
	ev.type = IREventType::OnTap;
	ev.expression = key;
	_self->node->addEvent(ev);
	return ALIF_NEWREF((AlifObject*)_self);
}

static AlifMethodDef _elementMethods_[] = {
	{"لون",         ALIF_CPPFUNCTION_CAST(element_color),    METHOD_O},
	{"لون_خلفية",   ALIF_CPPFUNCTION_CAST(element_bgColor),  METHOD_O},
	{"نص",          ALIF_CPPFUNCTION_CAST(element_text),     METHOD_O},
	{"حجم_خط",      ALIF_CPPFUNCTION_CAST(element_fontSize), METHOD_O},
	{"عرض",         ALIF_CPPFUNCTION_CAST(element_width),    METHOD_O},
	{"ارتفاع",      ALIF_CPPFUNCTION_CAST(element_height),   METHOD_O},
	{"حشوة",        ALIF_CPPFUNCTION_CAST(element_padding),  METHOD_O},
	{"تباعد",       ALIF_CPPFUNCTION_CAST(element_spacing),  METHOD_O},
	{"عند_النقر",   ALIF_CPPFUNCTION_CAST(element_onTap),    METHOD_O},
	{nullptr, nullptr}
};

static AlifTypeSlot _elementTypeSlots_[] = {
	{ALIF_TP_METHODS, _elementMethods_},
	{ALIF_TP_FREE,    (void*)element_free},
	{0, 0},
};

static AlifTypeSpec _elementTypeSpec_ = {
	.name = "رسومات.عنصر",
	.basicsize = sizeof(ElementObject),
	.flags = ALIF_TPFLAGS_DEFAULT,
	.slots = _elementTypeSlots_
};

/* ═══ البنّاؤون ═══ */

/* نص_عنصر("…") — الخصائصُ تُضبط بالسَّلسلة بعدها */
static AlifObject* graphics_text(AlifObject* _module, AlifObject* _value) {
	std::string text;
	AlifObject* asStr = nullptr;
	if (!ALIFUSTR_CHECK(_value)) {
		asStr = alifObject_str(_value);
		if (asStr == nullptr) return nullptr;
		_value = asStr;
	}
	if (!asUTF8(_value, &text)) { ALIF_XDECREF(asStr); return nullptr; }
	ALIF_XDECREF(asStr);
	NodePtr node = IRNode::create(UINodeType::Text);
	node->setProperty(props::TEXT, text);
	return newElement(_module, std::move(node));
}

static AlifObject* buildContainer(AlifObject* _module, UINodeType _type,
                                  AlifObject* _args, const char* _where) {
	NodePtr node = IRNode::create(_type);
	AlifSizeT count = alifTuple_size(_args);
	for (AlifSizeT i = 0; i < count; i++) {
		NodePtr child = nodeOf(_module, alifTuple_getItem(_args, i), _where);
		if (child == nullptr) return nullptr;
		node->addChild(child);
	}
	return newElement(_module, std::move(node));
}

static AlifObject* graphics_column(AlifObject* m, AlifObject* a) { return buildContainer(m, UINodeType::Column, a, "عمود"); }
static AlifObject* graphics_row(AlifObject* m, AlifObject* a)    { return buildContainer(m, UINodeType::Row,    a, "صف"); }
static AlifObject* graphics_button(AlifObject* _module, AlifObject* _value) {
	std::string text;
	if (!asUTF8(_value, &text)) return nullptr;
	NodePtr node = IRNode::create(UINodeType::Button);
	node->setProperty(props::TEXT, text);
	return newElement(_module, std::move(node));
}

static AlifObject* graphics_stack(AlifObject* m, AlifObject* a)  { return buildContainer(m, UINodeType::Stack,  a, "رصة"); }

/* ═══ التشغيل ═══ */

/* يُعيد شجرةَ IR من نتيجةِ نداءِ دالّةِ البناء، أو من عنصرٍ مباشرةً.
   يحتفظ بمرجعِ ألفَ في `_keepAlive` حتّى ينتهي الترقيع. */
static NodePtr buildTree(AlifObject* _module, AlifObject* _source,
                         AlifObject** _keepAlive, const char* _where) {
	*_keepAlive = nullptr;
	AlifObject* obj = _source;
	
	if (isCallable(_source)) {
		AlifObject* noArgs = alifTuple_new(0);
		if (noArgs == nullptr) return nullptr;
		obj = alifObject_callObject(_source, noArgs);
		ALIF_DECREF(noArgs);
		if (obj == nullptr) return nullptr;
		*_keepAlive = obj;
	}
	NodePtr tree = nodeOf(_module, obj, _where);
	if (tree == nullptr) {
		ALIF_XDECREF(*_keepAlive);
		*_keepAlive = nullptr;
	}
	return tree;
}

static bool needsRelayout(const sad::ui::DiffResult& _d) {
	for (const auto& p : _d.patches) {
		if (p.type == sad::ui::PatchType::INSERT_CHILD
		    or p.type == sad::ui::PatchType::REMOVE_CHILD
		    or p.type == sad::ui::PatchType::REORDER_CHILDREN
		    or p.type == sad::ui::PatchType::REPLACE) return true;
	}
	return false;
}

/* تشغيل_تطبيق(دالّة_البناء أو عنصر، عنوان=، عرض=، ارتفاع=)
 *
 * إن كان الوسيطُ دالّةً استُدعيت لبناء الشجرة، وأُعيد استدعاؤها بعد كلّ
 * معالِجِ حدث. الفرقُ بين الشجرتَين يُحسب بـ`diff` ويُرقَّع بـ`patch`،
 * فلا يُعاد بناءُ النافذة ولا تُصفَّر التحريكات.
 */
static AlifObject* graphics_run(AlifObject* _module, AlifObject* _args, AlifObject* _kwargs) {
	static const char* const kwlist[] = {"", "عنوان", "عرض", "ارتفاع", nullptr};
	AlifObject* source = nullptr;
	const char* title = "ألف";
	AlifIntT width = 800;
	AlifIntT height = 600;
	if (!alifArg_parseTupleAndKeywords(_args, _kwargs, "O|sii", kwlist,
	                                   &source, &title, &width, &height)) return nullptr;

	AlifObject* liveRef = nullptr;
	NodePtr liveTree = buildTree(_module, source, &liveRef, "تشغيل_تطبيق");
	if (liveTree == nullptr) return nullptr;

	sad::ui::Reconciler reconciler{};
	bool ok = false;
	bool failed = false;
	{
		sad::ui::desktop::DesktopWindow window;
		sad::ui::desktop::WindowOptions options;
		options.title = title;
		options.width = width;
		options.height = height;

		/* حلقةُ SDL حاجزةٌ — نُطلِق القفلَ العامّ طوالها.
		   واللامدا تُعرَّف داخلَ الكتلةِ عمداً: ماكرَوا BLOCK/UNBLOCK
		   يتوسّعان إلى `_save` المصرَّحِ في BEGIN_ALLOW_THREADS وحدَها. */
		ALIF_BEGIN_ALLOW_THREADS
		if (window.create(options)) {
			window.setContent(liveTree);

			window.setOnEventCallback(
				[&](IREventType _type, const std::string& _expr,
				    const IRNode*, const sad::ui::EventData&) {
					if (_type != IREventType::OnTap) return;
					auto it = _graphicsHandlers_.find(_expr);
					if (it == _graphicsHandlers_.end()) return;

					/* نستعيد القفلَ لنداءِ ألف، ثمّ نُطلِقه ثانيةً */
					ALIF_BLOCK_THREADS

					/* المعالِجُ نفسُه سيُمحى مع الجيل القديم، فنُمسكه أوّلاً */
					AlifObject* handler = ALIF_NEWREF(it->second);
					AlifObject* noArgs = alifTuple_new(0);
					AlifObject* result = (noArgs == nullptr) ? nullptr
					                   : alifObject_callObject(handler, noArgs);
					ALIF_XDECREF(noArgs);
					if (result == nullptr) { failed = true; window.close(); }
					else {
						ALIF_DECREF(result);
						if (isCallable(source)) {
							/* جيلٌ جديد: نُخلي السجلَّ فتُسجَّل معالِجاتُ الشجرة
							   الجديدة وحدَها، ثمّ نُسقط مراجعَ الجيل السابق. */
							auto previous = std::move(_graphicsHandlers_);
							_graphicsHandlers_.clear();

							AlifObject* newRef = nullptr;
							NodePtr newTree = buildTree(_module, source, &newRef, "تشغيل_تطبيق");
							if (newTree == nullptr) { failed = true; window.close(); }
							else {
								auto d = reconciler.diff(liveTree, newTree);
								if (!d.isEmpty() and reconciler.patch(liveTree, d)) {
									window.applyPatches(d.size(), needsRelayout(d));
								}
								ALIF_XDECREF(newRef);
							}
							for (auto& kv : previous) ALIF_XDECREF(kv.second);
						}
					}
					ALIF_DECREF(handler);

					ALIF_UNBLOCK_THREADS
				});

			window.run();
			window.destroy();
			ok = true;
		}
		ALIF_END_ALLOW_THREADS
	}

	ALIF_XDECREF(liveRef);
	clearHandlers();

	if (failed) return nullptr;   /* الاستثناءُ مضبوطٌ من نداءِ ألف */
	if (!ok) {
		alifErr_setString(_alifExcRuntimeError_, "تشغيل_تطبيق: تعذّر إنشاءُ النافذة");
		return nullptr;
	}
	return ALIF_NONE;
}

/* رسم_ولقطة(جذر، مسار، عرض=، ارتفاع=) — للفحص الآليّ بلا تفاعل */
static AlifObject* graphics_snapshot(AlifObject* _module, AlifObject* _args) {
	AlifObject* rootObj = nullptr;
	AlifObject* pathObj = nullptr;
	AlifIntT width = 800;
	AlifIntT height = 600;
	if (!alifArg_parseTuple(_args, "OO|ii", &rootObj, &pathObj, &width, &height)) return nullptr;

	AlifObject* keep = nullptr;
	NodePtr root = buildTree(_module, rootObj, &keep, "رسم_ولقطة");
	if (root == nullptr) return nullptr;
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
	ALIF_XDECREF(keep);
	return alifBool_fromLong(ok ? 1 : 0);
}

/* ═══ التسجيل ═══ */

static AlifMethodDef _alifGraphicsMethods_[] = {
	{"نص_عنصر",       ALIF_CPPFUNCTION_CAST(graphics_text),     METHOD_O},
	{"عمود",          ALIF_CPPFUNCTION_CAST(graphics_column),   METHOD_VARARGS},
	{"صف",            ALIF_CPPFUNCTION_CAST(graphics_row),      METHOD_VARARGS},
	{"رصة",           ALIF_CPPFUNCTION_CAST(graphics_stack),    METHOD_VARARGS},
	{"زر",            ALIF_CPPFUNCTION_CAST(graphics_button),   METHOD_O},
	{"تشغيل_تطبيق",   ALIF_CPPFUNCTION_CAST(graphics_run),      METHOD_VARARGS | METHOD_KEYWORDS},
	{"رسم_ولقطة",     ALIF_CPPFUNCTION_CAST(graphics_snapshot), METHOD_VARARGS},
	{nullptr, nullptr}
};

static AlifIntT graphics_exec(AlifObject* _module) {
	GraphicsState* state = getGraphicsState(_module);
	state->elementType = alifType_fromModuleAndSpec(_module, &_elementTypeSpec_, nullptr);
	if (state->elementType == nullptr) return -1;
	if (alifModule_addType(_module, (AlifTypeObject*)state->elementType) < 0) return -1;
	return 0;
}

static AlifModuleDefSlot _alifGraphicsSlots_[] = {
	{ALIF_MOD_EXEC, (void*)graphics_exec},
	{0, nullptr}
};

AlifModuleDef _alifGraphicsModule_ = {
	.base = ALIFMODULEDEF_HEAD_INIT,
	.name = "رسومات",
	.size = sizeof(GraphicsState),
	.methods = _alifGraphicsMethods_,
	.slots = _alifGraphicsSlots_,
};

AlifObject* alifInit_graphics(void) {
	return alifModuleDef_init(&_alifGraphicsModule_);
}
