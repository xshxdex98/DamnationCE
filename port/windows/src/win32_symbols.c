/*
WIN32_SYMBOLS.C

The names of the functions in a crash's call stack, for the log
(source/cseries/stack_walk_windows.c): the Xbox named them from its map
file, which the native build has none of. Windows' symbol library
(dbghelp.dll) names them from halo.pdb beside the executable, which the
build writes. It is loaded only when a stack is printed, and without it, or
without the .pdb, the log shows bare addresses as before.
*/

#include <windows.h>
#include <stdio.h>
#include <string.h>

/* (dbghelp.h's, declared here to load the library only when it's needed) */
typedef struct
{
	ULONG SizeOfStruct;
	ULONG TypeIndex;
	ULONG64 Reserved[2];
	ULONG Index;
	ULONG Size;
	ULONG64 ModBase;
	ULONG Flags;
	ULONG64 Value;
	ULONG64 Address;
	ULONG Register;
	ULONG Scope;
	ULONG Tag;
	ULONG NameLen;
	ULONG MaxNameLen;
	CHAR Name[1];
} symbol_info;

typedef struct
{
	DWORD SizeOfStruct;
	PVOID Key;
	DWORD LineNumber;
	PCHAR FileName;
	DWORD64 Address;
} line_info;

#define SYMOPT_UNDNAME 0x00000002
#define SYMOPT_DEFERRED_LOADS 0x00000004
#define SYMOPT_LOAD_LINES 0x00000010
#define MAXIMUM_SYMBOL_NAME 255

typedef DWORD (WINAPI *sym_set_options_proc)(DWORD options);
typedef BOOL (WINAPI *sym_initialize_proc)(HANDLE process, PCSTR search_path, BOOL invade_process);
typedef BOOL (WINAPI *sym_from_addr_proc)(HANDLE process, DWORD64 address, PDWORD64 displacement, symbol_info *symbol);
typedef BOOL (WINAPI *sym_get_line_from_addr_proc)(HANDLE process, DWORD64 address, PDWORD displacement,
	line_info *line);

static struct
{
	BOOL tried;
	sym_from_addr_proc from_addr;
	sym_get_line_from_addr_proc get_line;
} symbols;

/* dbghelp.dll loaded and set to read halo.pdb; FALSE if it can't be */
static BOOL symbols_ready(void)
{
	HMODULE library;
	sym_set_options_proc set_options;
	sym_initialize_proc initialize;

	if (symbols.tried)
		return symbols.from_addr != NULL;
	symbols.tried = TRUE;
	library = LoadLibraryA("dbghelp.dll");
	if (!library)
		return FALSE;
	set_options = (sym_set_options_proc)(void *)GetProcAddress(library, "SymSetOptions");
	initialize = (sym_initialize_proc)(void *)GetProcAddress(library, "SymInitialize");
	symbols.get_line = (sym_get_line_from_addr_proc)(void *)GetProcAddress(library, "SymGetLineFromAddr64");
	if (!set_options || !initialize)
		return FALSE;
	set_options(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
	/* (the executable's folder is searched for its .pdb) */
	if (!initialize(GetCurrentProcess(), NULL, TRUE))
		return FALSE;
	symbols.from_addr = (sym_from_addr_proc)(void *)GetProcAddress(library, "SymFromAddr");
	return symbols.from_addr != NULL;
}

/* the function at address and its source line ("unit_update+0x1c
(units.c:2410)"), into text; FALSE if it can't be named */
int win32_describe_address(unsigned long address, char *text, unsigned long size)
{
	union
	{
		symbol_info info;
		char storage[sizeof(symbol_info) + MAXIMUM_SYMBOL_NAME];
	} symbol;
	line_info line;
	DWORD64 displacement = 0;
	DWORD line_displacement = 0;
	const char *file;

	if (!symbols_ready())
		return FALSE;
	ZeroMemory(&symbol, sizeof(symbol));
	symbol.info.SizeOfStruct = sizeof(symbol.info);
	symbol.info.MaxNameLen = MAXIMUM_SYMBOL_NAME;
	if (!symbols.from_addr(GetCurrentProcess(), address, &displacement, &symbol.info))
		return FALSE;
	ZeroMemory(&line, sizeof(line));
	line.SizeOfStruct = sizeof(line);
	if (symbols.get_line && symbols.get_line(GetCurrentProcess(), address, &line_displacement, &line) &&
		line.FileName)
	{
		file = strrchr(line.FileName, '\\');
		snprintf(text, size, "%s+0x%llx (%s:%lu)", symbol.info.Name, (unsigned long long)displacement,
			file ? file + 1 : line.FileName, (unsigned long)line.LineNumber);
	}
	else
	{
		snprintf(text, size, "%s+0x%llx", symbol.info.Name, (unsigned long long)displacement);
	}
	return TRUE;
}
