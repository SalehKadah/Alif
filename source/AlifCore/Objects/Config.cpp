#include "alif.h"



//extern AlifObject* alifInit__abc(void); // 8
extern AlifObject* alifInit_math(void); // 16
extern AlifObject* alifInit_time(void);

extern AlifObject* alifInit__random(void); // 41

extern AlifObject* alifInit__io(void);

#ifdef ALIF_WITH_GRAPHICS
extern AlifObject* alifInit_graphics(void);
#endif



class InitTable _alifImportInitTab_[] = { // 87
	//{"صنف_اساس_مجرد", alifInit__abc},

	{"الرياضيات", alifInit_math}, // 96

	{"الوقت", alifInit_time},



	{"عشوائي", alifInit__random},

	{"_imp", alifInit__imp},

	/* These entries are here for sys.builtin_module_names */
	{"builtins", nullptr},
	{"النظام", nullptr},


	{"تبادل", alifInit__io},

#ifdef ALIF_WITH_GRAPHICS
	{"رسومات", alifInit_graphics},
#endif

	/* Sentinel */
	{0, 0}
};
