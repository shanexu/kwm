#include "application.h"
#include "element.h"
#include "sharedworkspace.h"
#include "event.h"
#include "display.h"
#include "axlib.h"

#define internal static
#define local_persist static

#define AX_APPLICATION_RETRIES 10
enum ax_application_notifications
{
    AXApplication_Notification_WindowCreated,
    AXApplication_Notification_WindowFocused,
    AXApplication_Notification_WindowMoved,
    AXApplication_Notification_WindowResized,
    AXApplication_Notification_WindowTitle,

    AXApplication_Notification_Count
};

internal inline CFStringRef
AXNotificationFromEnum(int Type)
{
    switch(Type)
    {
        case AXApplication_Notification_WindowCreated:
        {
            return kAXWindowCreatedNotification;
        } break;
        case AXApplication_Notification_WindowFocused:
        {
            return kAXFocusedWindowChangedNotification;
        } break;
        case AXApplication_Notification_WindowMoved:
        {
            return kAXWindowMovedNotification;
        } break;
        case AXApplication_Notification_WindowResized:
        {
            return kAXWindowResizedNotification;
        } break;
        case AXApplication_Notification_WindowTitle:
        {
            return kAXTitleChangedNotification;
        } break;
        default: { return NULL; /* NOTE(koekeishiya): Should never happen */ } break;
    }
}

internal inline ax_window *
AXLibGetWindowByRef(ax_application *Application, AXUIElementRef WindowRef)
{
    uint32_t WID = AXLibGetWindowID(WindowRef);
    return AXLibFindApplicationWindow(Application, WID);
}

internal inline void
AXLibDestroyInvalidWindow(ax_window *Window)
{
    AXLibRemoveObserverNotification(&Window->Application->Observer, Window->Ref, kAXUIElementDestroyedNotification);
    AXLibDestroyWindow(Window);
}

