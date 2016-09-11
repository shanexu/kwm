#ifndef CONFIG_H
#define CONFIG_H

#include "tokenizer.h"
#include <string>

void KwmParseKwmc(tokenizer *Tokenizer, int ClientSockFD);
void KwmParseConfig(std::string File);
void KwmReloadConfig();

#endif
