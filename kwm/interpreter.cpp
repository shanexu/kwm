#include "interpreter.h"
#include "kwm.h"
#include "helpers.h"
#include "keys.h"
#include "rules.h"
#include "config.h"
#include "tokenizer.h"
#include "../axlib/axlib.h"

#define internal static

internal void
KwmBindCommand(std::vector<std::string> &Tokens, bool Passthrough)
{
    bool BindCode = Tokens[0].find("bindcode") != std::string::npos;

    if(Tokens.size() > 2)
        KwmAddHotkey(Tokens[1], CreateStringFromTokens(Tokens, 2), Passthrough, BindCode);
    else
        KwmAddHotkey(Tokens[1], "", Passthrough, BindCode);
}

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
    else if(Tokens[0] == "write")
        KwmEmitKeystrokes(CreateStringFromTokens(Tokens, 1));
    else if(Tokens[0] == "press")
        KwmEmitKeystroke(Tokens[1]);
    else if(Tokens[0] == "bindsym" || Tokens[0] == "bindcode")
        KwmBindCommand(Tokens, false);
    else if(Tokens[0] == "bindsym_passthrough" || Tokens[0] == "bindcode_passthrough")
        KwmBindCommand(Tokens, true);
    else if(Tokens[0] == "unbindsym" || Tokens[0] == "unbindcode")
        KwmRemoveHotkey(Tokens[1], Tokens[0] == "unbindcode");
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