OBSERVER_CALLBACK(AXApplicationCallback)
{
    ax_application *Application = (ax_application *) Reference;

    if(CFEqual(Notification, kAXWindowCreatedNotification))
    {
        ax_window *Window = AXLibConstructWindow(Application, Element);
        if(AXLibAddObserverNotification(&Application->Observer, Window->Ref, kAXUIElementDestroyedNotification, Window) == kAXErrorSuccess)
        {
            AXLibAddApplicationWindow(Application, Window);

            /* NOTE(koekeishiya): Triggers an AXEvent_WindowCreated and passes a pointer to the new ax_window */
            uint32_t *WindowID = (uint32_t *) malloc(sizeof(uint32_t));
            *WindowID = Window->ID;
            AXLibConstructEvent(AXEvent_WindowCreated, WindowID, false);

            /* NOTE(koekeishiya): When a new window is created, we incorrectly receive the kAXFocusedWindowChangedNotification
                                  first, for some reason. We discard that notification and restore it when we have the window to work with. */
            AXApplicationCallback(Observer, Window->Ref, kAXFocusedWindowChangedNotification, Application);
        }
        else
        {
            /* NOTE(koekeishiya): This element is not destructible and cannot be an application window (?) */
            AXLibDestroyInvalidWindow(Window);
        }
    }
    else if(CFEqual(Notification, kAXUIElementDestroyedNotification))
    {
        /* NOTE(koekeishiya): If the destroyed UIElement is a window, remove it from the list. */
        ax_window *Window = (ax_window *) Reference;
        if(Window)
        {
            Window->Application->Focus = AXLibGetFocusedWindow(Window->Application);

            /* NOTE(koekeishiya): The callback is responsible for calling AXLibDestroyWindow(Window);
                                  and AXLibRemoveApplicationWindow(Window->Application, Window->ID); */
            uint32_t *WindowID = (uint32_t *) malloc(sizeof(uint32_t));
            *WindowID = Window->ID;
            AXLibConstructEvent(AXEvent_WindowDestroyed, WindowID, false);
        }
    }
    else if(CFEqual(Notification, kAXFocusedWindowChangedNotification))
    {
        /* NOTE(koekeishiya): This notification could be received before the window itself is created.
                              Make sure that the window actually exists before we notify our callback. */
        ax_window *Window = AXLibGetWindowByRef(Application, Element);
        if(Window)
        {
            /* NOTE(koekeishiya): If the currently focused window and the window requesting
             * focus are both on the same display, reset our IgnoreFocus flag. */
            if((AXLibHasFlags(Application, AXApplication_IgnoreFocus)) &&
               (Application->Focus) &&
               (AXLibWindowDisplay(Window) == AXLibWindowDisplay(Application->Focus)))
            {
                AXLibClearFlags(Application, AXApplication_IgnoreFocus);
            }

            if(AXLibHasFlags(Application, AXApplication_IgnoreFocus))
            {
                /* NOTE(koekeishiya): When Kwm tries to focus the window of an application that
                 * has windows open on multiple displays, OSX prioritizes the window on the
                 * active display. If this application has been flagged, we ignore the OSX notification. */
                AXLibClearFlags(Application, AXApplication_IgnoreFocus);

                /* NOTE(koekeishiya): Even if this request is ignored, OSX does make that window
                 * the focused window for the application in question. We restore focus back to
                 * the window that had focus before OSX decided to **** things up. */
                AXLibAddFlags(Application, AXApplication_RestoreFocus);
                AXLibSetWindowProperty(Application->Focus->Ref, kAXMainAttribute, kCFBooleanTrue);
            }
            else if(AXLibHasFlags(Application, AXApplication_RestoreFocus))
            {
                /* NOTE(koekeishiya): The window that we restore focus to is already marked as the focused
                 * window for this application as far as Kwm is concerned and we do not have to update our
                 * state, which is why we do not emit a new AXEvent_WindowFocused event. */
                AXLibClearFlags(Application, AXApplication_RestoreFocus);
            }
            else
            {
                /* NOTE(koekeishiya): When a window is deminimized, we receive a FocusedWindowChanged notification before the
                   window is visible. Only notify our callback when we know that we can interact with the window in question. */
                if(!AXLibHasFlags(Window, AXWindow_Minimized))
                {
                    uint32_t *WindowID = (uint32_t *) malloc(sizeof(uint32_t));
                    *WindowID = Window->ID;
                    AXLibConstructEvent(AXEvent_WindowFocused, WindowID, false);
                }

                /* NOTE(koekeishiya): If the application corresponding to this window is flagged for activation and
                                      the window is visible to the user, this should be the focused application. */
                if(AXLibHasFlags(Window->Application, AXApplication_Activate))
                {
                    AXLibClearFlags(Window->Application, AXApplication_Activate);
                    if(!AXLibHasFlags(Window, AXWindow_Minimized))
                    {
                        pid_t *ApplicationPID = (pid_t *) malloc(sizeof(pid_t));
                        *ApplicationPID = Window->Application->PID;
                        AXLibConstructEvent(AXEvent_ApplicationActivated, ApplicationPID, false);
                    }
                }
            }
        }
    }
    else if(CFEqual(Notification, kAXWindowMiniaturizedNotification))
    {
        /* NOTE(koekeishiya): Triggers an AXEvent_WindowMinimized and passes a pointer to the ax_window */
        ax_window *Window = (ax_window *) Reference;
        if(Window)
        {
            AXLibAddFlags(Window, AXWindow_Minimized);
            uint32_t *WindowID = (uint32_t *) malloc(sizeof(uint32_t));
            *WindowID = Window->ID;
            AXLibConstructEvent(AXEvent_WindowMinimized, WindowID, false);
        }
    }
    else if(CFEqual(Notification, kAXWindowDeminiaturizedNotification))
    {
        /* NOTE(koekeishiya): Triggers an AXEvent_WindowDeminimized and passes a pointer to the ax_window */
        ax_window *Window = (ax_window *) Reference;
        if(Window)
        {
            /* NOTE(koekeishiya): If a window was minimized before AXLib is initialized, the WindowID is
                                  reported as 0. We check if this window is one of these, update the
                                  WindowID and place it in our managed window list. */
            if(Window->ID == 0)
            {
                for(int Index = 0; Index < Window->Application->NullWindows.size(); ++Index)
                {
                    if(Window->Application->NullWindows[Index] == Window)
                    {
                        Window->Application->NullWindows.erase(Window->Application->NullWindows.begin() + Index);
                        break;
                    }
                }

                Window->ID = AXLibGetWindowID(Window->Ref);
                Window->Application->Windows[Window->ID] = Window;
            }

            /* NOTE(koekeishiya): kAXWindowDeminiaturized is sent before didActiveSpaceChange, when a deminimized
                                  window pulls you to the space of that window. If the active space of the display
                                  is not equal to the space of the window, we should ignore this event and let the
                                  next space changed event handle it. */

            AXLibClearFlags(Window, AXWindow_Minimized);
            ax_display *Display = AXLibWindowDisplay(Window);
            if(AXLibSpaceHasWindow(Window, Display->Space->ID))
            {
                uint32_t *WindowID = (uint32_t *) malloc(sizeof(uint32_t));
                *WindowID = Window->ID;
                AXLibConstructEvent(AXEvent_WindowDeminimized, WindowID, false);

                WindowID = (uint32_t *) malloc(sizeof(uint32_t));
                *WindowID = Window->ID;
                AXLibConstructEvent(AXEvent_WindowFocused, WindowID, false);
            }
        }
    }
    else if(CFEqual(Notification, kAXWindowMovedNotification))
    {
        /* NOTE(koekeishiya): Triggers an AXEvent_WindowMoved and passes a pointer to the ax_window */
        ax_window *Window = AXLibGetWindowByRef(Application, Element);
        if(Window)
        {
            Window->Position = AXLibGetWindowPosition(Window->Ref);

            bool Intrinsic = AXLibHasFlags(Window, AXWindow_MoveIntrinsic);
            uint32_t *WindowID = (uint32_t *) malloc(sizeof(uint32_t));
            *WindowID = Window->ID;

            AXLibClearFlags(Window, AXWindow_MoveIntrinsic);
            AXLibConstructEvent(AXEvent_WindowMoved, WindowID, Intrinsic);
        }
    }
    else if(CFEqual(Notification, kAXWindowResizedNotification))
    {
        /* NOTE(koekeishiya): Triggers an AXEvent_WindowResized and passes a pointer to the ax_window */
        ax_window *Window = AXLibGetWindowByRef(Application, Element);
        if(Window)
        {
            Window->Position = AXLibGetWindowPosition(Window->Ref);
            Window->Size = AXLibGetWindowSize(Window->Ref);

            bool Intrinsic = AXLibHasFlags(Window, AXWindow_SizeIntrinsic);
            uint32_t *WindowID = (uint32_t *) malloc(sizeof(uint32_t));
            *WindowID = Window->ID;

            AXLibClearFlags(Window, AXWindow_SizeIntrinsic);
            AXLibConstructEvent(AXEvent_WindowResized, WindowID, Intrinsic);
        }
    }
    else if(CFEqual(Notification, kAXTitleChangedNotification))
    {
        uint32_t *WindowID = (uint32_t *) malloc(sizeof(uint32_t));
        *WindowID = AXLibGetWindowID(Element);
        AXLibConstructEvent(AXEvent_WindowTitleChanged, WindowID, false);
    }
}

