#pragma once

#ifdef __APPLE__
#include <SDL3/SDL.h>

// Initializes the macOS native menu bar with File -> Load, Exit
void MacMenu_Init();

// Sets the SDL custom event type to be posted upon selecting File->Load
void MacMenu_SetLoadEventType(Uint32 type);

#endif
