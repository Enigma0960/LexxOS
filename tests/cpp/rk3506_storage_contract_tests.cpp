#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <type_traits>

#include <lexxos/app/rk3506/picosd_mount_policy.h>
#include <lexxos/app/rk3506/picosd_storage.h>
#include <lexxos/app/rk3506/picosd_storage_service.h>
#include <lexxos/composition/products/lyra_sd_protocol.h>
#include <lexxos/composition/products/lyra_storage_service.h>
#include <lexxos/devices/rk3506/native_picosd_transport.h>
#include <lexxos/devices/rk3506/picosd_block_device.h>
#include <lexxos/devices/rk3506/picosd_diagnostic_session.h>
#include <lexxos/devices/rk3506/picosd_diagnostics.h>
#include <lexxos/devices/rk3506/picosd_dma_pl330.h>
#include <lexxos/devices/rk3506/picosd_spi1_hw.h>
#include <lexxos/devices/rk3506/sd_protocol.h>
#include <lexxos/fs/fat/fat_mount_backend.h>
#include <lexxos/hardware/products/picocalc_lyra_policy.h>

#include "fake_sd_protocol_io.h"

namespace {

lexxos::platform::rk3506::SdProtocol* lastProtocol{};
bool ready = true;
bool writeSucceeds = true;
bool readSucceeds = true;
bool allowMount = true;
unsigned reads{}, writes{}, failures{}, preparations{}, finishes{}, unmounts{};
bool lastSuccess{};
bool inserted = true;
unsigned probes{}, invalidations{};
std::uint32_t clockMHz = 12;

void failed() {
	++failures;
}

void expect(bool value, char const* message) {
	if (!value) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

} // namespace

namespace lexxos::platform::rk3506 {

SdProtocol::SdProtocol(SdProtocolIo& io)
    : _io(io) {
}

bool SdProtocol::probeAuto() {
	lastProtocol = this;
	expect(lexxos::composition::products::lyraSpi1Queue().busy(), "native probe bypassed queue");
	++probes;
	return ::ready = inserted;
}

void SdProtocol::invalidateRemoved() {
	lastProtocol = this;
	expect(lexxos::composition::products::lyraSpi1Queue().busy(), "native removal bypassed queue");
	++invalidations;
	::ready = false;
}

bool SdProtocol::cardInserted() {
	lastProtocol = this;
	return inserted;
}

PicoSdDiagnosticStatus SdProtocol::diagnosticStatus() {
	lastProtocol = this;
	return {.configured = true, .ready = ::ready, .highCapacity = true};
}

std::uint32_t picoSpi1ClockMHz() {
	return clockMHz;
}

bool picoSpi1SetClockMHz(std::uint32_t value) {
	expect(lexxos::composition::products::lyraSpi1Queue().busy(), "native clock change bypassed queue");
	clockMHz = value;
	return true;
}

PicoSpi1DmaInfo picoSdDmaInfo() {
	return {.available = true};
}

bool picoSdDmaEnabled() {
	return true;
}

bool SdProtocol::ready() {
	lastProtocol = this;
	return ::ready;
}

std::uint64_t SdProtocol::sectorCount() {
	lastProtocol = this;
	return 16;
}

bool SdProtocol::readSectors(std::uint32_t, std::uint8_t*, std::size_t) {
	lastProtocol = this;
	expect(lexxos::composition::products::lyraSpi1Queue().busy(), "native read bypassed queue");
	++reads;
	return readSucceeds;
}

bool SdProtocol::writeSectors(std::uint32_t, std::uint8_t const*, std::size_t) {
	lastProtocol = this;
	expect(lexxos::composition::products::lyraSpi1Queue().busy(), "native write bypassed queue");
	++writes;
	return writeSucceeds;
}

bool picoSdPrepareManualMount() {
	++preparations;
	return allowMount;
}

void picoSdManualMountFinished(bool success) {
	++finishes;
	lastSuccess = success;
}

void picoSdNotifyManualUnmount() {
	++unmounts;
}

} // namespace lexxos::platform::rk3506

namespace lexxos::composition::products {

platform::rk3506::SdProtocol& lyraSdProtocol() {
	static FakeSdProtocolIo io;
	static platform::rk3506::SdProtocol protocol{io};
	return protocol;
}

} // namespace lexxos::composition::products

// Заглушка общего реестра нужна только для проверки production composition.
namespace lexxos::fs::fat {

bool isMounted(std::string_view) {
	return false;
}

FatMountStatus mount(vfs::Vfs&, std::string_view, std::string_view) {
	return FatMountStatus::NoDevice;
}

vfs::UnmountStatus unmount(vfs::Vfs&, std::string_view) {
	return vfs::UnmountStatus::Ok;
}

vfs::UnmountStatus forceUnmount(vfs::Vfs&, std::string_view) {
	return vfs::UnmountStatus::Ok;
}

} // namespace lexxos::fs::fat

namespace {

class FakeTransport final : public lexxos::devices::SdMediaTransport {
public:
	bool online{true};
	bool present{true};
	bool writeOk{true};
	unsigned readCalls{}, writeCalls{}, probeCalls{};

