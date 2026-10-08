/*
TELNET_CONSOLE.H
*/

#ifndef __TELNET_CONSOLE_H
#define __TELNET_CONSOLE_H
#pragma once

/* ---------- prototypes/TELNET_CONSOLE.C */

void telnet_console_initialize(
	void);
void telnet_console_dispose(
	void);
void telnet_console_print(
	char *string);
void telnet_console_process(
	void);

#endif // __TELNET_CONSOLE_H
