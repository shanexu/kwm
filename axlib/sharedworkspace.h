#ifndef AXLIB_SHAREDWORKSPACE_H
#define AXLIB_SHAREDWORKSPACE_H

#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <map>

#include "application.h"

typedef std::map<pid_t, std::string> shared_ws_map;
typedef std::map<pid_t, std::string>::iterator shared_ws_map_iter;

void SharedWorkspaceInitialize();
shared_ws_map SharedWorkspaceRunningApplications();

void SharedWorkspaceActivateApplication(pid_t PID);
bool SharedWorkspaceIsApplicationActive(pid_t PID);
bool SharedWorkspaceIsApplicationHidden(pid_t PID);

#endif
