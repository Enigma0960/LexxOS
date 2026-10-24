#pragma once

#include <lexxos/app/console_mirror.h>

namespace lexxos::app::detail {

class PicoCalcLcdConsoleMirrorSink final : public lexxos::app::ConsoleMirrorSink {
public:
	void initialize(lexxos::os::OperatingSystem& operatingSystem) override;

	void write(std::string_view text) override;

	void flush() override;

	void clear() override;

	void refresh() override;

	void setMode(lexxos::app::ConsoleMirrorMode mode) override;

	[[nodiscard]] lexxos::app::ConsoleMirrorMode mode() const override;

	bool update() override;

	[[nodiscard]] bool isReady() const override;

private:
	lexxos::app::ConsoleMirrorMode _mode{lexxos::app::ConsoleMirrorMode::Text};
};

} // namespace lexxos::app::detail
