#pragma once

#include <string_view>

namespace lexxos::platform {
struct PlatformServices;
}

namespace lexxos::app::console_output {

struct InteractiveLineGuard {
	void* context{nullptr};
	void (*clear)(void* context){nullptr};
	void (*redraw)(void* context){nullptr};
};

class ScopedInteractiveLineGuardBypass {
public:
	ScopedInteractiveLineGuardBypass();
	~ScopedInteractiveLineGuardBypass();

	ScopedInteractiveLineGuardBypass(ScopedInteractiveLineGuardBypass const&) = delete;
	ScopedInteractiveLineGuardBypass& operator=(ScopedInteractiveLineGuardBypass const&) = delete;
	ScopedInteractiveLineGuardBypass(ScopedInteractiveLineGuardBypass&&) = delete;
	ScopedInteractiveLineGuardBypass& operator=(ScopedInteractiveLineGuardBypass&&) = delete;

private:
	bool _active{false};
};

void setInteractiveLineGuard(InteractiveLineGuard guard);
void clearInteractiveLineGuard(void const* context);
void write(lexxos::platform::PlatformServices const& platform, std::string_view text);
void writeLine(lexxos::platform::PlatformServices const& platform, std::string_view text);
bool writeNormalized(
    lexxos::platform::PlatformServices const& platform,
    std::string_view text,
    bool& lastWasCarriageReturn);
void clear(lexxos::platform::PlatformServices const& platform);

} // namespace lexxos::app::console_output
