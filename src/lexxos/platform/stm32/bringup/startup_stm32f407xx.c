#include <stdint.h>

extern uint32_t _estack;
extern uint32_t _etext;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;
extern uint32_t _sccmram;
extern uint32_t _eccmram;

extern void SystemInit(void);
extern int main(void);
extern void __libc_init_array(void);

void Reset_Handler(void);
void Default_Handler(void);

void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void) __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void) __attribute__((weak, alias("Default_Handler")));

#define WEAK_IRQ(name) void name(void) __attribute__((weak, alias("Default_Handler")))

WEAK_IRQ(WWDG_IRQHandler);
WEAK_IRQ(PVD_IRQHandler);
WEAK_IRQ(TAMP_STAMP_IRQHandler);
WEAK_IRQ(RTC_WKUP_IRQHandler);
WEAK_IRQ(FLASH_IRQHandler);
WEAK_IRQ(RCC_IRQHandler);
WEAK_IRQ(EXTI0_IRQHandler);
WEAK_IRQ(EXTI1_IRQHandler);
WEAK_IRQ(EXTI2_IRQHandler);
WEAK_IRQ(EXTI3_IRQHandler);
WEAK_IRQ(EXTI4_IRQHandler);
WEAK_IRQ(DMA1_Stream0_IRQHandler);
WEAK_IRQ(DMA1_Stream1_IRQHandler);
WEAK_IRQ(DMA1_Stream2_IRQHandler);
WEAK_IRQ(DMA1_Stream3_IRQHandler);
WEAK_IRQ(DMA1_Stream4_IRQHandler);
WEAK_IRQ(DMA1_Stream5_IRQHandler);
WEAK_IRQ(DMA1_Stream6_IRQHandler);
WEAK_IRQ(ADC_IRQHandler);
WEAK_IRQ(CAN1_TX_IRQHandler);
WEAK_IRQ(CAN1_RX0_IRQHandler);
WEAK_IRQ(CAN1_RX1_IRQHandler);
WEAK_IRQ(CAN1_SCE_IRQHandler);
WEAK_IRQ(EXTI9_5_IRQHandler);
WEAK_IRQ(TIM1_BRK_TIM9_IRQHandler);
WEAK_IRQ(TIM1_UP_TIM10_IRQHandler);
WEAK_IRQ(TIM1_TRG_COM_TIM11_IRQHandler);
WEAK_IRQ(TIM1_CC_IRQHandler);
WEAK_IRQ(TIM2_IRQHandler);
WEAK_IRQ(TIM3_IRQHandler);
WEAK_IRQ(TIM4_IRQHandler);
WEAK_IRQ(I2C1_EV_IRQHandler);
WEAK_IRQ(I2C1_ER_IRQHandler);
WEAK_IRQ(I2C2_EV_IRQHandler);
WEAK_IRQ(I2C2_ER_IRQHandler);
WEAK_IRQ(SPI1_IRQHandler);
WEAK_IRQ(SPI2_IRQHandler);
WEAK_IRQ(USART1_IRQHandler);
WEAK_IRQ(USART2_IRQHandler);
WEAK_IRQ(USART3_IRQHandler);
WEAK_IRQ(EXTI15_10_IRQHandler);
WEAK_IRQ(RTC_Alarm_IRQHandler);
WEAK_IRQ(OTG_FS_WKUP_IRQHandler);
WEAK_IRQ(TIM8_BRK_TIM12_IRQHandler);
WEAK_IRQ(TIM8_UP_TIM13_IRQHandler);
WEAK_IRQ(TIM8_TRG_COM_TIM14_IRQHandler);
WEAK_IRQ(TIM8_CC_IRQHandler);
WEAK_IRQ(DMA1_Stream7_IRQHandler);
WEAK_IRQ(FSMC_IRQHandler);
WEAK_IRQ(SDIO_IRQHandler);
WEAK_IRQ(TIM5_IRQHandler);
WEAK_IRQ(SPI3_IRQHandler);
WEAK_IRQ(UART4_IRQHandler);
WEAK_IRQ(UART5_IRQHandler);
WEAK_IRQ(TIM6_DAC_IRQHandler);
WEAK_IRQ(TIM7_IRQHandler);
WEAK_IRQ(DMA2_Stream0_IRQHandler);
WEAK_IRQ(DMA2_Stream1_IRQHandler);
WEAK_IRQ(DMA2_Stream2_IRQHandler);
WEAK_IRQ(DMA2_Stream3_IRQHandler);
WEAK_IRQ(DMA2_Stream4_IRQHandler);
WEAK_IRQ(ETH_IRQHandler);
WEAK_IRQ(ETH_WKUP_IRQHandler);
WEAK_IRQ(CAN2_TX_IRQHandler);
WEAK_IRQ(CAN2_RX0_IRQHandler);
WEAK_IRQ(CAN2_RX1_IRQHandler);
WEAK_IRQ(CAN2_SCE_IRQHandler);
WEAK_IRQ(OTG_FS_IRQHandler);
WEAK_IRQ(DMA2_Stream5_IRQHandler);
WEAK_IRQ(DMA2_Stream6_IRQHandler);
WEAK_IRQ(DMA2_Stream7_IRQHandler);
WEAK_IRQ(USART6_IRQHandler);
WEAK_IRQ(I2C3_EV_IRQHandler);
WEAK_IRQ(I2C3_ER_IRQHandler);
WEAK_IRQ(OTG_HS_EP1_OUT_IRQHandler);
WEAK_IRQ(OTG_HS_EP1_IN_IRQHandler);
WEAK_IRQ(OTG_HS_WKUP_IRQHandler);
WEAK_IRQ(OTG_HS_IRQHandler);
WEAK_IRQ(DCMI_IRQHandler);
WEAK_IRQ(CRYP_IRQHandler);
WEAK_IRQ(HASH_RNG_IRQHandler);
WEAK_IRQ(FPU_IRQHandler);

