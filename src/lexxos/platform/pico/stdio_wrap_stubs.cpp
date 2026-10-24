#include <cstdio>

extern "C" {

__attribute__((weak)) int __wrap_putchar(int character) {
	return character;
}

__attribute__((weak)) int __wrap_puts(char const* text) {
	(void) text;
	return 0;
}

__attribute__((weak)) int __wrap_getchar() {
	return EOF;
}
}
