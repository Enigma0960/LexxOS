#pragma once

#include <cstddef>
#include <string_view>

#include <lexxos/io/output_stream.h>

namespace lexxos::platform {

struct PlatformServices;

} // namespace lexxos::platform

namespace lexxos::io {

class ConsoleOutputStream final : public OutputStream {
public:
	ConsoleOutputStream();
	explicit ConsoleOutputStream(lexxos::platform::PlatformServices const& platform);

	bool write(std::string_view text) override;

private:
	lexxos::platform::PlatformServices const* _platform{nullptr};
	bool _lastWasCarriageReturn{false};
};

} // namespace lexxos::io
