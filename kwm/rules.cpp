#include "rules.h"
#include "tokenizer.h"
#include "display.h"
#include "space.h"
#include "window.h"
#include "tree.h"
#include "helpers.h"
#include "scratchpad.h"
#include <regex>

#define internal static
extern kwm_settings KWMSettings;

internal inline void
ReportInvalidRule(const std::string &Command)
{
    std::cerr << "Error (Parse Rule): " << Command << std::endl;
}

internal bool
ParseIdentifier(tokenizer *Tokenizer, std::string *Member)
{
    if(RequireToken(Tokenizer, Token_Equals))
    {
        token Token = GetToken(Tokenizer);
        switch(Token.Type)
        {
            case Token_String:
            {
                std::string String;
                for(int Index = 0; Index < Token.TextLength; ++Index)
                    String += Token.Text[Index];
                *Member = String;
                return true;
            } break;
            default:
            {
                ReportInvalidRule("Expected token of type Token_String: '" + std::string(Token.Text, Token.TextLength) + "'");
            } break;
        }
    }
    else
    {
        ReportInvalidRule("Expected token: '='");
    }

    return false;
}

internal bool
ParseProperties(tokenizer *Tokenizer, window_properties *Properties)
{
    if(RequireToken(Tokenizer, Token_Equals))
    {
        if(RequireToken(Tokenizer, Token_OpenBrace))
        {
            Properties->Scratchpad = -1;
            Properties->Display = -1;
            Properties->Space = -1;
            Properties->Float = -1;
            bool ValidState = true;

            while(ValidState)
            {
                token Token = GetToken(Tokenizer);
                switch(Token.Type)
                {
                    case Token_SemiColon: { continue; } break;
                    case Token_CloseBrace: { ValidState = false; } break;
                    case Token_Identifier:
                    {
                        if(TokenEquals(Token, "float"))
                        {
                            std::string Value;
                            if(ParseIdentifier(Tokenizer, &Value))
                            {
                                if(Value == "true")
                                    Properties->Float = 1;
                                else if(Value == "false")
                                    Properties->Float = 0;
                            }
                        }
                        else if(TokenEquals(Token, "display"))
                        {
                            std::string Value;
                            if(ParseIdentifier(Tokenizer, &Value))
                                Properties->Display = ConvertStringToInt(Value);
                        }
                        else if(TokenEquals(Token, "space"))
                        {
                            std::string Value;
                            if(ParseIdentifier(Tokenizer, &Value))
                                Properties->Space = ConvertStringToInt(Value);
                        }
                        else if(TokenEquals(Token, "scratchpad"))
                        {
                            std::string Value;
                            if(ParseIdentifier(Tokenizer, &Value))
                            {
                                if(Value == "visible")
                                    Properties->Scratchpad = 1;
                                else if(Value == "hidden")
                                    Properties->Scratchpad = 0;
                            }
                        }
                        else if(TokenEquals(Token, "role"))
                        {
                            std::string Value;
                            if(ParseIdentifier(Tokenizer, &Value))
                                    Properties->Role = Value;
                        }
                    } break;
                    default: { ReportInvalidRule("Expected token of type Token_Identifier: '" + std::string(Token.Text, Token.TextLength) + "'"); } break;
                }
            }

            return true;
        }
        else
        {
            ReportInvalidRule("Expected token '{'");
        }
    }
    else
    {
        ReportInvalidRule("Expected token '='");
    }

    return false;
}

internal bool
KwmParseRule(std::string RuleSym, window_rule *Rule)
{
    tokenizer Tokenizer = {};
    Tokenizer.At = const_cast<char*>(RuleSym.c_str());

    bool Result = true;
    bool Parsing = true;
    while(Parsing)
    {
        token Token = GetToken(&Tokenizer);
        switch(Token.Type)
        {
            case Token_EndOfStream:
            {
                Parsing = false;
            } break;
            case Token_Unknown:
            {
            } break;
            case Token_Identifier:
            {
                if(TokenEquals(Token, "owner"))
                    Result = Result && ParseIdentifier(&Tokenizer, &Rule->Owner);
                else if(TokenEquals(Token, "name"))
                    Result = Result && ParseIdentifier(&Tokenizer, &Rule->Name);
                else if(TokenEquals(Token, "role"))
                    Result = Result && ParseIdentifier(&Tokenizer, &Rule->Role);
                else if(TokenEquals(Token, "crole"))
                    Result = Result && ParseIdentifier(&Tokenizer, &Rule->CustomRole);
                else if(TokenEquals(Token, "properties"))
                    Result = Result && ParseProperties(&Tokenizer, &Rule->Properties);
                else if(TokenEquals(Token, "except"))
                    Result = Result && ParseIdentifier(&Tokenizer, &Rule->Except);
            } break;
            default: { } break;
        }
    }

    return Result;
}

