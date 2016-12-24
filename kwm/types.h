#ifndef TYPES_H
#define TYPES_H

#include <Carbon/Carbon.h>

#include <iostream>
#include <vector>
#include <queue>
#include <stack>
#include <map>
#include <fstream>
#include <sstream>
#include <string>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libproc.h>
#include <signal.h>

#include <pthread.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/types.h>

struct space_identifier;
struct color;
struct modifier_keys;
struct space_settings;
struct container_offset;

struct window_properties;
struct window_rule;
struct space_info;
struct node_container;
struct tree_node;
struct scratchpad;

struct kwm_mach;
struct kwm_border;
struct kwm_path;
struct kwm_settings;

#ifdef DEBUG_BUILD
    #define DEBUG(x) std::cout << x << std::endl
    #define Assert(Expression) do \
                               { if(!(Expression)) \
                                   {\
                                       std::cout << "Assertion failed: " << #Expression << std::endl;\
                                       *(volatile int*)0 = 0;\
                                   } \
                               } while(0)
#else
    #define DEBUG(x) do {} while(0)
    #define Assert(Expression) do {} while(0)
#endif

enum focus_option
{
    FocusModeAutoraise,
    FocusModeStandby,
    FocusModeDisabled
};

enum cycle_focus_option
{
    CycleModeScreen,
    CycleModeDisabled
};

enum space_tiling_option
{
    SpaceModeBSP,
    SpaceModeMonocle,
    SpaceModeFloating,
    SpaceModeDefault
};

enum split_type
{
    SPLIT_NONE = 0,
    SPLIT_OPTIMAL = -1,
    SPLIT_VERTICAL = 1,
    SPLIT_HORIZONTAL = 2
};

enum container_type
{
    CONTAINER_NONE = 0,

    CONTAINER_LEFT = 1,
    CONTAINER_RIGHT = 2,

    CONTAINER_UPPER = 3,
    CONTAINER_LOWER = 4
};

enum node_type
{
    NodeTypeTree,
    NodeTypeLink
};

enum border_type
{
    BORDER_FOCUSED,
    BORDER_MARKED,
};

struct space_identifier
{
    int ScreenID, SpaceID;

    bool operator<(const space_identifier &Other) const
    {
        return (ScreenID < Other.ScreenID) ||
               (ScreenID == Other.ScreenID && SpaceID < Other.SpaceID);
    }
};

struct modifier_keys
{
    uint32_t Flags;
};

struct container_offset
{
    double PaddingTop, PaddingBottom;
    double PaddingLeft, PaddingRight;
    double VerticalGap, HorizontalGap;
};

struct color
{
    double Red;
    double Green;
    double Blue;
    double Alpha;
};

struct node_container
{
    double X, Y;
    double Width, Height;
    container_type Type;
};

struct link_node
{
    uint32_t WindowID;
    node_container Container;

    link_node *Prev;
    link_node *Next;
};

struct tree_node
{
    uint32_t WindowID;
    node_type Type;
    node_container Container;

    link_node *List;

    tree_node *Parent;
    tree_node *LeftChild;
    tree_node *RightChild;

    split_type SplitMode;
    double SplitRatio;
};

struct window_properties
{
    int Display;
    int Space;
    int Float;
    int Scratchpad;
    std::string Role;
};

struct window_rule
{
    window_properties Properties;
    std::string Except;
    std::string Owner;
    std::string Name;
    std::string Role;
    std::string CustomRole;
};

struct ax_window;
struct scratchpad
{
    std::map<int, ax_window *> Windows;
    int LastFocus;
};

struct space_settings
{
    container_offset Offset;
    space_tiling_option Mode;
    CGSize FloatDim;
    std::string Layout;
    std::string Name;
};

struct space_info
{
    space_settings Settings;
    bool ResolutionChanged;
    bool Initialized;

    tree_node *RootNode;
};

struct kwm_mach
{
    CFRunLoopSourceRef RunLoopSource;
    CFMachPortRef EventTap;
    CGEventMask EventMask;
};

struct kwm_border
{
    bool Enabled;
    uint32_t BorderId;
    border_type Type;

    double Radius;
    color Color;
    double Width;
};

struct kwm_path
{
    std::string FilePath;
    std::string EnvHome;
    std::string Config;

    std::string Home;
    std::string Include;
    std::string Layouts;
};

struct kwm_settings
{
    space_tiling_option Space;
    cycle_focus_option Cycle;
    focus_option Focus;

    container_offset DefaultOffset;
    split_type SplitMode;
    double SplitRatio;
    double OptimalRatio;
    uint32_t Flags;

    std::map<unsigned int, space_settings> DisplaySettings;
    std::map<space_identifier, space_settings> SpaceSettings;
    std::vector<window_rule> WindowRules;
};

enum kwm_toggleable
{
    Settings_MouseFollowsFocus = (1 << 0),
    Settings_StandbyOnFloat = (1 << 1),
    Settings_CenterOnFloat = (1 << 2),
    Settings_SpawnAsLeftChild = (1 << 3),
    Settings_FloatNonResizable = (1 << 4),
    Settings_LockToContainer = (1 << 5),
    Settings_MouseDrag = (1 << 6),
};

inline void
AddFlags(kwm_settings *Settings, uint32_t Flag)
{
    Settings->Flags |= Flag;
}

inline bool
HasFlags(kwm_settings *Settings, uint32_t Flag)
{
    bool Result = Settings->Flags & Flag;
    return Result;
}

inline void
ClearFlags(kwm_settings *Settings, uint32_t Flag)
{
    Settings->Flags &= ~Flag;
}

#endif
