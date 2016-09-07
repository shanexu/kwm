#ifndef KEYS_H
#define KEYS_H

#include "types.h"

/* Taken from: /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/
				Developer/SDKs/MacOSX10.11.sdk/System/Library/Frameworks/IOKit.framework/
				Versions/A/Headers/hidsystem/IOLLEvent.h

NOTE(koekeishiya): All left and right-masks are device-dependent for some reason. */

enum hotkey_modifier
{
    Hotkey_Modifier_Alt = 0x00080000,
    Hotkey_Modifier_LAlt = 0x00000020,
    Hotkey_Modifier_RAlt = 0x00000040,

    Hotkey_Modifier_Shift = 0x00020000,
    Hotkey_Modifier_LShift = 0x00000002,
    Hotkey_Modifier_RShift = 0x00000004,

    Hotkey_Modifier_Cmd = 0x00100000,
    Hotkey_Modifier_LCmd = 0x00000008,
    Hotkey_Modifier_RCmd = 0x00000010,

    Hotkey_Modifier_Control = 0x00040000,
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

    Hotkey_Modifier_Flag_Passthrough = (1 << 10),
};

bool HotkeyForCGEvent(CGEventRef Event, hotkey *Hotkey);
bool MouseDragKeyMatchesCGEvent(CGEventRef Event);

void KwmAddHotkey(std::string KeySym, std::string Command, bool Passthrough, bool KeycodeInHex);
void KwmRemoveHotkey(std::string KeySym, bool KeycodeInHex);
void KwmEmitKeystrokes(std::string Text);
void KwmEmitKeystroke(std::string KeySym);
void KwmSetMouseDragKey(std::string KeySym);

mode *GetBindingMode(std::string Mode);
void KwmActivateBindingMode(std::string Mode);
void KwmExecuteSystemCommand(std::string Command);

#endif
