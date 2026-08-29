#pragma once

/* لواحق ملفات لغة ألف - المصدر الوحيد لتعريفها.

   لكل لاحقة صيغتان: عربية (الأصل) وإنجليزية (متوافقة مع ما سبق):

		البرنامج	.الف		.alif
		المكتبة		.مكتبة		.aliflib
		المترجَم	.الفم		.alifc
		برنامج نوافذ	.الفن		.alifw		(ويندوز فقط)

   تُكتب الحروف العربية هنا بترميزها الصريح - \u للنصوص العريضة و \x
   لبايتات UTF-8 للنصوص الضيقة - لا بحروف عربية مباشرة.
   السبب: المترجم قد يقرأ الملف المصدري بترميز نظام التشغيل لا بترميز UTF-8،
   فتتحول اللاحقة العربية إلى بايتات لا تطابق اسم الملف على القرص، فيفشل
   تشغيل الملفات ذات اللاحقة العربية أو استيراد المكتبات دون سبب ظاهر.
   الترميز الصريح يجعل النتيجة واحدة مهما كان المترجم أو لغة النظام. */


   /* ------------------------ اللواحق العريضة (wchar_t) ------------------------ */

/* ".الف" */
#define ALIF_WSUFFIX_SOURCE_AR		L".\u0627\u0644\u0641"
#define ALIF_WSUFFIX_SOURCE_EN		L".alif"

/* ".مكتبة" */
#define ALIF_WSUFFIX_LIB_AR			L".\u0645\u0643\u062A\u0628\u0629"
#define ALIF_WSUFFIX_LIB_EN			L".aliflib"


/* ------------------------ اللواحق الضيقة (UTF-8) ------------------------ */

/* ".الف" */
#define ALIF_SUFFIX_SOURCE_AR		".\xD8\xA7\xD9\x84\xD9\x81"
#define ALIF_SUFFIX_SOURCE_EN		".alif"

/* ".مكتبة" */
#define ALIF_SUFFIX_LIB_AR			".\xD9\x85\xD9\x83\xD8\xAA\xD8\xA8\xD8\xA9"
#define ALIF_SUFFIX_LIB_EN			".aliflib"

/* ".الفم" - الملف المترجَم */
#define ALIF_SUFFIX_COMPILED_AR		".\xD8\xA7\xD9\x84\xD9\x81\xD9\x85"
#define ALIF_SUFFIX_COMPILED_EN		".alifc"

/* ".الفن" - برنامج نوافذ (ويندوز فقط) */
#define ALIF_SUFFIX_WINDOWED_AR		".\xD8\xA7\xD9\x84\xD9\x81\xD9\x86"
#define ALIF_SUFFIX_WINDOWED_EN		".alifw"


/* ------------------------ ملف تهيئة الحزمة ------------------------ */

/* "__تهيئة__" */
#define ALIF_INITMODULE_NAME		"__\xD8\xAA\xD9\x87\xD9\x8A\xD8\xA6\xD8\xA9__"
/* الطول بالبايتات لا بالحروف: "__تهيئة__" تسعة حروف وأربعة عشر بايتاً.
   يُحسب من النص نفسه، فلا يحتاج تحديثاً إذا تغيّر اسم ملف التهيئة.
   sizeof تشمل الصفر الختامي فنطرحه. لا تصلح strlen هنا لأن القيمة
   تُستعمل في سياقات تتطلب ثابتاً في وقت الترجمة. */
#define ALIF_INITMODULE_NAMELEN		(sizeof(ALIF_INITMODULE_NAME) - 1)

/* "__تهيئة__." - للمطابقة العريضة مع أي لاحقة */
#define ALIF_WINITMODULE_PREFIX		L"__\u062A\u0647\u064A\u0626\u0629__."
/* الطول بالحروف العريضة لا بالبايتات، لأن المقارنة تجري بـ wcsncmp.
   القسمة على sizeof(wchar_t) تضبط الحالتين: حرفان لكل محرف على
   ويندوز وأربعة على غيره. يُحسب من النص نفسه كسابقه. */
#define ALIF_WINITMODULE_PREFIXLEN	(sizeof(ALIF_WINITMODULE_PREFIX) / sizeof(wchar_t) - 1)
