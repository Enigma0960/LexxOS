#pragma once

// Portable, deterministic, non-cryptographic PRNG. No OS entropy is implied.
// xoshiro128++: four 32-bit words, only shift/rotate/xor/add on RV32I.
namespace lexx::random {

using u32 = unsigned int;
static_assert(sizeof(u32) == 4);

struct Generator {
	u32 state[4]{0x243F6A88U, 0x85A308D3U, 0x13198A2EU, 0x03707344U};

	static constexpr u32 rotl(u32 value, unsigned bits) {
		return (value << bits) | (value >> (32U - bits));
	}

	// splitmix32 initialization; seed=0 is valid and deterministic.
	void seed(u32 value) {
		for (auto& element : state) {
			value += 0x9E3779B9U;
			u32 z = value;
			z ^= z >> 16U;
			z *= 0x85EBCA6BU;
			z ^= z >> 13U;
			z *= 0xC2B2AE35U;
			z ^= z >> 16U;
			element = z;
		}

		if ((state[0] | state[1] | state[2] | state[3]) == 0U) {
			state[0] = 1U;
		}
	}

	[[nodiscard]] u32 next_u32() {
		u32 const result = rotl(state[0] + state[3], 7U) + state[0];
		u32 const t = state[1] << 9U;
		state[2] ^= state[0];
		state[3] ^= state[1];
		state[1] ^= state[2];
		state[0] ^= state[3];
		state[2] ^= t;
		state[3] = rotl(state[3], 11U);
		return result;
	}

	// Rejection sampling avoids modulo bias; upper_bound is exclusive.
	[[nodiscard]] u32 below(u32 upper_bound) {
		if (upper_bound == 0U) {
			return next_u32();
		}

		u32 const threshold = (0U - upper_bound) % upper_bound;
		for (;;) {
			u32 const value = next_u32();
			if (value >= threshold) {
				return value % upper_bound;
			}
		}
	}

	// Inclusive endpoints, including the full uint32_t range.
	[[nodiscard]] u32 range(u32 first, u32 last) {
		if (first > last) {
			u32 const temporary = first;
			first = last;
			last = temporary;
		}

		u32 const span = last - first + 1U;
		return first + below(span);
	}

	// Uniform Q0.32 fraction in [0, 1): 0=0.0, 0x80000000=0.5.
	// Prefer this representation in RV32I games without a soft-float runtime.
	[[nodiscard]] u32 unit_q32() {
		return next_u32();
	}

	// Floating-point fraction [0, 1), at 24-bit precision.
	// Floating-point calls need software FP support on RV32I.
	[[nodiscard]] float unit() {
		return static_cast<float>(next_u32() >> 8U) * (1.0F / 16777216.0F);
	}

	// Floating-point interval [first, last) for first < last.
	[[nodiscard]] float range_float(float first, float last) {
		return first + (last - first) * unit();
	}
};

} // namespace lexx::random
