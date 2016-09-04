#ifndef AXLIB_DOCK_H
#define AXLIB_DOCK_H

#include <stdint.h>

typedef int CGSConnectionID;
bool AXLibHijackUniversalOwner(CGSConnectionID Connection);

void AXLibSetWindowLevel(uint32_t WindowID, int32_t Level);
void AXLibSetWindowAlpha(uint32_t WindowID, double Alpha);

#endif
