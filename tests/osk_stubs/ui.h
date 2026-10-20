#pragma once
#include <stdint.h>
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;
extern u32 running;
void waitflip();
void clearScreen(u32);
void rsxSync();
void flip();
void init_btns();
