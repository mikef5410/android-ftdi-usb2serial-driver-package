#include <AT91SAM7.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>
#include <USB-CDC.h>
#include <board.h>
#include <compile.h>
#include <string.h>

#include "env.h"
#include "cmd.h"
#include "openpicc.h"
#include "led.h"
#include "da.h"
#include "adc.h"
#include "pll.h"
#include "tc_cdiv.h"
#include "tc_cdiv_sync.h"
#include "pio_irq.h"
#include "ssc.h"
#include "clock_switch.h"
#include "usb_print.h"
#include "load_modulation.h"
#include "tc_sniffer.h"
#include "iso14443a_pretender.h"

xQueueHandle xCmdQueue;
xTaskHandle xCmdTask;
xTaskHandle xCmdRecvUsbTask;
xTaskHandle xFieldMeterTask;
xSemaphoreHandle xFieldMeterMutex;

volatile int fdt_offset=-20  -16; // -16 for the SSC Tx set to continous bug
volatile int load_mod_level_set=3;

#if ( configUSE_TRACE_FACILITY == 1 )
static signed portCHAR pcWriteBuffer[512];
#endif

/* Whether to require a colon (':') be typed before long commands, similar to e.g. vi
 * This makes a distinction between long commands and short commands: long commands may be
 * longer than one character and/or accept optional parameters, short commands are only one
 * character with no arguments (typically some toggling command). Short commands execute
 * immediately when the character is pressed, long commands must be completed with return.
 * */
static const portBASE_TYPE USE_COLON_FOR_LONG_COMMANDS = 0;
/* When not USE_COLON_FOR_LONG_COMMANDS then short commands will be recognized by including
 * their character in the string SHORT_COMMANDS
 * */
static const char *SHORT_COMMANDS = "!prc+-l?hq9fjka#i";
/* Note that the long/short command distinction only applies to the USB serial console
 * */

/**********************************************************************/
void DumpUIntToUSB(unsigned int data)
{
    int i=0;
    unsigned char buffer[10],*p=&buffer[sizeof(buffer)];

    do {
        *--p='0'+(unsigned char)(data%10);
        data/=10;
        i++;
    } while(data);

    while(i--)
        usb_print_char(*p++);
}
/**********************************************************************/

void DumpStringToUSB(const char* text)
{
    usb_print_string(text);
}
/**********************************************************************/

static inline unsigned char HexChar(unsigned char nibble)
{
   return nibble + ((nibble<0x0A) ? '0':('A'-0xA));
}

void DumpBufferToUSB(char* buffer, int len)
{
    int i;

    for(i=0; i<len; i++) {
	usb_print_char(HexChar( *buffer  >>   4));
	usb_print_char(HexChar( *buffer++ & 0xf));
    }
}
/**********************************************************************/

void DumpTimeToUSB(long ticks)
{
	int h, s, m, ms;
	ms = ticks;
	
	s=ms/1000;
	ms%=1000;
	h=s/3600;
	s%=3600;
	m=s/60;
	s%=60;
	DumpUIntToUSB(h);
	DumpStringToUSB("h:");
	if(m < 10) DumpStringToUSB("0");
	DumpUIntToUSB(m);
	DumpStringToUSB("m:");
	if(s < 10) DumpStringToUSB("0");
	DumpUIntToUSB(s);
	DumpStringToUSB("s.");
	if(ms < 10) DumpStringToUSB("0");
	if(ms < 100) DumpStringToUSB("0");
	DumpUIntToUSB(ms);
	DumpStringToUSB("ms");
}

/*
 * Convert a string to an integer. Ignores leading spaces.
 * Optionally returns a pointer to the end of the number in the string */
int atoiEx(const char * nptr, char * * eptr)
{
	portBASE_TYPE sign = 1, i=0;
	int curval = 0;
	while(nptr[i] == ' ' && nptr[i] != 0) i++;
	if(nptr[i] == 0) goto out;
	if(nptr[i] == '-') {sign *= -1; i++; }
	else if(nptr[i] == '+') { i++; } 
	while(nptr[i] != 0 && nptr[i] >= '0' && nptr[i] <= '9')
		curval = curval * 10 + (nptr[i++] - '0');
	
	out:
	if(eptr != NULL) *eptr = (char*)nptr+i;
	return sign * curval;
}

