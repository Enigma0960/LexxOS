#include <lexxos/drivers/core/dma/pl330_debug_port.h>

namespace lexxos::drivers::pl330 {

Pl330DebugPort::Pl330DebugPort(DebugPortAccess access)
    : _access(access) {
}

bool Pl330DebugPort::startNonsecure(std::uint32_t ownedChannels, unsigned channel, std::uint32_t programAddress) {
	return programAddress != 0 && execute(ownedChannels, channel, programAddress, true);
}

bool Pl330DebugPort::kill(std::uint32_t ownedChannels, unsigned channel) {
	return execute(ownedChannels, channel, 0, false);
}

bool Pl330DebugPort::execute(std::uint32_t ownedChannels, unsigned channel, std::uint32_t address, bool manager) {
	if (channel >= 8 || (ownedChannels & (1U << channel)) == 0 || !_access.read || !_access.write || !_access.barrier || !_access.nowMs || !_access.enter || !_access.leave) {
		return false;
	}

	auto* context = _access.context;
	auto state = _access.enter(context);
	if (_busy) {
		_access.leave(context, state);
		return false;
	}

	_busy = true;
	_access.leave(context, state);

	bool ready = true;
	auto const start = _access.nowMs(context);
	while ((_access.read(context, 0xD00U) & 1U) != 0U) {
		if (std::uint32_t(_access.nowMs(context) - start) >= 20U) {
			ready = false;
			break;
		}
	}

	if (ready) {
		auto const instruction = manager
		    ? (0xA2U << 16U) | (channel << 24U) // DMAGO, non-secure
		    : (0x01U << 16U) | (channel << 8U) | 1U; // DMAKILL, channel thread
		_access.write(context, 0xD08U, instruction);
		_access.write(context, 0xD0CU, address);
		_access.barrier(context);
		_access.write(context, 0xD04U, 0U);
		// Publish the command before allowing another client to read DBGSTATUS.
		_access.barrier(context);
	}

	state = _access.enter(context);
	_busy = false;
	_access.leave(context, state);
	return ready;
}

} // namespace lexxos::drivers::pl330
