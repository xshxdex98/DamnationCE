/*
PLATFORM.H

Shared internals of the Linux platform layer, which implements the Xbox SDK
(XAPI, Direct3D 8, DirectSound, XNet, ...) and MSVC runtime interfaces that
the game calls. Files including this header are compiled with the game's
ABI and see the XDK declarations, so every definition is type-checked
against the SDK prototype it replaces.
*/

#ifndef __HALO_LINUX_PLATFORM_H
#define __HALO_LINUX_PLATFORM_H

/* The host's own declarations come first. The XDK's Winsock names and
types (select, fd_set, timeval, ...) are then moved out of glibc's way while
the SDK headers are read, exactly as game code sees them. */
#include <pthread.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>

/* as cseries_windows.h does, so xtl.h also declares the debug keyboard */
#define DEBUG_KEYBOARD
#include "halo_linux_winsock_names.h"
#include <xtl.h>
#include <xbdm.h>
#include <d3d8perf.h>
#define HALO_LINUX_WINSOCK_NAMES_UNDEFINE
#include "halo_linux_winsock_names.h"
#undef HALO_LINUX_WINSOCK_NAMES_UNDEFINE

/* ---------- logging */

/* prints "halo-linux: <message>" to stderr */
void platform_log(const char *format, ...) __attribute__((format(printf, 1, 2)));

/* reports an SDK entry point the Linux port does not implement, once per
name; the caller then behaves as a failing or empty call */
void platform_unimplemented(const char *name);

#define PLATFORM_UNIMPLEMENTED() platform_unimplemented(__func__)

/* ---------- errors */

/* translate errno into a Win32 error code and store it as GetLastError() */
DWORD platform_set_last_error_from_errno(int error_number);

/* ---------- handles */

enum platform_handle_type
{
	_platform_handle_file = 1,
	_platform_handle_event,
	_platform_handle_mutex,
	_platform_handle_thread,
	_platform_handle_find,
	_platform_handle_signature,
	_platform_handle_other,
};

/* Every HANDLE the layer returns points at one of these. Waitable objects
(events, mutexes, threads) share the embedded lock and condition. */
struct platform_handle
{
	unsigned long signature;
	long type;
	pthread_mutex_t lock;
	pthread_cond_t condition;
	BOOL signaled;
	BOOL manual_reset;
	/* mutexes */
	pthread_t owner;
	long recursion;
	/* type specific data */
	void *data;
	void (*destroy)(struct platform_handle *handle);
};

struct platform_handle *platform_handle_new(long type, void *data,
	void (*destroy)(struct platform_handle *handle));
/* returns NULL (and sets ERROR_INVALID_HANDLE) unless handle has this type */
struct platform_handle *platform_handle_get(HANDLE handle, long type);
/* marks a waitable handle signaled and wakes waiters */
void platform_handle_signal(struct platform_handle *handle);

/* ---------- asynchronous procedure calls (ReadFileEx completions) */

typedef void (*platform_apc_routine)(void *context0, void *context1, void *context2);
/* queue a routine for the calling thread's next alertable wait */
void platform_queue_apc(platform_apc_routine routine, void *context0, void *context1, void *context2);
/* run the calling thread's queued routines; returns how many ran */
long platform_run_apcs(void);

/* ---------- paths */

/* Translate an Xbox path (d:\maps\a10.map, t:\..., u:\..., z:\...) into a
host path below the data root. Components are matched case-insensitively
against what exists on disk, as the Xbox file system is case-insensitive. */
void platform_translate_path(const char *xbox_path, char *host_path, unsigned long host_path_size);
const char *platform_data_root(void);
/* the Halo Custom Edition install the Xbox drive h:\ is (paths.custom_edition),
or "" when none is set or it has no maps folder */
const char *platform_custom_edition_root(void);
/* on the desktop, when the data root has no maps folder: offers to copy it
out of an Xbox disc image into destination (sdl_platform.c), and quits if
the player declines; nonzero once destination has one */
BOOL platform_offer_game_data(const char *destination);
/* the macOS application's folder for its data and settings, when the game
runs as an application (DamnationCE.app): ~/Library/Application
Support/DamnationCE, made if need be, into path; 0 otherwise (and on
other systems). An application's own files are not written: it is signed,
and may be where its player cannot write (port_config.c) */
int platform_app_folder(char *path, unsigned long size);
const char *platform_save_root(void);
#ifdef HALO_GAME_BROWSER
/* a web page opened in the web browser (from any thread: the main thread
opens it) */
void platform_open_url(const char *url);
#endif

/* ---------- contiguous ("physical") memory

The Xbox maps physical memory at virtual 0x80000000 + P. The layer commits
that window of the Xbox address space (cseries/xbox_address.h) at start-up and
hands out page-granular blocks from it, so the physical/virtual arithmetic
the game and Direct3D rely on keeps working. PLATFORM_CONTIGUOUS_BASE is an
Xbox address. */