/* Common code between increment/decrement UID/nonce */
static int change_value( int(* const getter)(u_int8_t*, size_t), int(* const setter)(u_int8_t*, size_t), 
		const s_int32_t increment, const char *name )
{
	u_int8_t uid[4];
	int ret;
	if( getter(uid, sizeof(uid)) >= 0) {
		(*((u_int32_t*)uid)) += increment;
		taskENTER_CRITICAL();
		ret = setter(uid, sizeof(uid));
		taskEXIT_CRITICAL();
		if(ret < 0) {
			DumpStringToUSB("Failed to set the ");
			DumpStringToUSB(name);
			DumpStringToUSB("\n\r");
		} else {
			DumpStringToUSB("Successfully set ");
			DumpStringToUSB(name);
			DumpStringToUSB(" to ");
			DumpBufferToUSB((char*)uid, sizeof(uid));
			DumpStringToUSB("\n\r");
		}
	} else {
		DumpStringToUSB("Couldn't get ");
		DumpStringToUSB(name);
		DumpStringToUSB("\n\r");
		ret = -1;
	}
	return ret;
}

#define DYNAMIC_PIN_PLL_LOCK 1
static struct { int pin; int dynpin; char * description; } PIO_PINS[] = {
	{0,DYNAMIC_PIN_PLL_LOCK,      "pll lock   "},
	{OPENPICC_PIO_FRAME,0,        "frame start"},
};
void print_pio(void)
{
        int data = *AT91C_PIOA_PDSR;
        unsigned int i;
	DumpStringToUSB(
		" *****************************************************\n\r"
        	" * Current PIO readings:                             *\n\r"
        	" *****************************************************\n\r"
        	" *\n\r");
	for(i=0; i<sizeof(PIO_PINS)/sizeof(PIO_PINS[0]); i++) {
			if(PIO_PINS[i].dynpin != 0) continue;
        	DumpStringToUSB(" * ");
        	DumpStringToUSB(PIO_PINS[i].description);
        	DumpStringToUSB(": ");
        	DumpUIntToUSB((data & PIO_PINS[i].pin) ? 1 : 0 );
        	DumpStringToUSB("\n\r");
	}
	DumpStringToUSB(" *****************************************************\n\r");
}


static const AT91PS_SPI spi = AT91C_BASE_SPI;
#define SPI_MAX_XFER_LEN 33

