// Freestanding RV32I arithmetic/memory support (same ABI helpers as fileman).
namespace {
unsigned divideUnsigned(unsigned numerator, unsigned denominator, unsigned* remainder) {
	if (!denominator) {
		if (remainder) {
			*remainder = numerator;
		}
		return ~0U;
	}
	unsigned quotient = 0, residue = 0;
	for (int bit = 31; bit >= 0; --bit) {
		unsigned const high = residue >> 31U;
		residue = (residue << 1U) | ((numerator >> static_cast<unsigned>(bit)) & 1U);
		if (high || residue >= denominator) {
			residue -= denominator;
			quotient |= 1U << static_cast<unsigned>(bit);
		}
	}
	if (remainder) {
		*remainder = residue;
	}
	return quotient;
}

unsigned magnitude(int value) {
	unsigned const v = static_cast<unsigned>(value);
	return value < 0 ? ~v + 1U : v;
}
} // namespace

extern "C" {
int __mulsi3(int left, int right) {
	unsigned a = static_cast<unsigned>(left), b = static_cast<unsigned>(right), result = 0;
	while (b) {
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
	unsigned remainder = 0;
	(void) divideUnsigned(left, right, &remainder);
	return remainder;
}

int __divsi3(int left, int right) {
	unsigned result = divideUnsigned(magnitude(left), magnitude(right), nullptr);
	if ((left < 0) != (right < 0)) {
		result = ~result + 1U;
	}
	return static_cast<int>(result);
}

int __modsi3(int left, int right) {
	unsigned remainder = 0;
	(void) divideUnsigned(magnitude(left), magnitude(right), &remainder);
	return left < 0 ? -static_cast<int>(remainder) : static_cast<int>(remainder);
}

void* memcpy(void* destination, void const* source, __SIZE_TYPE__ count) {
	auto* dst = static_cast<unsigned char*>(destination);
	auto const* src = static_cast<unsigned char const*>(source);
	for (__SIZE_TYPE__ i = 0; i < count; ++i) {
		dst[i] = src[i];
	}
	return destination;
}

void* memset(void* destination, int value, __SIZE_TYPE__ count) {
	auto* dst = static_cast<unsigned char*>(destination);
	for (__SIZE_TYPE__ i = 0; i < count; ++i) {
		dst[i] = static_cast<unsigned char>(value);
	}
	return destination;
}

void* memmove(void* destination, void const* source, __SIZE_TYPE__ count) {
	auto* dst = static_cast<unsigned char*>(destination);
	auto const* src = static_cast<unsigned char const*>(source);
	if (reinterpret_cast<__UINTPTR_TYPE__>(dst) <= reinterpret_cast<__UINTPTR_TYPE__>(src)) {
		for (__SIZE_TYPE__ i = 0; i < count; ++i) {
			dst[i] = src[i];
		}
	} else {
		while (count) {
			--count;
			dst[count] = src[count];
		}
	}
	return destination;
}
} // extern "C"
