/*
MEMORY_WATCH.C

Write tracking for guest memory that the renderer caches, and the report of
a genuine crash.

Textures live in the Xbox contiguous window, where the game (or its
streaming threads) can rewrite them at any time. Instead of hashing their
contents every frame, the pages behind a cached texture are made read-only;
the first write faults, the handler records a new generation for the page,
makes it writable again and lets the write proceed. A cache entry is stale
when any of its pages has a generation newer than the entry. Tracking works
in host pages (16 KB on Apple silicon), so a write next to a texture can
mark it stale; that costs a re-upload, never a missed change.

Writes that the kernel performs on the game's behalf (read() into a
buffer) would fail with EFAULT instead of faulting, so the file layer reads
guest memory through a bounce buffer (xbox_files.c). Unprotecting ahead of
such a write (memory_watch_prepare_write) is not enough on its own: the
renderer can protect the pages again before the kernel writes them.

Every other fault is a crash: it is reported to the terminal and to the
game's debug.txt (which a player sends), then handed to whatever handled
the signal before.
*/

#include "platform.h"

#include <execinfo.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ucontext.h>
#include <unistd.h>

/* enough pages for 4 KB host pages; larger pages use fewer */
#define WATCH_PAGE_COUNT_MAXIMUM (PLATFORM_CONTIGUOUS_SIZE / 0x1000U)

static unsigned char page_protected[WATCH_PAGE_COUNT_MAXIMUM];
static unsigned long page_generation[WATCH_PAGE_COUNT_MAXIMUM];
static volatile unsigned long current_generation = 1;
static unsigned int watch_page_size = 0x1000U;
static BOOL watch_active = FALSE;
static struct sigaction previous_segv_action;
static struct sigaction previous_bus_action;

/* ---------- pages (Xbox addresses) */

static BOOL window_contains(unsigned long address)
{
	return address >= PLATFORM_CONTIGUOUS_BASE && address - PLATFORM_CONTIGUOUS_BASE < PLATFORM_CONTIGUOUS_SIZE;
}

static unsigned long page_index(unsigned long address)
{
	return (address - PLATFORM_CONTIGUOUS_BASE) / watch_page_size;
}

static void *page_pointer(unsigned long page)
{
	return xbox_pointer(PLATFORM_CONTIGUOUS_BASE + page * watch_page_size);
}

/* the Xbox address of a host pointer; FALSE outside the Xbox's memory */
static BOOL guest_address(const void *pointer, unsigned long long *address)
{
#ifdef HALO_64BIT
	*address = (unsigned long long)(uintptr_t)pointer - XBOX_ADDRESS_SPACE_BASE;
	return (uintptr_t)pointer >= XBOX_ADDRESS_SPACE_BASE && *address < XBOX_ADDRESS_SPACE_SIZE;
#else
	*address = (unsigned long long)(uintptr_t)pointer;
	return TRUE;
#endif
}

/* the watched pages [first, last] a range of Xbox addresses touches, its
end clipped to the window; FALSE if none */
static BOOL page_range(unsigned long address, unsigned long size, unsigned long *first, unsigned long *last)
{
	unsigned long end;

	if (!size || !window_contains(address))
		return FALSE;
	end = address + size - 1;
	if (end < address || !window_contains(end))
		end = PLATFORM_CONTIGUOUS_BASE + PLATFORM_CONTIGUOUS_SIZE - 1;
	*first = page_index(address);
	*last = page_index(end);
	return TRUE;
}

/* the watched pages [first, last] a host range touches, clipped to the
window; FALSE if none */
static BOOL host_page_range(void *pointer, unsigned long size, unsigned long *first, unsigned long *last)
{
	unsigned long long window_end = (unsigned long long)PLATFORM_CONTIGUOUS_BASE + PLATFORM_CONTIGUOUS_SIZE;
	unsigned long long begin, end;

	if (!size || !guest_address(pointer, &begin))
		return FALSE;
	end = begin + size;
	if (end <= PLATFORM_CONTIGUOUS_BASE || begin >= window_end)
		return FALSE;
	if (begin < PLATFORM_CONTIGUOUS_BASE)
		begin = PLATFORM_CONTIGUOUS_BASE;
	if (end > window_end)
		end = window_end;
	return page_range((unsigned long)begin, (unsigned long)(end - begin), first, last);
}

static void mark_written(unsigned long page)
{
	page_generation[page] = __sync_add_and_fetch(&current_generation, 1);
	page_protected[page] = 0;
	mprotect(page_pointer(page), watch_page_size, PROT_READ | PROT_WRITE);
}

/* ---------- crashes */

/* errors.c's: debug.txt (a player sends it; the terminal's lines may be
gone) */
void write_to_error_file(char *string, unsigned char date);

/* a line of a crash report (ending in a newline) to debug.txt, best effort:
the crash may be in the middle of writing it */
static void crash_debug_line(const char *line)
{
	char text[200];
	size_t length = strcspn(line, "\n");

	if (length > sizeof(text) - 3)
		length = sizeof(text) - 3;
	memcpy(text, line, length);
	memcpy(text + length, "\r\n", 3);
	write_to_error_file(text, 1);
}

/* ... and to the terminal */
static void crash_line(const char *line)
{
	write(STDERR_FILENO, line, strlen(line));
	crash_debug_line(line);
}

