#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include <lexxos/apps/storage/rk3506/sdmmc_program.h>
#include <lexxos/os/operating_system.h>
#include <lexxos/platform/rockchip/rk3506/sdmmc_probe.h>

namespace {

bool mounted = false;
bool probeSucceeds = true;
bool readSucceeds = true;
unsigned probes = 0;
unsigned reads = 0;
unsigned scans = 0;
std::uint32_t lastLba = 0;

void expect(bool condition, char const* message) {
	if (!condition) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

class CommandOutput final : public lexxos::io::OutputStream {
public:
	bool write(std::string_view text) override {
		output.append(text);
		return true;
	}

	std::string output;
};

} // namespace

// Подставляется только hardware boundary; parser, registration и вывод команды настоящие.
namespace lexxos::platform::rk3506 {

SdmmcStatus sdmmcStatus() {
	return {.version = 0x1234, .ready = true};
}

SdmmcResult sdmmcResult() {
	return {.rca = 0x10000, .ocr = 1U << 30U, .lastInterrupt = 0x80, .lastCommand = 17};
}

bool sdmmcProbe() {
	++probes;
	return probeSucceeds;
}

bool sdmmcFatMounted() {
	return mounted;
}

bool scanSdmmc(os::AppContext&) {
	++scans;
	return true;
}

bool sdmmcReadSector(std::uint32_t lba, std::uint8_t* destination) {
	++reads;
	lastLba = lba;
	std::memset(destination, 0, 512);
	destination[510] = 0x55;
	destination[511] = 0xAA;
	return readSucceeds;
}

} // namespace lexxos::platform::rk3506

void checkSdmmcCommand() {
	lexxos::os::OperatingSystem os;
	lexxos::apps::storage_programs::registerSdmmcProgram(os.programs());
	CommandOutput output;
	auto context = os.createAppContext({.standardOutput = &output, .standardError = &output});
	auto run = [&](int argc, char const** argv) {
		output.output.clear();
		auto result = context.launch({.name = "sdmmc", .argc = argc, .argv = argv});
		expect(result.launchStatus == lexxos::program::ProgramLaunchStatus::Available, "sdmmc command dispatch failed");
		return result.exitCode;
	};
	char const* status[] = {"sdmmc", "status"};
	expect(run(2, status) == 0 && output.output.find("VERID: 0x00001234") != std::string::npos, "snapshot formatting changed");
	expect(output.output.find("probe-ready: yes") != std::string::npos, "ready snapshot lost");
	char const* probe[] = {"sdmmc", "probe"};
	mounted = true;
	expect(run(2, probe) == 1 && probes == 0, "mounted filesystem must prevent reprobe");
	char const* scan[] = {"sdmmc", "scan"};
	expect(run(2, scan) == 1 && scans == 0, "mounted filesystem must prevent scan");
	mounted = false;
	expect(run(2, probe) == 0 && probes == 1, "probe adapter not called");
	expect(output.output.find("RCA=0x00010000") != std::string::npos && output.output.find("(SDHC/SDXC)") != std::string::npos, "probe result formatting changed");
	probeSucceeds = false;
	expect(run(2, probe) == 1 && output.output.find("failed at CMD17 RINTSTS=0x00000080") != std::string::npos, "probe failure diagnostic changed");
	char const* read[] = {"sdmmc", "read", "0"};
	expect(run(3, read) == 0 && reads == 1 && lastLba == 0, "sector read dispatch failed");
	expect(output.output.find("MBR signature: 55aa") != std::string::npos, "sector output changed");
	readSucceeds = false;
	expect(run(3, read) == 1 && reads == 2, "sector read error lost");
	char const* invalid[] = {"sdmmc", "read", "8388608"};
	expect(run(3, invalid) == 1 && reads == 2, "out-of-range LBA reached backend");
	invalid[2] = "-1";
	expect(run(3, invalid) == 1 && reads == 2, "invalid LBA reached backend");
	expect(run(2, scan) == 0 && scans == 1, "scan adapter not called");
}
