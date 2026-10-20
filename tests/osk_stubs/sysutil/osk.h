#pragma once
#include "ui.h"
#include <sys/memory.h>
enum { OSK_OK, OSK_CANCELED, OSK_ABORT, OSK_NO_TEXT };
enum { OSK_PANEL_TYPE_PASSWORD = 1, OSK_PANEL_TYPE_ALPHABET = 2,
       OSK_PANEL_TYPE_NUMERAL = 4, OSK_PANEL_TYPE_ENGLISH = 8 };
enum { OSK_PROHIBIT_RETURN = 2, OSK_10KEY_PANEL = 1, OSK_FULLKEY_PANEL = 2 };
struct oskParam { u32 allowedPanels, firstViewPanel; struct { float x, y; } controlPoint; s32 prohibitFlags; };
struct oskInputFieldInfo { u16 *message, *startText; s32 maxLength; };
struct oskCallbackReturnParam { int res, len; u16 *str; };
int oskLoadAsync(sys_mem_container_t, oskParam *, oskInputFieldInfo *);
int oskUnloadAsync(oskCallbackReturnParam *);
int oskAbort();
int oskAddSupportLanguage(u32);
int oskSetKeyLayoutOption(u32);
