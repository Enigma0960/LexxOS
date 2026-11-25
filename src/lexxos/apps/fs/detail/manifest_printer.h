#pragma once
#include <array>
#include <cstddef>
#include <string_view>

namespace lexxos::os {

class AppContext;

} // namespace lexxos::os

namespace lexxos::apps::filesystem_programs::detail {

// Два ограниченных прохода: проверка всего документа, затем печать полей.
class ManifestPrinter final {
public:
	ManifestPrinter(os::AppContext& context, std::string_view json, bool emit);
	bool run();

private:
	void spaces();
	bool consume(char ch);
	bool string(std::string_view& value);
	bool scalar(bool show);
	bool value(unsigned depth, bool show);
	bool object(unsigned depth, bool show = true);
	os::AppContext& context_;
	std::string_view json_;
	bool emit_{false};
	std::size_t position_{0};
	std::array<char, 256> label_{};
	std::size_t length_{0};
};

} // namespace lexxos::apps::filesystem_programs::detail
