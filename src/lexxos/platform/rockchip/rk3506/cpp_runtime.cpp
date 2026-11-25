/* Freestanding C++20 support for the first RK3506 bring-up.
 * Fixed-size heap is deliberately isolated from the bootloader-owned DDR
 * and from the separate scheduler stack pool. CPU0 only: not IRQ-safe.
 */
#include <cstddef>
#include <cstdint>
#include <new>

#include <lexxos/runtime/selected_runtime_profile.h>

namespace {

constexpr std::size_t kHeapBytes = lexxos::runtime::standardRuntimeProfile.kernel.heapBytes;
static_assert(kHeapBytes >= 1024 && kHeapBytes % 16 == 0);

constexpr std::size_t kAlignment = 16;

// CPU0-only bootstrap allocator. Disabling IRQ prevents FreeRTOS
// preemption while modifying the free-list. Nested use preserves the
// previous interrupt state. Never allocate from IRQ context.
struct AllocatorGuard {
#if defined(LEXXOS_RK3506_FREERTOS_PERIPHERALS)
	std::uint32_t previousCpsr{};

	AllocatorGuard() {
		asm volatile("mrs %0, cpsr; cpsid i"
		    : "=r"(previousCpsr) : : "memory");
	}

	~AllocatorGuard() {
		if ((previousCpsr & 0x80U) == 0U) {
			asm volatile("cpsie i" ::: "memory");
		}
	}
#endif
};

struct alignas(16) Block {
	std::size_t bytes;
	Block* next;
	bool available;
};

static_assert(sizeof(Block) % kAlignment == 0);
alignas(16) std::uint8_t g_heap[kHeapBytes];
Block* g_head{};
std::size_t g_lowWater = kHeapBytes - sizeof(Block);

std::size_t alignUp(std::size_t n) {
	return (n + kAlignment - 1) & ~(kAlignment - 1);
}

void initializeHeap() {
	if (g_head) {
		return;
	}

	g_head = reinterpret_cast<Block*>(g_heap);
	g_head->bytes = kHeapBytes - sizeof(Block);
	g_head->next = nullptr;
	g_head->available = true;
}

void coalesce() {
	for (Block* current = g_head; current && current->next;) {
		Block* next = current->next;
		if (current->available && next->available) {
			current->bytes += sizeof(Block) + next->bytes;
			current->next = next->next;
		} else {
			current = next;
		}
	}
}

std::size_t availableBytes() {
	[[maybe_unused]] AllocatorGuard guard;

	initializeHeap();
	std::size_t result = 0;
	for (Block* block = g_head; block; block = block->next) {
		if (block->available) {
			result += block->bytes;
		}
	}

	return result;
}

void* allocate(std::size_t bytes) {
	[[maybe_unused]] AllocatorGuard guard;

	initializeHeap();
	if (!bytes) {
		bytes = 1;
	}

	if (bytes > kHeapBytes - kAlignment) {
		return nullptr;
	}

	bytes = alignUp(bytes);
	for (Block* block = g_head; block; block = block->next) {
		if (!block->available || block->bytes < bytes) {
			continue;
		}

		if (block->bytes >= bytes + sizeof(Block) + kAlignment) {
			auto* extra = reinterpret_cast<Block*>(reinterpret_cast<std::uint8_t*>(block + 1) + bytes);
			extra->bytes = block->bytes - bytes - sizeof(Block);
			extra->next = block->next;
			extra->available = true;
			block->next = extra;
			block->bytes = bytes;
		}

		block->available = false;
		auto const freeNow = availableBytes();
		if (freeNow < g_lowWater) {
			g_lowWater = freeNow;
		}

		return block + 1;
	}

	return nullptr;
}

void release(void* pointer) {
	[[maybe_unused]] AllocatorGuard guard;

	if (!pointer) {
		return;
	}

	initializeHeap();
	// Validate against our linked list before modifying allocator metadata.
	for (Block* block = g_head; block; block = block->next) {
		if (static_cast<void*>(block + 1) == pointer) {
			if (!block->available) {
				block->available = true;
			}

			coalesce();
			return;
		}
	}
}

extern "C" void rk3506_console_write(char const*);

[[noreturn]] void outOfMemory() {
	rk3506_console_write("\nFATAL: RK3506 C++ heap exhausted\n");
	for (;;) {
		asm volatile("wfi");
	}
}

} // namespace

// Expose the stage-5 heap explicitly to the shared ProgramMemoryArena.
// This avoids depending on freestanding std::malloc/std::free declarations.
extern "C" void* rk3506_heap_allocate(std::size_t bytes) {
	return allocate(bytes);
}

extern "C" void rk3506_heap_release(void* pointer) {
	release(pointer);
}

extern "C" std::uint32_t rk3506_heap_free_bytes() {
	return static_cast<std::uint32_t>(availableBytes());
}

extern "C" std::uint32_t rk3506_heap_low_water_bytes() {
	[[maybe_unused]] AllocatorGuard guard;

	initializeHeap();
	return static_cast<std::uint32_t>(g_lowWater);
}

// malloc/free are also used by newlib and by selected C++ library routines.
extern "C" void* malloc(std::size_t bytes) {
	return allocate(bytes);
}

extern "C" void free(void* ptr) {
	release(ptr);
}

