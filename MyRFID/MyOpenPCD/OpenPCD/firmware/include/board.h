#ifndef __BOARD_H__
#define __BOARD_H__

#include <AT91SAM7.h>
#include <lib_AT91SAM7.h>

/*--------------*/
/* Master Clock */
/*--------------*/
#define EXT_OSC			18432000	// External Crystal Oscillator
#define MCK			47923200	// Resulting PLL CLock
#define ENVIRONMENT_SIZE	 ( AT91C_IFLASH_PAGE_SIZE )

#endif/*__BOARD_H__*/
