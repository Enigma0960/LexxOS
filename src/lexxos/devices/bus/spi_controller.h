#pragma once

#include <memory>

#include <lexxos/devices/bus/spi_bus.h>

namespace lexxos::devices {

// Контроллер и его очередь живут дольше всех созданных им устройств.
// Инициализация и создание устройств выполняются до запуска рабочих задач.
class SpiController {
public:
	virtual ~SpiController() = default;
	virtual bool initialize(platform::SpiInterfacePins const& pins) = 0;
	virtual std::unique_ptr<SpiBus> createDevice(platform::SpiDeviceConfig const& config) = 0;
};

} // namespace lexxos::devices
