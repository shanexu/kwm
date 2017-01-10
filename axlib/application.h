#ifndef AXLIB_APPLICATION_H
#define AXLIB_APPLICATION_H

#include <Carbon/Carbon.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <map>
#include <vector>

#include "window.h"
#include "observer.h"

enum ax_application_flags
{
    AXApplication_Activate = (1 << 0),
    AXApplication_PrepIgnoreFocus = (1 << 1),
    AXApplication_IgnoreFocus = (1 << 2),
    AXApplication_RestoreFocus = (1 << 3),
};

struct ax_application
{
    AXUIElementRef Ref;
    std::string Name;
    pid_t PID;

    ProcessSerialNumber PSN;
    ax_observer Observer;
    uint32_t Flags;
    uint32_t Notifications;
    unsigned int Retries;

    ax_window *Focus;
    std::map<uint32_t, ax_window *> Windows;
    std::vector<ax_window *> NullWindows;
};

inline bool
AXLibHasFlags(ax_application *Application, uint32_t Flag)
{
    bool Result = Application->Flags & Flag;
    return Result;
}

inline void
AXLibAddFlags(ax_application *Application, uint32_t Flag)
{
    Application->Flags |= Flag;
}

inline void
AXLibClearFlags(ax_application *Application, uint32_t Flag)
{
    Application->Flags &= ~Flag;
}

ax_application *AXLibConstructApplication(pid_t PID, std::string Name);
void AXLibDestroyApplication(ax_application *Application);

bool AXLibInitializeApplication(pid_t PID);
void AXLibInitializedApplication(ax_application *Application);

void AXLibAddApplicationWindows(ax_application *Application);
void AXLibRemoveApplicationWindows(ax_application *Application);

ax_window *AXLibFindApplicationWindow(ax_application *Application, uint32_t WID);
void AXLibAddApplicationWindow(ax_application *Application, ax_window *Window);
void AXLibRemoveApplicationWindow(ax_application *Application, uint32_t WID);

void AXLibActivateApplication(ax_application *Application);
bool AXLibIsApplicationActive(ax_application *Application);
bool AXLibIsApplicationHidden(ax_application *Application);

#endif