	bool ready() override { return online; }

	bool inserted() override { return present; }

	bool probe() override {
		++probeCalls;
		return online = present;
	}

	void invalidateRemoved() override { online = false; }

	std::uint64_t sectorCount() override { return 4; }

	bool read(std::uint32_t, std::uint8_t* data, std::size_t) override {
		++readCalls;
		data[0] = 42;
		return online;
	}

	bool write(std::uint32_t, std::uint8_t const*, std::size_t) override {
		++writeCalls;
		return writeOk;
	}

	bool configured() override { return true; }

	bool highCapacity() override { return false; }

	std::uint32_t clockMHz() override { return 6; }

	bool setClockMHz(std::uint32_t) override { return false; }

	bool dmaAvailable() override { return false; }

	bool dmaEnabled() override { return false; }
};

class FakeFilesystem final : public lexxos::fs::fat::FatMountBackend {
public:
	bool mounted{};

	bool isMounted(std::string_view target) override {
		expect(target == "/mnt/sd", "wrong mount target");
		return mounted;
	}

	lexxos::fs::fat::FatMountStatus mount(lexxos::vfs::Vfs&, std::string_view, std::string_view) override { return lexxos::fs::fat::FatMountStatus::NoDevice; }

	lexxos::vfs::UnmountStatus unmount(lexxos::vfs::Vfs&, std::string_view) override { return lexxos::vfs::UnmountStatus::Ok; }