static int clock_select=0;
void startstop_field_meter(void);
extern volatile unsigned portLONG ulCriticalNesting;
void prvExecCommand(u_int32_t cmd, portCHAR *args) {
	static int led = 0;
	portCHAR cByte = cmd & 0xff;
	int i,ms;
	if(cByte>='A' && cByte<='Z')
	    cByte-=('A'-'a');
	
	DumpStringToUSB("Got command ");
	DumpUIntToUSB(cmd);
	DumpStringToUSB(" with args ");
	DumpStringToUSB(args);
	DumpStringToUSB("\n\r");
	    
	i=0;
	// Note: Commands have been uppercased when this code is called
	    switch(cmd)
	    {
		case 'THRU': {
				/*char buffer[64];
				memset(buffer, 'A', sizeof(buffer));
				usb_print_flush();
				while(1) usb_print_buffer(buffer, 0, sizeof(buffer));*/
				while(1) vUSBSendBuffer((unsigned char*)"AAAAAABCDEBBBBB", 0, 15);
			}
			break;
		case 'Z':
		    i=atoiEx(args, &args);
		    if(i==0) {
			tc_cdiv_sync_disable();
			DumpStringToUSB("cdiv_sync disabled \n\r");
		    } else {
			tc_cdiv_sync_enable();
			DumpStringToUSB("cdiv_sync enabled \n\r");
		    }
		    break;
		case 'G':
			if(!OPENPICC->features.data_gating) {
				DumpStringToUSB("This hardware does not have data gating capability\n\r");
				break;
			}
		    i=atoiEx(args, &args);
		    ssc_set_gate(i);
		    if(i==0) {
		    	DumpStringToUSB("SSC_DATA disabled \n\r");
		    } else {
		    	DumpStringToUSB("SSC_DATA enabled \n\r");
		    }
		    break;
		case 'D':
		    i=atoiEx(args, &args);
		    tc_cdiv_set_divider(i);
		    DumpStringToUSB("tc_cdiv set to ");
		    DumpUIntToUSB(i);
		    DumpStringToUSB("\n\r");
		    break;
		case 'P':
		    print_pio();
		    break;
/*		case 'R':
			start_stop_sniffing();
			break;*/
		case 'C':
		    DumpStringToUSB(
			" *****************************************************\n\r"
			" * Current configuration:                            *\n\r"
			" *****************************************************\n\r"
			" * Version " COMPILE_SVNREV "\n\r"
			" * compiled " COMPILE_DATE " by " COMPILE_BY "\n\r"
			" * running on ");
		    DumpStringToUSB(OPENPICC->release_name);
		    DumpStringToUSB("\n\r *\n\r");
		    DumpStringToUSB(" * Uptime is ");
		    ms=xTaskGetTickCount();
		    DumpTimeToUSB(ms);
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * The comparator threshold is ");
		    DumpUIntToUSB(da_get_value());
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * Number of PIO IRQs handled: ");
		    i = pio_irq_get_count() & (~(unsigned int)0);
		    DumpUIntToUSB(i);
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * current field strength: ");
		    DumpUIntToUSB(adc_get_field_strength());
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * fdt_offset: ");
		    if(fdt_offset < 0) {
		    	DumpStringToUSB("-");
		    	DumpUIntToUSB(-fdt_offset);
		    } else DumpUIntToUSB(fdt_offset);
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * load_mod_level: ");
		    DumpUIntToUSB(load_mod_level_set);
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * SSC performance metrics:\n\r");
		    for(i=0; i<_MAX_METRICS; i++) {
		    	char *name; 
		    	int value;
		    	if(ssc_get_metric(i, &name, &value)) {
		    		DumpStringToUSB(" * \t");
		    		DumpStringToUSB(name);
		    		DumpStringToUSB(": ");
		    		DumpUIntToUSB(value);
		    		DumpStringToUSB("\n\r");
		    	}
		    }
		    DumpStringToUSB(" * SSC status: ");
		    DumpUIntToUSB(AT91C_BASE_SSC->SSC_SR);
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * TC0_CV value: ");
		    DumpUIntToUSB(*AT91C_TC0_CV);
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * TC2_CV value: ");
		    DumpUIntToUSB(*AT91C_TC2_CV);
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * TC2_IMR value: ");
		    DumpUIntToUSB(*AT91C_TC2_IMR);
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * TC2_SR value: ");
		    DumpUIntToUSB(*AT91C_TC2_SR);
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * TC2_RB value: ");
		    DumpUIntToUSB(*AT91C_TC2_RB);
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * TC2_RC value: ");
		    DumpUIntToUSB(*AT91C_TC2_RC);
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * SSC_SR value: ");
		    DumpUIntToUSB(*AT91C_SSC_SR);
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * SSC_RCMR value: ");
		    DumpUIntToUSB(*AT91C_SSC_RCMR);
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * SSC_TCMR value: ");
		    DumpUIntToUSB(*AT91C_SSC_TCMR);
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * SSC_TFMR value: ");
		    DumpUIntToUSB(*AT91C_SSC_TFMR);
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * SSC_TPR value: ");
		    DumpUIntToUSB(*AT91C_SSC_TPR);
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * SSC_TCR value: ");
		    DumpUIntToUSB(*AT91C_SSC_TCR);
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * SSC_RPR value: ");
		    DumpUIntToUSB(*AT91C_SSC_RPR);
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * SSC_RCR value: ");
		    DumpUIntToUSB(*AT91C_SSC_RCR);
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(" * SSC_IMR value: ");
		    DumpUIntToUSB(*AT91C_SSC_IMR);
		    DumpStringToUSB("\n\r");
		    DumpStringToUSB(
			" *\n\r"
			" *****************************************************\n\r"
			);
		    break;
		case '+':
		case '-':
		    if(cmd == '+')
		    {
			if(da_get_value() < 255)
			    da_comp_carr(da_get_value()+1);
		    }
		    else
			if(da_get_value() > 0)
			    da_comp_carr(da_get_value()-1);;
		    			
		    DumpStringToUSB(" * Comparator threshold set to ");
		    DumpUIntToUSB(da_get_value());		    
		    DumpStringToUSB("\n\r");
		    break;
		case 'U+':
		    i=atoiEx(args, &args);
		    change_value(get_UID, set_UID, i==0 ? +1 : +i , "UID"); break;
		case 'U-': 
		    i=atoiEx(args, &args);
		    change_value(get_UID, set_UID, i==0 ? -1 : -i , "UID"); break;
		case 'N+':
		    i=atoiEx(args, &args);
			change_value(get_nonce, set_nonce, i==0 ? +1 : +i, "nonce"); break;
		case 'N-': 
		    i=atoiEx(args, &args);
			change_value(get_nonce, set_nonce, i==0 ? -1 : -i, "nonce"); break;
		case 'L':
		    led = (led+1)%4;
		    vLedSetRed( (led&1) );
		    vLedSetGreen( led&2 );
		    DumpStringToUSB(" * LEDs set to ");
		    vUSBSendByte( (char)led + '0' );
		    DumpStringToUSB("\n\r");
		    break;
		case '!':
		    tc_cdiv_sync_reset();
		    break;
		case '#':
			if(!OPENPICC->features.clock_switching) {
				DumpStringToUSB("* This hardware is not clock switching capable\n\r");
				break;
			}
			clock_select = (clock_select+1) % 2;
			clock_switch(clock_select);
			DumpStringToUSB("Active clock is now ");
			DumpUIntToUSB(clock_select);
			DumpStringToUSB("\n\r");
			break;
		case 'J':
		    fdt_offset++;
		    DumpStringToUSB("fdt_offset is now ");
		    if(fdt_offset<0) { DumpStringToUSB("-"); DumpUIntToUSB(-fdt_offset); }
		    else { DumpStringToUSB("+"); DumpUIntToUSB(fdt_offset); }
		    DumpStringToUSB("\n\r");
		    break;
		case 'K':
		    fdt_offset--;
		    DumpStringToUSB("fdt_offset is now ");
		    if(fdt_offset<0) { DumpStringToUSB("-"); DumpUIntToUSB(-fdt_offset); }
		    else { DumpStringToUSB("+"); DumpUIntToUSB(fdt_offset); }
		    DumpStringToUSB("\n\r");
		    break;
		case 'F':
		    startstop_field_meter();
		    break;
		case 'I':
			pll_inhibit(!pll_is_inhibited());
			if(pll_is_inhibited()) DumpStringToUSB(" * PLL is now inhibited\n\r");
			else DumpStringToUSB(" * PLL is now running\n\r");
#if ( configUSE_TRACE_FACILITY == 1 )
		case 'T':
		    memset(pcWriteBuffer, 0, sizeof(pcWriteBuffer));
		    vTaskList(pcWriteBuffer);
		    DumpStringToUSB((char*)pcWriteBuffer);
		    break;
#endif
		case 'Q':
		    //BROKEN new ssc code
			//ssc_rx_start();
		    while(0) {
		    	DumpUIntToUSB(AT91C_BASE_SSC->SSC_SR);
		    	DumpStringToUSB("\n\r");
		    }
		    break;
		case 'A':
		    load_mod_level_set = (load_mod_level_set+1) % 4;
		    load_mod_level(load_mod_level_set);
		    DumpStringToUSB("load_mod_level is now ");
		    DumpUIntToUSB(load_mod_level_set);
		    DumpStringToUSB("\n\r");
		    break;
		case 'H':	
		case '?':
		    DumpStringToUSB(
			" *****************************************************\n\r"
			" * OpenPICC USB terminal                             *\n\r"
			" * (C) 2007 Milosch Meriac <meriac@openbeacon.de>    *\n\r"
			" * (C) 2007 Henryk Plötz <henryk@ploetzli.ch>        *\n\r"
			" *****************************************************\n\r"
			" * Version " COMPILE_SVNREV "\n\r"
			" * compiled " COMPILE_DATE " by " COMPILE_BY "\n\r"
			" * running on ");
		    DumpStringToUSB(OPENPICC->release_name);
		    DumpStringToUSB("\n\r *\n\r"
		    " * thru - test throughput\n\r"
#if ( configUSE_TRACE_FACILITY == 1 )
			" * t    - print task list and stack usage\n\r"
#endif
			" * c    - print configuration\n\r"
			" * +,-  - decrease/increase comparator threshold\n\r"
		    " * #    - switch clock\n\r"
			" * l    - cycle LEDs\n\r"
			" * p    - print PIO pins\n\r"
		    " * r    - start/stop receiving\n\r"
			" * z 0/1- enable or disable tc_cdiv_sync\n\r"
			" * i    - inhibit/uninhibit PLL\n\r"
			" * !    - reset tc_cdiv_sync\n\r"
			" * q    - start rx\n\r"
			" * f    - start/stop field meter\n\r"
			" * d div- set tc_cdiv divider value 16, 32, 64, ...\n\r"
			" * j,k  - increase, decrease fdt_offset\n\r"
			" * a    - change load modulation level\n\r"
			" * g 0/1- disable or enable SSC_DATA through gate\n\r"
			" * 9    - reset CPU\n\r"
			" * ?,h  - display this help screen\n\r"
			" *\n\r"
			" *****************************************************\n\r"
			);
		    break;
		case '9':
		    AT91F_RSTSoftReset(AT91C_BASE_RSTC, AT91C_RSTC_PROCRST|
			AT91C_RSTC_PERRST|AT91C_RSTC_EXTRST);
		    break;
	    }
    
}

