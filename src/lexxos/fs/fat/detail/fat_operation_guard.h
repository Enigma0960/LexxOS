#pragma once

namespace lexxos::fs::fat {

class FatVolume;

namespace detail {

	class FatOperationGuard {
	public:
		explicit FatOperationGuard(FatVolume const& volume);
		~FatOperationGuard();

		[[nodiscard]] bool locked() const;

	private:
		FatVolume const& _volume;
		bool _locked{false};
	};

} // namespace lexxos::fs::fat::detail

} // namespace lexxos::fs::fat
