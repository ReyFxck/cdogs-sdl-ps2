#pragma once
/* PS2 debug streams have no POSIX terminal ioctls or ANSI colour state. */
enum { GREY, WHITE, GREEN, YELLOW, RED, LIGHTBLUE, BROWN, CYAN };
#define setStreamColor(stream, color) ((void)(stream), (void)(color))
#define resetStreamColor(stream) ((void)(stream))
#define saveStreamDefaultColor(stream) ((void)(stream))