// A task to execute commands
void vCmdCode(void *pvParameters) {
	(void) pvParameters;
	u_int32_t cmd;
	portBASE_TYPE i, j=0;
	
	for(;;) {
		cmd_type next_command;
		cmd = j = 0;
		 if( xQueueReceive(xCmdQueue, &next_command, ( portTickType ) 100 ) ) {
			DumpStringToUSB("Command received:");
			DumpStringToUSB(next_command.command);
			DumpStringToUSB("\n\r");
			while(next_command.command[j] == ' ' && next_command.command[j] != 0) {
				j++;
			}
			for(i=0;i<4;i++) {
				portCHAR cByte = next_command.command[i+j];
				if(next_command.command[i+j] == 0 || next_command.command[i+j] == ' ')
					break;
				if(cByte>='a' && cByte<='z') {
					cmd = (cmd<<8) | (cByte+('A'-'a'));
				} else cmd = (cmd<<8) | cByte;
			}
			while(next_command.command[i+j] == ' ' && next_command.command[i+j] != 0) {
				i++;
			}
			prvExecCommand(cmd, next_command.command+i+j);
		 } else {
		 }
	}
}



// A task to read commands from USB
void vCmdRecvUsbCode(void *pvParameters) {
	portBASE_TYPE len=0;
	portBASE_TYPE short_command=1, submit_it=0;
	cmd_type next_command = { source: SRC_USB, command: ""};
	(void) pvParameters;
    
	for( ;; ) {
		if(vUSBRecvByte(&next_command.command[len], 1, 100)) {
			if(USE_COLON_FOR_LONG_COMMANDS) {
				if(len == 0 && next_command.command[len] == ':')
					short_command = 0;
			} else {
				if(strchr(SHORT_COMMANDS, next_command.command[len]) == NULL)
					short_command = 0;
			}
			next_command.command[len+1] = 0;
			DumpStringToUSB(next_command.command + len);
			if(next_command.command[len] == '\n' || next_command.command[len] == '\r') {
				next_command.command[len] = 0;
				submit_it = 1;
			}
			if(short_command==1) {
				submit_it = 1;
			}
			if(submit_it) {
				if(len > 0 || short_command) {
			    		if( xQueueSend(xCmdQueue, &next_command, 0) != pdTRUE) {
			    			DumpStringToUSB("Queue full, command can't be processed.\n");
			    		}
				}
		    		len=0;
		    		submit_it=0;
		    		short_command=1;
			} else if( len>0 || next_command.command[len] != ':') len++;
			if(len >= MAX_CMD_LEN-1) {
				DumpStringToUSB("ERROR: Command too long. Ignored.");
				len=0;
			}
	    	}
    	}
}