internal bool
MatchWindowRule(window_rule *Rule, ax_window *Window)
{
    if(!Window)
        return false;

    bool Match = true;
    if(!Rule->Owner.empty())
    {
        std::regex Exp(Rule->Owner);
        Match = std::regex_match(Window->Application->Name, Exp);
    }

    if(!Rule->Name.empty() && Window->Name)
    {
        std::regex Exp(Rule->Name);
        Match = Match && std::regex_match(Window->Name, Exp);
    }

    if(!Rule->Role.empty())
    {
        CFTypeRef AXRole = CFStringCreateWithCString(NULL, Rule->Role.c_str(), kCFStringEncodingMacRoman);
        Match = Match && AXLibWindowHasRole(Window, AXRole);
        CFRelease(AXRole);
    }

    if(!Rule->CustomRole.empty())
    {
        CFTypeRef AXRole = CFStringCreateWithCString(NULL, Rule->CustomRole.c_str(), kCFStringEncodingMacRoman);
        Match = Match && AXLibWindowHasCustomRole(Window, AXRole);
        CFRelease(AXRole);
    }

    if(!Rule->Except.empty() && Window->Name)
    {
        std::regex Exp(Rule->Except);
        Match = Match && !std::regex_match(Window->Name, Exp);
    }

    return Match;
}

void KwmAddRule(std::string RuleSym)
{
    window_rule Rule = {};
    if(!RuleSym.empty() && KwmParseRule(RuleSym, &Rule))
        KWMSettings.WindowRules.push_back(Rule);
}

/* TODO(koekeishiya): This entire system is just stupid. Reimplement in a proper way. */
bool ApplyWindowRules(ax_window *Window)
{
    bool Skip = false;
    for(int Index = 0; Index < KWMSettings.WindowRules.size(); ++Index)
    {
        window_rule *Rule = &KWMSettings.WindowRules[Index];
        if(MatchWindowRule(Rule, Window))
        {
            if(Rule->Properties.Float == 1)
            {
                AXLibAddFlags(Window, AXWindow_Floating);
                if(HasFlags(&KWMSettings, Settings_CenterOnFloat))
                {
                    ax_display *Display = AXLibWindowDisplay(Window);
                    CenterWindow(Display, Window);
                }
            }

            if(!Rule->Properties.Role.empty())
                Window->Type.CustomRole = CFStringCreateWithCString(NULL,
                                                                    Rule->Properties.Role.c_str(),
                                                                    kCFStringEncodingMacRoman);

            if(Rule->Properties.Scratchpad != -1)
            {
                AddWindowToScratchpad(Window);
                if(Rule->Properties.Scratchpad == 0)
                {
                    HideScratchpadWindow(GetScratchpadSlotOfWindow(Window));
                    Skip = true;
                }
            }

            if(Rule->Properties.Display != -1 && Rule->Properties.Space == -1)
            {
                if(!AXLibIsWindowStandard(Window) &&
                   !AXLibIsWindowCustom(Window))
                    continue;

                ax_display *Display = AXLibArrangementDisplay(Rule->Properties.Display);
                if(Display && Display != AXLibWindowDisplay(Window))
                {
                    MoveWindowToDisplay(Window, Display->ArrangementID, false);
                    Skip = true;
                }
            }

            if(Rule->Properties.Space != -1)
            {
                if(!AXLibIsWindowStandard(Window) &&
                   !AXLibIsWindowCustom(Window))
                    continue;

                int Display = Rule->Properties.Display == -1 ? 0 : Rule->Properties.Display;
                ax_display *SourceDisplay = AXLibWindowDisplay(Window);
                ax_display *DestinationDisplay = AXLibArrangementDisplay(Display);
                int TotalSpaces = AXLibDisplaySpacesCount(DestinationDisplay);
                if(Rule->Properties.Space <= TotalSpaces && Rule->Properties.Space >= 1)
                {
                    int SourceCGSSpaceID = SourceDisplay->Space->ID;
                    int DestinationCGSSpaceID = AXLibCGSSpaceIDFromDesktopID(DestinationDisplay, Rule->Properties.Space);
                    if(!AXLibSpaceHasWindow(Window, DestinationCGSSpaceID))
                    {
                        AXLibSpaceAddWindow(DestinationCGSSpaceID, Window->ID);
                        AXLibSpaceRemoveWindow(SourceCGSSpaceID, Window->ID);
                        Skip = true;
                    }
                }
            }
        }
    }

    return Skip;
}
