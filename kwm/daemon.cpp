#include "daemon.h"
#include "interpreter.h"

#define internal static

internal int KwmSockFD;
internal bool KwmDaemonIsRunning;
internal int KwmDaemonPort = 3020;
internal pthread_t KwmDaemonThread;

std::string KwmReadFromSocket(int ClientSockFD)
{
    char Cur;
    std::string Message;
    while(recv(ClientSockFD, &Cur, 1, 0))
    {
        if(Cur == '\n')
            break;
        Message += Cur;
    }
    return Message;
}

void KwmWriteToSocket(std::string Msg, int ClientSockFD)
{
    send(ClientSockFD, Msg.c_str(), Msg.size(), 0);
    shutdown(ClientSockFD, SHUT_RDWR);
    close(ClientSockFD);
}

internal void *
KwmDaemonHandleConnectionBG(void *)
{
    while(KwmDaemonIsRunning)
    {
        int ClientSockFD;
        struct sockaddr_in ClientAddr;
        socklen_t SinSize = sizeof(struct sockaddr);

        ClientSockFD = accept(KwmSockFD, (struct sockaddr*)&ClientAddr, &SinSize);
        if(ClientSockFD != -1)
        {
            std::string Message = KwmReadFromSocket(ClientSockFD);
            KwmInterpretCommand(Message, ClientSockFD);
        }
    }

    return NULL;
}

void KwmTerminateDaemon()
{
    KwmDaemonIsRunning = false;
    close(KwmSockFD);
}

bool KwmStartDaemon()
{
    struct sockaddr_in SrvAddr;
    int _True = 1;

    if((KwmSockFD = socket(PF_INET, SOCK_STREAM, 0)) == -1)
        return false;

    if(setsockopt(KwmSockFD, SOL_SOCKET, SO_REUSEADDR, &_True, sizeof(int)) == -1)
        printf("Could not set socket option: SO_REUSEADDR!\n");

    SrvAddr.sin_family = AF_INET;
    SrvAddr.sin_port = htons(KwmDaemonPort);
    SrvAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    memset(&SrvAddr.sin_zero, '\0', 8);

    if(bind(KwmSockFD, (struct sockaddr*)&SrvAddr, sizeof(struct sockaddr)) == -1)
        return false;

    if(listen(KwmSockFD, 10) == -1)
        return false;

    KwmDaemonIsRunning = true;
    pthread_create(&KwmDaemonThread, NULL, &KwmDaemonHandleConnectionBG, NULL);
    return true;
}
