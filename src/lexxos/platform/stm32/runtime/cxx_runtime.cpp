#include <lexxos/platform/platform.h>

extern "C" void* __dso_handle = nullptr;

extern "C" void _init() {
}

extern "C" void _fini() {
}

extern "C" int __cxa_atexit(void (*)(void*), void*, void*) {
	return 0;
}

extern "C" void __cxa_finalize(void*) {
}

extern "C" void
__cxa_pure_virtual() {
	lexxos::platform::writeConsole("panic: pure virtual call\r\n");
	while (true) {
		__asm volatile("wfi");
	}
}
