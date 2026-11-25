#pragma once

namespace lexxos::platform {

struct PlatformServices;

} // namespace lexxos::platform

namespace lexxos::devices {

class DeviceManager;

} // namespace lexxos::devices

namespace lexxos::core {

class CoreSystem;
using DeviceRegistrar = void (*)(lexxos::devices::DeviceManager&, lexxos::platform::PlatformServices const&);

// Один раз при старте, до создания задач. Привязка планировщика глобальна для процесса.
// Standard использует этот шаг перед собственной инициализацией VFS и реестра.
void initializePlatform(lexxos::platform::PlatformServices const& platform);

// Минимальный запуск CE: платформа, реестр, устройства, capabilities. Рабочие задачи создаёт приложение.
bool bootstrap(CoreSystem& system, DeviceRegistrar registerDevices = nullptr);

} // namespace lexxos::core
