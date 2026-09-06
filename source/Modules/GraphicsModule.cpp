/* الوحدةُ تُبنى فقط حين تتوفّر SDL2 و SDL2_ttf.
 * على ويندوز يُعرَّف ALIF_WITH_GRAPHICS في vcxproj، وعلى لينكس/ماك في
 * Makefile بعد كشفِ المكتبتَين بـpkg-config. وبدونه يُصرَّف هذا الملفُّ
 * إلى لا شيء، فتبقى ألفُ كما كانت. */
#ifdef ALIF_WITH_GRAPHICS

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

#include <chrono>
#include <memory>
#include <thread>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>

/* قياسُ الذاكرة المقيمة — لكلّ منصّةٍ بابُها */
#if defined(_WIN32)
#  include <windows.h>
#  include <psapi.h>
#  pragma comment(lib, "psapi.lib")
#elif defined(__APPLE__)
#  include <mach/mach.h>
#else
#  include <unistd.h>
#  include <cstdio>
#endif

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

/* المعالِجُ قد يحمل وسيطاً مقيَّداً: `ألسنة` و`تقويم` تُسجّلان معالِجاً
   واحداً لعدّةِ عناصر، وكلُّ عنصرٍ يُمرّر رقمَه (فهرسَ اللسان أو اليوم).
   بلا هذا يلزم المستعملَ أن يكتب دالّةً لكلّ لسان. */
class HandlerEntry {
public:
	AlifObject* fn{};    /* مرجعٌ قويّ */
	AlifObject* arg{};   /* مرجعٌ قويّ، أو nullptr فلا وسيط */
};

static std::unordered_map<std::string, HandlerEntry> _graphicsHandlers_{};
static unsigned long long _graphicsHandlerSeq_ = 0;

/* حلقةُ SDL واحدةٌ على الخيط، وسجلُّ المعالِجات واحد. فتعشيشُ `تشغيل_تطبيق`
   داخلَ معالِجٍ يجعل الاستدعاءَ الداخليَّ يمسح معالِجاتِ الخارجيّ عند خروجه،
   فتموت أزرارُه صامتةً. نمنع التعشيشَ صراحةً. */
static bool _graphicsRunning_ = false;

/* يجمع مفاتيحَ المعالِجات المذكورةَ في الشجرة الحيّة */
static void collectLiveKeys(const NodePtr& _node, std::unordered_set<std::string>& _out) {
	if (_node == nullptr) return;
	for (const auto& ev : _node->getEvents()) {
		if (!ev.expression.empty()) _out.insert(ev.expression);
	}
	for (const auto& child : _node->getChildren()) collectLiveKeys(child, _out);
}

/* يُسقط مراجعَ المعالِجات التي لم تعُد الشجرةُ تذكرها */
static void pruneHandlers(const NodePtr& _live) {
	std::unordered_set<std::string> alive;
	collectLiveKeys(_live, alive);
	for (auto it = _graphicsHandlers_.begin(); it != _graphicsHandlers_.end();) {
		if (alive.find(it->first) == alive.end()) {
			ALIF_XDECREF(it->second.fn);
			ALIF_XDECREF(it->second.arg);
			it = _graphicsHandlers_.erase(it);
		} else ++it;
	}
}

/* ينادي المعالِجَ بالوسيطِ الملائم. أولويّةُ الوسيطِ المقيَّد على قيمةِ
   الحدث: الأوّلُ من صنعِنا ونعرف معناه، والثانيةُ من الودجة. */
static AlifObject* invokeHandler(const HandlerEntry& _e, const std::string& _changeValue) {
	AlifObject* handler = ALIF_NEWREF(_e.fn);
	AlifObject* result = nullptr;
	if (_e.arg != nullptr) {
		/* الوسيطُ المقيَّدُ من صنعِنا ونعرف معناه، فمعالِجٌ لا يقبله خطأٌ
		   يُبلَّغ لا يُبتلَع. ولا نُعيد النداءَ بلا وسيطٍ هنا: ذلك يترك
		   استثناءً معلّقاً ثمّ ينادي فوقَه. */
		result = alifObject_callOneArg(handler, _e.arg);
		ALIF_DECREF(handler);
		return result;
	}
	if (!_changeValue.empty()) {
		AlifObject* arg = alifUStr_fromString(_changeValue.c_str());
		if (arg != nullptr) {
			result = alifObject_callOneArg(handler, arg);
			ALIF_DECREF(arg);
			/* معالِجٌ لا يقبل وسيطاً: نُعيد المحاولةَ بلا وسيط */
			if (result == nullptr) alifErr_clear();
		}
	}
	if (result == nullptr) {
		AlifObject* noArgs = alifTuple_new(0);
		result = (noArgs == nullptr) ? nullptr
		       : alifObject_callObject(handler, noArgs);
		ALIF_XDECREF(noArgs);
	}
	ALIF_DECREF(handler);
	return result;
}

