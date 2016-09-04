#include "dock.h"
#include <Carbon/Carbon.h>
#include <sys/sysctl.h>

typedef int CGSConnectionID;
extern "C" CGError CGSSetUniversalOwner(CGSConnectionID cid);

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
