#ifndef _RC632_HIGHLEVEL_H
#define _RC632_HIGHLEVEL_H

#include <sys/types.h>
#include <librfid/rfid_asic.h>

int 
rc632_turn_on_rf(struct rfid_asic_handle *handle);

int 
rc632_turn_off_rf(struct rfid_asic_handle *handle);

int
rc632_read_eeprom(struct rfid_asic_handle *handle, u_int16_t addr, u_int8_t len,
		  u_int8_t *recvbuf);

int rc632_get_serial(struct rfid_asic_handle *handle,
		     u_int32_t *serial);
#endif /* _RC632_HIGHLEVEL_H */
