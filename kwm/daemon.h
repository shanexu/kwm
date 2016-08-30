#ifndef DAEMON_H
#define DAEMON_H

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <string>

bool KwmStartDaemon();
void KwmTerminateDaemon();

std::string KwmReadFromSocket(int ClientSockFD);
void KwmWriteToSocket(std::string Msg, int ClientSockFD);

#endif
