	/* Sample initialization file */

	.extern main
	.extern exit
	.extern AT91F_LowLevelInit
	.extern pio_irq_isr_value
	.extern tc_sniffer_next_buffer_for_fiq

	.text
	.code 32


	.align  0

	.extern __stack_end__
	.extern __bss_beg__
	.extern __bss_end__
	.extern __data_beg__
	.extern __data_end__
	.extern __data+beg_src__

	.global start
	.global endless_loop

	/* Stack Sizes */
    .set  UND_STACK_SIZE, 0x00000004
    .set  ABT_STACK_SIZE, 0x00000004
    .set  FIQ_STACK_SIZE, 0x00000400
    .set  IRQ_STACK_SIZE, 0X00000400
    .set  SVC_STACK_SIZE, 0x00000400

	/* Standard definitions of Mode bits and Interrupt (I & F) flags in PSRs */
    .set  MODE_USR, 0x10            /* User Mode */
    .set  MODE_FIQ, 0x11            /* FIQ Mode */
    .set  MODE_IRQ, 0x12            /* IRQ Mode */
    .set  MODE_SVC, 0x13            /* Supervisor Mode */
    .set  MODE_ABT, 0x17            /* Abort Mode */
    .set  MODE_UND, 0x1B            /* Undefined Mode */
    .set  MODE_SYS, 0x1F            /* System Mode */

    .equ  I_BIT, 0x80               /* when I bit is set, IRQ is disabled */
    .equ  F_BIT, 0x40               /* when F bit is set, FIQ is disabled */

.equ AT91C_BASE_AIC,  (0xFFFFF000)
.equ AT91C_BASE_MC,   (0xFFFFFF00)
.equ AT91C_BASE_PIOA, 0xFFFFF400
.equ AT91C_BASE_TC0,  0xFFFA0000
.equ AT91C_BASE_TC2,  0xFFFA0080
.equ AT91C_BASE_SSC,  0xFFFD4000
.equ SSC_CR,          0x0
.equ SSC_RCMR,        0x10
.equ SSC_CR_TXEN,     0x100
.equ AT91C_TC_SWTRG,  ((1 << 2)|1)
.equ AT91C_TC_CLKEN,  (1 << 0)
.equ PIO_DATA,        (1 << 18)
.equ PIOA_SODR,       0x30
.equ PIOA_CODR,       0x34
.equ PIOA_PDSR,       0x3c
.equ PIOA_IDR,        0x44
.equ PIOA_ISR,        0x4c
.equ TC_CCR,          0x00
.equ TC2_CV,          (0x80+0x10)
.equ AIC_EOICR,       (304)
.equ PIO_LED1,        (1 << 25)
.equ PIO_LED2,        (1 << 12)
.equ MC_RCR,          0xFFFFFF00
.equ AIC_ISCR,        (0x12C)
.equ PIO_SECONDARY_IRQ, 31
.equ PIO_SECONDARY_IRQ_BIT, (1 << PIO_SECONDARY_IRQ)

.equ BUFSIZE, 1024

start:
_start:
_mainCRTStartup:

	/* Setup a stack for each mode - note that this only sets up a usable stack
	for system/user, SWI and IRQ modes.   Also each mode is setup with
	interrupts initially disabled. */
    ldr   r0, .LC6
    msr   CPSR_c, #MODE_UND|I_BIT|F_BIT /* Undefined Instruction Mode */
    mov   sp, r0
    sub   r0, r0, #UND_STACK_SIZE
    msr   CPSR_c, #MODE_ABT|I_BIT|F_BIT /* Abort Mode */
    mov   sp, r0
    sub   r0, r0, #ABT_STACK_SIZE
    msr   CPSR_c, #MODE_FIQ|I_BIT|F_BIT /* FIQ Mode */
    /* Preload registers for FIQ handler */
    ldr     r10, =AT91C_BASE_PIOA
    ldr     r12, =AT91C_BASE_TC0
    ldr     r8, =AT91C_BASE_AIC
    /*ldr     r9, =AT91C_BASE_SSC*/
    mov   sp, r0
    sub   r0, r0, #FIQ_STACK_SIZE
    msr   CPSR_c, #MODE_IRQ|I_BIT|F_BIT /* IRQ Mode */
    mov   sp, r0
    sub   r0, r0, #IRQ_STACK_SIZE
    msr   CPSR_c, #MODE_SVC|I_BIT|F_BIT /* Supervisor Mode */
    mov   sp, r0
    sub   r0, r0, #SVC_STACK_SIZE
    msr   CPSR_c, #MODE_SYS|I_BIT|F_BIT /* System Mode */
    mov   sp, r0

	/* We want to start in supervisor mode.  Operation will switch to system
	mode when the first task starts. */
	msr   CPSR_c, #MODE_SVC|I_BIT|F_BIT

    bl		AT91F_LowLevelInit

	/* Clear BSS. */

	mov     a2, #0			/* Fill value */
	mov		fp, a2			/* Null frame pointer */
	mov		r7, a2			/* Null frame pointer for Thumb */

	ldr		r1, .LC1		/* Start of memory block */
	ldr		r3, .LC2		/* End of memory block */
	subs	r3, r3, r1      /* Length of block */
	beq		.end_clear_loop
	mov		r2, #0

