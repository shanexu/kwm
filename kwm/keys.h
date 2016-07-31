#ifndef KEYS_H
#define KEYS_H

#include "types.h"

enum hotkey_modifier
{
    Hotkey_Modifier_Alt = kCGEventFlagMaskAlternate,
    Hotkey_Modifier_LAlt = 524576,
    Hotkey_Modifier_RAlt = 524608,

    Hotkey_Modifier_Shift = kCGEventFlagMaskShift,
    Hotkey_Modifier_LShift = 131330,
    Hotkey_Modifier_RShift = 131332,

    Hotkey_Modifier_Cmd = kCGEventFlagMaskCommand,
    Hotkey_Modifier_LCmd = 1048840,
    Hotkey_Modifier_RCmd = 1048848,

    Hotkey_Modifier_Control = kCGEventFlagMaskControl,
};

enum hotkey_modifier_flag
{
    Hotkey_Modifier_Flag_Alt = (1 << 0),
    Hotkey_Modifier_Flag_LAlt = (1 << 1),
    Hotkey_Modifier_Flag_RAlt = (1 << 2),

    Hotkey_Modifier_Flag_Shift = (1 << 3),
    Hotkey_Modifier_Flag_LShift = (1 << 4),
    Hotkey_Modifier_Flag_RShift = (1 << 5),

    Hotkey_Modifier_Flag_Cmd = (1 << 6),
    Hotkey_Modifier_Flag_LCmd = (1 << 7),
    Hotkey_Modifier_Flag_RCmd = (1 << 8),

    Hotkey_Modifier_Flag_Control = (1 << 9),
};

void CreateHotkeyFromCGEvent(CGEventRef Event, hotkey *Hotkey);

void KwmAddHotkey(std::string KeySym, std::string Command, bool Passthrough, bool KeycodeInHex);
void KwmRemoveHotkey(std::string KeySym, bool KeycodeInHex);
bool HotkeyExists(uint32_t Flags, CGKeyCode Keycode, hotkey *Hotkey, std::string &Mode);
void KwmEmitKeystrokes(std::string Text);
void KwmEmitKeystroke(modifiers Mod, std::string Key);
void KwmEmitKeystroke(std::string KeySym);

mode *GetBindingMode(std::string Mode);
void KwmActivateBindingMode(std::string Mode);
void CheckPrefixTimeout();

#endif
