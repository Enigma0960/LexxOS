#include <lexxos/devices/storage/sd_spi_transport.h>

namespace lexxos::devices {
SdSpiTransport::SdSpiTransport(SpiBus& bus, lexxos::platform::SpiPins pins)
    : _bus(bus)
    , _pins(pins) {
}

lexxos::platform::SpiPins const& SdSpiTransport::pins() const {
	return _pins;
}

std::uint32_t SdSpiTransport::configuredBaudRate() const {
	return _pins.baudRate;
}

bool SdSpiTransport::initialize() {
	return _pins.isValid() && _bus.initialize(_pins);
}

bool SdSpiTransport::isReady() const {
	return _bus.isReady();
}

bool SdSpiTransport::setBaudRate(std::uint32_t baudRate) {
	return _bus.setBaudRate(baudRate);
}

bool SdSpiTransport::setClockMode(SpiClockMode mode) {
	return _bus.setClockMode(mode);
}

SpiClockMode SdSpiTransport::clockMode() const {
	return _bus.clockMode();
}

void SdSpiTransport::select() {
	_bus.select();
}

void SdSpiTransport::deselect() {
	_bus.deselect();
}

bool SdSpiTransport::write(std::uint8_t const* data, std::size_t size) {
	return _bus.write(data, size);
}

bool SdSpiTransport::read(std::uint8_t fill, std::uint8_t* data, std::size_t size) {
	return _bus.read(fill, data, size);
}

bool SdSpiTransport::supportsDma() const {
	return _bus.supportsDma();
}

bool SdSpiTransport::startDmaTransfer(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size) {
	return _bus.startDmaTransfer(tx, rx, size);
}

bool SdSpiTransport::waitDmaTransfer(std::uint32_t timeoutMs) {
	return _bus.waitDmaTransfer(timeoutMs);
}

} // namespace lexxos::devices
