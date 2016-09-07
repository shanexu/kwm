#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <string>

enum token_type
{
    Token_Colon,
    Token_SemiColon,
    Token_Equals,
    Token_Dash,

    Token_OpenParen,
    Token_CloseParen,
    Token_OpenBracket,
    Token_CloseBracket,
    Token_OpenBrace,
    Token_CloseBrace,

    Token_Identifier,
    Token_String,
    Token_Digit,
    Token_Comment,
    Token_Hex,

    Token_EndOfStream,
    Token_Unknown,
};

struct token
{
    token_type Type;

    int TextLength;
    char *Text;
};

struct tokenizer
{
    char *At;
};

inline bool
IsDot(char C)
{
    bool Result = ((C == '.') || (C == ','));
    return Result;
}

inline bool
IsEndOfLine(char C)
{
    bool Result = ((C == '\n') ||
                   (C == '\r'));

    return Result;
}

inline bool
IsWhiteSpace(char C)
{
    bool Result = ((C == ' ') ||
                   (C == '\t') ||
                   IsEndOfLine(C));

    return Result;
}

inline bool
IsAlpha(char C)
{
    bool Result = (((C >= 'a') && (C <= 'z')) ||
                   ((C >= 'A') && (C <= 'Z')));

    return Result;
}

inline bool
IsNumeric(char C)
{
    bool Result = ((C >= '0') && (C <= '9'));
    return Result;
}

inline bool
IsHexadecimal(char C)
{
    bool Result = (((C >= 'a') && (C <= 'f')) ||
                   ((C >= 'A') && (C <= 'F')) ||
                   (IsNumeric(C)));
    return Result;
}

inline bool
TokenEquals(token Token, const char *Match)
{
    const char *At = Match;
    for(int Index = 0; Index < Token.TextLength; ++Index, ++At)
    {
        if((*At == 0) || (Token.Text[Index] != *At))
            return false;
    }

    bool Result = (*At == 0);
    return Result;
}

std::string GetTextTilEndOfLine(tokenizer *Tokenizer);
token GetToken(tokenizer *Tokenizer);
bool RequireToken(tokenizer *Tokenizer, token_type DesiredType);

#endif
