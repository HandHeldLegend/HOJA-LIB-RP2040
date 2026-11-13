/* Start-up for the nRF52833 behind a bootloader that jumps to it as it is.
 *
 * Valve's bootloader (the Steam Controller's) starts the firmware without resetting the core: its
 * vector table, memory protection, stack choice and interrupts are all still in place. Until those
 * are ours, a fault or an interrupt runs the bootloader's handlers against RAM we are overwriting.
 * So the reset code takes the core back before it touches RAM, then sets up RAM as usual (data
 * copied in, .bss cleared), runs the chip's SystemInit and enters __START with interrupts masked.
 *
 * The vector table follows the chip's (nrfx mdk). Every handler is weak: a board or the HAL defines
 * the ones it uses.
 */

    .syntax unified
    .arch armv7e-m

#ifndef __STACK_SIZE
#define __STACK_SIZE 8192
#endif
#ifndef __HEAP_SIZE
#define __HEAP_SIZE 0
#endif
#ifndef __START
#define __START _start
#endif

/* Sized here, placed by the linker script (nrf_common.ld's .stack_dummy and .heap) */
    .section .stack
    .align 3
    .globl __StackTop
    .globl __StackLimit
__StackLimit:
    .space __STACK_SIZE
    .size __StackLimit, . - __StackLimit
__StackTop:
    .size __StackTop, . - __StackTop

    .section .heap
    .align 3
    .globl __HeapBase
    .globl __HeapLimit
__HeapBase:
    .if __HEAP_SIZE
    .space __HEAP_SIZE
    .endif
    .size __HeapBase, . - __HeapBase
__HeapLimit:
    .size __HeapLimit, . - __HeapLimit

    .section .isr_vector, "ax"
    .align 2
    .globl __isr_vector
__isr_vector:
    .long   __StackTop
    .long   Reset_Handler
    .long   NMI_Handler
    .long   HardFault_Handler
    .long   MemoryManagement_Handler
    .long   BusFault_Handler
    .long   UsageFault_Handler
    .long   0
    .long   0
    .long   0
    .long   0
    .long   SVC_Handler
    .long   DebugMon_Handler
    .long   0
    .long   PendSV_Handler
    .long   SysTick_Handler
    .long   POWER_CLOCK_IRQHandler
    .long   RADIO_IRQHandler
    .long   UARTE0_UART0_IRQHandler
    .long   SPIM0_SPIS0_TWIM0_TWIS0_SPI0_TWI0_IRQHandler
    .long   SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQHandler
    .long   NFCT_IRQHandler
    .long   GPIOTE_IRQHandler
    .long   SAADC_IRQHandler
    .long   TIMER0_IRQHandler
    .long   TIMER1_IRQHandler
    .long   TIMER2_IRQHandler
    .long   RTC0_IRQHandler
    .long   TEMP_IRQHandler
    .long   RNG_IRQHandler
    .long   ECB_IRQHandler
    .long   CCM_AAR_IRQHandler
    .long   WDT_IRQHandler
    .long   RTC1_IRQHandler
    .long   QDEC_IRQHandler
    .long   COMP_LPCOMP_IRQHandler
    .long   SWI0_EGU0_IRQHandler
    .long   SWI1_EGU1_IRQHandler
    .long   SWI2_EGU2_IRQHandler
    .long   SWI3_EGU3_IRQHandler
    .long   SWI4_EGU4_IRQHandler
    .long   SWI5_EGU5_IRQHandler
    .long   TIMER3_IRQHandler
    .long   TIMER4_IRQHandler
    .long   PWM0_IRQHandler
    .long   PDM_IRQHandler
    .long   0
    .long   0
    .long   MWU_IRQHandler
    .long   PWM1_IRQHandler
    .long   PWM2_IRQHandler
    .long   SPIM2_SPIS2_SPI2_IRQHandler
    .long   RTC2_IRQHandler
    .long   I2S_IRQHandler
    .long   FPU_IRQHandler
    .long   USBD_IRQHandler
    .long   UARTE1_IRQHandler
    .long   0
    .long   0
    .long   0
    .long   0
    .long   PWM3_IRQHandler
    .long   0
    .long   SPIM3_IRQHandler
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .size __isr_vector, . - __isr_vector

    .text
    .thumb
    .thumb_func
    .align 1
    .globl Reset_Handler
    .type Reset_Handler, %function
