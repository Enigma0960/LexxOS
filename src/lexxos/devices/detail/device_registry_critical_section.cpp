#include <lexxos/devices/detail/device_registry_critical_section.h>

#if defined(LEXXOS_RK3506_FREERTOS_PERIPHERALS)
	#include <FreeRTOS.h>
	#include <task.h>
#endif

namespace lexxos::devices::detail {

DeviceRegistryCriticalSection::DeviceRegistryCriticalSection() {
#if defined(LEXXOS_RK3506_FREERTOS_PERIPHERALS)
	taskENTER_CRITICAL();
#endif
}

DeviceRegistryCriticalSection::~DeviceRegistryCriticalSection() {
#if defined(LEXXOS_RK3506_FREERTOS_PERIPHERALS)
	taskEXIT_CRITICAL();
#endif
}

} // namespace lexxos::devices::detail
