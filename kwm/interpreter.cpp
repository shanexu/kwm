#include "interpreter.h"
#include "kwm.h"
#include "helpers.h"
#include "rules.h"
#include "config.h"
#include "tokenizer.h"
#include "../axlib/axlib.h"

void KwmInterpretCommand(std::string Message, int ClientSockFD)
{
    std::vector<std::string> Tokens = SplitString(Message, ' ');
    tokenizer Tokenizer = {};
    Tokenizer.At = (char *) Message.c_str();

    if(Tokens[0] == "quit")
        KwmQuit();
    else if((Tokens[0] == "config") ||
            (Tokens[0] == "mode") ||
            (Tokens[0] == "window") ||
            (Tokens[0] == "tree") ||
            (Tokens[0] == "display") ||
            (Tokens[0] == "space") ||
            (Tokens[0] == "scratchpad") ||
            (Tokens[0] == "query"))
        KwmParseKwmc(&Tokenizer, ClientSockFD);
    else if(Tokens[0] == "rule")
        KwmAddRule(CreateStringFromTokens(Tokens, 1));
    else if(Tokens[0] == "whitelist")
        CarbonWhitelistProcess(CreateStringFromTokens(Tokens, 1));

    if(Tokens[0] != "query")
    {
        shutdown(ClientSockFD, SHUT_RDWR);
        close(ClientSockFD);
    }
}
