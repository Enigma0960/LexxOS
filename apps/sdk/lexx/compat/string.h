#pragma once

// Minimal C string declarations for the freestanding RV32I guest toolchain.
// Implementations are provided by apps/sdk/lexx/runtime.cpp and the
// application's optional guest string support; no hosted libc is linked.
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t strlen(char const* text);
int strcmp(char const* left, char const* right);
int strncmp(char const* left, char const* right, size_t count);
int memcmp(void const* left, void const* right, size_t count);
void* memcpy(void* destination, void const* source, size_t count);
void* memmove(void* destination, void const* source, size_t count);
void* memset(void* destination, int value, size_t count);

#ifdef __cplusplus
}
#endif