static void clearHandlers() {
	for (auto& kv : _graphicsHandlers_) {
		ALIF_XDECREF(kv.second.fn);
		ALIF_XDECREF(kv.second.arg);
	}
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
static AlifObject* element_source(ElementObject* s, AlifObject* v)   { return setTextProp(s, v, props::SOURCE); }
static AlifObject* element_hint(ElementObject* s, AlifObject* v)     { return setTextProp(s, v, props::HINT); }
static AlifObject* element_value(ElementObject* s, AlifObject* v)    { return setTextProp(s, v, props::VALUE); }
static AlifObject* element_borderColor(ElementObject* s, AlifObject* v) { return setTextProp(s, v, props::BORDER_COLOR); }

/* مسجّلُ معالِجٍ عامٌّ — يُستعمل لِـ`عند_النقر` و`عند_تغير` */
static std::string bindHandler(AlifObject* _callable, AlifObject* _boundArg) {
	char key[32]{};
	snprintf(key, sizeof(key), "h%llu", ++_graphicsHandlerSeq_);
	HandlerEntry e{};
	e.fn = ALIF_NEWREF(_callable);
	e.arg = (_boundArg == nullptr) ? nullptr : ALIF_NEWREF(_boundArg);
	_graphicsHandlers_.emplace(key, e);
	return std::string(key);
}

/* يُعلّق معالِجاً على عقدةٍ خامٍّ (لا على عنصرِ ألف) — تستعمله البنّاءاتُ
   المركَّبةُ التي تُنشئ عُقَدَها بنفسِها. */
static void attachHandler(const NodePtr& _node, AlifObject* _callable,
                          IREventType _type, AlifObject* _boundArg) {
	sad::ui::IREvent ev{};
	ev.type = _type;
	ev.expression = bindHandler(_callable, _boundArg);
	_node->addEvent(ev);
}

static AlifObject* registerHandler(ElementObject* _self, AlifObject* _callable,
                                   IREventType _type, const char* _where) {
	if (!isCallable(_callable)) {
		alifErr_format(_alifExcTypeError_, "%s: يُتوقّع شيءٌ قابلٌ للنداء", _where);
		return nullptr;
	}
	/* المفتاحُ عدّادٌ لا عنوانُ كائن: `كائن.طريقة` واللامدا يُنشئان كائناً
	   جديداً في كلّ نداءِ بناء، فالفهرسةُ بالعنوان تُنمي السجلَّ بلا حدّ.
	   ويُمسح ما لم يعُد في الشجرة الحيّة بعد كلّ إعادةِ بناء. */
	sad::ui::IREvent ev{};
	ev.type = _type;
	ev.expression = bindHandler(_callable, nullptr);
	_self->node->addEvent(ev);
	return ALIF_NEWREF((AlifObject*)_self);
}

/* .عند_تغير(دالّة) — للمفاتيح وخاناتِ الاختيار والمنزلقات وحقولِ النصّ */
static AlifObject* element_onTap(ElementObject* _self, AlifObject* _callable) {
	return registerHandler(_self, _callable, IREventType::OnTap, "عند_النقر");
}

/* .عند_تغير(دالّة) — للمفاتيح وخاناتِ الاختيار والمنزلقات وحقولِ النصّ */
/* .معرف("اسم") — يُثبّت هويّةَ العنصر بين الأجيال.
 * (سُمّي معرّفاً لا مفتاحاً لأنّ `مفتاح` عنصرُ التبديل.)
 *
 * بلا مفتاحٍ صريحٍ يُشتقّ الاسمُ من الموضع (`ج.0.2`)، فإدراجُ عنصرٍ في وسطِ
 * قائمةٍ يُزحزح أسماءَ ما بعده كلَّه: تُصلَّح الصورةُ لكن ينتقل التركيزُ
 * والتمريرُ والتحريكُ الجاري إلى الجار. المفتاحُ الصريحُ يمنع ذلك.
 */
/* .مفعل(منطقي) — المفاتيحُ وخاناتُ الاختيار تقرأ `مفعّل` لا `قيمة` */
static AlifObject* element_enabled(ElementObject* _self, AlifObject* _value) {
	AlifIntT truth = alifObject_isTrue(_value);
	if (truth < 0) return nullptr;
	_self->node->setProperty(props::ENABLED, truth ? true : false);
	return ALIF_NEWREF((AlifObject*)_self);
}

/* .قيمة_رقم(عدد) — المنزلقُ وشريطُ التقدّم يقرآن قيمةً رقميّة */
static AlifObject* element_numValue(ElementObject* s, AlifObject* v) { return setNumProp(s, v, props::VALUE); }

static AlifObject* element_key(ElementObject* _self, AlifObject* _value) {
	std::string k;
	if (!asUTF8(_value, &k)) return nullptr;
	_self->node->setId(k);
	return ALIF_NEWREF((AlifObject*)_self);
}

static AlifObject* element_onChange(ElementObject* _self, AlifObject* _callable) {
	return registerHandler(_self, _callable, IREventType::OnChange, "عند_تغير");
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
	{"عند_تغير",    ALIF_CPPFUNCTION_CAST(element_onChange), METHOD_O},
	{"معرف",        ALIF_CPPFUNCTION_CAST(element_key),      METHOD_O},
	{"مفعل",        ALIF_CPPFUNCTION_CAST(element_enabled),  METHOD_O},
	{"قيمة_رقم",    ALIF_CPPFUNCTION_CAST(element_numValue), METHOD_O},
	{"مصدر",        ALIF_CPPFUNCTION_CAST(element_source),   METHOD_O},
	{"تلميح",       ALIF_CPPFUNCTION_CAST(element_hint),     METHOD_O},
	{"قيمة",        ALIF_CPPFUNCTION_CAST(element_value),    METHOD_O},
	{"حد_لون",      ALIF_CPPFUNCTION_CAST(element_borderColor), METHOD_O},
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
static AlifObject* graphics_card(AlifObject* m, AlifObject* a)   { return buildContainer(m, UINodeType::Card,   a, "بطاقة"); }
static AlifObject* graphics_scroll(AlifObject* m, AlifObject* a) { return buildContainer(m, UINodeType::ScrollView, a, "لفافة"); }

/* بنّاءُ عنصرٍ ورقيٍّ بخاصّيّةٍ نصّيّةٍ واحدة */
static AlifObject* buildLeaf(AlifObject* _module, UINodeType _type,
                             const char* _key, AlifObject* _value) {
	std::string text;
	AlifObject* asStr = nullptr;
	if (!ALIFUSTR_CHECK(_value)) {
		asStr = alifObject_str(_value);
		if (asStr == nullptr) return nullptr;
		_value = asStr;
	}
	if (!asUTF8(_value, &text)) { ALIF_XDECREF(asStr); return nullptr; }
	ALIF_XDECREF(asStr);
	NodePtr node = IRNode::create(_type);
	node->setProperty(_key, text);
	return newElement(_module, std::move(node));
}

static AlifObject* graphics_image(AlifObject* m, AlifObject* v)  { return buildLeaf(m, UINodeType::Image,     props::SOURCE, v); }
static AlifObject* graphics_field(AlifObject* m, AlifObject* v)  { return buildLeaf(m, UINodeType::TextField, props::HINT,   v); }
static AlifObject* graphics_toggle(AlifObject* m, AlifObject* v) { return buildLeaf(m, UINodeType::Toggle,    props::TEXT,   v); }
static AlifObject* graphics_check(AlifObject* m, AlifObject* v)  { return buildLeaf(m, UINodeType::Checkbox,  props::TEXT,   v); }

/* فاصلٌ مرن: `فاصل()` بلا وسائط، أو `فاصل(20)` بمقدارٍ ثابت */
static AlifObject* graphics_spacer(AlifObject* _module, AlifObject* _args) {
	double size = 0.0;
	AlifObject* sizeObj = nullptr;
	if (!alifArg_parseTuple(_args, "|O", &sizeObj)) return nullptr;
	NodePtr node = IRNode::create(UINodeType::Spacer);
	if (sizeObj != nullptr and sizeObj != ALIF_NONE) {
		size = alifFloat_asDouble(sizeObj);
		if (size == -1.0 and alifErr_occurred()) return nullptr;
		node->setProperty(props::HEIGHT, size);
		node->setProperty(props::WIDTH, size);
	}
	return newElement(_module, std::move(node));
}

static AlifObject* graphics_divider(AlifObject* _module, AlifObject* /*_ignored*/) {
	NodePtr node = IRNode::create(UINodeType::Divider);
	return newElement(_module, std::move(node));
}


/* ═══ بقيّةُ السطح ═══
 * ما دون هذا مُصيَّرٌ فعلاً في `platform_renderer.cpp`، لا مجرَّدَ مدخلةٍ
 * في التعداد. وما كان يرسم شكلاً بلا بيانات صار يُبنى تركيباً من
 * عُقَدٍ تعمل (انظر «العناصرُ المركَّبة» أدناه)، وما لا يُركَّب من
 * موجود (مشغل_فيديو، عرض_ويب…) يُوصَل حين يُنفَّذ لا قبله.
 */

/* عناصرُ حاويةٌ: تأخذ أبناءً */
static AlifObject* graphics_center(AlifObject* m, AlifObject* a)    { return buildContainer(m, UINodeType::Center,    a, "وسط"); }
static AlifObject* graphics_padding(AlifObject* m, AlifObject* a)   { return buildContainer(m, UINodeType::Padding,   a, "بحشوة"); }
static AlifObject* graphics_sized(AlifObject* m, AlifObject* a)     { return buildContainer(m, UINodeType::SizedBox,  a, "بمقاس"); }
static AlifObject* graphics_expanded(AlifObject* m, AlifObject* a)  { return buildContainer(m, UINodeType::Expanded,  a, "موسع"); }
static AlifObject* graphics_align(AlifObject* m, AlifObject* a)     { return buildContainer(m, UINodeType::Align,     a, "محاذاة"); }
static AlifObject* graphics_container(AlifObject* m, AlifObject* a) { return buildContainer(m, UINodeType::Container, a, "حاوية"); }
static AlifObject* graphics_grid(AlifObject* m, AlifObject* a)      { return buildContainer(m, UINodeType::Grid,      a, "شبكة"); }
static AlifObject* graphics_wrap(AlifObject* m, AlifObject* a)      { return buildContainer(m, UINodeType::Wrap,      a, "التفاف"); }
static AlifObject* graphics_group(AlifObject* m, AlifObject* a)     { return buildContainer(m, UINodeType::GroupBox,  a, "صندوق_تجميع"); }
static AlifObject* graphics_appBar(AlifObject* m, AlifObject* a)    { return buildContainer(m, UINodeType::AppBar,    a, "شريط_تطبيق"); }
static AlifObject* graphics_statusBar(AlifObject* m, AlifObject* a) { return buildContainer(m, UINodeType::StatusBar, a, "شريط_حالة"); }

/* عناصرُ ورقيّةٌ نصّيّة */
static AlifObject* graphics_icon(AlifObject* m, AlifObject* v)      { return buildLeaf(m, UINodeType::Icon,      props::TEXT, v); }
static AlifObject* graphics_badge(AlifObject* m, AlifObject* v)     { return buildLeaf(m, UINodeType::Badge,     props::TEXT, v); }
static AlifObject* graphics_chip(AlifObject* m, AlifObject* v)      { return buildLeaf(m, UINodeType::Chip,      props::TEXT, v); }
static AlifObject* graphics_avatar(AlifObject* m, AlifObject* v)    { return buildLeaf(m, UINodeType::Avatar,    props::TEXT, v); }
static AlifObject* graphics_tooltip(AlifObject* m, AlifObject* v)   { return buildLeaf(m, UINodeType::Tooltip,   props::TEXT, v); }
static AlifObject* graphics_textArea(AlifObject* m, AlifObject* v)  { return buildLeaf(m, UINodeType::TextArea,  props::HINT, v); }
static AlifObject* graphics_searchBar(AlifObject* m, AlifObject* v) { return buildLeaf(m, UINodeType::SearchBar, props::HINT, v); }
static AlifObject* graphics_fab(AlifObject* m, AlifObject* v)       { return buildLeaf(m, UINodeType::FAB,       props::TEXT, v); }
static AlifObject* graphics_radio(AlifObject* m, AlifObject* v)     { return buildLeaf(m, UINodeType::Radio,     props::TEXT, v); }
static AlifObject* graphics_code(AlifObject* m, AlifObject* v)      { return buildLeaf(m, UINodeType::CodeBlock, props::TEXT, v); }
static AlifObject* graphics_spinner(AlifObject* m, AlifObject* /*x*/) {
	return newElement(m, IRNode::create(UINodeType::Spinner));
}

/* عنصرانِ رقميّان: القيمةُ عددٌ لا نصّ، ومداها **٠–١٠٠** لا ٠–١
   (`platform_renderer.cpp:1053` يقسم على ١٠٠). */
static AlifObject* buildNumeric(AlifObject* _module, UINodeType _type, AlifObject* _value) {
	double v = alifFloat_asDouble(_value);
	if (v == -1.0 and alifErr_occurred()) return nullptr;
	NodePtr node = IRNode::create(_type);
	node->setProperty(props::VALUE, v);
	return newElement(_module, std::move(node));
}
static AlifObject* graphics_progress(AlifObject* m, AlifObject* v) { return buildNumeric(m, UINodeType::ProgressBar, v); }
static AlifObject* graphics_slider(AlifObject* m, AlifObject* v)   { return buildNumeric(m, UINodeType::Slider,      v); }

/* ═══ العناصرُ المركَّبة ═══
 *
 * ثلاثةٌ كانت مستبعَدةً عمداً لأنّ `platform_renderer.cpp` يرسم لها شكلاً
 * بلا بيانات: `DataTable` يرسم صندوقاً وخطوطاً **ويتجاهل أبناءَه**
 * (`platform_renderer.cpp:1516`)، و`Calendar` صندوقٌ ورمزُ نتيجةٍ واحد
 * (‏:2350)، و`Tabs` شريطٌ رماديٌّ لا يختار (‏:1570).
 *
 * فوصلناها **تركيباً لا ترقيعاً**: كلٌّ منها تُبنى من عُقَدٍ مقيسةٍ تعمل
 * أصلاً (‏Column · Row · Container · Text · Button). والثمنُ مقصود:
 * `source/Modules/Graphics/core` يبقى نسخةً حرفيّةً من لغة ص — لا تفرّعَ
 * فيه إلّا اختصارُ الهويّةِ في `reconciler.cpp`. ومتى نفّذت ص العُقَدَ
 * الأصليّةَ انتقلنا إليها بلا كسرِ سطح: الأسماءُ والوسائطُ هي هي.
 */

/* لوحُ الألوان الافتراضيّ — يوافق أمثلةَ المستودع الداكنة. */
namespace palette {
	constexpr const char* HEAD_BG    = "#2b3346";
	constexpr const char* HEAD_TEXT  = "#ffd479";
	constexpr const char* ROW_A      = "#1b2437";
	constexpr const char* ROW_B      = "#141c2c";
	constexpr const char* CELL_TEXT  = "#e6e6e6";
	constexpr const char* MUTED_TEXT = "#8fa3bf";
	constexpr const char* ACCENT     = "#1e88e5";
	constexpr const char* MUTED      = "#3a4256";
}

/* أيُّ كائنِ ألفٍ إلى نصٍّ للعرض */
static bool displayString(AlifObject* _obj, std::string* _out) {
	AlifObject* asStr = nullptr;
	AlifObject* v = _obj;
	if (!ALIFUSTR_CHECK(v)) {
		asStr = alifObject_str(v);
		if (asStr == nullptr) return false;
		v = asStr;
	}
	bool ok = asUTF8(v, _out);
	ALIF_XDECREF(asStr);
	return ok;
}

static NodePtr makeText(const std::string& _text, const char* _color, double _fontSize) {
	NodePtr n = IRNode::create(UINodeType::Text);
	n->setProperty(props::TEXT, _text);
	n->setProperty(props::COLOR, std::string(_color));
	n->setProperty(props::FONT_SIZE, _fontSize);
	return n;
}

/* خليّةٌ: حاويةٌ بعرضٍ ثابتٍ وخلفيّةٍ، فيها نصّ. العرضُ الثابتُ هو ما يجعل
   الأعمدةَ تصطفّ — لا قياسَ عمودٍ تلقائيّاً في محرّك التخطيط. */
static NodePtr makeCell(const std::string& _text, double _width,
                        const char* _bg, const char* _color, double _fontSize) {
	NodePtr cell = IRNode::create(UINodeType::Container);
	cell->setProperty(props::WIDTH, _width);
	cell->setProperty(props::PADDING, 8.0);
	cell->setProperty(props::BG_COLOR, std::string(_bg));
	cell->addChild(makeText(_text, _color, _fontSize));
	return cell;
}

/* جدول_بيانات(رؤوس، صفوف، عرض_عمود=140، حجم_خط=18)
 *
 *     جدول_بيانات(["الاسم", "العمر"], [["زيد", 30], ["عمرو", 25]])
 *
 * عددُ الأعمدةِ من الرؤوس؛ الصفُّ الأقصرُ يُكمَّل فراغاً والأطولُ يُقصّ،
 * فلا يلتوي الاصطفافُ ببياناتٍ ناقصة. */
static AlifObject* graphics_dataTable(AlifObject* _module, AlifObject* _args,
                                      AlifObject* _kwargs) {
	static const char* const kwlist[] = {"", "", "عرض_عمود", "حجم_خط", nullptr};
	AlifObject* headersObj = nullptr;
	AlifObject* rowsObj = nullptr;
	double colWidth = 140.0;
	double fontSize = 18.0;
	if (!alifArg_parseTupleAndKeywords(_args, _kwargs, "OO|dd", kwlist,
	                                   &headersObj, &rowsObj, &colWidth, &fontSize))
		return nullptr;

	AlifObject* headers = alifSequence_fast(headersObj, "جدول_بيانات: الرؤوسُ ليست قائمة");
	if (headers == nullptr) return nullptr;
	AlifObject* rows = alifSequence_fast(rowsObj, "جدول_بيانات: الصفوفُ ليست قائمة");
	if (rows == nullptr) { ALIF_DECREF(headers); return nullptr; }

	AlifSizeT ncols = ALIFSEQUENCE_FAST_GET_SIZE(headers);
	AlifSizeT nrows = ALIFSEQUENCE_FAST_GET_SIZE(rows);
	if (ncols < 1) {
		alifErr_setString(_alifExcValueError_, "جدول_بيانات: لا رؤوسَ للجدول");
		ALIF_DECREF(headers); ALIF_DECREF(rows);
		return nullptr;
	}

	NodePtr root = IRNode::create(UINodeType::Column);
	root->setProperty(props::SPACING, 1.0);
	root->setProperty(props::BG_COLOR, std::string(palette::MUTED));

	/* صفُّ الرؤوس */
	{
		NodePtr head = IRNode::create(UINodeType::Row);
		head->setProperty(props::SPACING, 1.0);
		AlifObject** items = ALIFSEQUENCE_FAST_ITEMS(headers);
		for (AlifSizeT c = 0; c < ncols; c++) {
			std::string text;
			if (!displayString(items[c], &text)) {
				ALIF_DECREF(headers); ALIF_DECREF(rows); return nullptr;
			}
			head->addChild(makeCell(text, colWidth,
			                        palette::HEAD_BG, palette::HEAD_TEXT, fontSize));
		}
		root->addChild(head);
	}

	/* صفوفُ البيانات — تلوينٌ متناوبٌ ليُتتبَّع السطرُ بالعين */
	AlifObject** rowItems = ALIFSEQUENCE_FAST_ITEMS(rows);
	for (AlifSizeT r = 0; r < nrows; r++) {
		AlifObject* cells = alifSequence_fast(rowItems[r], "جدول_بيانات: الصفُّ ليس قائمة");
		if (cells == nullptr) { ALIF_DECREF(headers); ALIF_DECREF(rows); return nullptr; }
		AlifSizeT have = ALIFSEQUENCE_FAST_GET_SIZE(cells);
		AlifObject** cellItems = ALIFSEQUENCE_FAST_ITEMS(cells);

		NodePtr row = IRNode::create(UINodeType::Row);
		row->setProperty(props::SPACING, 1.0);
		const char* bg = (r % 2 == 0) ? palette::ROW_A : palette::ROW_B;
		bool failed = false;
		for (AlifSizeT c = 0; c < ncols; c++) {
			std::string text;
			if (c < have and !displayString(cellItems[c], &text)) { failed = true; break; }
			row->addChild(makeCell(text, colWidth, bg, palette::CELL_TEXT, fontSize));
		}
		ALIF_DECREF(cells);
		if (failed) { ALIF_DECREF(headers); ALIF_DECREF(rows); return nullptr; }
		root->addChild(row);
	}

	ALIF_DECREF(headers);
	ALIF_DECREF(rows);
	return newElement(_module, std::move(root));
}

/* ═══ التقويم ═══ */

static bool isLeapYear(int _y) {
	return (_y % 4 == 0 and _y % 100 != 0) or (_y % 400 == 0);
}

static int daysInMonth(int _y, int _m) {
	static const int len[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	if (_m == 2 and isLeapYear(_y)) return 29;
	return len[_m - 1];
}

/* يومُ الأسبوع بجدولِ سَكِنْدرسون: ٠ = الأحد */
static int weekdayOf(int _y, int _m, int _d) {
	static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
	int y = _y;
	if (_m < 3) y -= 1;
	int w = (y + y / 4 - y / 100 + y / 400 + t[_m - 1] + _d) % 7;
	return (w + 7) % 7;
}

/* تقويم(سنة، شهر، يوم=0، عند_اختيار=None، عرض_خلية=44)
 *
 * شبكةُ شهرٍ حقيقيّة: اسمُ الشهرِ والسنة، ثمّ رؤوسُ الأيّام، ثمّ الأسابيع.
 * اليومُ المحدَّدُ يُبرَز، وإن مُرِّر معالِجٌ نُودِيَ برقمِ اليومِ المنقور. */
static AlifObject* graphics_calendar(AlifObject* _module, AlifObject* _args,
                                     AlifObject* _kwargs) {
	static const char* const kwlist[] = {"", "", "يوم", "عند_اختيار", "عرض_خلية", nullptr};
	AlifIntT year = 0, month = 0, day = 0;
	AlifObject* onPick = nullptr;
	double cellW = 44.0;
	if (!alifArg_parseTupleAndKeywords(_args, _kwargs, "ii|iOd", kwlist,
	                                   &year, &month, &day, &onPick, &cellW))
		return nullptr;

	if (month < 1 or month > 12) {
		alifErr_setString(_alifExcValueError_, "تقويم: الشهرُ خارجَ المدى ١–١٢");
		return nullptr;
	}
	if (year < 1 or year > 9999) {
		alifErr_setString(_alifExcValueError_, "تقويم: السنةُ خارجَ المدى ١–٩٩٩٩");
		return nullptr;
	}
	if (onPick == ALIF_NONE) onPick = nullptr;
	if (onPick != nullptr and !isCallable(onPick)) {
		alifErr_setString(_alifExcTypeError_, "تقويم: عند_اختيار ليس قابلاً للنداء");
		return nullptr;
	}

	static const char* const monthNames[] = {
		"يناير", "فبراير", "مارس", "أبريل", "مايو", "يونيو",
		"يوليو", "أغسطس", "سبتمبر", "أكتوبر", "نوفمبر", "ديسمبر"
	};
	static const char* const dayNames[] = {"ح", "ن", "ث", "ر", "خ", "ج", "س"};

	NodePtr root = IRNode::create(UINodeType::Column);
	root->setProperty(props::SPACING, 2.0);
	root->setProperty(props::BG_COLOR, std::string(palette::ROW_B));
	root->setProperty(props::PADDING, 10.0);

	/* العنوانُ عقدتان لا واحدة: نصٌّ عربيٌّ فيه عددٌ لاتينيٌّ يخرج معكوسَ
	   الأرقام (`سبتمبر 2026` تُرسَم `سبتمبر 6202`) — مقيسٌ في لقطة
	   `مشهد_مركب`. العددُ وحدَه يخرج سليماً، والصفُّ يرتّبهما من اليمين. */
	{
		NodePtr title = IRNode::create(UINodeType::Row);
		title->setProperty(props::SPACING, 8.0);
		title->addChild(makeText(monthNames[month - 1], palette::HEAD_TEXT, 22.0));
		title->addChild(makeText(std::to_string(year), palette::HEAD_TEXT, 22.0));
		root->addChild(title);
	}

	{
		NodePtr head = IRNode::create(UINodeType::Row);
		head->setProperty(props::SPACING, 2.0);
		for (int i = 0; i < 7; i++) {
			head->addChild(makeCell(dayNames[i], cellW,
			                        palette::HEAD_BG, palette::MUTED_TEXT, 15.0));
		}
		root->addChild(head);
	}

	int lead = weekdayOf(year, month, 1);
	int total = daysInMonth(year, month);
	int weeks = (lead + total + 6) / 7;
	int d = 1;
	for (int w = 0; w < weeks; w++) {
		NodePtr week = IRNode::create(UINodeType::Row);
		week->setProperty(props::SPACING, 2.0);
		for (int c = 0; c < 7; c++) {
			bool blank = (w == 0 and c < lead) or d > total;
			if (blank) {
				week->addChild(makeCell("", cellW, palette::ROW_B,
				                        palette::CELL_TEXT, 16.0));
				continue;
			}
			bool picked = (d == day);
			NodePtr cell = makeCell(std::to_string(d), cellW,
			                        picked ? palette::ACCENT : palette::ROW_A,
			                        picked ? "#ffffff" : palette::CELL_TEXT, 16.0);
			/* الاسمُ صريحٌ: بلا هذا يشتقُّ `assignIds` اسماً من الموضع، فيتبدّل
			   اسمُ كلّ خليّةٍ حين يتبدّل عددُ أسابيعِ الشهر عند تغيّره. */
			cell->setId("يوم-" + std::to_string(month) + "-" + std::to_string(d));
			if (onPick != nullptr) {
				AlifObject* dayNum = alifLong_fromLong(d);
				if (dayNum == nullptr) return nullptr;
				attachHandler(cell, onPick, IREventType::OnTap, dayNum);
				ALIF_DECREF(dayNum);   /* attachHandler أخذ مرجعَه الخاصّ */
			}
			week->addChild(cell);
			d++;
		}
		root->addChild(week);
	}
	return newElement(_module, std::move(root));
}

/* ألسنة(عناوين، مختار=0، عند_اختيار=None، عرض_لسان=0)
 *
 *     ألسنة(["الملف", "تحرير", "عرض"], ح.اللسان, بدل_اللسان)
 *
 * يُنادى `بدل_اللسان(فهرس)` برقمِ اللسانِ المنقور. الشريطُ وحدَه — المحتوى
 * على المستعمِل، فاللسانُ لا يعرف ما تحته. */
static AlifObject* graphics_tabs(AlifObject* _module, AlifObject* _args,
                                 AlifObject* _kwargs) {
	static const char* const kwlist[] = {"", "مختار", "عند_اختيار", "عرض_لسان", nullptr};
	AlifObject* labelsObj = nullptr;
	AlifIntT selected = 0;
	AlifObject* onPick = nullptr;
	double tabWidth = 0.0;
	if (!alifArg_parseTupleAndKeywords(_args, _kwargs, "O|iOd", kwlist,
	                                   &labelsObj, &selected, &onPick, &tabWidth))
		return nullptr;

	if (onPick == ALIF_NONE) onPick = nullptr;
	if (onPick != nullptr and !isCallable(onPick)) {
		alifErr_setString(_alifExcTypeError_, "ألسنة: عند_اختيار ليس قابلاً للنداء");
		return nullptr;
	}

	AlifObject* labels = alifSequence_fast(labelsObj, "ألسنة: العناوينُ ليست قائمة");
	if (labels == nullptr) return nullptr;
	AlifSizeT n = ALIFSEQUENCE_FAST_GET_SIZE(labels);
	AlifObject** items = ALIFSEQUENCE_FAST_ITEMS(labels);

	NodePtr bar = IRNode::create(UINodeType::Row);
	bar->setProperty(props::SPACING, 2.0);
	bar->setProperty(props::BG_COLOR, std::string(palette::ROW_B));

	for (AlifSizeT i = 0; i < n; i++) {
		std::string label;
		if (!displayString(items[i], &label)) { ALIF_DECREF(labels); return nullptr; }
		bool active = ((AlifSizeT)selected == i);

		NodePtr btn = IRNode::create(UINodeType::Button);
		btn->setProperty(props::TEXT, label);
		btn->setProperty(props::BG_COLOR,
		                 std::string(active ? palette::ACCENT : palette::MUTED));
		btn->setProperty(props::COLOR,
		                 std::string(active ? "#ffffff" : palette::MUTED_TEXT));
		btn->setProperty(props::FONT_SIZE, 18.0);
		if (tabWidth > 0.0) btn->setProperty(props::WIDTH, tabWidth);
		btn->setId("لسان-" + std::to_string((long long)i));
		if (onPick != nullptr) {
			AlifObject* idx = alifLong_fromLong((long)i);
			if (idx == nullptr) { ALIF_DECREF(labels); return nullptr; }
			attachHandler(btn, onPick, IREventType::OnTap, idx);
			ALIF_DECREF(idx);
		}

		/* خطُّ الإبراز تحت اللسان — هو ما يميّز المختارَ حين يمرّ الفأرُ فوق
		   لسانٍ آخرَ فتتبدّل خلفيّتُه مؤقّتاً. */
		NodePtr underline = IRNode::create(UINodeType::Container);
		underline->setProperty(props::HEIGHT, 3.0);
		if (tabWidth > 0.0) underline->setProperty(props::WIDTH, tabWidth);
		underline->setProperty(props::BG_COLOR,
		                       std::string(active ? palette::HEAD_TEXT : palette::ROW_B));

		NodePtr tab = IRNode::create(UINodeType::Column);
		tab->setProperty(props::SPACING, 0.0);
		tab->addChild(btn);
		tab->addChild(underline);
		bar->addChild(tab);
	}
	ALIF_DECREF(labels);
	return newElement(_module, std::move(bar));
}

/* ═══ تذكيرُ النتائج (memo) ═══
 *
 * `مذكرة(دالّة، مدخل…)` تُعيد العنصرَ الذي أنتجته الدالّةُ آخرَ مرّةٍ إن لم
 * تتغيّر مدخلاتُها. فتُشارك الشجرتان القديمةُ والجديدةُ المؤشّرَ نفسَه،
 * ويتخطّى `Reconciler::diff` الفرعَ كلَّه باختصارِ الهويّة الذي أُضيف في
 * `reconciler.cpp` — بدلَ أن يمشي فيه عقدةً عقدة.
 *
 * مقيسٌ على شجرةِ ٣٤١ عقدةً بثلاثةِ أفرعٍ مذكَّرةٍ من أربعة:
 *     مطابقةٌ كاملة  ٠٫٤٥٢ م.ث   (‏٣٤١ عقدةً قُورنت)
 *     مع التذكير     ٠٫١١٤ م.ث   (‏٨٦ عقدة)      ⇒ ٤×
 * ويُضاف إليه تخطّي بناءِ الفرعِ في ألفَ أصلاً.
 *
 * الفخّ: مدخلٌ منسيّ (متغيّرٌ تقرؤه الدالّةُ ولا يُمرَّر) يُجمّد الفرعَ
 * صامتاً. مرّرْ كلَّ ما تقرأ.
 */

class MemoEntry {
public:
	AlifObject* args{};     /* صفُّ المدخلات — مرجعٌ قويّ */
	AlifObject* result{};   /* العنصرُ الناتج — مرجعٌ قويّ */
};

static std::unordered_map<unsigned long long, MemoEntry> _graphicsMemo_{};

static void clearMemo() {
	for (auto& kv : _graphicsMemo_) {
		ALIF_XDECREF(kv.second.args);
		ALIF_XDECREF(kv.second.result);
	}
	_graphicsMemo_.clear();
}

static AlifObject* graphics_memo(AlifObject* _module, AlifObject* _args) {
	AlifSizeT n = alifTuple_size(_args);
	if (n < 1) {
		alifErr_setString(_alifExcTypeError_, "مذكرة: يلزم وسيطٌ أوّلُ قابلٌ للنداء");
		return nullptr;
	}
	AlifObject* fn = alifTuple_getItem(_args, 0);
	if (!isCallable(fn)) {
		alifErr_setString(_alifExcTypeError_, "مذكرة: الوسيطُ الأوّلُ ليس قابلاً للنداء");
		return nullptr;
	}

	AlifObject* inputs = alifTuple_getSlice(_args, 1, n);
	if (inputs == nullptr) return nullptr;

	unsigned long long slot = (unsigned long long)(uintptr_t)fn;
	auto it = _graphicsMemo_.find(slot);
	if (it != _graphicsMemo_.end() and it->second.args != nullptr) {
		AlifIntT same = alifObject_richCompareBool(it->second.args, inputs, ALIF_EQ);
		if (same < 0) { ALIF_DECREF(inputs); return nullptr; }
		if (same > 0) {
			ALIF_DECREF(inputs);
			return ALIF_NEWREF(it->second.result);   /* المؤشّرُ نفسُه ⇒ يتخطّاه diff */
		}
	}

	AlifObject* noArgs = alifTuple_new(0);
	if (noArgs == nullptr) { ALIF_DECREF(inputs); return nullptr; }
	AlifObject* produced = alifObject_callObject(fn, noArgs);
	ALIF_DECREF(noArgs);
	if (produced == nullptr) { ALIF_DECREF(inputs); return nullptr; }

	GraphicsState* state = getGraphicsState(_module);
	if (!ALIF_IS_TYPE(produced, (AlifTypeObject*)state->elementType)) {
		alifErr_setString(_alifExcTypeError_, "مذكرة: الدالّةُ لم تُعِد عنصرَ رسومات");
		ALIF_DECREF(produced); ALIF_DECREF(inputs);
		return nullptr;
	}

	MemoEntry& e = _graphicsMemo_[slot];
	ALIF_XDECREF(e.args);
	ALIF_XDECREF(e.result);
	e.args = inputs;                       /* نأخذ ملكيّةَ inputs */
	e.result = ALIF_NEWREF(produced);
	return produced;
}

/* ═══ التشغيل ═══ */

/* أسماءٌ ثابتةٌ بحسبِ الموضع.
 *
 * بلا هذا يُسنِد المُصيِّرُ إلى كلّ عقدةٍ بلا اسمٍ رقماً متزايداً
 * (`platform_renderer.cpp:2746` ⇒ `widget_1000000`…). فالشجرةُ المعروضةُ
 * تحمل أسماءً، والشجرةُ المبنيّةُ حديثاً لا تحمل شيئاً، فيراها `diff`
 * أبناءً مختلفين كلَّهم: حذفٌ للثلاثة وإدراجٌ لثلاثةٍ بدلَها. قِسناه:
 * ستُّ رقعٍ (‏REMOVE×٣ ثمّ INSERT×٣) لتغييرِ رقمٍ واحد.
 * الاسمُ المشتقُّ من الموضعِ ثابتٌ بين الأجيال، فيصير الفرقُ رقعةً واحدة.
 */
static void assignIds(const NodePtr& _node, const std::string& _path) {
	if (_node == nullptr) return;
	if (_node->getId().empty()) _node->setId(_path);   /* المفتاحُ الصريحُ يُصان */
	const auto& children = _node->getChildren();
	for (size_t i = 0; i < children.size(); i++) {
		assignIds(children[i], _path + "." + std::to_string(i));
	}
}

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
		return tree;
	}
	assignIds(tree, "ج");
	return tree;
}

/* هل في مجموعةِ الرقع تغييرٌ بنيويّ؟
 *
 * هذا ليس سؤالَ أداءٍ بل سؤالَ سلامة: معالِجُ الفأرة يحتفظ بمؤشّراتٍ خامٍّ
 * إلى العُقَد (‏hoveredNode_ و pressedNode_ و focusedNode_)، و`setContent`
 * وحدَه يُصفّرها (`window.cpp:358 clearNodeRefs`) — أمّا `applyPatches`
 * فلا. ونحن نُرقّع من داخلِ إرسالِ الحدث والمعالِجُ ما يزال ممسِكاً
 * بالعقدةِ المضغوطة. فأيُّ رقعةٍ تُزيح عقدةً تُخلّف مؤشّراً متدلّياً.
 * لذلك: تغييرُ الخصائص وحدَه ⇒ ترقيعٌ موضعيّ، وما عداه ⇒ محتوًى جديد.
 */
static bool isStructural(const sad::ui::DiffResult& _d) {
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

	if (_graphicsRunning_) {
		alifErr_setString(_alifExcRuntimeError_,
			"تشغيل_تطبيق: لا يجوز تشغيلُ تطبيقٍ داخلَ معالِجِ تطبيقٍ آخر");
		return nullptr;
	}

	AlifObject* liveRef = nullptr;
	NodePtr liveTree = buildTree(_module, source, &liveRef, "تشغيل_تطبيق");
	if (liveTree == nullptr) return nullptr;
	_graphicsRunning_ = true;

	sad::ui::Reconciler reconciler{};
	bool ok = false;
	bool stopping = false;   /* رُفع عند أوّلِ خطأٍ فلا يُنادى معالِجٌ بعده */
	{
		sad::ui::desktop::DesktopWindow window;
		sad::ui::desktop::WindowOptions options;
		options.title = title;
		options.width = width;
		options.height = height;

		/* حلقةُ SDL حاجزةٌ — نُطلِق القفلَ العامّ طوالها.
		   واللامدا تُعرَّف داخلَ الكتلةِ عمداً: ماكرَوا BLOCK/UNBLOCK
		   يتوسّعان إلى `_save` المصرَّحِ في BEGIN_ALLOW_THREADS وحدَها. */
		/* حلقةُ SDL حاجزةٌ — نُطلِق القفلَ العامّ طوالها.
		   واللامدا تُعرَّف داخلَ الكتلةِ عمداً: ماكرَوا BLOCK/UNBLOCK
		   يتوسّعان إلى `_save` المصرَّحِ في BEGIN_ALLOW_THREADS وحدَها. */
		ALIF_BEGIN_ALLOW_THREADS
		if (window.create(options)) {
			window.setContent(liveTree);

			/* المعالِجُ لا يُرقّع الشجرةَ بنفسِه.
			 *
			 * ردُّ النداء يجري من داخلِ إرسالِ الحدث، و`dispatchEvent`
			 * (`event_dispatch.cpp:83`) يلتقط مؤشّراتٍ خامّاً إلى العقدةِ
			 * الهدفِ وأسلافِها **قبل** النداء، ثمّ يمرّ عليها في أطوارِ
			 * الالتقاطِ والهدفِ والفقاعات **بعده**. فلو حرّرنا الشجرةَ
			 * القديمةَ هناك لقرأ المُرسِلُ ذاكرةً محرَّرة.
			 * لذلك: المعالِجُ يُنادي دالّةَ ألف ويرفع علَماً، وإعادةُ البناء
			 * والمطابقةُ والترقيعُ تجري في رأسِ الحلقة بعد أن يفرغ الإرسال.
			 */
			bool pendingRebuild = false;

			window.setOnEventCallback(
				[&](IREventType _type, const std::string& _expr,
				    const IRNode*, const sad::ui::EventData& _data) {
					if (stopping) return;
					if (_type != IREventType::OnTap
					    and _type != IREventType::OnChange) return;
					auto it = _graphicsHandlers_.find(_expr);
					if (it == _graphicsHandlers_.end()) return;

					ALIF_BLOCK_THREADS

					AlifObject* result = invokeHandler(
						it->second,
						_type == IREventType::OnChange ? _data.value : std::string());
					if (result == nullptr) { stopping = true; window.close(); }
					else { ALIF_DECREF(result); pendingRebuild = true; }

					ALIF_UNBLOCK_THREADS
				});

			/* حلقتُنا بدل `window.run()`: تُتيح الترقيعَ خارجَ الإرسال */
			while (window.isOpen() and !stopping) {
				window.runOneFrame();
				if (!pendingRebuild) {
					std::this_thread::sleep_for(std::chrono::milliseconds(4));
					continue;
				}
				pendingRebuild = false;
				if (!isCallable(source)) continue;

				ALIF_BLOCK_THREADS
				AlifObject* newRef = nullptr;
				NodePtr newTree = buildTree(_module, source, &newRef, "تشغيل_تطبيق");
				if (newTree == nullptr) { stopping = true; window.close(); }
				else if (newTree == liveTree) {
					/* الدالّةُ أعادت الجذرَ نفسَه: لا فرقَ أبداً فتتجمّد
					   الواجهةُ صامتةً. نُبلِّغ بدل الصمت. */
					alifErr_setString(_alifExcRuntimeError_,
						"تشغيل_تطبيق: دالّةُ البناءِ أعادت الجذرَ ذاتَه — "
						"ابنِ جذراً جديداً في كلّ نداء (والتذكيرُ للأفرع لا للجذر)");
					stopping = true; window.close();
				}
				else {
					auto d = reconciler.diff(liveTree, newTree);
					if (!d.isEmpty()) {
						bool structural = isStructural(d);
						if (reconciler.patch(liveTree, d)) {
							/* `patch` يُعيد ربطَ الجذر عند رقعةِ استبدالٍ جذريّة
							   (`reconciler.cpp:537`)، والنافذةُ تبقى ممسكةً بالقديم.
							   و`setContent` يُصلح هذا ويُصفّر مراجعَ الفأرة معاً. */
							if (structural) window.setContent(liveTree);
							else window.applyPatches(d.size(), false);
						}
					}
					pruneHandlers(liveTree);
					ALIF_XDECREF(newRef);
				}
				ALIF_UNBLOCK_THREADS
			}
			window.destroy();
			ok = true;
		}
		ALIF_END_ALLOW_THREADS
	}

	_graphicsRunning_ = false;
	ALIF_XDECREF(liveRef);
	clearHandlers();

	if (stopping) return nullptr;   /* الاستثناءُ مضبوطٌ من نداءِ ألف */
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
	if (!asUTF8(pathObj, &path)) { ALIF_XDECREF(keep); return nullptr; }

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
			/* `runOneFrame` لا يُصيّر إلّا حين يُطلَب الرسمُ صراحةً، فبلا
			   `invalidate()` تُصيَّر الإطارةُ الأولى وحدَها وتمضي الباقيةُ
			   بلا عمل. و`takeScreenshot` يقرأ من هدفِ التصيير بعد
			   `SDL_RenderPresent`، ومحتواه غيرُ معرَّفٍ على بعض المنصّات
			   (‏Metal على ماك). فنُجبِر تصييراً في كلّ إطار. */
			/* اثنتا عشرةَ إطارةً بفاصلٍ قصير: عدّاءُ ماك في تهيئةِ التصحيح
			   يُنهي بعضَ المشاهدِ قبل أن يستقرّ التصيير (٢ من ٤ في الدورة
			   ‏34024665887)، والإصدارُ يمرّ بأربعةٍ من أربعة. */
			for (int i = 0; i < 12; i++) {
				window.invalidate();
				window.runOneFrame();
				std::this_thread::sleep_for(std::chrono::milliseconds(16));
			}
			ok = window.takeScreenshot(path);
			window.destroy();
		}
		ALIF_END_ALLOW_THREADS
	}
	ALIF_XDECREF(keep);
	return alifBool_fromLong(ok ? 1 : 0);
}

