#ifndef __WDL_TYPES_H__
#define __WDL_TYPES_H__

// Minimal stub for Cockos WDL types
// Provides INT_PTR definition needed by ns-eel2 on non-Windows platforms

#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
#else
// INT_PTR: pointer-sized integer, same as intptr_t
#ifndef INT_PTR
#define INT_PTR intptr_t
#endif
#endif

#endif