Reset_Handler:
    cpsid   i

    /* Our vector table first: anything that goes wrong from here comes to our handlers */
    ldr     r0, =__isr_vector
    ldr     r1, =0xE000ED08         /* SCB->VTOR */
    str     r0, [r1]

    /* No memory protection: the bootloader's regions mean nothing to us */
    movs    r2, #0
    ldr     r1, =0xE000ED94         /* MPU->CTRL */
    str     r2, [r1]
    dsb
    isb

    /* The main stack, from our table, in use; privileged; no floating-point context */
    ldr     r1, [r0]
    msr     msp, r1
    msr     control, r2
    isb

    /* SysTick off */
    ldr     r1, =0xE000E010         /* SysTick->CTRL */
    str     r2, [r1]

    /* Every interrupt disabled and nothing pending */
    ldr     r1, =0xE000E180         /* NVIC->ICER[0] */
    ldr     r3, =0xE000E280         /* NVIC->ICPR[0] */
    ldr     r4, =0xFFFFFFFF
    movs    r5, #8
1:
    str     r4, [r1], #4
    str     r4, [r3], #4
    subs    r5, r5, #1
    bne     1b

    /* A pended PendSV or SysTick of the bootloader's dropped */
    ldr     r1, =0xE000ED04         /* SCB->ICSR */
    ldr     r3, =0x0A000000         /* PENDSVCLR | PENDSTCLR */
    str     r3, [r1]
    dsb
    isb

    /* RAM: initialised data copied in, the rest cleared */
    ldr     r1, =__data_start
    ldr     r2, =__data_end
    ldr     r3, =__data_load_start
    bl      _copy_region
    ldr     r1, =__sdata_start
    ldr     r2, =__sdata_end
    ldr     r3, =__sdata_load_start
    bl      _copy_region
    ldr     r1, =__tdata_start
    ldr     r2, =__tdata_end
    ldr     r3, =__tdata_load_start
    bl      _copy_region
    ldr     r1, =__fast_start
    ldr     r2, =__fast_end
    ldr     r3, =__fast_load_start
    bl      _copy_region

    ldr     r1, =__bss_start__
    ldr     r2, =__bss_end__
    bl      _clear_region
    ldr     r1, =__tbss_start__
    ldr     r2, =__tbss_end__
    bl      _clear_region
    ldr     r1, =__sbss_start__
    ldr     r2, =__sbss_end__
    bl      _clear_region

    bl      SystemInit
    bl      __START
    b       .

/* r1 destination, r2 its end, r3 source; whole words */
    .thumb_func
_copy_region:
    subs    r2, r2, r1
    ble     2f
1:
    subs    r2, r2, #4
    ldr     r0, [r3, r2]
    str     r0, [r1, r2]
    bgt     1b
2:
    bx      lr

/* r1 start, r2 end; whole words */
    .thumb_func
_clear_region:
    movs    r0, #0
    subs    r2, r2, r1
    ble     2f
1:
    subs    r2, r2, #4
    str     r0, [r1, r2]
    bgt     1b
2:
    bx      lr

    .pool
    .size   Reset_Handler, . - Reset_Handler

/* Anything without a handler of its own: stop, and let the watchdog restart the chip */
    .thumb_func
    .type   Default_Handler, %function