ax_application AXLibConstructApplication(pid_t PID, std::string Name)
{
    ax_application Application = {};

    Application.Ref = AXUIElementCreateApplication(PID);
    GetProcessForPID(PID, &Application.PSN);
    Application.Name = Name;
    Application.PID = PID;

    return Application;
}

bool AXLibInitializeApplication(std::map<pid_t, ax_application> *Applications, ax_application *Application)
{
    bool Result = AXLibAddApplicationObserver(Application);
    if(Result)
    {
        AXLibAddApplicationWindows(Application);
        Application->Focus = AXLibGetFocusedWindow(Application);
    }
    else
    {
        AXLibRemoveApplicationObserver(Application);
        if(++Application->Retries < AX_APPLICATION_RETRIES)
        {
#ifdef DEBUG_BUILD
            printf("AX: %s - Not responding, retry %d\n", Application->Name.c_str(), Application->Retries);
#endif
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC), dispatch_get_main_queue(),
            ^{
                if(AXLibInitializeApplication(Applications, Application))
                {
                    pid_t *ApplicationPID = (pid_t *) malloc(sizeof(pid_t));
                    *ApplicationPID = Application->PID;
                    AXLibConstructEvent(AXEvent_ApplicationLaunched, ApplicationPID, false);
                }
            });
        }
        else
        {
#ifdef DEBUG_BUILD
            printf("AX: %s did not respond, remove application reference\n", Application->Name.c_str());
#endif
            CFRelease(Application->Ref);
            Applications->erase(Application->PID);
        }
    }

    return Result;
}

bool AXLibHasApplicationObserverNotification(ax_application *Application)
{
    for(int Notification = AXApplication_Notification_WindowCreated;
            Notification < AXApplication_Notification_Count;
            ++Notification)
    {
        if(!(Application->Notifications & (1 << Notification)))
        {
            return false;
        }
    }

    return true;
}

bool AXLibAddApplicationObserver(ax_application *Application)
{
    AXLibConstructObserver(Application, AXApplicationCallback);
    if(Application->Observer.Valid)
    {
        for(int Notification = AXApplication_Notification_WindowCreated;
                Notification < AXApplication_Notification_Count;
                ++Notification)
        {
            /* NOTE(koekeishiya): Mark the notification as successful. */
            if(AXLibAddObserverNotification(&Application->Observer, Application->Ref, AXNotificationFromEnum(Notification), Application) == kAXErrorSuccess)
            {
                Application->Notifications |= (1 << Notification);
            }
        }

        AXLibStartObserver(&Application->Observer);
        return AXLibHasApplicationObserverNotification(Application);
    }

    return false;
}

