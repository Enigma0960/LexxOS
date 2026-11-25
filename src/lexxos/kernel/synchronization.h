#pragma once

#include <cstdint>
#include <limits>

#include <lexxos/kernel/event.h>
#include <lexxos/kernel/mutex.h>
#include <lexxos/kernel/semaphore.h>

namespace lexxos::kernel {

// RTOS: блокирующие операции выполняются в контексте задачи.
// Cooperative CE: операции захвата возвращают результат сразу, независимо от timeout.
// При false обработчик повторяет попытку на следующем шаге цикла.
// Used only for the short completion handshake after a wakeup; never for
// steady-state bus arbitration.
void synchronizationBackoff() noexcept;

// Auto-reset binary event: one signal wakes at most one waiter; an early
// signal remains pending. A duplicate signal returns false.
// Queue users must still check the protected condition.

// Non-recursive mutex with priority inheritance on FreeRTOS.

} // namespace lexxos::kernel
