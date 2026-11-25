#include <lexxos/apps/storage/rk3506/sdmmc_program.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/os/app_context.h>
#include <lexxos/platform/rockchip/rk3506/sdmmc_probe.h>
#include <lexxos/program/program_manager.h>

namespace lexxos::platform::rk3506 {

// Временный мост к существующим GPT и FAT-диагностике.
bool scanSdmmc(os::AppContext& context);
bool sdmmcFatMounted();

} // namespace lexxos::platform::rk3506

namespace lexxos::apps::storage_programs {

namespace {

	void hexByte(lexxos::os::AppContext& context, std::uint8_t value) {
		constexpr char digits[] = "0123456789abcdef";
		char out[] = {digits[value >> 4U], digits[value & 15U], '\0'};
		context.write(out);
	}

	void hex32(lexxos::os::AppContext& context, std::uint32_t value) {
		// The previous implementation called hexByte() for each *nibble*,
		// resulting in sixteen digits (for example RCA 0x0000000100000000).
		// Produce exactly eight hex digits for a 32-bit register.
		constexpr char digits[] = "0123456789abcdef";
		char out[] = "0x00000000";
		for (unsigned i = 0; i < 8; ++i) {
			out[2 + i] = digits[(value >> (28U - 4U * i)) & 15U];
		}

		context.write(out);
	}

	void decimal(lexxos::os::AppContext& context, std::uint32_t value) {
		char s[11]{};
		std::size_t pos = sizeof(s) - 1;
		do {
			s[--pos] = static_cast<char>('0' + value % 10U);
			value /= 10U;
		} while (value);

		context.write(s + pos);
	}

	void line(lexxos::os::AppContext& context, char const* key, std::uint32_t value) {
		context.write(key);
		hex32(context, value);
		context.writeLine("");
	}

