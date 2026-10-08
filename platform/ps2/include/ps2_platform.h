#pragma once
#include <stdbool.h>
#include "config.h"

bool CDogsPS2InitPaths(int argc, char **argv);
const char *CDogsPS2DataRoot(void);
const char *CDogsPS2ConfigPath(const char *name);
bool CDogsPS2IsAbsolutePath(const char *path);
void CDogsPS2ResolvePath(const char *path, char *out);
void CDogsPS2DataPath(const char *path, char *out);
void CDogsPS2ApplyConfig(Config *config);
void CDogsPS2AddControllerMappings(void);
void CDogsPS2AudioPump(void);