extern "C" void* calloc(std::size_t count, std::size_t size) {
	if (size && count > static_cast<std::size_t>(-1) / size) {
		return nullptr;
	}

	std::size_t const total = count * size;
	auto* result = static_cast<std::uint8_t*>(allocate(total));
	if (result) {
		for (std::size_t i = 0; i < total; ++i) {
			result[i] = 0;
		}
	}

	return result;
}

extern "C" void* realloc(void* ptr, std::size_t bytes) {
	[[maybe_unused]] AllocatorGuard guard;

	if (!ptr) {
		return allocate(bytes);
	}

	if (!bytes) {
		release(ptr);
		return nullptr;
	}

	for (Block* block = g_head; block; block = block->next) {
		if (static_cast<void*>(block + 1) != ptr) {
			continue;
		}

		if (block->bytes >= bytes) {
			return ptr;
		}

		void* replacement = allocate(bytes);
		if (!replacement) {
			return nullptr;
		}

		auto* source = static_cast<std::uint8_t*>(ptr);
		auto* dest = static_cast<std::uint8_t*>(replacement);
		for (std::size_t i = 0; i < block->bytes; ++i) {
			dest[i] = source[i];
		}

		release(ptr);
		return replacement;
	}

	return nullptr;
}

void* operator new(std::size_t bytes) {
	if (void* p = allocate(bytes)) {
		return p;
	}
	outOfMemory(); // -fno-exceptions: never pretend throwing new succeeded.
}

void* operator new[](std::size_t bytes) {
	return ::operator new(bytes);
}

void operator delete(void* ptr) noexcept {
	release(ptr);
}

void operator delete[](void* ptr) noexcept {
	release(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept {
	release(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
	release(ptr);
}

void* operator new(std::size_t bytes, std::nothrow_t const&) noexcept {
	return allocate(bytes);
}

void* operator new[](std::size_t bytes, std::nothrow_t const&) noexcept {
	return allocate(bytes);
}

void operator delete(void* ptr, std::nothrow_t const&) noexcept {
	release(ptr);
}

void operator delete[](void* ptr, std::nothrow_t const&) noexcept {
	release(ptr);
}

extern "C" {
using Initializer = void (*)();
extern Initializer __preinit_array_start[];
extern Initializer __preinit_array_end[];
extern Initializer __init_array_start[];
extern Initializer __init_array_end[];

void rk3506_cpp_initialize() {
	for (auto* p = __preinit_array_start; p != __preinit_array_end; ++p) {
		(*p)();
	}

	for (auto* p = __init_array_start; p != __init_array_end; ++p) {
		(*p)();
	}
}

void* __dso_handle = nullptr;

int __cxa_atexit(void (*)(void*), void*, void*) {
	return 0;
}

void __cxa_finalize(void*) {
}

[[noreturn]] void __cxa_pure_virtual() {
	rk3506_console_write("\nFATAL: pure virtual call\n");
	for (;;) {
		asm volatile("wfi");
	}
}
} // extern C

// Small libc primitives required when GCC lowers aggregate moves and when
// standard-library string_view code is linked without a hosted libc.
extern "C" void* memcpy(void* destination, void const* source, std::size_t count) {
	auto* d = static_cast<std::uint8_t*>(destination);
	auto const* s = static_cast<std::uint8_t const*>(source);
	for (std::size_t i = 0; i < count; ++i) {
		d[i] = s[i];
	}

	return destination;
}

extern "C" void* memmove(void* destination, void const* source, std::size_t count) {
	auto* d = static_cast<std::uint8_t*>(destination);
	auto const* s = static_cast<std::uint8_t const*>(source);
	if (d < s) {
		for (std::size_t i = 0; i < count; ++i) {
			d[i] = s[i];
		}
	} else {
		for (std::size_t i = count; i != 0; --i) {
			d[i - 1] = s[i - 1];
		}
	}

	return destination;
}

extern "C" void* memset(void* destination, int value, std::size_t count) {
	auto* d = static_cast<std::uint8_t*>(destination);
	for (std::size_t i = 0; i < count; ++i) {
		d[i] = static_cast<std::uint8_t>(value);
	}
	return destination;
}

extern "C" int memcmp(void const* lhs, void const* rhs, std::size_t count) {
	auto const* a = static_cast<std::uint8_t const*>(lhs);
	auto const* b = static_cast<std::uint8_t const*>(rhs);
	for (std::size_t i = 0; i < count; ++i) {
		if (a[i] != b[i]) {
			return static_cast<int>(a[i]) - static_cast<int>(b[i]);
		}
	}

	return 0;
}

extern "C" std::size_t strlen(char const* value) {
	std::size_t length = 0;
	while (value[length]) {
		++length;
	}
	return length;
}

extern "C" void __aeabi_memcpy(void* d, void const* s, std::size_t n) {
	(void) memcpy(d, s, n);
}

extern "C" void __aeabi_memcpy4(void* d, void const* s, std::size_t n) {
	(void) memcpy(d, s, n);
}

extern "C" void __aeabi_memcpy8(void* d, void const* s, std::size_t n) {
	(void) memcpy(d, s, n);
}

extern "C" void __aeabi_memmove(void* d, void const* s, std::size_t n) {
	(void) memmove(d, s, n);
}

extern "C" void __aeabi_memset(void* d, std::size_t n, int v) {
	(void) memset(d, v, n);
}

extern "C" void __aeabi_memclr(void* d, std::size_t n) {
	(void) memset(d, 0, n);
}
