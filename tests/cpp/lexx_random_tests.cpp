#include <cassert>

#include <lexx/random.h>

int main() {
	lexx::random::Generator first{}, second{};
	first.seed(0U);
	second.seed(0U);
	for (int i = 0; i < 1000; ++i) {
		assert(first.next_u32() == second.next_u32());
	}

	lexx::random::Generator other{};
	other.seed(1U);
	assert(first.next_u32() != other.next_u32());
	for (int i = 0; i < 2000; ++i) {
		assert(other.below(17U) < 17U);
		auto const value = other.range(20U, 35U);
		assert(value >= 20U && value <= 35U);
		assert(other.range(7U, 7U) == 7U);
	}

	return 0;
}
