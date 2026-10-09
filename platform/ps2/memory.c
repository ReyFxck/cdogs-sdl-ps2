/* EE heap diagnostics for actual emulator/hardware logs, not host estimates. */
#include "ps2_platform.h"
#include <stdio.h>
#ifdef _EE
#include <kernel.h>
#include <malloc.h>
#include <stdint.h>
extern void *_sbrk(size_t increment);
#endif

void CDogsPS2LogMemory(const char *stage)
{
#ifdef _EE
    const struct mallinfo info = mallinfo();
    const uintptr_t limit = (uintptr_t)EndOfHeap();
    const uintptr_t next = (uintptr_t)_sbrk(0);
    const unsigned uncommitted = limit > next ? (unsigned)(limit - next) : 0;
    printf("PS2: heap %s: used=%u free=%u bytes\n", stage,
        (unsigned)info.uordblks, (unsigned)info.fordblks + uncommitted);
#else
    (void)stage; /* Host tests do not have the EE memory map. */
#endif
}
