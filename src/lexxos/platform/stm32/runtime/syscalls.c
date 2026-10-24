#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

extern uint32_t _end;
extern uint32_t _estack;

void lexxos_stm32_uart_write_raw(char const* data, size_t size);

caddr_t _sbrk(int increment) {
	static uint8_t* heap_end = NULL;
	uint8_t* const stack_limit = ((uint8_t*) &_estack) - (8U * 1024U);

	if (heap_end == NULL) {
		heap_end = (uint8_t*) &_end;
	}

	uint8_t* const previous_heap_end = heap_end;
	uint8_t* const next_heap_end = heap_end + increment;
	if (increment < 0 || next_heap_end > stack_limit) {
		errno = ENOMEM;
		return (caddr_t) -1;
	}

	heap_end = next_heap_end;
	return (caddr_t) previous_heap_end;
}

int _write(int file, char const* data, int size) {
	(void) file;
	if (data == NULL || size <= 0) {
		return 0;
	}
	lexxos_stm32_uart_write_raw(data, (size_t) size);
	return size;
}

int _read(int file, char* data, int size) {
	(void) file;
	(void) data;
	(void) size;
	errno = EAGAIN;
	return -1;
}

int _close(int file) {
	(void) file;
	return -1;
}

int _fstat(int file, struct stat* status) {
	(void) file;
	if (status != NULL) {
		status->st_mode = S_IFCHR;
	}
	return 0;
}

int _isatty(int file) {
	(void) file;
	return 1;
}

int _lseek(int file, int offset, int whence) {
	(void) file;
	(void) offset;
	(void) whence;
	return 0;
}

void _exit(int status) {
	(void) status;
	while (1) {
		__asm volatile("wfi");
	}
}

int _kill(int pid, int signal) {
	(void) pid;
	(void) signal;
	errno = EINVAL;
	return -1;
}

int _getpid(void) {
	return 1;
}
