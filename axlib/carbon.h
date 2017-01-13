#ifndef AXLIB_CARBON_H
#define AXLIB_CARBON_H

#include <Carbon/Carbon.h>
#include <string>

struct carbon_event_handler
{
    EventTargetRef EventTarget;
    EventHandlerUPP EventHandler;
    EventTypeSpec EventType[2];
    EventHandlerRef CurHandler;
};

bool AXLibInitializeCarbonEventHandler(carbon_event_handler *Carbon);
void CarbonWhitelistProcess(std::string Name);

#endif
