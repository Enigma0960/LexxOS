// Minimal freestanding helpers required by Clang/GCC for plain RV32I (no M extension).
// Kept application-local until LexxOS SDK offers a shared guest runtime.
namespace {
unsigned divideUnsigned(unsigned numerator, unsigned denominator, unsigned* remainder) {
	if (denominator == 0U) {
		if (remainder) {
			*remainder = numerator;
		}
		return ~0U;
	}
	unsigned quotient = 0;
	unsigned residue = 0;
	for (int bit = 31; bit >= 0; --bit) {
		auto const high = residue >> 31U;
		residue = (residue << 1U) | ((numerator >> static_cast<unsigned>(bit)) & 1U);
		if (high != 0U || residue >= denominator) {
			residue -= denominator;
			quotient |= (1U << static_cast<unsigned>(bit));
		}
	}
	if (remainder) {
		*remainder = residue;
	}
	return quotient;
}

unsigned magnitude(int value) {
	auto const unsignedValue = static_cast<unsigned>(value);
	return value < 0 ? (~unsignedValue + 1U) : unsignedValue;
}
} // namespace

extern "C" {

int __mulsi3(int left, int right) {
	unsigned a = static_cast<unsigned>(left);
	unsigned b = static_cast<unsigned>(right);
	unsigned result = 0;
	while (b != 0U) {
		if (b & 1U) {
			result += a;
		}
		a <<= 1U;
		b >>= 1U;
	}
	return static_cast<int>(result);
}

unsigned __udivsi3(unsigned left, unsigned right) {
	return divideUnsigned(left, right, nullptr);
}

unsigned __umodsi3(unsigned left, unsigned right) {
	unsigned result = 0;
	(void) divideUnsigned(left, right, &result);
	return result;
}

int __divsi3(int left, int right) {
	auto result = divideUnsigned(magnitude(left), magnitude(right), nullptr);
	if ((left < 0) != (right < 0)) {
		result = ~result + 1U;
	}
	return static_cast<int>(result);
}

int __modsi3(int left, int right) {
	unsigned result = 0;
	(void) divideUnsigned(magnitude(left), magnitude(right), &result);
	if (left < 0) {
		result = ~result + 1U;
	}
	return static_cast<int>(result);
}

void* memcpy(void* destination, void const* source, __SIZE_TYPE__ bytes) {
	auto* target = static_cast<unsigned char volatile*>(destination);
	auto const* input = static_cast<unsigned char const volatile*>(source);
	for (__SIZE_TYPE__ i = 0; i < bytes; ++i) {
		target[i] = input[i];
	}
	return destination;
}

void* memset(void* destination, int value, __SIZE_TYPE__ bytes) {
	auto* target = static_cast<unsigned char volatile*>(destination);
	for (__SIZE_TYPE__ i = 0; i < bytes; ++i) {
		target[i] = static_cast<unsigned char>(value);
	}
	return destination;
}

void* memmove(void* destination, void const* source, __SIZE_TYPE__ bytes) {
	auto* target = static_cast<unsigned char volatile*>(destination);
	auto const* input = static_cast<unsigned char const volatile*>(source);
	if (reinterpret_cast<__UINTPTR_TYPE__>(target) <= reinterpret_cast<__UINTPTR_TYPE__>(input)) {
		for (__SIZE_TYPE__ i = 0; i < bytes; ++i) {
			target[i] = input[i];
		}
	} else {
		while (bytes != 0) {
			--bytes;
			target[bytes] = input[bytes];
		}
	}
	return destination;
}

} // extern "C"
