/* Synchronize TC_CDIV divided sample clock with the SOF of the packet */

#include <lib_AT91SAM7.h>
#include <AT91SAM7.h>
#include "tc_cdiv_sync.h"
#include "dbgu.h"
#include "pio_irq.h"
#include "openpicc.h"
#include "led.h"
#include "tc_cdiv.h"

#define USE_IRQ

static u_int8_t enabled;

static void pio_data_change(u_int32_t pio)
{
	(void)pio;
	DEBUGP("PIO_FRAME_IRQ: ");
	/* we get one interrupt for each change. If now, after the
	 * change the level is high, then it must have been a rising
	 * edge */
	if (*AT91C_PIOA_PDSR & OPENPICC_PIO_FRAME) {
		vLedSetGreen(1);
		/* This code is now replaced by hard-coded assembler code in os/boot/boot.s my_fiq_handler */
		/* *AT91C_TC0_CCR = AT91C_TC_SWTRG;*/
		DEBUGPCR("CDIV_SYNC_FLIP SWTRG CV=0x%08x",
			  *AT91C_TC0_CV);
		vLedSetGreen(0);
	} else
		DEBUGPCR("");
	//vLedSetGreen(0);
}

void tc_cdiv_sync_reset(void)
{
	if (enabled) {
		u_int32_t tmp = *AT91C_PIOA_ISR;
		(void)tmp;
		volatile int i;
		DEBUGPCRF("CDIV_SYNC_FLOP");
		//vLedSetGreen(1);

		/* reset the hardware flipflop, this clears FRAME */
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA,
				      OPENPICC_PIO_SSC_DATA_CONTROL);
		for (i = 0; i < 0xff; i++) ;
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA,
				    OPENPICC_PIO_SSC_DATA_CONTROL);
		
		if(OPENPICC->features.clock_gating) {
			/* reset tc_cdiv counter, the cleared frame signal stopped the tc_cdiv clock */
			tc_cdiv_reset();
		}
	}
}

void tc_cdiv_sync_disable(void)
{
	enabled = 0;	
	*AT91C_PIOA_IDR = OPENPICC_PIO_FRAME;
}

void tc_cdiv_sync_enable(void)
{
	enabled = 1;

	DEBUGPCRF("CDIV_SYNC_ENABLE ");
	tc_cdiv_sync_reset();
	*AT91C_PIOA_IER = OPENPICC_PIO_FRAME;
}

void tc_cdiv_sync_init()
{
	pio_irq_init_once();
	DEBUGPCRF("initializing");

	enabled = 0;

	AT91F_PIOA_CfgPMC();
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPICC_PIO_SSC_DATA_CONTROL);
	
	/* This code is now replaced by hard-coded assembler code in os/boot/boot.s my_fiq_handler */
	/*pio_irq_register(OPENPICC_PIO_FRAME, &pio_data_change);*/
	(void)pio_data_change;
	
	//vLedSetGreen(0);
	tc_cdiv_sync_disable();
}