.clear_loop:
	strb	r2, [r1], #1
	subs	r3, r3, #1
	bgt		.clear_loop

.end_clear_loop:

	/* Initialise data. */

	ldr		r1, .LC3		/* Start of memory block */
	ldr		r2, .LC4		/* End of memory block */
	ldr		r3, .LC5
	subs	r3, r3, r1		/* Length of block */
	beq		.end_set_loop

.set_loop:
	ldrb	r4, [r2], #1
	strb	r4, [r1], #1
	subs	r3, r3, #1
	bgt		.set_loop

.end_set_loop:

	/* call main */
	mov		r0, #0          /* no arguments  */
	mov		r1, #0          /* no argv either */

    ldr lr, =main	
	bx	lr

endless_loop:
	b               endless_loop


	.align 0

	.LC1:
	.word   __bss_beg__
	.LC2:
	.word   __bss_end__
	.LC3:
	.word   __data_beg__
	.LC4:
	.word   __data_beg_src__
	.LC5:
	.word   __data_end__
	.LC6:
	.word	__stack_end__


	/* Setup vector table.  Note that undf, pabt, dabt, fiq just execute
	a null loop. */

.section .startup,"ax"
         .code 32
         .align 0

	b     _start						/* reset - _start			*/
	ldr   pc, _undf						/* undefined - _undf		*/
	ldr   pc, _swi						/* SWI - _swi				*/
	ldr   pc, _pabt						/* program abort - _pabt	*/
	ldr   pc, _dabt						/* data abort - _dabt		*/
	nop									/* reserved					*/
	ldr   pc, [pc,#-0xF20]				/* IRQ - read the AIC		*/
/*	ldr   pc, [pc,#-0xF20]				/* FIQ - fall through to fiq_handler		*/


/* Following is modified from openpcd/firmware/src/start/Cstartup_app.S */
#define LED_TRIGGER

        .global fiq_handler
        .func fiq_handler
my_fiq_handler:
                /* code that uses pre-initialized FIQ reg */
                /* r8   tmp
                   r9   tmp
                   r10  AT91C_BASE_PIOA
                   r11  tmp
                   r12  AT91C_BASE_TC0
                   r13  stack
                   r14  lr
                 */

#ifdef LED_TRIGGER
                mov   r11, #PIO_LED1
                str   r11, [r10, #PIOA_CODR] /* enable LED */
#endif
                ldr     r8, [r10, #PIOA_ISR]
                
                /* Store the retrieved PIO ISR value into pio_irq_isr_value */
                ldr   r11, =pio_irq_isr_value
                str   r8, [r11]
                
                tst     r8, #PIO_DATA           /* check for PIO_DATA change */
                beq     .no_buffer
                
                ldr   r11, [r10, #PIOA_PDSR]
                tst   r11, #PIO_DATA          /* check for PIO_DATA == 1 */
                beq   .no_buffer

/*                mov     r11, #PIO_LED2
                str     r11, [r10, #PIOA_CODR] /* enable LED */

                /* Load the TC2.CV into r9 */
                ldr r9, [r12, #TC2_CV]
                
                ldr r11, =tc_sniffer_next_buffer_for_fiq
                ldr r11, [r11]
                /* r11 now contains the value of tc_sniffer_next_buffer_for_fiq, e.q. the address
                 * of the next buffer */
                
                /* Jump to .no_buffer if the pointer is 0, indicating that no buffer is set */
                cmp r11, #0
                beq .no_buffer 
                
		/* Increment the value at the location the pointer points to */
		ldr r8, [r11]
		add r8, r8, #1
		str r8, [r11]
		
		/* At this point:
		   r8  = count
		   r9  = TC2.CV
		   r11 = pointer to buffer
		 */
		
		cmp r8, #BUFSIZE
		bge .no_buffer
		
		str r9, [r11, r8, LSL #2]
		
.no_buffer:
/*                mov     r11, #PIO_LED2
                str     r11, [r10, #PIOA_SODR] /* disable LED */
                
                /* Trigger PIO_SECONDARY_IRQ */
                mov r11, #PIO_SECONDARY_IRQ_BIT
                ldr r8, =AT91C_BASE_AIC
                str r11, [r8, #AIC_ISCR]

#ifdef LED_TRIGGER
                mov     r11, #PIO_LED1
                str     r11, [r10, #PIOA_SODR] /* disable LED */
#endif
            
                /*- Mark the End of Interrupt on the AIC */
                ldr     r11, =AT91C_BASE_AIC
                str     r11, [r11, #AIC_EOICR]

                /*- Restore the Program Counter using the LR_fiq directly in the PC */
                subs        pc, lr, #4

        .size   my_fiq_handler, . - my_fiq_handler
        .endfunc

_undf:  .word __undf                    /* undefined				*/
_swi:   .word swi_handler				/* SWI						*/
_pabt:  .word __pabt                    /* program abort			*/
_dabt:  .word __dabt                    /* data abort				*/
_fiq:   .word __fiq                     /* FIQ						*/

__undf: b     .                         /* undefined				*/
__pabt: b     .                         /* program abort			*/
__dabt: b     .                         /* data abort				*/
__fiq:  b     .                         /* FIQ						*/

        .end
        