Default_Handler:
    b       .
    .size   Default_Handler, . - Default_Handler

    .weak   NMI_Handler
    .thumb_set NMI_Handler, Default_Handler
    .weak   HardFault_Handler
    .thumb_set HardFault_Handler, Default_Handler
    .weak   MemoryManagement_Handler
    .thumb_set MemoryManagement_Handler, Default_Handler
    .weak   BusFault_Handler
    .thumb_set BusFault_Handler, Default_Handler
    .weak   UsageFault_Handler
    .thumb_set UsageFault_Handler, Default_Handler
    .weak   SVC_Handler
    .thumb_set SVC_Handler, Default_Handler
    .weak   DebugMon_Handler
    .thumb_set DebugMon_Handler, Default_Handler
    .weak   PendSV_Handler
    .thumb_set PendSV_Handler, Default_Handler
    .weak   SysTick_Handler
    .thumb_set SysTick_Handler, Default_Handler
    .weak   POWER_CLOCK_IRQHandler
    .thumb_set POWER_CLOCK_IRQHandler, Default_Handler
    .weak   RADIO_IRQHandler
    .thumb_set RADIO_IRQHandler, Default_Handler
    .weak   UARTE0_UART0_IRQHandler
    .thumb_set UARTE0_UART0_IRQHandler, Default_Handler
    .weak   SPIM0_SPIS0_TWIM0_TWIS0_SPI0_TWI0_IRQHandler
    .thumb_set SPIM0_SPIS0_TWIM0_TWIS0_SPI0_TWI0_IRQHandler, Default_Handler
    .weak   SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQHandler
    .thumb_set SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQHandler, Default_Handler
    .weak   NFCT_IRQHandler
    .thumb_set NFCT_IRQHandler, Default_Handler
    .weak   GPIOTE_IRQHandler
    .thumb_set GPIOTE_IRQHandler, Default_Handler
    .weak   SAADC_IRQHandler
    .thumb_set SAADC_IRQHandler, Default_Handler
    .weak   TIMER0_IRQHandler
    .thumb_set TIMER0_IRQHandler, Default_Handler
    .weak   TIMER1_IRQHandler
    .thumb_set TIMER1_IRQHandler, Default_Handler
    .weak   TIMER2_IRQHandler
    .thumb_set TIMER2_IRQHandler, Default_Handler
    .weak   RTC0_IRQHandler
    .thumb_set RTC0_IRQHandler, Default_Handler
    .weak   TEMP_IRQHandler
    .thumb_set TEMP_IRQHandler, Default_Handler
    .weak   RNG_IRQHandler
    .thumb_set RNG_IRQHandler, Default_Handler
    .weak   ECB_IRQHandler
    .thumb_set ECB_IRQHandler, Default_Handler
    .weak   CCM_AAR_IRQHandler
    .thumb_set CCM_AAR_IRQHandler, Default_Handler
    .weak   WDT_IRQHandler
    .thumb_set WDT_IRQHandler, Default_Handler
    .weak   RTC1_IRQHandler
    .thumb_set RTC1_IRQHandler, Default_Handler
    .weak   QDEC_IRQHandler
    .thumb_set QDEC_IRQHandler, Default_Handler
    .weak   COMP_LPCOMP_IRQHandler
    .thumb_set COMP_LPCOMP_IRQHandler, Default_Handler
    .weak   SWI0_EGU0_IRQHandler
    .thumb_set SWI0_EGU0_IRQHandler, Default_Handler
    .weak   SWI1_EGU1_IRQHandler
    .thumb_set SWI1_EGU1_IRQHandler, Default_Handler
    .weak   SWI2_EGU2_IRQHandler
    .thumb_set SWI2_EGU2_IRQHandler, Default_Handler
    .weak   SWI3_EGU3_IRQHandler
    .thumb_set SWI3_EGU3_IRQHandler, Default_Handler
    .weak   SWI4_EGU4_IRQHandler
    .thumb_set SWI4_EGU4_IRQHandler, Default_Handler
    .weak   SWI5_EGU5_IRQHandler
    .thumb_set SWI5_EGU5_IRQHandler, Default_Handler
    .weak   TIMER3_IRQHandler
    .thumb_set TIMER3_IRQHandler, Default_Handler
    .weak   TIMER4_IRQHandler
    .thumb_set TIMER4_IRQHandler, Default_Handler
    .weak   PWM0_IRQHandler
    .thumb_set PWM0_IRQHandler, Default_Handler
    .weak   PDM_IRQHandler
    .thumb_set PDM_IRQHandler, Default_Handler
    .weak   MWU_IRQHandler
    .thumb_set MWU_IRQHandler, Default_Handler
    .weak   PWM1_IRQHandler
    .thumb_set PWM1_IRQHandler, Default_Handler
    .weak   PWM2_IRQHandler
    .thumb_set PWM2_IRQHandler, Default_Handler
    .weak   SPIM2_SPIS2_SPI2_IRQHandler
    .thumb_set SPIM2_SPIS2_SPI2_IRQHandler, Default_Handler
    .weak   RTC2_IRQHandler
    .thumb_set RTC2_IRQHandler, Default_Handler
    .weak   I2S_IRQHandler
    .thumb_set I2S_IRQHandler, Default_Handler
    .weak   FPU_IRQHandler
    .thumb_set FPU_IRQHandler, Default_Handler
    .weak   USBD_IRQHandler
    .thumb_set USBD_IRQHandler, Default_Handler
    .weak   UARTE1_IRQHandler
    .thumb_set UARTE1_IRQHandler, Default_Handler
    .weak   PWM3_IRQHandler
    .thumb_set PWM3_IRQHandler, Default_Handler
    .weak   SPIM3_IRQHandler
    .thumb_set SPIM3_IRQHandler, Default_Handler

    .end