void AXLibRemoveApplicationObserver(ax_application *Application)
{
    if(Application->Observer.Valid)
    {
        AXLibStopObserver(&Application->Observer);

        for(int Notification = AXApplication_Notification_WindowCreated;
                Notification < AXApplication_Notification_Count;
                ++Notification)
        {
            AXLibRemoveObserverNotification(&Application->Observer, Application->Ref, AXNotificationFromEnum(Notification));
        }

        AXLibDestroyObserver(&Application->Observer);
    }
}

void AXLibAddApplicationWindows(ax_application *Application)
{
    CFArrayRef Windows = (CFArrayRef) AXLibGetWindowProperty(Application->Ref, kAXWindowsAttribute);
    if(Windows)
    {
        CFIndex Count = CFArrayGetCount(Windows);
        for(CFIndex Index = 0; Index < Count; ++Index)
        {
            AXUIElementRef Ref = (AXUIElementRef) CFArrayGetValueAtIndex(Windows, Index);
            if(!AXLibGetWindowByRef(Application, Ref))
            {
                ax_window *Window = AXLibConstructWindow(Application, Ref);
                if(AXLibAddObserverNotification(&Application->Observer, Window->Ref, kAXUIElementDestroyedNotification, Window) == kAXErrorSuccess)
                {
                    AXLibAddApplicationWindow(Application, Window);
                }
                else
                {
                    AXLibDestroyWindow(Window);
                }
            }
        }
        CFRelease(Windows);
    }
}

void AXLibRemoveApplicationWindows(ax_application *Application)
{
    std::map<uint32_t, ax_window *> Windows = Application->Windows;
    Application->Windows.clear();

    std::map<uint32_t, ax_window *>::iterator It;
    for(It = Windows.begin(); It != Windows.end(); ++It)
    {
        ax_window *Window = It->second;
        AXLibRemoveObserverNotification(&Window->Application->Observer, Window->Ref, kAXUIElementDestroyedNotification);
        AXLibRemoveObserverNotification(&Window->Application->Observer, Window->Ref, kAXWindowMiniaturizedNotification);
        AXLibRemoveObserverNotification(&Window->Application->Observer, Window->Ref, kAXWindowDeminiaturizedNotification);
        AXLibDestroyWindow(Window);
    }
}

ax_window *AXLibFindApplicationWindow(ax_application *Application, uint32_t WID)
{
    std::map<uint32_t, ax_window *>::iterator It;
    It = Application->Windows.find(WID);
    if(It != Application->Windows.end())
        return It->second;
    else
        return NULL;
}

void AXLibAddApplicationWindow(ax_application *Application, ax_window *Window)
{
    if(!AXLibFindApplicationWindow(Application, Window->ID))
    {
        AXLibAddObserverNotification(&Application->Observer, Window->Ref, kAXWindowMiniaturizedNotification, Window);
        AXLibAddObserverNotification(&Application->Observer, Window->Ref, kAXWindowDeminiaturizedNotification, Window);

        if(Window->ID == 0)
            Application->NullWindows.push_back(Window);
        else
            Application->Windows[Window->ID] = Window;
    }
}

void AXLibRemoveApplicationWindow(ax_application *Application, uint32_t WID)
{
    ax_window *Window = AXLibFindApplicationWindow(Application, WID);
    if(Window)
    {
        AXLibRemoveObserverNotification(&Window->Application->Observer, Window->Ref, kAXUIElementDestroyedNotification);
        AXLibRemoveObserverNotification(&Window->Application->Observer, Window->Ref, kAXWindowMiniaturizedNotification);
        AXLibRemoveObserverNotification(&Window->Application->Observer, Window->Ref, kAXWindowDeminiaturizedNotification);
        Application->Windows.erase(WID);
    }
}

void AXLibActivateApplication(ax_application *Application)
{
    SharedWorkspaceActivateApplication(Application->PID);
}

bool AXLibIsApplicationActive(ax_application *Application)
{
    return SharedWorkspaceIsApplicationActive(Application->PID);
}

bool AXLibIsApplicationHidden(ax_application *Application)
{
    return SharedWorkspaceIsApplicationHidden(Application->PID);
}

void AXLibDestroyApplication(ax_application *Application)
{
    AXLibRemoveApplicationWindows(Application);
    AXLibRemoveApplicationObserver(Application);
    CFRelease(Application->Ref);
    Application->Ref = NULL;
}