__attribute__((used, section(".isr_vector"))) void (*const g_vector_table[])(void) = {
    (void (*)(void)) &_estack,
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    0,
    0,
    0,
    0,
    SVC_Handler,
    DebugMon_Handler,
    0,
    PendSV_Handler,
    SysTick_Handler,
    WWDG_IRQHandler,
    PVD_IRQHandler,
    TAMP_STAMP_IRQHandler,
    RTC_WKUP_IRQHandler,
    FLASH_IRQHandler,
    RCC_IRQHandler,
    EXTI0_IRQHandler,
    EXTI1_IRQHandler,
    EXTI2_IRQHandler,
    EXTI3_IRQHandler,
    EXTI4_IRQHandler,
    DMA1_Stream0_IRQHandler,
    DMA1_Stream1_IRQHandler,
    DMA1_Stream2_IRQHandler,
    DMA1_Stream3_IRQHandler,
    DMA1_Stream4_IRQHandler,
    DMA1_Stream5_IRQHandler,
    DMA1_Stream6_IRQHandler,
    ADC_IRQHandler,
    CAN1_TX_IRQHandler,
    CAN1_RX0_IRQHandler,
    CAN1_RX1_IRQHandler,
    CAN1_SCE_IRQHandler,
    EXTI9_5_IRQHandler,
    TIM1_BRK_TIM9_IRQHandler,
    TIM1_UP_TIM10_IRQHandler,
    TIM1_TRG_COM_TIM11_IRQHandler,
    TIM1_CC_IRQHandler,
    TIM2_IRQHandler,
    TIM3_IRQHandler,
    TIM4_IRQHandler,
    I2C1_EV_IRQHandler,
    I2C1_ER_IRQHandler,
    I2C2_EV_IRQHandler,
    I2C2_ER_IRQHandler,
    SPI1_IRQHandler,
    SPI2_IRQHandler,
    USART1_IRQHandler,
    USART2_IRQHandler,
    USART3_IRQHandler,
    EXTI15_10_IRQHandler,
    RTC_Alarm_IRQHandler,
    OTG_FS_WKUP_IRQHandler,
    TIM8_BRK_TIM12_IRQHandler,
    TIM8_UP_TIM13_IRQHandler,
    TIM8_TRG_COM_TIM14_IRQHandler,
    TIM8_CC_IRQHandler,
    DMA1_Stream7_IRQHandler,
    FSMC_IRQHandler,
    SDIO_IRQHandler,
    TIM5_IRQHandler,
    SPI3_IRQHandler,
    UART4_IRQHandler,
    UART5_IRQHandler,
    TIM6_DAC_IRQHandler,
    TIM7_IRQHandler,
    DMA2_Stream0_IRQHandler,
    DMA2_Stream1_IRQHandler,
    DMA2_Stream2_IRQHandler,
    DMA2_Stream3_IRQHandler,
    DMA2_Stream4_IRQHandler,
    ETH_IRQHandler,
    ETH_WKUP_IRQHandler,
    CAN2_TX_IRQHandler,
    CAN2_RX0_IRQHandler,
    CAN2_RX1_IRQHandler,
    CAN2_SCE_IRQHandler,
    OTG_FS_IRQHandler,
    DMA2_Stream5_IRQHandler,
    DMA2_Stream6_IRQHandler,
    DMA2_Stream7_IRQHandler,
    USART6_IRQHandler,
    I2C3_EV_IRQHandler,
    I2C3_ER_IRQHandler,
    OTG_HS_EP1_OUT_IRQHandler,
    OTG_HS_EP1_IN_IRQHandler,
    OTG_HS_WKUP_IRQHandler,
    OTG_HS_IRQHandler,
    DCMI_IRQHandler,
    0,
    CRYP_IRQHandler,
    HASH_RNG_IRQHandler,
    FPU_IRQHandler,
};

void Reset_Handler(void) {
	uint32_t* source = &_etext;
	for (uint32_t* destination = &_sdata; destination < &_edata;) {
		*destination++ = *source++;
	}

	for (uint32_t* destination = &_sbss; destination < &_ebss;) {
		*destination++ = 0;
	}

	for (uint32_t* destination = &_sccmram; destination < &_eccmram;) {
		*destination++ = 0;
	}

	uint32_t volatile* const scb_cpacr = (uint32_t volatile*) 0xE000ED88U;
	*scb_cpacr |= (0xFU << 20U);

	SystemInit();
	__libc_init_array();
	(void) main();

	while (1) {
	}
}

void Default_Handler(void) {
	while (1) {
	}
}
