/*
CRC.H
*/

#ifndef __CRC_H
#define __CRC_H
#pragma once

/* ---------- prototypes/CRC.C */

void crc_new(
	unsigned long *crc_reference);
void crc_checksum_buffer(
	unsigned long *crc_reference,
	void const *buffer,
	long buffer_size);

#endif // __CRC_H
