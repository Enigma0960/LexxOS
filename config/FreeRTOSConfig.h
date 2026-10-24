#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

// #if defined(LEXXOS_PLATFORM_PICO)
// 	#include <pico.h>
// #endif

#define configUSE_PREEMPTION 1
#define configUSE_TICKLESS_IDLE 0
#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configTICK_RATE_HZ ((TickType_t) 1000)
#define configMAX_PRIORITIES 8
#define configMINIMAL_STACK_SIZE ((configSTACK_DEPTH_TYPE) 512)
#define configUSE_16_BIT_TICKS 0
#define configIDLE_SHOULD_YIELD 1
#define configUSE_TIME_SLICING 1

#define configUSE_MUTEXES 1
#define configUSE_RECURSIVE_MUTEXES 1
#define configUSE_COUNTING_SEMAPHORES 1
#define configUSE_QUEUE_SETS 1
#define configQUEUE_REGISTRY_SIZE 8
#define configUSE_TASK_NOTIFICATIONS 1
#define configUSE_APPLICATION_TASK_TAG 0
#define configUSE_NEWLIB_REENTRANT 0
#define configENABLE_BACKWARD_COMPATIBILITY 1
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 5

#define configSTACK_DEPTH_TYPE uint32_t
#define configMESSAGE_BUFFER_LENGTH_TYPE size_t

#ifndef configSUPPORT_STATIC_ALLOCATION
	#define configSUPPORT_STATIC_ALLOCATION 0
#endif

#ifndef configSUPPORT_DYNAMIC_ALLOCATION
	#define configSUPPORT_DYNAMIC_ALLOCATION 1
#endif

#if defined(LEXXOS_PLATFORM_STM32F4)
	#define configCPU_CLOCK_HZ ((uint32_t) 168000000)
	#define configSYSTICK_CLOCK_HZ configCPU_CLOCK_HZ
	#define configPRIO_BITS 4
	#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15
	#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
	#define configKERNEL_INTERRUPT_PRIORITY (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
	#define configMAX_SYSCALL_INTERRUPT_PRIORITY (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
	#define configTOTAL_HEAP_SIZE (64 * 1024)
	#define xPortPendSVHandler PendSV_Handler
	#define vPortSVCHandler SVC_Handler
	#define xPortSysTickHandler SysTick_Handler
#elif defined(LEXXOS_PLATFORM_PICO)
    // Pico/PicoCalc runtime allocates task stacks, foreground program arenas and
    // large optional workspaces from the FreeRTOS heap. RP2350 has substantially
    // more SRAM than RP2040, so keep the RP2040 default conservative but give
    // Pico 2 W enough headroom for the Game Boy workspace after display, Wi-Fi,
    // SD/FAT and gamepad services are enabled.
    //
    // CMake sets the first-generation PicoCalc Pico W profile to 88 KiB so
    // RP2040 can keep Wi-Fi, display, SD/FAT and the shell linked together.
    // The value can still be overridden from CMake, for example:
    //   -DLEXXOS_PICO_FREERTOS_HEAP_KB=88
    //   -DLEXXOS_PICO_FREERTOS_HEAP_KB=192
    //   -DLEXXOS_PICO_FREERTOS_HEAP_KB=224
	#if defined(LEXXOS_PICO_FREERTOS_HEAP_KB) && LEXXOS_PICO_FREERTOS_HEAP_KB > 0
		#define configTOTAL_HEAP_SIZE (LEXXOS_PICO_FREERTOS_HEAP_KB * 1024)
	#elif defined(PICO_RP2350) && PICO_RP2350
		#define configTOTAL_HEAP_SIZE (224 * 1024)
	#else
		#define configTOTAL_HEAP_SIZE (126 * 1024)
	#endif
#else
	#define configTOTAL_HEAP_SIZE (128 * 1024)
#endif

#if defined(LEXXOS_PLATFORM_STM32F4)
	#define configAPPLICATION_ALLOCATED_HEAP 1
#else
	#define configAPPLICATION_ALLOCATED_HEAP 0
#endif

#define configCHECK_FOR_STACK_OVERFLOW 2
#define configUSE_MALLOC_FAILED_HOOK 1
#define configUSE_DAEMON_TASK_STARTUP_HOOK 0

#define configGENERATE_RUN_TIME_STATS 0
#define configUSE_TRACE_FACILITY 1
#define configUSE_STATS_FORMATTING_FUNCTIONS 0

#define configUSE_CO_ROUTINES 0
#define configMAX_CO_ROUTINE_PRIORITIES 1

#define configUSE_TIMERS 1
#define configTIMER_TASK_PRIORITY (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH 10
#define configTIMER_TASK_STACK_DEPTH 1024

