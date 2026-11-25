// newlib syscall bridge for bare-metal Cortex-A7; no Linux is present.
// These functions never access the host filesystem. Program files come from
// LexxOS VFS only. Heap ownership stays in cpp_runtime.cpp (malloc/free).
#include <cerrno>
#include <cstddef>
#include <cstdint>

#include <sys/stat.h>

extern "C" void rk3506_uart_write_bytes(char const*, std::size_t);
extern "C" {

int _write(int fd, char const* buffer, int count) {
	if (fd != 1 && fd != 2) {
		errno = EBADF;
		return -1;
	}

	if (!buffer || count < 0) {
		errno = EINVAL;
		return -1;
	}

	rk3506_uart_write_bytes(buffer, static_cast<std::size_t>(count));
	return count;
}

int _read(int, char*, int) {
	errno = EAGAIN;
	return -1;
}

int _close(int) {
	errno = EBADF;
	return -1;
}

int _fstat(int fd, struct stat* out) {
	if (fd < 0 || fd > 2 || !out) {
		errno = EBADF;
		return -1;
	}

	out->st_mode = S_IFCHR;
	return 0;
}

int _isatty(int fd) {
	return fd >= 0 && fd <= 2 ? 1 : 0;
}

int _lseek(int, int, int) {
	errno = ESPIPE;
	return -1;
}

int _getpid() {
	return 1;
}

int _kill(int, int) {
	errno = EINVAL;
	return -1;
}

// Any newlib _sbrk allocation is deliberately refused: newlib uses our
// malloc/free, and a second heap allocator would corrupt RAM reservations.
void* _sbrk(std::ptrdiff_t) {
	errno = ENOMEM;
	return reinterpret_cast<void*>(-1);
}

[[noreturn]] void _exit(int) {
	for (;;) {
		__asm volatile("wfi");
	}
}
} // extern C
