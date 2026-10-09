#pragma once
/* Host-only CDVD transport for testing the real, unmodified PS2SDK parser. */
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef struct { u8 trycount, spindlctrl, datapattern, pad; } sceCdRMode;
enum { SCECdPSCD = 1, SCECdPSCDDA, SCECdPS2CD, SCECdPS2CDDA,
       SCECdPS2DVD, SCECdDVDV, SCECdSpinStm, SCECdSecS2048,
       SCECdINoD, SCECdTrayCheck };
int sceCdInit(int);
int sceCdRead(u32, u32, void *, sceCdRMode *);
int sceCdReadDVDV(u32, u32, void *, sceCdRMode *);
int sceCdSync(int);
int sceCdGetError(void);
int sceCdGetDiskType(void);
int sceCdDiskReady(int);
int sceCdTrayReq(int, u32 *);
