/* Desktop storefront discovery is deliberately unavailable on PS2. */
#pragma once
static inline void fsg_get_steam_game_path(char *out, const char *name)
{ (void)name; out[0] = '\0'; }
static inline void fsg_get_gog_game_path(char *out, const char *id)
{ (void)id; out[0] = '\0'; }
