#ifndef KWM_H
#define KWM_H

#include "types.h"

extern "C" bool CGSIsSecureEventInputSet(void);
extern "C" void NSApplicationLoad(void);

void KwmQuit();
void KwmReloadConfig();

#endif
