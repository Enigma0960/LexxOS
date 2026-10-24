#pragma once

#include <cstdint>

#include <lexxos/platform/board.h>

namespace lexxos::devices::espidf_ethernet {

struct Status {
	bool supported{false};
	bool initialized{false};
	bool started{false};
	bool linkUp{false};
	bool gotIp{false};
	bool macValid{false};
	std::uint8_t mac[6]{};
	bool speedValid{false};
	std::uint32_t speedMbps{0};
	bool duplexValid{false};
	bool fullDuplex{false};
	char ip[16]{};
	char netmask[16]{};
	char gateway[16]{};
	char hostname[33]{};
	int lastError{0};
	char lastErrorName[24]{};
};

bool start(lexxos::platform::BoardDescriptor const& board);
bool stop();
bool restart(lexxos::platform::BoardDescriptor const& board);
Status status();

} // namespace lexxos::devices::espidf_ethernet