	int run(lexxos::os::AppContext& context,
	    lexxos::program::ProgramInvocation const& inv,
	    lexxos::program::ParsedArguments const&) {
		auto const sub = inv.argc > 1 ? std::string_view(inv.argv[1]) : "status";
		if (sub == "status") {
			auto const snapshot = platform::rk3506::sdmmcStatus();
			context.writeLine("RK3506 SDMMC @ 0xff480000 (read-only register snapshot)");
			line(context, "VERID: ", snapshot.version);
			line(context, "HCON: ", snapshot.hostConfig);
			line(context, "CTRL: ", snapshot.control);
			line(context, "PWREN: ", snapshot.powerEnable);
			line(context, "CLKDIV: ", snapshot.clockDivider);
			line(context, "CLKENA: ", snapshot.clockEnable);
			line(context, "CTYPE: ", snapshot.cardType);
			line(context, "CDETECT: ", snapshot.cardDetect);
			line(context, "STATUS: ", snapshot.status);
			line(context, "RINTSTS: ", snapshot.interruptStatus);
			context.write("probe-ready: ");
			context.writeLine(snapshot.ready ? "yes" : "no");
			return 0;
		}

		if (sub == "probe") {
			if (platform::rk3506::sdmmcFatMounted()) {
				context.writeLine("SDMMC: unmount /mnt/sd using 'sdmount unmount' before probe");
				return 1;
			}

			context.writeLine("SDMMC: explicit read-only card enumeration; no filesystem writes");
			if (!platform::rk3506::sdmmcProbe()) {
				context.write("SDMMC: probe failed at CMD");
				decimal(context, platform::rk3506::sdmmcResult().lastCommand);
				context.write(" RINTSTS=");
				hex32(context, platform::rk3506::sdmmcResult().lastInterrupt);
				context.writeLine("");
				return 1;
			}

			context.write("SDMMC: SD v2 ready, RCA=");
			hex32(context, platform::rk3506::sdmmcResult().rca);
			context.write(" OCR=");
			hex32(context, platform::rk3506::sdmmcResult().ocr);
			context.writeLine((platform::rk3506::sdmmcResult().ocr & (1U << 30U)) ? " (SDHC/SDXC)" : " (SDSC)");
			return 0;
		}

		if (sub == "scan") {
			if (platform::rk3506::sdmmcFatMounted()) {
				context.writeLine("SDMMC: unmount /mnt/sd using 'sdmount unmount' before scan");
				return 1;
			}

			return platform::rk3506::scanSdmmc(context) ? 0 : 1;
		}

		if (sub == "read") {
			if (inv.argc != 3) {
				context.writeLine("usage: sdmmc read <lba>  (after sdmmc probe)");
				return 1;
			}

			std::uint32_t lba = 0;
			auto const arg = std::string_view(inv.argv[2]);
			if (arg.empty() || arg.size() > 9) {
				return 1;
			}

			for (char c : arg) {
				if (c < '0' || c > '9') {
					return 1;
				}

				lba = lba * 10 + static_cast<std::uint32_t>(c - '0');
			}

			if (lba > 0x007FFFFFU) {
				context.writeLine("SDMMC: diagnostic LBA out of permitted range");
				return 1;
			}

			// Guard the caller's stack buffer against an unexpected PIO overrun.
			// The storage is still 4-byte aligned for the 32-bit FIFO accesses.
			struct alignas(4) GuardedSector {
				std::uint32_t before;
				std::uint8_t bytes[512];
				std::uint32_t after;
			};

			GuardedSector sector{0xA55A1EAFU, {}, 0x5AA5E1F0U};
			if (!platform::rk3506::sdmmcReadSector(lba, sector.bytes)) {
				context.write("SDMMC: read failed at CMD");
				decimal(context, platform::rk3506::sdmmcResult().lastCommand);
				context.write(" RINTSTS=");
				hex32(context, platform::rk3506::sdmmcResult().lastInterrupt);
				context.writeLine("");
				return 1;
			}

			if (sector.before != 0xA55A1EAFU || sector.after != 0x5AA5E1F0U) {
				context.writeLine("SDMMC: ERROR: sector buffer guard corrupted");
				return 1;
			}

			// The previous firmware reached this point but took a Data Abort
			// immediately after writing `SDMMC: sector `. Avoid the old decimal
			// formatter while locating the fault and mark progress explicitly.
			context.write("SDMMC: CMD17 completed; LBA=");
			hex32(context, lba);
			context.write(" RINTSTS=");
			hex32(context, platform::rk3506::sdmmcResult().lastInterrupt);
			context.writeLine("");
			context.writeLine("SDMMC: first 64 bytes:");

			for (std::size_t i = 0; i < 64; ++i) {
				hexByte(context, sector.bytes[i]);
				context.write(i % 16 == 15 ? "\r\n" : " ");
			}

			if (lba == 0) {
				context.write("MBR signature: ");
				context.writeLine(sector.bytes[510] == 0x55 && sector.bytes[511] == 0xAA ? "55aa" : "missing");
			} else if (lba == 1) {
				context.write("GPT signature: ");
				context.writeLine(
				    sector.bytes[0] == 'E'
				            && sector.bytes[1] == 'F'
				            && sector.bytes[2] == 'I'
				            && sector.bytes[3] == ' '
				            && sector.bytes[4] == 'P'
				            && sector.bytes[5] == 'A'
				            && sector.bytes[6] == 'R'
				            && sector.bytes[7] == 'T'
				        ? "EFI PART"
				        : "missing");
			}

			return 0;
		}

		context.writeLine("usage: sdmmc [status | probe | read <lba> | scan]");
		return 1;
	}

} // namespace

void registerSdmmcProgram(lexxos::program::ProgramManager& programs) {
	programs.registerProgram({
	    .name = "sdmmc",
	    .help = "RK3506 SDMMC register status / read-only card probe",
	    .description = "Experimental SDMMC PIO diagnostics. Writes no blocks; probe reinitializes the controller.",
	    .usage = "sdmmc [status | probe | read <lba> | scan]",
	    .handler = run,
	    .allowExtraArguments = true,
	    .enabled = true,
	});
}

} // namespace lexxos::apps::storage_programs
