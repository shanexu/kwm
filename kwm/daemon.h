#ifndef DAEMON_H
#define DAEMON_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "types.h"
#include "interpreter.h"

std::string KwmReadFromSocket(int ClientSockFD);
void KwmWriteToSocket(std::string Msg, int ClientSockFD);
void * KwmDaemonHandleConnectionBG(void *);
void KwmDaemonHandleConnection();
void KwmTerminateDaemon();
bool KwmStartDaemon();

#endif
