#include <lexxos/apps/storage/detail/sd_diagnostics_support.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::storage_programs::detail {
namespace {

	[[nodiscard]] bool isFallbackDiagnostic(lexxos::devices::SdCardDiagnosticCode code) {
		return code == lexxos::devices::SdCardDiagnosticCode::ReadFallbackToSafeBaud
		    || code == lexxos::devices::SdCardDiagnosticCode::WriteFallbackToSafeBaud;
	}

} // namespace

void printDiagnosticEntry(lexxos::os::AppContext& context, lexxos::devices::SdCardDiagnosticEntry const& entry, char const* prefix) {
	context.write(prefix);
	context.write("#");
	writeUnsigned(context, entry.sequence);
	context.write(" t=");
	writeUnsigned(context, entry.uptimeMs);
	context.write("ms error=\"");
	context.write(lexxos::devices::toString(entry.code));
	context.write("\" state=");
	context.write(lexxos::devices::toString(entry.state));
	context.write(" type=");
	context.write(lexxos::devices::toString(entry.cardType));
	context.write(" baud=");
	writeUnsigned(context, entry.activeBaudRate);
	context.write(" mode=");
	writeUnsigned(context, static_cast<std::uint32_t>(entry.mode));
	if (entry.command != 0xFFU) {
		context.write(" cmd=");
		writeUnsigned(context, entry.command);
	}
	if (entry.r1 != 0xFFFFFFFFU) {
		context.write(" r1=0x");
		writeHex32(context, entry.r1);
	}
	if (entry.blockCount != 0) {
		context.write(" block=");
		writeUnsigned64(context, entry.block);
		context.write(" count=");
		writeUnsigned(context, entry.blockCount);
	}
	context.writeLine("");
}

void printSdDiagnostics(lexxos::os::AppContext& context, char const* prefix) {
	auto const diagnostics = lexxos::devices::sd_card::diagnostics();
	context.write(prefix);
	context.write("diagnostics=");
	context.write(diagnostics.enabled ? "on" : "off");
	context.write(" stored=");
	writeUnsigned(context, diagnostics.storedCount);
	context.write(" dropped=");
	writeUnsigned(context, diagnostics.droppedCount);
	context.write(" seq=");
	writeUnsigned(context, diagnostics.sequence);
	context.writeLine("");
	for (std::uint32_t index = 0; index < diagnostics.storedCount; ++index) {
		printDiagnosticEntry(context, diagnostics.entries[index], prefix);
	}
}

void printSdLastDiagnostic(lexxos::os::AppContext& context, char const* prefix) {
	auto const diagnostics = lexxos::devices::sd_card::diagnostics();
	if (diagnostics.storedCount == 0) {
		context.write(prefix);
		context.writeLine("sd diagnostic: no recorded driver error");
		return;
	}

	auto const& last = diagnostics.entries[diagnostics.storedCount - 1];
	if (isFallbackDiagnostic(last.code) && diagnostics.storedCount >= 2) {
		context.write(prefix);
		context.write("sd diagnostic cause: ");
		printDiagnosticEntry(context, diagnostics.entries[diagnostics.storedCount - 2], "");
		context.write(prefix);
		context.write("sd diagnostic action: ");
		printDiagnosticEntry(context, last, "");
		return;
	}

	context.write(prefix);
	context.write("sd diagnostic: ");
	printDiagnosticEntry(context, last, "");
}

} // namespace lexxos::apps::storage_programs::detail