#define PLATFORM_CONTIGUOUS_BASE 0x80000000U
#ifdef HALO_ANDROID
/* 128 MB, a development kit's: Android's guest image is linked just above
the window (port/android/include/halo_android_abi.h) */
#define PLATFORM_CONTIGUOUS_SIZE 0x08000000U
#else
/* 512 MB on the desktop builds: Custom Edition maps need more texture,
model and sound memory than the Xbox's (coldsnap ran out of 128 MB). Only
pages in use are committed. port/windows/src/win32_memory_watch.c must use
the same size. */
#define PLATFORM_CONTIGUOUS_SIZE 0x20000000U
#endif
#define PLATFORM_ANY_PHYSICAL_ADDRESS 0xffffffffU

/* the host's page size, which protection works in (4 KB or more) */
extern unsigned int platform_host_page_size;

/* returns NULL on failure; physical_address places the block exactly */
void *platform_contiguous_alloc(unsigned long size, unsigned long alignment,
	unsigned long physical_address, DWORD protect);
void platform_contiguous_free(void *address);
BOOL platform_is_contiguous(const void *address);
/* write guest memory like DMA does, ignoring its page protection */
void platform_contiguous_write(void *destination, const void *source, size_t size);
#define PLATFORM_PHYSICAL_TO_VIRTUAL(physical) xbox_pointer((unsigned long)(physical) | PLATFORM_CONTIGUOUS_BASE)
#define PLATFORM_VIRTUAL_TO_PHYSICAL(address) ((unsigned long)XBOX_ADDRESS(address) & ~PLATFORM_CONTIGUOUS_BASE)

/* ---------- the game's heap (xbox_heap.c), in the Xbox address space */

void *xbox_heap_allocate(size_t size, BOOL zero);
void xbox_heap_free(void *pointer);
size_t xbox_heap_capacity(void *pointer);
BOOL xbox_heap_contains(const void *pointer);

/* The window Halo Custom Edition tag data are linked to (0x40440000), for
the experimental Custom Edition map loading: reserved at start-up when the
game.custom_edition setting is on, else NULL (also declared for the game in
halo_linux_source_fixups.h). */
void *halo_custom_edition_tag_cache(void);
/* Which textures hold their channels where Halo PC keeps them, for the same
(xbox_textures.c; also declared for the game there) */
void halo_custom_edition_texels_channels(const void *texels, unsigned char channel_order);
void halo_custom_edition_texels_forget(void);
/* whether a HUD meter is being drawn, for the texels drawn in a meter's
channel order only then (rasterizer_xbox_dynavobgeom.c) */
void halo_hud_meter_drawing(int drawing);

/* The window Halo Custom Edition tag data are linked to (0x40440000):
reserved at start-up when the game.custom_edition setting is on, else NULL
(also declared for the game in halo_linux_source_fixups.h). */
void *halo_custom_edition_tag_cache(void);
/* which textures hold their channels where Halo PC keeps them
(xbox_textures.c; also declared for the game there) */
void halo_custom_edition_texels_channels(const void *texels, unsigned char channel_order);
void halo_custom_edition_texels_forget(void);

/* ---------- guest memory write tracking (memory_watch.c)

Pages of the contiguous window that the renderer has cached (textures) are
write-protected; the first write marks them written and unprotects them.
Page generations let a cache entry tell whether any of its pages changed
since it was built. Tracking works in host pages; ranges are given as Xbox
addresses. */

void memory_watch_initialize(void);
void memory_watch_protect(unsigned long address, unsigned long size);
/* newest write generation of any page in the range */
unsigned long memory_watch_generation(unsigned long address, unsigned long size);
/* changes whenever any page's generation does: while it stays the same, so
do all generations */
unsigned long memory_watch_serial(void);
/* call before the host itself (read(), the kernel) writes into the range */
void memory_watch_prepare_write(void *address, unsigned long size);
/* the range was remapped or reprotected: treat it as written and unwatched */
void memory_watch_forget(void *address, unsigned long size);
/* a new frame starts (Present): tracking that compares page contents
(Android under ARM translation, host_watch_hash.h) hashes a page at most
once a frame, so it sees a write in the next frame, not at once */
void memory_watch_begin_frame(void);

/* ---------- time */

/* sleep until a CLOCK_MONOTONIC deadline (clock_nanosleep with
TIMER_ABSTIME, which macOS lacks) */
static inline void platform_sleep_until(const struct timespec *deadline)
{
#ifndef __APPLE__
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, deadline, NULL);
#else
	for (;;)
	{
		struct timespec now, remaining;

		clock_gettime(CLOCK_MONOTONIC, &now);
		remaining.tv_sec = deadline->tv_sec - now.tv_sec;
		remaining.tv_nsec = deadline->tv_nsec - now.tv_nsec;
		if (remaining.tv_nsec < 0)
		{
			remaining.tv_nsec += 1000000000L;
			remaining.tv_sec--;
		}
		if (remaining.tv_sec < 0)
			return;
		if (nanosleep(&remaining, NULL) == 0)
			return;
	}
#endif
}

/* 100 ns intervals since 1601-01-01, as FILETIME uses */
void platform_unix_time_to_filetime(unsigned long seconds, unsigned long nanoseconds, FILETIME *file_time);
void platform_filetime_to_unix_time(const FILETIME *file_time, unsigned long *seconds, unsigned long *nanoseconds);

#endif
