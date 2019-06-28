/* See COPYRIGHT for copyright information. */

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/monitor.h>
#include <kern/console.h>

#include <inc/uefi.h>

// Test the stack backtrace function (lab 1 only)
void
test_backtrace(int x)
{
	cprintf("entering test_backtrace %d\n", x);
	if (x > 0)
		test_backtrace(x-1);
	else
		mon_backtrace(0, 0, 0);
	cprintf("leaving test_backtrace %d\n", x);
}

void
i386_init(void)
{
	extern char edata[], end[];

	// Before doing anything else, complete the ELF loading process.
	// Clear the uninitialized global data (BSS) section of our program.
	// This ensures that all static/global variables start out zero.

  LOADER_PARAMS* uefi_params = UEFI_LP; //ugly way to save uefi params pointer
	memset(edata, 0, end - edata);
  UEFI_LP = uefi_params; //ugly way to save uefi params pointer

	// Initialize the console.
	// Can't call cprintf until after we do this!
	cons_init();
/*
  cprintf("get buffer pointer addr %p\n", UEFI_LP);
  cprintf("frame buffer pointer  %p\n", (void*)(uintptr_t)(UEFI_LP->GPU_Configs[0].GPUArray[0].FrameBufferBase));
  cprintf("resolution horiz %d vert %d\n", UEFI_LP->GPU_Configs[0].GPUArray[0].Info->HorizontalResolution,
                                           UEFI_LP->GPU_Configs[0].GPUArray[0].Info->VerticalResolution);

  cprintf("Memory_Map_Descriptor_Size pointer addr %p\r\n", &(UEFI_LP->Memory_Map_Descriptor_Size));
  cprintf("Memory_Map pointer addr %p\r\n", &(UEFI_LP->Memory_Map));
  cprintf("Memory_Map_Size pointer addr %p\r\n", &(UEFI_LP->Memory_Map_Size));
  cprintf("Kernel_BaseAddress pointer addr %p\r\n", &(UEFI_LP->Kernel_BaseAddress));
  cprintf("Kernel_Pages pointer addr %p\r\n", &(UEFI_LP->Kernel_Pages));
  cprintf("ESP_Root_Device_Path pointer addr %p\r\n", &(UEFI_LP->ESP_Root_Device_Path));
  cprintf("ESP_Root_Size pointer addr %p\r\n", &(UEFI_LP->ESP_Root_Size));

  cprintf("Kernel_Path pointer addr %p\r\n", &(UEFI_LP->Kernel_Path));
  cprintf("Kernel_Path_Size pointer addr %p\r\n", &(UEFI_LP->Kernel_Path_Size));
  cprintf("Kernel_Options pointer addr %p\r\n", &(UEFI_LP->Kernel_Options));
  cprintf("Kernel_Options_Size pointer addr %p\r\n", &(UEFI_LP->Kernel_Options_Size));
  cprintf("RTServices pointer addr %p\r\n", &(UEFI_LP->RTServices));
  cprintf("GPU_Configs pointer addr %p\n", &(UEFI_LP->GPU_Configs));
*/
	cprintf("6828 decimal is %o octal!\n", 6828);
  cprintf("alignof uint64_t is %d\n", __alignof(uint64_t));
	// Test the stack backtrace function (lab 1 only)
	test_backtrace(5);

	// Drop into the kernel monitor.
	while (1)
		monitor(NULL);
}


/*
 * Variable panicstr contains argument to first call to panic; used as flag
 * to indicate that the kernel has already called panic.
 */
const char *panicstr;

/*
 * Panic is called on unresolvable fatal errors.
 * It prints "panic: mesg", and then enters the kernel monitor.
 */
void
_panic(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	if (panicstr)
		goto dead;
	panicstr = fmt;

	// Be extra sure that the machine is in as reasonable state
	__asm __volatile("cli; cld");

	va_start(ap, fmt);
	cprintf("kernel panic at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);

dead:
	/* break into the kernel monitor */
	while (1)
		monitor(NULL);
}

/* like panic, but don't */
void
_warn(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	va_start(ap, fmt);
	cprintf("kernel warning at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);
}