static portBASE_TYPE field_meter_enabled = 0;
#define FIELD_METER_WIDTH 80
#define FIELD_METER_MAX_VALUE 160
#define FIELD_METER_SCALE_FACTOR (FIELD_METER_MAX_VALUE/FIELD_METER_WIDTH)
// A task to print the field strength as a bar graph
void vFieldMeter(void *pvParameters) {
	(void) pvParameters;
	char meter_string[FIELD_METER_WIDTH+2];
	
	while(1) {
		if(xSemaphoreTake(xFieldMeterMutex, portMAX_DELAY)) {
			int i,ad_value = adc_get_field_strength();
			meter_string[0] = '\r';
			
			for(i=0; i<FIELD_METER_WIDTH; i++) 
				meter_string[i+1] = 
					(ad_value / FIELD_METER_SCALE_FACTOR < i) ? 
					' ' : 
					((i*FIELD_METER_SCALE_FACTOR)%10==0 ? 
						(((i*FIELD_METER_SCALE_FACTOR)/10)%10)+'0' : '#' );
			meter_string[i+1] = 0;
			usb_print_string(meter_string);
			
			vTaskDelay(100*portTICK_RATE_MS);
			if(field_meter_enabled == 1) xSemaphoreGive(xFieldMeterMutex);
		}
	}
}