/* the faulting instruction, frame and stack pointers */
static void crash_registers(void *context, uintptr_t *pc, uintptr_t *frame, uintptr_t *stack)
{
	ucontext_t *ucontext = context;

	*pc = *frame = *stack = 0;
#if defined(__APPLE__) && defined(__aarch64__)
	*pc = ucontext->uc_mcontext->__ss.__pc;
	*frame = ucontext->uc_mcontext->__ss.__fp;
	*stack = ucontext->uc_mcontext->__ss.__sp;
#elif defined(__APPLE__) && defined(__x86_64__)
	*pc = ucontext->uc_mcontext->__ss.__rip;
	*frame = ucontext->uc_mcontext->__ss.__rbp;
	*stack = ucontext->uc_mcontext->__ss.__rsp;
#elif defined(__linux__) && defined(__aarch64__)
	*pc = ucontext->uc_mcontext.pc;
	*frame = ucontext->uc_mcontext.regs[29];
	*stack = ucontext->uc_mcontext.sp;
#elif defined(__linux__) && defined(__x86_64__)
	*pc = ucontext->uc_mcontext.gregs[REG_RIP];
	*frame = ucontext->uc_mcontext.gregs[REG_RBP];
	*stack = ucontext->uc_mcontext.gregs[REG_RSP];
#elif defined(__linux__) && defined(__i386__)
	*pc = ucontext->uc_mcontext.gregs[REG_EIP];
	*frame = ucontext->uc_mcontext.gregs[REG_EBP];
	*stack = ucontext->uc_mcontext.gregs[REG_ESP];
#else
	(void)ucontext;
#endif
}

static void report_crash(siginfo_t *information, void *context)
{
	char line[200];
	void *frames[48];
	uintptr_t pc, frame, stack;
	int count, index;

	crash_registers(context, &pc, &frame, &stack);
	snprintf(line, sizeof(line), "halo-linux: fault at %p, pc %llx fp %llx sp %llx\n", information->si_addr,
		(unsigned long long)pc, (unsigned long long)frame, (unsigned long long)stack);
	crash_line(line);
	if (stack)
	{
		/* the return address a call through a bad pointer left behind */
		const uintptr_t *words = (const uintptr_t *)stack;

		snprintf(line, sizeof(line), "halo-linux: stack %llx %llx %llx %llx %llx %llx\n", (unsigned long long)words[0],
			(unsigned long long)words[1], (unsigned long long)words[2], (unsigned long long)words[3],
			(unsigned long long)words[4], (unsigned long long)words[5]);
		crash_line(line);
	}
	/* (the terminal gets the frames' symbols, debug.txt their addresses) */
	count = backtrace(frames, 48);
	backtrace_symbols_fd(frames, count, STDERR_FILENO);
	for (index = 0; index < count; index++)
	{
		snprintf(line, sizeof(line), "halo-linux: called from %p\n", frames[index]);
		crash_debug_line(line);
	}
}

/* the signal handed to whatever handled it before; returning re-executes
the faulting instruction under that handler */
static void chain(struct sigaction *previous, int signal_number, siginfo_t *information, void *context)
{
	sigaction(signal_number, previous, NULL);
	if (previous->sa_flags & SA_SIGINFO)
	{
		if (previous->sa_sigaction)
			previous->sa_sigaction(signal_number, information, context);
	}
	else if (previous->sa_handler != SIG_DFL && previous->sa_handler != SIG_IGN)
	{
		previous->sa_handler(signal_number);
	}
}

/* macOS reports a write to a read-only page as SIGBUS, Linux as SIGSEGV */
static void fault_handler(int signal_number, siginfo_t *information, void *context)
{
	/* (a fault while reporting one, a bad stack's: straight to the previous
	handler) */
	static volatile int reporting;
	unsigned long long address;

	if (guest_address(information->si_addr, &address) && window_contains((unsigned long)address) &&
		page_protected[page_index((unsigned long)address)])
	{
		mark_written(page_index((unsigned long)address));
		return;
	}
	if (!reporting)
	{
		reporting = 1;
		report_crash(information, context);
	}
	chain(signal_number == SIGBUS ? &previous_bus_action : &previous_segv_action, signal_number, information, context);
}

/* ---------- the watch */

void memory_watch_initialize(void)
{
	struct sigaction action;

	if (watch_active)
		return;
#ifdef HALO_64BIT
	watch_page_size = platform_host_page_size;
#endif
	memset(&action, 0, sizeof(action));
	action.sa_sigaction = fault_handler;
	action.sa_flags = SA_SIGINFO | SA_NODEFER;
	sigemptyset(&action.sa_mask);
	if (sigaction(SIGSEGV, &action, &previous_segv_action) == 0 &&
		sigaction(SIGBUS, &action, &previous_bus_action) == 0)
	{
		watch_active = TRUE;
	}
}

void memory_watch_protect(unsigned long address, unsigned long size)
{
	unsigned long first, last, page;

	if (!watch_active || !page_range(address, size, &first, &last))
		return;
	for (page = first; page <= last; page++)
	{
		if (!page_protected[page])
		{
			page_protected[page] = 1;
			mprotect(page_pointer(page), watch_page_size, PROT_READ);
		}
	}
}

unsigned long memory_watch_generation(unsigned long address, unsigned long size)
{
	unsigned long first, last, page, newest = 0;

	if (!page_range(address, size, &first, &last))
		return 0;
	for (page = first; page <= last; page++)
	{
		if (page_generation[page] > newest)
			newest = page_generation[page];
	}
	return newest;
}

unsigned long memory_watch_serial(void)
{
	return current_generation;
}

void memory_watch_prepare_write(void *address, unsigned long size)
{
	unsigned long first, last, page;

	if (!watch_active || !host_page_range(address, size, &first, &last))
		return;
	for (page = first; page <= last; page++)
	{
		if (page_protected[page])
			mark_written(page);
	}
}

void memory_watch_forget(void *address, unsigned long size)
{
	unsigned long first, last, page;

	if (!host_page_range(address, size, &first, &last))
		return;
	for (page = first; page <= last; page++)
	{
		page_protected[page] = 0;
		page_generation[page] = __sync_add_and_fetch(&current_generation, 1);
	}
}