#if defined(FREE_RTOS_KERNEL_SMP) && FREE_RTOS_KERNEL_SMP
	#ifndef configNUMBER_OF_CORES
		#if defined(LEXXOS_PICO_FREERTOS_CORES)
			#define configNUMBER_OF_CORES LEXXOS_PICO_FREERTOS_CORES
		#else
			#define configNUMBER_OF_CORES 1
		#endif
	#endif
	#define configNUM_CORES configNUMBER_OF_CORES
	#define configTICK_CORE 0
	#define configRUN_MULTIPLE_PRIORITIES 1
	#if configNUMBER_OF_CORES > 1
		#define configUSE_CORE_AFFINITY 1
		#define configTASK_DEFAULT_CORE_AFFINITY 1U
		#define configTIMER_SERVICE_TASK_CORE_AFFINITY 1U
		#define configIDLE_AFFINITY 1
	#endif
	#define configUSE_PASSIVE_IDLE_HOOK 0
#endif

#if defined(LEXXOS_PLATFORM_PICO)
	#if defined(PICO_RP2350) && PICO_RP2350
		#define configSUPPORT_PICO_SYNC_INTEROP 0
		#define configSUPPORT_PICO_TIME_INTEROP 0
	#else
		#define configSUPPORT_PICO_SYNC_INTEROP 1
		#define configSUPPORT_PICO_TIME_INTEROP 1
	#endif
#endif

#define configASSERT(x) assert(x)

#define INCLUDE_vTaskPrioritySet 1
#define INCLUDE_uxTaskPriorityGet 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_vTaskDelayUntil 1
#define INCLUDE_vTaskDelay 1
#define INCLUDE_xTaskGetSchedulerState 1
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_xTaskGetIdleTaskHandle 1
#define INCLUDE_eTaskGetState 1
#define INCLUDE_xTimerPendFunctionCall 1
#define INCLUDE_xTaskAbortDelay 1
#define INCLUDE_xTaskGetHandle 1
#define INCLUDE_xTaskResumeFromISR 1
#define INCLUDE_xQueueGetMutexHolder 1

#if defined(PICO_RP2350) && PICO_RP2350
	#ifndef configCPU_CLOCK_HZ
	/*
		* Keep FreeRTOSConfig.h free from hardware/clocks.h. This file is also
		* pulled into Pico SDK sources through PICO_CONFIG_RTOS_ADAPTER_HEADER
		* while pico.h is still being assembled, so including heavy hardware
		* headers here breaks Pico SDK inline/platform definitions.
		*
		* Pico 2 W uses RP2350's default 150 MHz system clock unless the board
		* profile overrides it before the scheduler starts.
		*/
		#if defined(LEXXOS_PICO_SYS_CLOCK_KHZ) && LEXXOS_PICO_SYS_CLOCK_KHZ > 0
			#define configCPU_CLOCK_HZ ((uint32_t) (LEXXOS_PICO_SYS_CLOCK_KHZ * 1000UL))
		#else
			#define configCPU_CLOCK_HZ ((uint32_t) 150000000UL)
		#endif
	#endif

	#if !(defined(FREE_RTOS_KERNEL_SMP) && FREE_RTOS_KERNEL_SMP)
		/*
		* The Cortex-M33 NTZ FreeRTOS port clears CLKSOURCE when
		* configSYSTICK_CLOCK_HZ is defined. RP2350 feeds that external SysTick
		* input from the TICKS_PROC0/TICKS_PROC1 generators, which Pico SDK starts
		* as a 1 MHz source.
		*/
		#define configSYSTICK_CLOCK_HZ ((uint32_t) 1000000UL)
	#endif
	#ifndef configPRIO_BITS
		#if defined(__NVIC_PRIO_BITS)
			#define configPRIO_BITS __NVIC_PRIO_BITS
		#else
			#define configPRIO_BITS 2
		#endif
	#endif
	#ifndef configLIBRARY_LOWEST_INTERRUPT_PRIORITY
		#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY ((1U << configPRIO_BITS) - 1U)
	#endif
	#ifndef configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
		#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 1U
	#endif
	#ifndef configKERNEL_INTERRUPT_PRIORITY
		#define configKERNEL_INTERRUPT_PRIORITY (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))
	#endif
	#ifndef configMAX_SYSCALL_INTERRUPT_PRIORITY
		#define configMAX_SYSCALL_INTERRUPT_PRIORITY (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))
	#endif
	#ifndef configENABLE_MPU
		#define configENABLE_MPU 0
	#endif
	#ifndef configENABLE_TRUSTZONE
		#define configENABLE_TRUSTZONE 0
	#endif
	#ifndef configRUN_FREERTOS_SECURE_ONLY
		#define configRUN_FREERTOS_SECURE_ONLY 1
	#endif
	#ifndef configENABLE_FPU
		#define configENABLE_FPU 1
	#endif
	/*
	 * The Pico SDK's double implementation sets LIB_PICO_DOUBLE_PICO, which the
	 * RP2350 port otherwise treats as permission to access coprocessor p4. That
	 * coprocessor raises NOCP on the PicoCalc target during PendSV.
	 */
	#ifndef configUSE_DCP_SAVE_RESTORE
		#define configUSE_DCP_SAVE_RESTORE 0
	#endif
	#ifndef configENABLE_PAC
		#define configENABLE_PAC 0
	#endif
	#ifndef configENABLE_BTI
		#define configENABLE_BTI 0
	#endif
#endif