	lexxos::vfs::UnmountStatus forceUnmount(lexxos::vfs::Vfs&, std::string_view) override { return lexxos::vfs::UnmountStatus::Ok; }
};

} // namespace

int main() {
	{
		using lexxos::platform::rk3506::PicoSdMediaHooks;

		struct HookState {
			bool mounted;
			unsigned suppressions;
		} first{false, 0}, second{true, 0};

		PicoSdMediaHooks empty;
		expect(empty.mounted(), "unbound hooks allow media access");
		empty.suppress();
		auto mounted = [](void* context) {
			return static_cast<HookState*>(context)->mounted;
		};

		auto suppress = [](void* context) {
			++static_cast<HookState*>(context)->suppressions;
		};

		PicoSdMediaHooks firstHooks{&first, mounted, suppress};
		PicoSdMediaHooks secondHooks{&second, mounted, suppress};
		expect(!firstHooks.mounted() && secondHooks.mounted(), "mounted callback used wrong context");
		firstHooks.suppress();
		expect(first.suppressions == 1 && second.suppressions == 0, "suppression leaked between contexts");
		auto copy = secondHooks;
		copy.suppress();
		expect(second.suppressions == 1, "copied hooks lost context");
		PicoSdMediaHooks noContext{nullptr, mounted, suppress};
		expect(noContext.mounted(), "null context permits access");
		noContext.suppress();
		expect(PicoSdMediaHooks{&first, nullptr, suppress}.mounted(), "missing mounted callback permits access");
		expect(PicoSdMediaHooks{&first, mounted, nullptr}.mounted(), "missing suppression callback permits access");
	}

	using lexxos::app::rk3506::PicoSdStorageService;
	static_assert(!std::is_copy_constructible_v<PicoSdStorageService>);
	static_assert(!std::is_move_constructible_v<PicoSdStorageService>);
	auto& owned = lexxos::composition::products::lyraStorageService();
	expect(&owned == &lexxos::composition::products::lyraStorageService(), "composition created two service instances");
	FakeTransport transport;
	FakeFilesystem filesystem;
	PicoSdStorageService independent{transport, filesystem};
	lexxos::devices::rk3506::NativePicoSdTransport nativeTransport{lexxos::composition::products::lyraSpi1Queue(), lexxos::composition::products::lyraSdProtocol()};
	expect(&owned.backend() != &independent.backend(), "service backend is still global");
	expect(owned.backend().isReadOnly() && independent.backend().isReadOnly(), "unconfigured service permits writes");
	auto storagePolicy = lexxos::hardware::products::picoCalcLyraStoragePolicy;
	expect(owned.configure(storagePolicy), "owned service configuration failed");
	storagePolicy[1].readOnlyByDefault = true;
	expect(independent.configure(storagePolicy), "independent storagePolicy failed");
	expect(!owned.configure(storagePolicy), "one service changed another service's configuration");

	auto ownedHooks = lexxos::composition::products::lyraStorageMediaHooks(owned);
	auto independentHooks = lexxos::composition::products::lyraStorageMediaHooks(independent);
	expect(ownedHooks.context == &owned && independentHooks.context == &independent, "composition bound wrong service");
	filesystem.mounted = true;
	expect(independentHooks.mounted() && !ownedHooks.mounted(), "filesystem dependency leaked between services");
	expect(!independent.prepareManualMount(), "mounted filesystem permits policy transition");
	filesystem.mounted = false;
	expect(!independentHooks.mounted(), "hooks cached mounted state");
	transport.online = false;
	expect(!independent.prepareManualMount(), "manual mount ignored injected transport readiness");
	transport.online = true;
	expect(independent.prepareManualMount() && independent.backend().isReadOnly(), "RO policy lost with injected dependencies");
	std::array<std::uint8_t, 512> fakeBuffer{};
	expect(independent.backend().readBlocks(0, fakeBuffer.data(), 1) && fakeBuffer[0] == 42 && transport.readCalls == 1 && reads == 0, "read used global transport");
	expect(!independent.backend().readBlocks(4, fakeBuffer.data(), 1), "injected capacity ignored");
	expect(!independent.backend().dmaSupported() && independent.backend().activeClockHz() == 6000000, "capabilities used global transport");
	FakeTransport otherTransport;
	FakeFilesystem otherFilesystem;
	PicoSdStorageService other{otherTransport, otherFilesystem};
	expect(other.configure(lexxos::hardware::products::picoCalcLyraStoragePolicy) && other.prepareManualMount(), "RW service preparation failed");
	expect(!other.backend().isReadOnly(), "board default RW not applied");
	other.notifyManualUnmount();
	expect(other.prepareManualMount() && !other.backend().isReadOnly(), "manual remount lost default RW");
	otherTransport.writeOk = false;
	expect(!other.backend().writeBlocks(0, fakeBuffer.data(), 1) && other.backend().isReadOnly(), "injected write failure did not disarm service");
	expect(otherTransport.writeCalls == 1 && transport.writeCalls == 0 && writes == 0, "write reached wrong transport");
	expect(other.prepareManualMount() && other.backend().isReadOnly(), "failed write was rearmed by manual mount");
	transport.present = false;
	transport.invalidateRemoved();
	expect(!independent.backend().initialize() && transport.probeCalls == 1 && probes == 0, "probe reached global transport");
	expect(owned.backend().isReady(), "fake removal affected production adapter");

	{
		bool firstArmed = true, secondArmed = true;
		unsigned firstFailures = 0, secondFailures = 0;
		auto callback = [](void* context) {
			++*static_cast<unsigned*>(context);
		};
		lexxos::devices::rk3506::PicoSdBlockDevice first{nativeTransport, firstArmed, callback, &firstFailures};
		lexxos::devices::rk3506::PicoSdBlockDevice second{nativeTransport, secondArmed, callback, &secondFailures};
		std::array<std::uint8_t, 512> data{};
		writeSucceeds = false;
		expect(!first.writeBlocks(0, data.data(), 1), "write failure lost");
		expect(!firstArmed && secondArmed && firstFailures == 1 && secondFailures == 0, "write callback affected wrong owner");
		writeSucceeds = true;
		writes = 0;
	}

	bool armed = false;
	lexxos::devices::rk3506::PicoSdBlockDevice disk{nativeTransport, armed, failed};
	std::array<std::uint8_t, 1024> buffer{};
	expect(disk.initialize() && disk.isReadOnly(), "initial read-only state");
	expect(probes == 0, "ready media must not be reprobed");
	expect(disk.state() == lexxos::devices::SdCardState::Ready, "ready state");
	expect(disk.cardType() == lexxos::devices::SdCardType::SdV2BlockAddressed, "high capacity card");
	expect(disk.activeClockHz() == 12000000U, "transport clock exposed");
	expect(!disk.setTransferBaudRate(25000000U) && clockMHz == 12, "unsupported clock rejected");
	expect(disk.setTransferBaudRate(6000000U) && clockMHz == 6, "validated clock forwarded");
	expect(disk.dmaSupported() && disk.dmaTransfersEnabled(), "DMA capabilities exposed");
	expect(!disk.writeBlocks(0, buffer.data(), 1) && writes == 0, "RO must reject writes before transport");
	expect(!disk.readBlocks(16, buffer.data(), 1), "end of device");
	expect(!disk.readBlocks(15, buffer.data(), 2), "crossing device boundary");
	expect(!disk.readBlocks(std::numeric_limits<std::uint64_t>::max(), buffer.data(), 2), "overflowing LBA");
	expect(!disk.readBlocks(0, nullptr, 1) && !disk.readBlocks(0, buffer.data(), 0) && reads == 0, "invalid reads must not reach transport");
	expect(disk.readBlocks(15, buffer.data(), 1) && reads == 1, "last sector readable");
	armed = true;
	expect(!disk.writeBlocks(15, buffer.data(), 2) && writes == 0, "invalid write range");
	expect(disk.writeBlocks(0, buffer.data(), 1) && writes == 1, "armed write");
	writeSucceeds = false;
	expect(!disk.writeBlocks(0, buffer.data(), 1) && failures == 1 && !armed, "write failure disarms and suppresses automount");
	expect(!disk.writeBlocks(0, buffer.data(), 1) && writes == 2, "failed write cannot be retried");
	inserted = false;
	::ready = false;
	expect(!disk.initialize() && disk.blockCount() == 0 && !disk.readBlocks(0, buffer.data(), 1), "removed media is inaccessible");

	expect(probes == 1 && disk.state() == lexxos::devices::SdCardState::NoCard, "absent media probe fails");
	inserted = true;
	expect(disk.initialize() && probes == 2 && disk.isReady(), "reinserted media can be initialized");
	expect(disk.isReadOnly(), "probe alone must not rearm failed writes");

	lexxos::app::rk3506::PicoSdMountPolicy policy;
	expect(policy.prepareMount("sd0", "/mnt/sd") && preparations == 0, "boot media must remain untouched");
	expect(policy.prepareMount("sd1", "/other") && preparations == 0, "preserve unrelated target behavior");
	expect(policy.prepareMount("sd1", "/mnt/sd") && preparations == 1, "short device name");
	allowMount = false;
	expect(!policy.prepareMount("/dev/sd1", "/mnt/sd") && preparations == 2, "canonical device name and refusal");
	policy.mountFinished("sd0", "/mnt/sd", true);
	expect(finishes == 0, "boot completion untouched");
	policy.mountFinished("sd1", "/mnt/sd", false);
	expect(finishes == 1 && !lastSuccess, "failed mount propagated");
	policy.mountFinished("/dev/sd1", "/mnt/sd", true);
	expect(finishes == 2 && lastSuccess, "successful mount propagated");
	policy.unmounted("/other");
	policy.unmounted("/mnt/sd");
	expect(unmounts == 1, "only removable target suppresses automount");
	// Занятый SPI1 не должен получать I/O, настройку или аварийный сброс SD.
	auto& spiQueue = lexxos::composition::products::lyraSpi1Queue();
	expect(!spiQueue.busy(), "native operation leaked its lease");
	expect(spiQueue.acquire(lexxos::devices::DeviceResourcePriority::Normal, 0), "cannot occupy SPI1");
	auto const readsBefore = reads, writesBefore = writes, probesBefore = probes;
	auto const clockBefore = clockMHz;
	expect(!nativeTransport.read(0, buffer.data(), 1), "read bypassed busy SPI1");
	expect(!nativeTransport.write(0, buffer.data(), 1), "write bypassed busy SPI1");
	expect(!nativeTransport.probe() && !nativeTransport.setClockMHz(12), "configuration bypassed busy SPI1");
	nativeTransport.invalidateRemoved();
	expect(invalidations == 0 && !nativeTransport.ready() && nativeTransport.sectorCount() == 0, "removal did not defer reset or close access");
	expect(reads == readsBefore && writes == writesBefore && probes == probesBefore && clockMHz == clockBefore, "busy request reached hardware");
	expect(spiQueue.busy(), "failed lease released another owner");
	spiQueue.release();
	expect(nativeTransport.probe() && invalidations == 1 && nativeTransport.ready(), "deferred removal was not completed before probe");
	expect(!spiQueue.busy(), "probe did not release queue");
	nativeTransport.invalidateRemoved();
	expect(invalidations == 2 && !spiQueue.busy(), "idle removal did not reset immediately");
	using lexxos::devices::rk3506::PicoSdDiagnosticSession;
	expect(nativeTransport.probe(), "diagnostic fixture probe failed");
	auto const initialReads = reads;
	{
		lexxos::devices::BusTransaction held{spiQueue};
		PicoSdDiagnosticSession denied{spiQueue, nativeTransport};
		expect(!denied && !denied.read(0, buffer.data(), 1) && reads == initialReads, "busy diagnostics reached I/O");
	}
	expect(!spiQueue.busy(), "busy session released wrong owner");
	{
		lexxos::devices::BusTransactionQueue wrongQueue;
		PicoSdDiagnosticSession wrong{wrongQueue, nativeTransport};
		expect(!wrong && !wrong.read(0, buffer.data(), 1) && reads == initialReads, "wrong queue accepted by native transport");
	}
	{
		PicoSdDiagnosticSession session{spiQueue, nativeTransport};
		expect(bool(session) && spiQueue.busy(), "diagnostic session did not acquire SPI1");
		expect(session.read(0, buffer.data(), 1), "diagnostic read recursively acquired queue");
		auto const validReads = reads;
		expect(!session.read(16, buffer.data(), 1) && !session.read(15, buffer.data(), 2) && !session.read(UINT64_MAX, buffer.data(), 1) && !session.read(0, nullptr, 1) && !session.read(0, buffer.data(), 0) && reads == validReads, "diagnostic range validation bypassed");
		readSucceeds = false;
		expect(!session.read(0, buffer.data(), 1) && spiQueue.busy(), "failed read released session before restore");
		readSucceeds = true;
		expect(!nativeTransport.setClockMHz(12), "second owner entered diagnostic session");
	}
	expect(!spiQueue.busy(), "completed diagnostics leaked queue");
	// A second adapter must dispatch through its supplied session, including diagnostics.
	{
		FakeSdProtocolIo secondIo;
		lexxos::platform::rk3506::SdProtocol secondProtocol{secondIo};
		lexxos::devices::rk3506::NativePicoSdTransport secondNative{spiQueue, secondProtocol};
		expect(secondNative.ready() && lastProtocol == &secondProtocol, "readiness ignored injected protocol");
		expect(secondNative.configured() && lastProtocol == &secondProtocol, "status ignored injected protocol");
		PicoSdDiagnosticSession session{spiQueue, secondNative};
		expect(bool(session) && session.read(0, buffer.data(), 1) && lastProtocol == &secondProtocol, "diagnostics used another protocol");
		secondNative.invalidateRemoved();
		expect(!secondNative.ready() && nativeTransport.ready(), "pending removal leaked between adapters");
	}

	expect(nativeTransport.ready() && lastProtocol == &lexxos::composition::products::lyraSdProtocol(), "adapter replaced composition protocol");
	auto cancelled = [&] {
		PicoSdDiagnosticSession session{spiQueue, nativeTransport};
		expect(bool(session), "cancel fixture could not acquire queue");
		struct Restore {
			lexxos::devices::BusTransactionQueue& queue;
			~Restore() { expect(queue.busy(), "queue released before restore"); }
		} restore{spiQueue};
		return false;
	};
	expect(!cancelled() && !spiQueue.busy(), "early exit leaked diagnostic session");
}