void startstop_field_meter(void) {
	if(field_meter_enabled) {
		field_meter_enabled = 0;
	} else {
		field_meter_enabled = 1;
		xSemaphoreGive(xFieldMeterMutex);
	}
}

portBASE_TYPE vCmdInit() {
	unsigned int i;
	for(i=0; i<sizeof(PIO_PINS)/sizeof(PIO_PINS[0]); i++) {
		if(PIO_PINS[i].dynpin == DYNAMIC_PIN_PLL_LOCK) {
			PIO_PINS[i].pin = OPENPICC->PLL_LOCK;
			PIO_PINS[i].dynpin = 0;
		}
	}
	
	/* FIXME Maybe modify to use pointers? */
	xCmdQueue = xQueueCreate( 10, sizeof(cmd_type) );
	if(xCmdQueue == 0) {
		return 0;
	}
	vSemaphoreCreateBinary( xFieldMeterMutex );
	if(xFieldMeterMutex == 0) {
		return 0;
	}
	xSemaphoreTake(xFieldMeterMutex, portMAX_DELAY);
	
	if(xTaskCreate(vCmdCode, (signed portCHAR *)"CMD", TASK_CMD_STACK, NULL, 
		TASK_CMD_PRIORITY, &xCmdTask) != pdPASS) {
		return 0;
	}
	
	if(xTaskCreate(vCmdRecvUsbCode, (signed portCHAR *)"CMDUSB", 128, NULL, 
		TASK_CMD_PRIORITY, &xCmdRecvUsbTask) != pdPASS) {
		return 0;
	}
	
	if(xTaskCreate(vFieldMeter, (signed portCHAR *)"FIELD METER", 128, NULL, 
		TASK_CMD_PRIORITY, &xFieldMeterTask) != pdPASS) {
		return 0;
	}
	
	return 1;
}
