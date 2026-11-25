#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

#include <lexxos/composition/products/lyra_device_policy.h>
#include <lexxos/hardware/legacy/lyra_board.h>

namespace {

namespace products = lexxos::hardware::products;
namespace legacy = lexxos::hardware::legacy;

void expect(bool value, char const* message) {
	if (!value) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

void testResolvedEndpoints() {
	namespace lyra = lexxos::hardware::modules::lyra;
	constexpr auto resolved = products::resolveLyraEndpoints(products::picoCalcLyraProduct);
	static_assert(resolved.has_value());
	static_assert(resolved->console.id.index == 0);
	static_assert(resolved->southbridge->route.id.index == 2);
	static_assert(resolved->southbridge->frequencyHz == 100000);
	static_assert(resolved->display->spi.route.clock == lyra::Pin{0, 7});
	static_assert(resolved->display->spi.route.mosi == lyra::Pin{0, 6});
	static_assert(!resolved->display->spi.route.miso);
	static_assert(resolved->display->spi.cs == lyra::Pin{0, 4});
	static_assert(resolved->display->dc == lyra::Pin{0, 3});
	static_assert(resolved->display->reset == lyra::Pin{0, 2});
	static_assert(resolved->removableStorage->route.id.index == 1);
	static_assert(resolved->removableStorage->route.miso == lyra::Pin{1, 27});
	static_assert(resolved->removableStorage->cs == lyra::Pin{1, 26});

	auto standalone = products::lyraStandaloneProduct;
	standalone.console = {1};
	auto const bare = products::resolveLyraEndpoints(standalone);
	expect(bare && bare->console.id.index == 1 && !bare->southbridge && !bare->display && !bare->removableStorage, "standalone invented carrier endpoints");

	// Resolution owns its route values, even when declarations borrow local data.
	auto routes = lyra::spiInterfaces;
	auto wiring = products::picoCalcLyraWiring;
	auto product = products::picoCalcLyraProduct;
	product.available.spis = routes;
	product.picoCalcWiring = &wiring;
	auto const snapshot = products::resolveLyraEndpoints(product);
	routes[0].clock = {1, 0};
	wiring.displayCs.number = 9;
	expect(snapshot && snapshot->display->spi.route.clock == lyra::Pin{0, 7} && snapshot->display->spi.cs == lyra::Pin{0, 4}, "resolved endpoint borrowed mutable declarations");
	expect(!products::resolveLyraEndpoints(product), "invalid routes emitted partial endpoints");
	product = products::picoCalcLyraProduct;
	product.console = {1};
	expect(!products::resolveLyraEndpoints(product), "conflicting product emitted endpoints");
	product = products::picoCalcLyraProduct;
	product.available.i2cs = {};
	expect(!products::resolveLyraEndpoints(product), "missing route emitted endpoints");
}

void testRuntimePolicy() {
	using lexxos::composition::products::applyLyraDevicePolicy;
	constexpr auto physical = *products::resolveLyraEndpoints(products::picoCalcLyraProduct);
	constexpr auto factory = applyLyraDevicePolicy(physical, false);
	constexpr auto jcs = applyLyraDevicePolicy(physical, true);
	static_assert(physical.southbridge->frequencyHz == 100000);
	static_assert(factory.southbridge->frequencyHz == 100000);
	static_assert(jcs.southbridge->frequencyHz == 10000);
	static_assert(jcs.southbridge->route == physical.southbridge->route);
	constexpr auto pins = legacy::lyraI2cPins(*jcs.southbridge);
	static_assert(pins.index == 2 && pins.sda.number == 11 && pins.scl.number == 10);
	static_assert(pins.baudRate == 10000);
	static_assert(legacy::lyraI2cPins(*factory.southbridge).baudRate == 100000);
	constexpr auto standalone = applyLyraDevicePolicy(
	    *products::resolveLyraEndpoints(products::lyraStandaloneProduct), true);
	static_assert(!standalone.southbridge && !standalone.display);
	expect(jcs.display->spi.route == physical.display->spi.route
	        && jcs.removableStorage->route == physical.removableStorage->route,
	    "JCS policy changed unrelated physical routes");
}

void testInterfaces() {
	namespace lyra = lexxos::hardware::modules::lyra;
	using products::LyraProductError;
	static_assert(lyra::interfaces.uarts.size() == 2);
	static_assert(lyra::interfaces.spis.size() == 2);
	static_assert(lyra::interfaces.i2cs.size() == 1);
	static_assert(lyra::findInterface(lyra::interfaces.spis, lyra::SpiId{1})->miso.has_value());
	static_assert(!lyra::findInterface(lyra::interfaces.spis, lyra::SpiId{0})->miso.has_value());
	auto product = products::picoCalcLyraProduct;
	product.console = {1};
	expect(products::validateLyraProduct(product) == LyraProductError::InterfaceConflict,
	    "UART1 and I2C2 simultaneously assigned shared pads");
	expect(!legacy::tryLyraBoard(product, true), "legacy ignored route conflict");
	product = products::lyraStandaloneProduct;
	product.console = {1};
	expect(products::validateLyraProduct(product) == LyraProductError::None,
	    "unused I2C2 incorrectly blocks UART1");
	expect(legacy::tryLyraBoard(product, false)->interfaces.consoleUart.index == 1,
	    "console did not resolve selected UART");
	product.console = {9};
	expect(products::validateLyraProduct(product) == LyraProductError::UnknownConsole, "unknown UART accepted");
	product = products::picoCalcLyraProduct;
	product.available.spis = lyra::interfaces.spis.first(1);
	expect(products::validateLyraProduct(product) == LyraProductError::InvalidWiring,
	    "missing physical SD interface accepted");
	product = products::picoCalcLyraProduct;
	product.available.i2cs = {};
	expect(products::validateLyraProduct(product) == LyraProductError::InvalidWiring, "missing I2C accepted");
	auto duplicates = lyra::uartInterfaces;
	duplicates[1].id = duplicates[0].id;
	product = products::lyraStandaloneProduct;
	product.available.uarts = duplicates;
	expect(products::validateLyraProduct(product) == LyraProductError::InvalidInterfaces, "duplicate controller ID accepted");
	duplicates = lyra::uartInterfaces;
	duplicates[0].pads[0] = {1, 9};
	expect(products::validateLyraProduct(product) == LyraProductError::InvalidInterfaces, "invented physical route accepted");
	auto wiring = products::picoCalcLyraWiring;
	wiring.storageBus = {0};
	expect(products::validatePicoCalcLyraWiring(wiring) == products::LyraWiringError::MissingReceiveSignal,
	    "SD assigned transmit-only SPI");
	wiring = products::picoCalcLyraWiring;
	wiring.displayBus = {1};
	expect(products::validatePicoCalcLyraWiring(wiring) == products::LyraWiringError::InvalidOrConflictingPin,
	    "two exclusive endpoints claimed the same SPI route");
}

void testStandalone() {
	constexpr auto board = legacy::lyraStandaloneBoard();
	static_assert(board.storageSlotCount == 1);
	static_assert(board.interfaces.systemI2c.sda.number == -1);
	static_assert(!board.spiTftDisplay.enabled);
	expect(std::string_view{board.boardId} == "lyra_standalone", "standalone identity");
	expect(board.interfaces.consoleUart.index == 0 && board.interfaces.consoleUart.baudRate == 1500000,
	    "standalone boot console");
	expect(board.storageSlots[0].readOnlyByDefault && !board.storageSlots[0].autoMount && board.storageSlots[0].mountPath == nullptr, "standalone system SD protection");
	expect(lexxos::platform::findBoardStorageSlot(board, "sd1") == nullptr, "standalone has phantom sd1");
	expect(!board.interfaces.systemI2c.isValid() && !board.spiTftDisplay.isValid(), "phantom PicoCalc peripherals");
	auto const forcedDisplay = legacy::tryLyraBoard(products::lyraStandaloneProduct, true);
	expect(forcedDisplay && !forcedDisplay->spiTftDisplay.enabled, "feature flag invented a carrier");
}

void testWiring() {
	using products::LyraPinDomain;
	using products::LyraWiringError;
	auto wiring = products::picoCalcLyraWiring;
	wiring.displayDc = {LyraPinDomain::Gpio0, 6};
	expect(!products::hasValidDistinctPins(wiring), "GPIO/RM_IO alias conflict accepted");
	wiring = products::picoCalcLyraWiring;
	wiring.displayDc = {LyraPinDomain::RmIo, 11};
	expect(!products::hasValidDistinctPins(wiring), "I2C alias conflict accepted");
	wiring = products::picoCalcLyraWiring;
	wiring.displayDc = {LyraPinDomain::RmIo, 8};
	expect(products::validatePicoCalcLyraWiring(wiring) == LyraWiringError::InvalidOrConflictingPin,
	    "unknown mapping guessed");
	wiring = products::picoCalcLyraWiring;
	wiring.displayBus = {9};
	expect(products::validatePicoCalcLyraWiring(wiring) == LyraWiringError::UnsupportedController, "wrong SPI accepted");
	wiring = products::picoCalcLyraWiring;
	wiring.southbridgeBus = {0};
	expect(products::validatePicoCalcLyraWiring(wiring) == LyraWiringError::UnsupportedController, "wrong I2C accepted");
	wiring = products::picoCalcLyraWiring;
	wiring.southbridgeI2cHz = 400000;
	expect(products::validatePicoCalcLyraWiring(wiring) == LyraWiringError::UnsupportedFrequency, "unsupported clock accepted");
	wiring = products::picoCalcLyraWiring;
	wiring.displayReset.number = 5;
	expect(products::hasValidDistinctPins(wiring), "distinct pins rejected");
	expect(products::validatePicoCalcLyraWiring(wiring) == LyraWiringError::UnsupportedPinmux, "unimplemented route accepted");
	auto product = products::picoCalcLyraProduct;
	product.picoCalcWiring = &wiring;
	expect(!legacy::tryLyraBoard(product, true), "adapter accepted invalid wiring");
}

void testStorage() {
	using products::StoragePolicyError;
	auto slots = products::picoCalcLyraStoragePolicy;
	slots[0].readOnlyByDefault = false;
	expect(products::validateStoragePolicy(slots) == StoragePolicyError::UnsafeSystemMedia, "writable boot SD accepted");
	slots = products::picoCalcLyraStoragePolicy;
	slots[0].autoMount = true;
	slots[0].mountPath = "/system";
	expect(products::validateStoragePolicy(slots) == StoragePolicyError::UnsafeSystemMedia, "boot automount accepted");
	slots = products::picoCalcLyraStoragePolicy;
	slots[1].deviceName = "sd0";
	expect(products::validateStoragePolicy(slots) == StoragePolicyError::DuplicateName, "duplicate device accepted");
	slots[1].deviceName = nullptr;
	expect(products::validateStoragePolicy(slots) == StoragePolicyError::InvalidName, "null device accepted");
	slots[1].deviceName = "bad/name";
	expect(products::validateStoragePolicy(slots) == StoragePolicyError::InvalidName, "path as device name accepted");
	slots = products::picoCalcLyraStoragePolicy;
	slots[1].mountPath = nullptr;
	expect(products::validateStoragePolicy(slots) == StoragePolicyError::InvalidMountPath, "automount without path accepted");
	slots[1].mountPath = "relative";
	expect(products::validateStoragePolicy(slots) == StoragePolicyError::InvalidMountPath, "relative mount accepted");
	slots = products::picoCalcLyraStoragePolicy;
	slots[1].role = static_cast<products::StorageRole>(255);
	expect(products::validateStoragePolicy(slots) == StoragePolicyError::InvalidRole, "invalid role accepted");
	std::array duplicates{products::picoCalcLyraStoragePolicy[1], products::picoCalcLyraStoragePolicy[1]};
	duplicates[1].deviceName = "sd2";
	expect(products::validateStoragePolicy(duplicates) == StoragePolicyError::DuplicateMountPath, "duplicate mount accepted");
}

void testComposition() {
	using products::LyraProductError;
	auto product = products::lyraStandaloneProduct;
	product.picoCalcWiring = &products::picoCalcLyraWiring;
	expect(products::validateLyraProduct(product) == LyraProductError::CarrierWiringMismatch, "orphan wiring accepted");
	product = products::picoCalcLyraProduct;
	product.picoCalcWiring = nullptr;
	expect(products::validateLyraProduct(product) == LyraProductError::CarrierWiringMismatch, "missing wiring accepted");
	product = products::lyraStandaloneProduct;
	product.storage = products::picoCalcLyraStoragePolicy;
	expect(products::validateLyraProduct(product) == LyraProductError::StorageCompositionMismatch, "orphan removable SD accepted");
	expect(!legacy::tryLyraBoard(product, false), "adapter emitted partial invalid product");
	product.storage = {};
	expect(products::validateLyraProduct(product) == LyraProductError::StorageCompositionMismatch, "missing system SD accepted");
	product = products::lyraStandaloneProduct;
	product.carrier = static_cast<products::LyraCarrier>(255);
	expect(products::validateLyraProduct(product) == LyraProductError::InvalidCarrier, "unknown carrier accepted");
	product = products::lyraStandaloneProduct;
	product.id = nullptr;
	expect(products::validateLyraProduct(product) == LyraProductError::InvalidIdentity, "null identity accepted");
	auto slots = products::lyraStandaloneStoragePolicy;
	slots[0].readOnlyByDefault = false;
	product = products::lyraStandaloneProduct;
	product.storage = slots;
	expect(products::validateLyraProduct(product) == LyraProductError::InvalidStoragePolicy, "invalid storage bypassed");
	expect(!legacy::tryLyraBoard(product, false), "adapter accepted writable system SD");
}

} // namespace

int main() {
	testResolvedEndpoints();
	testRuntimePolicy();
	testInterfaces();
	testStandalone();
	testWiring();
	testStorage();
	testComposition();
}
