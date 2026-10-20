#pragma once
#include "ui.h"
#define SYSUTIL_OSK_DONE 1
#define SYSUTIL_OSK_UNLOADED 2
typedef void (*input_cb)(u64, u64, void *);
int sysUtilRegisterCallback(int, input_cb, void *);
int sysUtilUnregisterCallback(int);
int sysUtilCheckCallback();