/* ═══ القياس ═══
 *
 * ما كان مقيساً حتّى الآن أطوارٌ منفردة: بناءٌ، مطابقةٌ، تخطيطٌ، تصيير.
 * وهي لا تُجيب عن السؤال الذي يهمّ المستعمِل: **كم يمضي بين نقرةٍ وصورة؟**
 * ولا عن السؤال الذي يمسك التسريب: **ماذا يبقى بعد عشرة آلافِ نقرة؟**
 *
 * `قياس_تفاعل` يُغلق البابَين: يُنادي معالِجَ أوّلِ زرٍّ في الشجرة كما
 * ينادي حدثُ SDL تماماً، ثمّ يُعيد البناءَ ويطابق ويُرقّع ويُصيّر، ويقيس
 * الدورةَ كاملة. النسبُ المئويّةُ لا المتوسّط: المتوسّطُ يُخفي التلعثمَ
 * الذي يراه المستعمِل، و٩٥ هي التي يشتكي منها.
 */


/* الذاكرةُ المقيمةُ بالكيلوبايت — صفرٌ إن تعذّر القياسُ على المنصّة */
static double residentKiB() {
#if defined(_WIN32)
	/* `PrivateUsage` لا `WorkingSetSize`: الثانيةُ صفحاتٌ لم يُقلّمها النظامُ
	   بعدُ، فترتفع وتنخفض بلا علاقةٍ بما خصّصناه — قِسناها فخرجت غيرَ
	   مطّردةٍ مع عدد الدورات. الأولى هي الالتزامُ الخاصُّ فعلاً. */
	PROCESS_MEMORY_COUNTERS_EX pmc{};
	if (GetProcessMemoryInfo(GetCurrentProcess(),
	                         (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
		return (double)pmc.PrivateUsage / 1024.0;
	return 0.0;
#elif defined(__APPLE__)
	mach_task_basic_info info{};
	mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
	              (task_info_t)&info, &count) == KERN_SUCCESS)
		return (double)info.resident_size / 1024.0;
	return 0.0;
#else
	FILE* f = fopen("/proc/self/statm", "r");
	if (f == nullptr) return 0.0;
	long total = 0, resident = 0;
	int got = fscanf(f, "%ld %ld", &total, &resident);
	fclose(f);
	if (got != 2) return 0.0;
	return (double)resident * (double)sysconf(_SC_PAGESIZE) / 1024.0;
#endif
}

/* ذاكرة() — الذاكرةُ الخاصّةُ الملتزَمةُ بالكيلوبايت.
 *
 * `قياس_تفاعل` يقيس الذاكرةَ عبر السلسلةِ كلِّها: بناءٌ ومطابقةٌ وترقيعٌ
 * وتصيير. فحين ينمو الرقمُ لا يقول أيَّ طبقةٍ تُمسك. وهذه تُقاس بلا
 * نافذةٍ أصلاً، فتفصل بناءَ الشجرةِ عن تصييرها.
 */
static AlifObject* graphics_memory(AlifObject* /*_module*/, AlifObject* /*_ignored*/) {
	return alifFloat_fromDouble(residentKiB());
}

/* أوّلُ مفتاحِ نقرٍ في الشجرة، بترتيبِ العمق — هو ما يجده مُرسِلُ الحدث */
static bool firstTapKey(const NodePtr& _node, std::string* _out) {
	if (_node == nullptr) return false;
	for (const auto& ev : _node->getEvents()) {
		if (ev.type == IREventType::OnTap and !ev.expression.empty()) {
			*_out = ev.expression;
			return true;
		}
	}
	for (const auto& child : _node->getChildren())
		if (firstTapKey(child, _out)) return true;
	return false;
}

static double percentile(std::vector<double>& _sorted, double _p) {
	if (_sorted.empty()) return 0.0;
	size_t i = (size_t)(_p * (double)(_sorted.size() - 1) + 0.5);
	if (i >= _sorted.size()) i = _sorted.size() - 1;
	return _sorted[i];
}

/* قياس_تفاعل(دالة_البناء، عدد=1000، عرض=480، ارتفاع=360)
 *
 * يُعيد صفّاً من ثلاثةَ عشرَ:
 *   ٠ عددُ العيّنات
 *   ١ الوسيط (م.ث)  ٢ ٩٥٪  ٣ ٩٩٪  ٤ الأقصى  ٥ المتوسّط   (الدورةُ كاملة)
 *   ٦ وسيطُ العمل  ٧ ٩٥٪ عمل  ٨ أقصى عمل  (بلا انتظارِ المزامنة)
 *   ٩ ذاكرةٌ بعد الإحماء  ١٠ عند المنتصف  ١١ في النهاية (ك.ب)
 *   ١٢ عددُ المعالِجات الباقية في السجلّ
 *
 * ساعتان لا واحدة: المُصيِّرُ يُنشَأ بـ`SDL_RENDERER_PRESENTVSYNC`
 * (`window.cpp:129`)، فالدورةُ الكاملةُ تنتهي عند نبضةِ الشاشةِ لا عند
 * فراغِ عملنا. الكاملةُ هي ما يشعر به المستعمِل، وساعةُ العمل
 * هي ما ينمو مع حجمِ الشجرة. خلطُهما يُخفي الاثنَين.
 *
 * والذاكرةُ ثلاثُ عيّناتٍ لا اثنتان: نقطتان لا تُفرّقان بين
 * تسريبٍ مطّردٍ ومخابئَ تبلغ استقرارَها.
 */
static AlifObject* graphics_bench(AlifObject* _module, AlifObject* _args,
                                  AlifObject* _kwargs) {
	static const char* const kwlist[] = {"", "عدد", "عرض", "ارتفاع", nullptr};
	AlifObject* source = nullptr;
	AlifIntT count = 1000;
	AlifIntT width = 480;
	AlifIntT height = 360;
	if (!alifArg_parseTupleAndKeywords(_args, _kwargs, "O|iii", kwlist,
	                                   &source, &count, &width, &height))
		return nullptr;

	if (_graphicsRunning_) {
		alifErr_setString(_alifExcRuntimeError_,
			"قياس_تفاعل: لا يجوز القياسُ داخلَ تطبيقٍ يعمل");
		return nullptr;
	}
	if (!isCallable(source)) {
		alifErr_setString(_alifExcTypeError_, "قياس_تفاعل: يلزم دالّةُ بناء");
		return nullptr;
	}
	if (count < 1) {
		alifErr_setString(_alifExcValueError_, "قياس_تفاعل: العددُ أقلُّ من واحد");
		return nullptr;
	}

	AlifObject* liveRef = nullptr;
	NodePtr liveTree = buildTree(_module, source, &liveRef, "قياس_تفاعل");
	if (liveTree == nullptr) return nullptr;
	_graphicsRunning_ = true;

	sad::ui::Reconciler reconciler{};
	std::vector<double> samples;   /* الدورةُ كاملةً — بانتظارِ المزامنة */
	std::vector<double> work;      /* حتّى الترقيع — بلا انتظار */
	samples.reserve((size_t)count);
	work.reserve((size_t)count);

	/* الإحماء: أوّلُ الدورات تحمل تكاليفَ لا تتكرّر — ذاكرةُ الخطّ المخبّأة
	   وتخصيصاتُ SDL الأولى. نقيسُها ولا نحسبها في النسب. */
	const AlifIntT warmup = (count > 100) ? 100 : (count / 10 + 1);

	bool created = false;
	bool noHandler = false;   /* لا زرَّ في الشجرة أصلاً */
	bool raised = false;      /* استثناءُ ألفٍ مضبوطٌ من نداءِ المستعمِل */
	bool frozen = false;      /* دالّةُ البناء أعادت الجذرَ ذاتَه */
	double rssWarm = 0.0;
	double rssMid = 0.0;
	double rssEnd = 0.0;
	size_t handlersEnd = 0;

	{
		sad::ui::desktop::DesktopWindow window;
		sad::ui::desktop::WindowOptions options;
		options.title = "ألف — قياس";
		options.width = width;
		options.height = height;

		ALIF_BEGIN_ALLOW_THREADS
		if (window.create(options)) {
			created = true;
			window.setContent(liveTree);
			window.invalidate();
			window.runOneFrame();

			for (AlifIntT i = 0; i < count; i++) {
				std::string key;
				if (!firstTapKey(liveTree, &key)) { noHandler = true; break; }

				auto t0 = std::chrono::steady_clock::now();

				ALIF_BLOCK_THREADS
				auto it = _graphicsHandlers_.find(key);
				if (it == _graphicsHandlers_.end()) {
					noHandler = true;
				} else {
					AlifObject* result = invokeHandler(it->second, std::string());
					if (result == nullptr) raised = true;
					else ALIF_DECREF(result);
				}
				if (!noHandler and !raised) {
					AlifObject* newRef = nullptr;
					NodePtr newTree = buildTree(_module, source, &newRef, "قياس_تفاعل");
					if (newTree == nullptr) {
						raised = true;
					} else if (newTree == liveTree) {
						frozen = true;
					} else {
						auto d = reconciler.diff(liveTree, newTree);
						if (!d.isEmpty()) {
							bool structural = isStructural(d);
							if (reconciler.patch(liveTree, d)) {
								if (structural) window.setContent(liveTree);
								else window.applyPatches(d.size(), false);
							}
						}
						pruneHandlers(liveTree);
					}
					ALIF_XDECREF(newRef);
				}
				ALIF_UNBLOCK_THREADS

				if (noHandler or raised or frozen) break;

				auto tWork = std::chrono::steady_clock::now();

				/* التصييرُ داخلَ القياس عمداً: النقرةُ لا تنتهي عند الترقيع
				   بل عند الصورة. و`invalidate` لازمٌ لأنّ `runOneFrame`
				   لا يُصيّر إلّا عند طلبِ رسمٍ صريح. */
				window.invalidate();
				window.runOneFrame();

				auto t1 = std::chrono::steady_clock::now();
				if (i >= warmup) {
					samples.push_back(
						std::chrono::duration<double, std::milli>(t1 - t0).count());
					work.push_back(
						std::chrono::duration<double, std::milli>(tWork - t0).count());
				}
				if (i == warmup) rssWarm = residentKiB();
				if (i == count / 2) rssMid = residentKiB();
			}

			rssEnd = residentKiB();
			handlersEnd = _graphicsHandlers_.size();
			window.destroy();
		}
		ALIF_END_ALLOW_THREADS
	}

	_graphicsRunning_ = false;
	ALIF_XDECREF(liveRef);
	clearHandlers();

	if (raised) return nullptr;   /* الاستثناءُ مضبوطٌ من نداءِ ألف */
	if (!created) {
		alifErr_setString(_alifExcRuntimeError_, "قياس_تفاعل: تعذّر إنشاءُ النافذة");
		return nullptr;
	}
	if (frozen) {
		alifErr_setString(_alifExcRuntimeError_,
			"قياس_تفاعل: دالّةُ البناءِ أعادت الجذرَ ذاتَه");
		return nullptr;
	}
	if (noHandler) {
		alifErr_setString(_alifExcRuntimeError_,
			"قياس_تفاعل: لا معالِجَ نقرٍ في الشجرة — لا شيءَ يُقاس");
		return nullptr;
	}

	std::vector<double> sorted = samples;
	std::sort(sorted.begin(), sorted.end());
	std::vector<double> sortedWork = work;
	std::sort(sortedWork.begin(), sortedWork.end());
	double sum = 0.0;
	for (double v : samples) sum += v;
	double mean = samples.empty() ? 0.0 : sum / (double)samples.size();

	const int N = 13;
	AlifObject* out = alifTuple_new(N);
	if (out == nullptr) return nullptr;
	AlifObject* values[N] = {
		alifLong_fromLong((long)samples.size()),
		alifFloat_fromDouble(percentile(sorted, 0.50)),
		alifFloat_fromDouble(percentile(sorted, 0.95)),
		alifFloat_fromDouble(percentile(sorted, 0.99)),
		alifFloat_fromDouble(sorted.empty() ? 0.0 : sorted.back()),
		alifFloat_fromDouble(mean),
		alifFloat_fromDouble(percentile(sortedWork, 0.50)),
		alifFloat_fromDouble(percentile(sortedWork, 0.95)),
		alifFloat_fromDouble(sortedWork.empty() ? 0.0 : sortedWork.back()),
		alifFloat_fromDouble(rssWarm),
		alifFloat_fromDouble(rssMid),
		alifFloat_fromDouble(rssEnd),
		alifLong_fromLong((long)handlersEnd),
	};
	for (int i = 0; i < N; i++) {
		if (values[i] == nullptr) {
			for (int j = 0; j < i; j++) ALIF_DECREF(values[j]);
			ALIF_DECREF(out);
			return nullptr;
		}
	}
	for (int i = 0; i < N; i++) ALIFTUPLE_SET_ITEM(out, i, values[i]);
	return out;
}

/* ═══ التسجيل ═══ */

static AlifMethodDef _alifGraphicsMethods_[] = {
	{"نص_عنصر",       ALIF_CPPFUNCTION_CAST(graphics_text),     METHOD_O},
	{"عمود",          ALIF_CPPFUNCTION_CAST(graphics_column),   METHOD_VARARGS},
	{"صف",            ALIF_CPPFUNCTION_CAST(graphics_row),      METHOD_VARARGS},
	{"رصة",           ALIF_CPPFUNCTION_CAST(graphics_stack),    METHOD_VARARGS},
	{"زر",            ALIF_CPPFUNCTION_CAST(graphics_button),   METHOD_O},
	{"بطاقة",         ALIF_CPPFUNCTION_CAST(graphics_card),     METHOD_VARARGS},
	{"لفافة",         ALIF_CPPFUNCTION_CAST(graphics_scroll),   METHOD_VARARGS},
	{"صورة",          ALIF_CPPFUNCTION_CAST(graphics_image),    METHOD_O},
	{"حقل_نص",        ALIF_CPPFUNCTION_CAST(graphics_field),    METHOD_O},
	{"مفتاح",         ALIF_CPPFUNCTION_CAST(graphics_toggle),   METHOD_O},
	{"خانة_اختيار",   ALIF_CPPFUNCTION_CAST(graphics_check),    METHOD_O},
	{"فاصل",          ALIF_CPPFUNCTION_CAST(graphics_spacer),   METHOD_VARARGS},
	{"فاصل_خط",       ALIF_CPPFUNCTION_CAST(graphics_divider),  METHOD_NOARGS},
	{"تشغيل_تطبيق",   ALIF_CPPFUNCTION_CAST(graphics_run),      METHOD_VARARGS | METHOD_KEYWORDS},
	{"رسم_ولقطة",     ALIF_CPPFUNCTION_CAST(graphics_snapshot), METHOD_VARARGS},
	{"وسط",           ALIF_CPPFUNCTION_CAST(graphics_center),    METHOD_VARARGS},
	{"بحشوة",         ALIF_CPPFUNCTION_CAST(graphics_padding),   METHOD_VARARGS},
	{"بمقاس",         ALIF_CPPFUNCTION_CAST(graphics_sized),     METHOD_VARARGS},
	{"موسع",          ALIF_CPPFUNCTION_CAST(graphics_expanded),  METHOD_VARARGS},
	{"محاذاة",        ALIF_CPPFUNCTION_CAST(graphics_align),     METHOD_VARARGS},
	{"حاوية",         ALIF_CPPFUNCTION_CAST(graphics_container), METHOD_VARARGS},
	{"شبكة",          ALIF_CPPFUNCTION_CAST(graphics_grid),      METHOD_VARARGS},
	{"التفاف",        ALIF_CPPFUNCTION_CAST(graphics_wrap),      METHOD_VARARGS},
	{"صندوق_تجميع",   ALIF_CPPFUNCTION_CAST(graphics_group),     METHOD_VARARGS},
	{"شريط_تطبيق",    ALIF_CPPFUNCTION_CAST(graphics_appBar),    METHOD_VARARGS},
	{"شريط_حالة",     ALIF_CPPFUNCTION_CAST(graphics_statusBar), METHOD_VARARGS},
	{"أيقونة",        ALIF_CPPFUNCTION_CAST(graphics_icon),      METHOD_O},
	{"شارة",          ALIF_CPPFUNCTION_CAST(graphics_badge),     METHOD_O},
	{"رقاقة",         ALIF_CPPFUNCTION_CAST(graphics_chip),      METHOD_O},
	{"صورة_رمزية",    ALIF_CPPFUNCTION_CAST(graphics_avatar),    METHOD_O},
	{"تلميح_عنصر",    ALIF_CPPFUNCTION_CAST(graphics_tooltip),   METHOD_O},
	{"منطقة_نص",      ALIF_CPPFUNCTION_CAST(graphics_textArea),  METHOD_O},
	{"شريط_بحث",      ALIF_CPPFUNCTION_CAST(graphics_searchBar), METHOD_O},
	{"زر_عائم",       ALIF_CPPFUNCTION_CAST(graphics_fab),       METHOD_O},
	{"زر_راديو",      ALIF_CPPFUNCTION_CAST(graphics_radio),     METHOD_O},
	{"كتلة_كود",      ALIF_CPPFUNCTION_CAST(graphics_code),      METHOD_O},
	{"مؤشر_انشغال",   ALIF_CPPFUNCTION_CAST(graphics_spinner),   METHOD_NOARGS},
	{"شريط_تقدم",     ALIF_CPPFUNCTION_CAST(graphics_progress),  METHOD_O},
	{"منزلق",         ALIF_CPPFUNCTION_CAST(graphics_slider),    METHOD_O},
	{"مذكرة",         ALIF_CPPFUNCTION_CAST(graphics_memo),     METHOD_VARARGS},
	{"جدول_بيانات",   ALIF_CPPFUNCTION_CAST(graphics_dataTable), METHOD_VARARGS | METHOD_KEYWORDS},
	{"تقويم",         ALIF_CPPFUNCTION_CAST(graphics_calendar),  METHOD_VARARGS | METHOD_KEYWORDS},
	{"ألسنة",         ALIF_CPPFUNCTION_CAST(graphics_tabs),      METHOD_VARARGS | METHOD_KEYWORDS},
	{"قياس_تفاعل",    ALIF_CPPFUNCTION_CAST(graphics_bench),     METHOD_VARARGS | METHOD_KEYWORDS},
	{"ذاكرة",         ALIF_CPPFUNCTION_CAST(graphics_memory),    METHOD_NOARGS},
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

#endif /* ALIF_WITH_GRAPHICS */
