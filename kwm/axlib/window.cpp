#include "window.h"
#include "element.h"

ax_window *AXLibConstructWindow(ax_application *Application, AXUIElementRef WindowRef)
{
    ax_window *Window = (ax_window *) malloc(sizeof(ax_window));
    memset(Window, '\0', sizeof(ax_window));

    Window->Ref = (AXUIElementRef) CFRetain(WindowRef);
    Window->Application = Application;
    Window->ID = AXLibGetWindowID(Window->Ref);
    Window->Name = AXLibGetWindowTitle(Window->Ref);
    Window->Position = AXLibGetWindowPosition(Window->Ref);
    Window->Size = AXLibGetWindowSize(Window->Ref);

    if(AXLibIsWindowMovable(Window->Ref))
        AXLibAddFlags(Window, AXWindow_Movable);

    if(AXLibIsWindowResizable(Window->Ref))
        AXLibAddFlags(Window, AXWindow_Resizable);

    if(AXLibIsWindowMinimized(Window->Ref))
        AXLibAddFlags(Window, AXWindow_Minimized);

    AXLibGetWindowRole(Window->Ref, &Window->Type.Role);
    AXLibGetWindowSubrole(Window->Ref, &Window->Type.Subrole);

    return Window;
}

bool AXLibIsWindowStandard(ax_window *Window)
{
    bool Result = ((Window->Type.Role && CFEqual(Window->Type.Role, kAXWindowRole)) &&
                   (Window->Type.Subrole && CFEqual(Window->Type.Subrole, kAXStandardWindowSubrole)));
    return Result;
}

bool AXLibIsWindowCustom(ax_window *Window)
{
    bool Result = ((Window->Type.CustomRole) &&
                   ((Window->Type.Role && CFEqual(Window->Type.Role, Window->Type.CustomRole)) ||
                    (Window->Type.Subrole && CFEqual(Window->Type.Subrole, Window->Type.CustomRole))));
    return Result;
}

bool AXLibWindowHasRole(ax_window *Window, CFTypeRef Role)
{
    bool Result = ((Window->Type.Role && CFEqual(Window->Type.Role, Role)) ||
                   (Window->Type.Subrole && CFEqual(Window->Type.Subrole, Role)));
    return Result;
}

bool AXLibWindowHasCustomRole(ax_window *Window, CFTypeRef Role)
{
    bool Result = ((Window->Type.CustomRole) &&
                   (CFEqual(Role, Window->Type.CustomRole)));
    return Result;
}

void AXLibDestroyWindow(ax_window *Window)
{
    if(Window->Ref)
        CFRelease(Window->Ref);

    if(Window->Type.Role)
        CFRelease(Window->Type.Role);

    if(Window->Type.Subrole)
        CFRelease(Window->Type.Subrole);

    if(Window->Type.CustomRole)
        CFRelease(Window->Type.CustomRole);

    if(Window->Name)
        free(Window->Name);

    free(Window);
}
