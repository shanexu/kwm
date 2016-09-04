#include "dock.h"
#include "axlib.h"
#include <Carbon/Carbon.h>
#include <sys/sysctl.h>

typedef int CGSConnectionID;
extern "C" CGError CGSSetUniversalOwner(CGSConnectionID Connection);

extern "C" CGError CGSSetWindowAlpha(CGSConnectionID Connection, CGWindowID WindowID, float Alpha);
extern "C" CGError CGSSetWindowLevel(CGSConnectionID Connection, CGWindowID WindowID, int Level);

bool AXLibHijackUniversalOwner(CGSConnectionID Connection)
{
    size_t Length = 0;
	struct kinfo_proc *Result = NULL;
	int Name[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };

	if((sysctl(Name, 3, NULL, &Length, NULL, 0)) ||
       ((Result = (kinfo_proc *) malloc(Length)) == NULL) ||
       (sysctl(Name, 3, Result, &Length, NULL, 0)))
    {
        return false;
    }

	size_t Count = Length / sizeof(struct kinfo_proc);
	struct kinfo_proc *Current = Result;
	while(Count--)
    {
		if(strcmp(Current->kp_proc.p_comm, "Dock") == 0)
			kill(Current->kp_proc.p_pid, SIGKILL);

		Current++;
    }

    CGSSetUniversalOwner(Connection);
    return true;
}

/* NOTE(koekeishiya)): The following window-level constants are available.
    kCGBaseWindowLevel
    kCGMinimumWindowLevel
    kCGDesktopWindowLevel
    kCGDesktopIconWindowLevel
    kCGBackstopMenuLevel
    kCGNormalWindowLevel
    kCGFloatingWindowLevel
    kCGTornOffMenuWindowLevel
    kCGDockWindowLevel
    kCGMainMenuWindowLevel
    kCGStatusWindowLevel
    kCGModalPanelWindowLevel
    kCGPopUpMenuWindowLevel
    kCGDraggingWindowLevel
    kCGScreenSaverWindowLevel
    kCGOverlayWindowLevel
    kCGHelpWindowLevel
    kCGUtilityWindowLevel
    kCGMaximumWindowLevel
*/
void AXLibSetWindowLevel(uint32_t WindowID, int32_t Level)
{
    CGSSetWindowLevel(CGSDefaultConnection, WindowID, Level);
}

void AXLibSetWindowAlpha(uint32_t WindowID, double Alpha)
{
    CGSSetWindowAlpha(CGSDefaultConnection, WindowID, Alpha);
}
