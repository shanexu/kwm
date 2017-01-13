#include "window.h"
#include "container.h"
#include "node.h"
#include "display.h"
#include "space.h"
#include "tree.h"
#include "border.h"
#include "helpers.h"
#include "rules.h"
#include "serializer.h"
#include "cursor.h"
#include "scratchpad.h"
#include "../axlib/axlib.h"

#include <cmath>

#define internal static
#define local_persist static

extern std::map<std::string, space_info> WindowTree;
extern ax_display *FocusedDisplay;
extern ax_application *FocusedApplication;
extern ax_window *MarkedWindow;

extern kwm_settings KWMSettings;
extern kwm_border MarkedBorder;
extern kwm_border FocusedBorder;

internal void
DrawFocusedBorder(ax_display *Display, ax_window *Window)
{
    if((Display) &&
       (Display->Space->Type == kCGSSpaceUser))
    {
        UpdateBorder(&FocusedBorder, Window);
    }
    else
    {
        ClearBorder(&FocusedBorder);
    }
}

internal bool
FloatNextWindow(ax_display *Display, ax_window *Window)
{
    bool Result = HasFlags(&KWMSettings, Settings_FloatNextWindow);
    if(Result)
    {
        ClearFlags(&KWMSettings, Settings_FloatNextWindow);

        AXLibAddFlags(Window, AXWindow_Floating);
        if(HasFlags(&KWMSettings, Settings_CenterOnFloat))
        {
            CenterWindow(Display, Window);
        }

        /*
        if((HasFlags(&KWMSettings, Settings_StandbyOnFloat)) &&
           (KWMSettings.Focus != FocusModeDisabled))
            KWMSettings.Focus = FocusModeStandby;
        */
    }

    return Result;
}

internal void
FloatNonResizable(ax_window *Window)
{
    if(Window)
    {
        if((HasFlags(&KWMSettings, Settings_FloatNonResizable)) &&
           (!AXLibHasFlags(Window, AXWindow_Resizable)))
        {
            AXLibAddFlags(Window, AXWindow_Floating);
        }
    }
}

internal void
StandbyOnFloat(ax_window *Window)
{
    if(Window)
    {
        if((HasFlags(&KWMSettings, Settings_StandbyOnFloat)) &&
           (KWMSettings.Focus != FocusModeDisabled))
        {
            if(AXLibHasFlags(Window, AXWindow_Floating))
                KWMSettings.Focus = FocusModeStandby;
            else
                KWMSettings.Focus = FocusModeAutoraise;
        }
    }
}

internal void
TileWindow(ax_display *Display, ax_window *Window)
{
    if(Window)
    {
        if((!AXLibHasFlags(Window, AXWindow_Minimized)) &&
           (AXLibIsWindowStandard(Window) || AXLibIsWindowCustom(Window)) &&
           (!AXLibHasFlags(Window, AXWindow_Floating)) &&
           (!AXLibStickyWindow(Window)))
        {
            AddWindowToNodeTree(Display, Window->ID);
        }
    }
}

internal inline void
ClearBorderIfFullscreenSpace(ax_display *Display)
{
    if(Display->Space->Type != kCGSSpaceUser)
        ClearBorder(&FocusedBorder);
}

/* TODO(koekeishiya): Event context is a pointer to the new display. */
EVENT_CALLBACK(Callback_AXEvent_DisplayAdded)
{
    DEBUG("AXEvent_DisplayAdded");
}

EVENT_CALLBACK(Callback_AXEvent_DisplayRemoved)
{
    DEBUG("AXEvent_DisplayRemoved");
}

internal inline void
ResizeDisplay(ax_display *Display)
{
    std::map<CGSSpaceID, ax_space>::iterator It;
    for(It = Display->Spaces.begin(); It != Display->Spaces.end(); ++It)
    {
        ax_space *Space = &It->second;
        space_info *SpaceInfo = &WindowTree[Space->Identifier];
        if(Space == Display->Space)
            UpdateSpaceOfDisplay(Display, SpaceInfo);
        else
            SpaceInfo->ResolutionChanged = true;
    }
}

/* NOTE(koekeishiya): Event context is a pointer to the resized display. */
EVENT_CALLBACK(Callback_AXEvent_DisplayResized)
{
    ax_display *Display = (ax_display *) Event->Context;
    DEBUG("AXEvent_DisplayResized");
    ResizeDisplay(Display);
}

/* NOTE(koekeishiya): Event context is a pointer to the moved display. */
EVENT_CALLBACK(Callback_AXEvent_DisplayMoved)
{
    ax_display *Display = (ax_display *) Event->Context;
    DEBUG("AXEvent_DisplayMoved");
    ResizeDisplay(Display);
}

/* NOTE(koekeishiya): Event context is NULL. */
EVENT_CALLBACK(Callback_AXEvent_DisplayChanged)
{
    ax_display *CurrentDisplay = AXLibMainDisplay();
    if(CurrentDisplay->ID != FocusedDisplay->ID)
    {
        FocusedDisplay = CurrentDisplay;

        ax_space *PrevSpace = FocusedDisplay->Space;
        FocusedDisplay->Space = AXLibGetActiveSpace(FocusedDisplay);
        if(FocusedDisplay->Space != PrevSpace)
            FocusedDisplay->PrevSpace = PrevSpace;

        DEBUG("AXEvent_DisplayChanged: " << FocusedDisplay->ArrangementID);

        AXLibRunningApplications();
        CreateWindowNodeTree(FocusedDisplay);
        RebalanceNodeTree(FocusedDisplay);

        ClearBorderIfFullscreenSpace(FocusedDisplay);
    }
}

/* NOTE(koekeishiya): Event context is a pointer to the display whos space was changed. */
EVENT_CALLBACK(Callback_AXEvent_SpaceChanged)
{
    ax_display *Display = (ax_display *) Event->Context;
    DEBUG("AXEvent_SpaceChanged");

    /* TODO(koekeishiya): Do we want to reset this flag if a space transition occurs ?
    ClearFlags(&KWMSettings, Settings_FloatNextWindow);
    */

    ClearBorder(&FocusedBorder);
    ClearMarkedWindow();

    FocusedDisplay = Display;
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];

    AXLibRunningApplications();
    CreateWindowNodeTree(Display);
    RebalanceNodeTree(Display);
    if(SpaceInfo->ResolutionChanged)
        UpdateSpaceOfDisplay(Display, SpaceInfo);

    /* NOTE(koekeishiya): If we trigger a space changed event through cmd+tab, we receive the 'didApplicationActivate'
                          notification before the 'didActiveSpaceChange' notification. If a space has not been visited
                          before, this will cause us to end up on that space with an unsynchronized focused application state.

                          Always update state of focused application and its window after a space transition.

                          If this space transition was triggered through AXLibSpaceTransition(..), OSX does not
                          update our focus, so we manually try to activate the appropriate window. */
    if(AXLibHasFlags(Display->Space, AXSpace_FastTransition))
    {
        AXLibClearFlags(Display->Space, AXSpace_FastTransition);
        ax_window *Window = GetWindowByID(Display->Space->FocusedWindow);
        if(Window && AXLibSpaceHasWindow(Window, Display->Space->ID))
        {
            DEBUG("FastTransition: Found window");
            AXLibSetFocusedWindow(Window);
            DrawFocusedBorder(Display, Window);
            MoveCursorToCenterOfWindow(Window);
        }
        else
        {
            DEBUG("FastTransition: No window found");
            ClearBorder(&FocusedBorder);
            FocusFirstLeafNode(Display);
        }
    }
    else
    {
        FocusedApplication = AXLibGetFocusedApplication();
        if(FocusedApplication)
        {
            FocusedApplication->Focus = AXLibGetFocusedWindow(FocusedApplication);
            if((FocusedApplication->Focus) &&
               (AXLibSpaceHasWindow(FocusedApplication->Focus, Display->Space->ID)))
            {
                DrawFocusedBorder(Display, FocusedApplication->Focus);
                MoveCursorToCenterOfWindow(FocusedApplication->Focus);
                Display->Space->FocusedWindow = FocusedApplication->Focus->ID;
            }
        }
    }

    ClearBorderIfFullscreenSpace(Display);
}

/* NOTE(koekeishiya): Event context is a pointer to the PID of the launched application. */
EVENT_CALLBACK(Callback_AXEvent_ApplicationLaunched)
{
    pid_t *ApplicationPID = (pid_t *) Event->Context;
    ax_application *Application = AXLibGetApplicationByPID(*ApplicationPID);
    free(ApplicationPID);

    if(Application)
    {
        DEBUG("AXEvent_ApplicationLaunched: " << Application->Name);

        ax_display *Display = AXLibCursorDisplay();
        for(ax_window_map_iter It = Application->Windows.begin();
            It != Application->Windows.end();
            ++It)
        {
            ax_window *Window = It->second;
            if(ApplyWindowRules(Window))
                continue;

            FloatNonResizable(Window);
            TileWindow(Display, Window);
        }
    }
}

/* NOTE(koekeishiya): Event context is a pointer to the PID of the application. */
EVENT_CALLBACK(Callback_AXEvent_ApplicationHidden)
{
    pid_t *ApplicationPID = (pid_t *) Event->Context;
    ax_application *Application = AXLibGetApplicationByPID(*ApplicationPID);
    free(ApplicationPID);

    if(Application)
    {
        DEBUG("AXEvent_ApplicationHidden: " << Application->Name);

        for(ax_window_map_iter It = Application->Windows.begin();
            It != Application->Windows.end();
            ++It)
        {
            ax_window *Window = It->second;
            if(AXLibSpaceHasWindow(Window, FocusedDisplay->Space->ID))
                RemoveWindowFromNodeTree(FocusedDisplay, Window->ID);
        }
    }
}

/* NOTE(koekeishiya): Event context is a pointer to the PID of the application. */
EVENT_CALLBACK(Callback_AXEvent_ApplicationVisible)
{
    pid_t *ApplicationPID = (pid_t *) Event->Context;
    ax_application *Application = AXLibGetApplicationByPID(*ApplicationPID);
    free(ApplicationPID);

    if(Application)
    {
        DEBUG("AXEvent_ApplicationVisible: " << Application->Name);

        for(ax_window_map_iter It = Application->Windows.begin();
            It != Application->Windows.end();
            ++It)
        {
            ax_window *Window = It->second;
            if(AXLibSpaceHasWindow(Window, FocusedDisplay->Space->ID))
                TileWindow(FocusedDisplay, Window);
        }
    }
}

/* NOTE(koekeishiya): Event context is NULL */
EVENT_CALLBACK(Callback_AXEvent_ApplicationTerminated)
{
    pid_t *ApplicationPID = (pid_t *) Event->Context;
    ax_application *Application = AXLibGetApplicationByPID(*ApplicationPID);
    free(ApplicationPID);

    if(Application)
    {
        DEBUG("AXEvent_ApplicationTerminated");

        /* TODO(koekeishiya): We probably want to flag every display for an update, as the application
           in question could have had windows on several displays and spaces. */
        ax_display *Display = AXLibMainDisplay();
        Assert(Display != NULL);
        RebalanceNodeTree(Display);

       if(FocusedApplication == Application)
           ClearBorder(&FocusedBorder);

        ax_application_map *Applications = BeginAXLibApplications();
        Applications->erase(Application->PID);
        EndAXLibApplications();

        AXLibDestroyApplication(Application);
    }
}

/* NOTE(koekeishiya): Event context is a pointer to the PID of the activated application. */
EVENT_CALLBACK(Callback_AXEvent_ApplicationActivated)
{
    pid_t *ApplicationPID = (pid_t *) Event->Context;
    ax_application *Application = AXLibGetApplicationByPID(*ApplicationPID);
    free(ApplicationPID);

    if(Application)
    {
        DEBUG("AXEvent_ApplicationActivated: " << Application->Name);

        FocusedApplication = Application;
        if(Application->Focus)
        {
            ax_display *Display = AXLibWindowDisplay(Application->Focus);
            if(AXLibSpaceHasWindow(Application->Focus, Display->Space->ID))
            {
                StandbyOnFloat(Application->Focus);
                DrawFocusedBorder(Display, Application->Focus);
                Display->Space->FocusedWindow = Application->Focus->ID;
            }
        }
    }
}

/* NOTE(koekeishiya): Event context is a pointer to the CGWindowID of the new window. */
EVENT_CALLBACK(Callback_AXEvent_WindowCreated)
{
    uint32_t *WindowID = (uint32_t *) Event->Context;
    ax_window *Window = GetWindowByID(*WindowID);
    free(WindowID);

    if(Window)
    {
        if(Window->Name)
            DEBUG("AXEvent_WindowCreated: " << Window->Application->Name << " - " << Window->Name);
        else
            DEBUG("AXEvent_WindowCreated: " << Window->Application->Name << " - [Unknown]");

        if(ApplyWindowRules(Window))
            return;

        ax_display *Display = AXLibCursorDisplay();
        if(Display)
        {
            FloatNonResizable(Window);
            if(!FloatNextWindow(Display, Window))
            {
                TileWindow(Display, Window);
            }
        }
    }
}

/* NOTE(koekeishiya): Event context is a pointer to the CGWindowID of the closed window.
                      Must call AXLibRemoveApplicationWindow() and AXLibDestroyWindow() */
EVENT_CALLBACK(Callback_AXEvent_WindowDestroyed)
{
    uint32_t *WindowID = (uint32_t *) Event->Context;
    ax_window *Window = GetWindowByID(*WindowID);
    free(WindowID);

    if(Window)
    {
        if(Window->Name)
            DEBUG("AXEvent_WindowDestroyed: " << Window->Application->Name << " - " << Window->Name);
        else
            DEBUG("AXEvent_WindowDestroyed: " << Window->Application->Name << " - [Unknown]");

        ax_display *Display = AXLibWindowDisplay(Window);
        RemoveWindowFromScratchpad(Window);
        RemoveWindowFromNodeTree(Display, Window->ID);
        RebalanceNodeTree(Display);

        if(FocusedApplication == Window->Application)
        {
            if(FocusedApplication->Focus == Window)
                Display->Space->FocusedWindow = 0;

            StandbyOnFloat(FocusedApplication->Focus);
            DrawFocusedBorder(Display, FocusedApplication->Focus);
        }

        if(MarkedWindow == Window)
            ClearMarkedWindow();

        AXLibRemoveApplicationWindow(Window->Application, Window->ID);
        AXLibDestroyWindow(Window);
    }
}

/* NOTE(koekeishiya): Event context is a pointer to the CGWindowID of the minimized window. */
EVENT_CALLBACK(Callback_AXEvent_WindowMinimized)
{
    uint32_t *WindowID = (uint32_t *) Event->Context;
    ax_window *Window = GetWindowByID(*WindowID);
    free(WindowID);

    if(Window)
    {
        if(Window->Name)
            DEBUG("AXEvent_WindowMinimized: " << Window->Application->Name << " - " << Window->Name);
        else
            DEBUG("AXEvent_WindowMinimized: " << Window->Application->Name << " - [Unknown]");

        ax_display *Display = AXLibWindowDisplay(Window);
        RemoveWindowFromNodeTree(Display, Window->ID);

        ClearBorder(&FocusedBorder);
        if(MarkedWindow == Window)
            ClearMarkedWindow();
    }
}

/* NOTE(koekeishiya): Event context is a pointer to the CGWindowID of the deminimized window. */
EVENT_CALLBACK(Callback_AXEvent_WindowDeminimized)
{
    uint32_t *WindowID = (uint32_t *) Event->Context;
    ax_window *Window = GetWindowByID(*WindowID);
    free(WindowID);

    if(Window)
    {
        if(Window->Name)
            DEBUG("AXEvent_WindowDeminimized: " << Window->Application->Name << " - " << Window->Name);
        else
            DEBUG("AXEvent_WindowDeminimized: " << Window->Application->Name << " - [Unknown]");

        ax_display *Display = AXLibWindowDisplay(Window);
        if((AXLibIsWindowStandard(Window) || AXLibIsWindowCustom(Window)) &&
           (!AXLibHasFlags(Window, AXWindow_Floating)))
        {
            AddWindowToNodeTree(Display, Window->ID);
        }
    }
}

/* NOTE(koekeishiya): Event context is a pointer to the CGWindowID of the focused window. */
EVENT_CALLBACK(Callback_AXEvent_WindowFocused)
{
    uint32_t *WindowID = (uint32_t *) Event->Context;
    ax_window *Window = GetWindowByID(*WindowID);
    free(WindowID);

    if(Window)
    {
        if(Window->Name)
            DEBUG("AXEvent_WindowFocused: " << Window->Application->Name << " - " << Window->Name);
        else
            DEBUG("AXEvent_WindowFocused: " << Window->Application->Name << " - [Unknown]");

        if((AXLibIsWindowStandard(Window) || AXLibIsWindowCustom(Window)))
        {
            Window->Application->Focus = Window;
            if(FocusedApplication == Window->Application)
            {
                ax_display *Display = AXLibWindowDisplay(Window);
                StandbyOnFloat(Window);
                DrawFocusedBorder(Display, Window);
                Display->Space->FocusedWindow = Window->ID;
            }
        }
    }
}

/* NOTE(koekeishiya): Event context is a pointer to the CGWindowID of the moved window. */
EVENT_CALLBACK(Callback_AXEvent_WindowMoved)
{
    uint32_t *WindowID = (uint32_t *) Event->Context;
    ax_window *Window = GetWindowByID(*WindowID);
    free(WindowID);

    if(Window)
    {
        if(Window->Name)
            DEBUG("AXEvent_WindowMoved: " << Window->Application->Name << " - " << Window->Name);
        else
            DEBUG("AXEvent_WindowMoved: " << Window->Application->Name << " - [Unknown]");

        if(!Event->Intrinsic)
        {
            RemoveWindowFromOtherDisplays(Window);
            if(HasFlags(&KWMSettings, Settings_LockToContainer))
                LockWindowToContainerSize(Window);
        }

        ax_display *Display = AXLibWindowDisplay(Window);
        if((FocusedApplication == Window->Application) &&
           (FocusedApplication->Focus == Window))
            DrawFocusedBorder(Display, Window);

        if(MarkedWindow == Window)
            UpdateBorder(&MarkedBorder, Window);
    }
}

/* NOTE(koekeishiya): Event context is a pointer to the CGWindowID of the resized window. */
EVENT_CALLBACK(Callback_AXEvent_WindowResized)
{
    uint32_t *WindowID = (uint32_t *) Event->Context;
    ax_window *Window = GetWindowByID(*WindowID);
    free(WindowID);

    if(Window)
    {
        if(Window->Name)
            DEBUG("AXEvent_WindowResized: " << Window->Application->Name << " - " << Window->Name);
        else
            DEBUG("AXEvent_WindowResized: " << Window->Application->Name << " - [Unknown]");

        if(!Event->Intrinsic && HasFlags(&KWMSettings, Settings_LockToContainer))
            LockWindowToContainerSize(Window);

        ax_display *Display = AXLibWindowDisplay(Window);
        if((FocusedApplication == Window->Application) &&
           (FocusedApplication->Focus == Window))
            DrawFocusedBorder(Display, Window);

        if(MarkedWindow == Window)
            UpdateBorder(&MarkedBorder, Window);
    }
}

/* NOTE(koekeishiya): Event context is a pointer to the CGWindowID of the window. */
EVENT_CALLBACK(Callback_AXEvent_WindowTitleChanged)
{
    uint32_t *WindowID = (uint32_t *) Event->Context;
    ax_window *Window = GetWindowByID(*WindowID);
    free(WindowID);

    if(Window)
    {
        if(Window->Name)
        {
            free(Window->Name);
            Window->Name = NULL;
        }

        Window->Name = AXLibGetWindowTitle(Window->Ref);
    }
}

internal std::vector<uint32_t>
GetAllWindowIDsInTree(space_info *Space)
{
    std::vector<uint32_t> Windows;
    if(Space->Settings.Mode == SpaceModeBSP)
    {
        tree_node *CurrentNode = NULL;
        GetFirstLeafNode(Space->RootNode, (void**)&CurrentNode);
        while(CurrentNode)
        {
            if(CurrentNode->WindowID != 0)
                Windows.push_back(CurrentNode->WindowID);

            link_node *Link = CurrentNode->List;
            while(Link)
            {
                Windows.push_back(Link->WindowID);
                Link = Link->Next;
            }

            CurrentNode = GetNearestTreeNodeToTheRight(CurrentNode);
        }
    }
    else if(Space->Settings.Mode == SpaceModeMonocle)
    {
        link_node *Link = Space->RootNode->List;
        while(Link)
        {
            Windows.push_back(Link->WindowID);
            Link = Link->Next;
        }
    }

    return Windows;
}

internal std::vector<uint32_t>
GetAllAXWindowIDsToRemoveFromTree(std::vector<ax_window *> &VisibleWindows, std::vector<uint32_t> &WindowIDsInTree)
{
    std::vector<uint32_t> Windows;
    for(std::size_t IDIndex = 0; IDIndex < WindowIDsInTree.size(); ++IDIndex)
    {
        bool Found = false;
        for(std::size_t WindowIndex = 0; WindowIndex < VisibleWindows.size(); ++WindowIndex)
        {
            if(VisibleWindows[WindowIndex]->ID == WindowIDsInTree[IDIndex])
            {
                Found = true;
                break;
            }
        }

        if(!Found)
            Windows.push_back(WindowIDsInTree[IDIndex]);
    }

    return Windows;
}

internal std::vector<ax_window *>
GetAllAXWindowsNotInTree(ax_display *Display, std::vector<ax_window *> &VisibleWindows, std::vector<uint32_t> &WindowIDsInTree)
{
    std::vector<ax_window *> Windows;
    for(std::size_t WindowIndex = 0; WindowIndex < VisibleWindows.size(); ++WindowIndex)
    {
        bool Found = false;
        ax_window *Window = VisibleWindows[WindowIndex];
        for(std::size_t IDIndex = 0; IDIndex < WindowIDsInTree.size(); ++IDIndex)
        {
            if(Window->ID == WindowIDsInTree[IDIndex])
            {
                Found = true;
                break;
            }
        }

        if((!Found) &&
           (AXLibSpaceHasWindow(Window, Display->Space->ID)) &&
           (!AXLibStickyWindow(Window)))
            Windows.push_back(Window);
    }

    return Windows;
}

internal std::vector<uint32_t>
GetAllWindowIDSOnDisplay(ax_display *Display)
{
    std::vector<uint32_t> Windows;
    std::vector<ax_window*> VisibleWindows = AXLibGetAllVisibleWindows();
    for(int Index = 0; Index < VisibleWindows.size(); ++Index)
    {
        ax_window *Window = VisibleWindows[Index];
        ax_display *DisplayOfWindow = AXLibWindowDisplay(Window);
        if(DisplayOfWindow)
        {
            if(DisplayOfWindow != Display)
            {
                space_info *SpaceOfWindow = &WindowTree[DisplayOfWindow->Space->Identifier];
                if(!SpaceOfWindow->Initialized ||
                   SpaceOfWindow->Settings.Mode == SpaceModeFloating ||
                   GetTreeNodeFromWindowID(SpaceOfWindow->RootNode, Window->ID) ||
                   GetLinkNodeFromWindowID(SpaceOfWindow->RootNode, Window->ID))
                    continue;
            }

            if((AXLibSpaceHasWindow(Window, Display->Space->ID)) &&
               (!AXLibStickyWindow(Window)))
                Windows.push_back(Window->ID);
        }
    }

    return Windows;
}

internal inline bool
IsWindowInTree(space_info *SpaceInfo, uint32_t WindowID)
{
    std::vector<uint32_t> WindowIDs = GetAllWindowIDsInTree(SpaceInfo);
    return std::find(WindowIDs.begin(), WindowIDs.end(), WindowID) != WindowIDs.end();
}

internal void
AddWindowToBSPTree(ax_display *Display, space_info *SpaceInfo, uint32_t WindowID)
{
    tree_node *RootNode = SpaceInfo->RootNode;
    if(RootNode)
    {
        tree_node *Insert = GetFirstPseudoLeafNode(SpaceInfo->RootNode);
        if(Insert && (Insert->WindowID = WindowID))
        {
            ApplyTreeNodeContainer(Insert);
            return;
        }

        tree_node *CurrentNode = NULL;
        ax_application *Application = FocusedApplication ? FocusedApplication : AXLibGetFocusedApplication();
        ax_window *Window = Application ? Application->Focus : NULL;
        if(MarkedWindow && MarkedWindow->ID != WindowID)
            CurrentNode = GetTreeNodeFromWindowIDOrLinkNode(RootNode, MarkedWindow->ID);

        if(!CurrentNode && Window && Window->ID != WindowID)
            CurrentNode = GetTreeNodeFromWindowIDOrLinkNode(RootNode, Window->ID);

        if(!CurrentNode)
            CurrentNode = FindFirstMinDepthLeafNode(RootNode);

        if(CurrentNode)
        {
            uint32_t CurrentNodeWindowID = CurrentNode->WindowID;
            bool ParentZoom = CurrentNode->Parent && CurrentNode->Parent->WindowID == CurrentNode->WindowID;

            if(CurrentNode->Type == NodeTypeTree)
            {
                split_type SplitMode = KWMSettings.SplitMode == SPLIT_OPTIMAL ? GetOptimalSplitMode(CurrentNode) : KWMSettings.SplitMode;
                CreateLeafNodePair(Display, CurrentNode, CurrentNode->WindowID, WindowID, SplitMode);
                ApplyTreeNodeContainer(CurrentNode);
            }
            else if(CurrentNode->Type == NodeTypeLink)
            {
                link_node *Link = CurrentNode->List;
                if(Link)
                {
                    while(Link->Next)
                        Link = Link->Next;

                    link_node *NewLink = CreateLinkNode();
                    NewLink->Container = CurrentNode->Container;

                    NewLink->WindowID = WindowID;
                    Link->Next = NewLink;
                    NewLink->Prev = Link;
                    ResizeWindowToContainerSize(NewLink);
                }
                else
                {
                    CurrentNode->List = CreateLinkNode();
                    CurrentNode->List->Container = CurrentNode->Container;
                    CurrentNode->List->WindowID = WindowID;
                    ResizeWindowToContainerSize(CurrentNode->List);
                }
            }

            if(ParentZoom)
            {
                CurrentNode->Parent->WindowID = 0;
                CurrentNode->WindowID = CurrentNodeWindowID;
                ResizeWindowToContainerSize(CurrentNode);
            }
        }
    }
}

internal void
RemoveWindowFromBSPTree(ax_display *Display, uint32_t WindowID)
{
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    if(!SpaceInfo->RootNode)
        return;

    tree_node *WindowNode = GetTreeNodeFromWindowID(SpaceInfo->RootNode, WindowID);
    if(WindowNode)
    {
        if((SpaceInfo->RootNode != WindowNode) &&
           (SpaceInfo->RootNode->WindowID == WindowID))
            SpaceInfo->RootNode->WindowID = 0;

        tree_node *Parent = WindowNode->Parent;
        if(Parent && Parent->LeftChild && Parent->RightChild)
        {
           if((SpaceInfo->RootNode->WindowID == Parent->LeftChild->WindowID) ||
              (SpaceInfo->RootNode->WindowID == Parent->RightChild->WindowID))
               SpaceInfo->RootNode->WindowID = 0;

            tree_node *AccessChild = IsRightChild(WindowNode) ? Parent->LeftChild : Parent->RightChild;
            Parent->LeftChild = NULL;
            Parent->RightChild = NULL;

            Parent->WindowID = AccessChild->WindowID;
            Parent->Type = AccessChild->Type;
            Parent->List = AccessChild->List;

            if(AccessChild->LeftChild && AccessChild->RightChild)
            {
                Parent->LeftChild = AccessChild->LeftChild;
                Parent->LeftChild->Parent = Parent;

                Parent->RightChild = AccessChild->RightChild;
                Parent->RightChild->Parent = Parent;

                CreateNodeContainers(Display, Parent, true);
            }

            ResizeLinkNodeContainers(Parent);
            ApplyTreeNodeContainer(Parent);
            free(AccessChild);
            free(WindowNode);
        }
        else if(!Parent)
        {
            free(SpaceInfo->RootNode);
            SpaceInfo->RootNode = NULL;
        }
    }
    else
    {
        link_node *Link = GetLinkNodeFromWindowID(SpaceInfo->RootNode, WindowID);
        tree_node *Root = GetTreeNodeFromLink(SpaceInfo->RootNode, Link);
        if(Link)
        {
            if(SpaceInfo->RootNode->WindowID == WindowID)
                SpaceInfo->RootNode->WindowID = 0;

            link_node *Prev = Link->Prev;
            link_node *Next = Link->Next;

            Link->Prev = NULL;
            Link->Next = NULL;

            if(Prev)
                Prev->Next = Next;

            if(!Prev)
                Root->List = Next;

            if(Next)
                Next->Prev = Prev;

            if(Link == Root->List)
                Root->List = NULL;

            free(Link);
        }
    }
}

internal void
AddWindowToMonocleTree(ax_display *Display, space_info *SpaceInfo, uint32_t WindowID)
{
    if(SpaceInfo->RootNode)
    {
        link_node *Link = SpaceInfo->RootNode->List;
        while(Link->Next)
            Link = Link->Next;

        link_node *NewLink = CreateLinkNode();
        SetLinkNodeContainer(Display, NewLink);

        NewLink->WindowID = WindowID;
        Link->Next = NewLink;
        NewLink->Prev = Link;

        ResizeWindowToContainerSize(NewLink);
    }
}

internal void
RemoveWindowFromMonocleTree(ax_display *Display, uint32_t WindowID)
{
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    if(SpaceInfo->RootNode && SpaceInfo->RootNode->List)
    {
        link_node *Link = GetLinkNodeFromTree(SpaceInfo->RootNode, WindowID);
        if(Link)
        {
            link_node *Prev = Link->Prev;
            link_node *Next = Link->Next;

            if(Prev)
                Prev->Next = Next;

            if(Next)
                Next->Prev = Prev;

            if(Link == SpaceInfo->RootNode->List)
            {
                SpaceInfo->RootNode->List = Next;

                if(!SpaceInfo->RootNode->List)
                {
                    free(SpaceInfo->RootNode);
                    SpaceInfo->RootNode = NULL;
                }
            }
            free(Link);
        }
    }
}

internal void
CreateSpaceInfoWithWindowTree(ax_display *Display, space_info *SpaceInfo, std::vector<uint32_t> *Windows)
{
    if((SpaceInfo->Settings.Mode == SpaceModeFloating) ||
       (Display->Space->Type != kCGSSpaceUser))
        return;

    if(SpaceInfo->Settings.Mode == SpaceModeBSP && !SpaceInfo->Settings.Layout.empty())
    {
        if(LoadBSPTreeFromFile(Display, SpaceInfo, SpaceInfo->Settings.Layout))
        {
            FillDeserializedTree(SpaceInfo->RootNode, Display, Windows);
        }
        else
        {
            SpaceInfo->RootNode = CreateTreeFromWindowIDList(Display, Windows);
        }
    }
    else
    {
        SpaceInfo->RootNode = CreateTreeFromWindowIDList(Display, Windows);
    }

    if(SpaceInfo->RootNode)
        ApplyTreeNodeContainer(SpaceInfo->RootNode);
}

void CreateWindowNodeTree(ax_display *Display)
{
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    if(!SpaceInfo->Initialized && !SpaceInfo->RootNode)
    {
        std::vector<ax_window *> KnownWindows = AXLibGetAllKnownWindows();
        for(std::size_t Index = 0; Index < KnownWindows.size(); ++Index)
        {
            ax_window *Window = KnownWindows[Index];
            ApplyWindowRules(Window);
        }

        SpaceInfo->Initialized = true;
        LoadSpaceSettings(Display, SpaceInfo);
        std::vector<uint32_t> Windows = GetAllWindowIDSOnDisplay(Display);
        CreateSpaceInfoWithWindowTree(Display, SpaceInfo, &Windows);
    }
    else if(SpaceInfo->Initialized && !SpaceInfo->RootNode)
    {
        std::vector<uint32_t> Windows = GetAllWindowIDSOnDisplay(Display);
        CreateSpaceInfoWithWindowTree(Display, SpaceInfo, &Windows);
    }
}

void LoadWindowNodeTree(ax_display *Display, std::string Layout)
{
    if(Display)
    {
        space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
        if(SpaceInfo->Settings.Mode == SpaceModeBSP)
        {
            std::vector<uint32_t> Windows = GetAllWindowIDSOnDisplay(Display);
            if(LoadBSPTreeFromFile(Display, SpaceInfo, Layout))
            {
                FillDeserializedTree(SpaceInfo->RootNode, Display, &Windows);
                ApplyTreeNodeContainer(SpaceInfo->RootNode);
            }
        }
    }
}

void ResetWindowNodeTree(ax_display *Display, space_tiling_option Mode)
{
    if(Display)
    {
        if(AXLibIsSpaceTransitionInProgress())
            return;

        space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
        if(SpaceInfo->Settings.Mode == Mode)
            return;

        DestroyNodeTree(SpaceInfo->RootNode);
        SpaceInfo->RootNode = NULL;
        SpaceInfo->Initialized = true;
        SpaceInfo->Settings.Mode = Mode;
        CreateWindowNodeTree(Display);
    }
}

void AddWindowToNodeTree(ax_display *Display, uint32_t WindowID)
{
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    if(!SpaceInfo->RootNode)
        CreateWindowNodeTree(Display);
    else if((SpaceInfo->Settings.Mode == SpaceModeBSP) &&
            (!IsWindowInTree(SpaceInfo, WindowID)))
        AddWindowToBSPTree(Display, SpaceInfo, WindowID);
    else if((SpaceInfo->Settings.Mode == SpaceModeMonocle) &&
            (!IsWindowInTree(SpaceInfo, WindowID)))
        AddWindowToMonocleTree(Display, SpaceInfo, WindowID);
}

void RemoveWindowFromNodeTree(ax_display *Display, uint32_t WindowID)
{
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    if(SpaceInfo->Settings.Mode == SpaceModeBSP)
        RemoveWindowFromBSPTree(Display, WindowID);
    else if(SpaceInfo->Settings.Mode == SpaceModeMonocle)
        RemoveWindowFromMonocleTree(Display, WindowID);
}

internal void
RebalanceBSPTree(ax_display *Display)
{
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    if(SpaceInfo->RootNode)
    {
        std::vector<ax_window *> VisibleWindows = AXLibGetAllVisibleWindows();
        std::vector<uint32_t> WindowIDsInTree = GetAllWindowIDsInTree(SpaceInfo);
        std::vector<ax_window *> WindowsToAdd = GetAllAXWindowsNotInTree(Display, VisibleWindows, WindowIDsInTree);
        std::vector<uint32_t> WindowsToRemove = GetAllAXWindowIDsToRemoveFromTree(VisibleWindows, WindowIDsInTree);

        for(std::size_t WindowIndex = 0; WindowIndex < WindowsToRemove.size(); ++WindowIndex)
        {
            DEBUG("RebalanceBSPTree() Remove Window " << WindowsToRemove[WindowIndex]);
            RemoveWindowFromBSPTree(Display, WindowsToRemove[WindowIndex]);
        }

        for(std::size_t WindowIndex = 0; WindowIndex < WindowsToAdd.size(); ++WindowIndex)
        {
            DEBUG("RebalanceBSPTree() Add Window " << WindowsToAdd[WindowIndex]->ID);
            TileWindow(Display, WindowsToAdd[WindowIndex]);
        }
    }
}

internal void
RebalanceMonocleTree(ax_display *Display)
{
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    if(SpaceInfo->RootNode && SpaceInfo->RootNode->List)
    {
        std::vector<ax_window *> VisibleWindows = AXLibGetAllVisibleWindows();
        std::vector<uint32_t> WindowIDsInTree = GetAllWindowIDsInTree(SpaceInfo);
        std::vector<ax_window *> WindowsToAdd = GetAllAXWindowsNotInTree(Display, VisibleWindows, WindowIDsInTree);
        std::vector<uint32_t> WindowsToRemove = GetAllAXWindowIDsToRemoveFromTree(VisibleWindows, WindowIDsInTree);

        for(std::size_t WindowIndex = 0; WindowIndex < WindowsToRemove.size(); ++WindowIndex)
        {
            DEBUG("RebalanceMonocleTree() Remove Window " << WindowsToRemove[WindowIndex]);
            RemoveWindowFromMonocleTree(Display, WindowsToRemove[WindowIndex]);
        }

        for(std::size_t WindowIndex = 0; WindowIndex < WindowsToAdd.size(); ++WindowIndex)
        {
            DEBUG("RebalanceMonocleTree() Add Window " << WindowsToAdd[WindowIndex]->ID);
            TileWindow(Display, WindowsToAdd[WindowIndex]);
        }
    }
}

/* NOTE(koekeishiya): Remove any window that should not be in the window-tree, caused by
 * any for of action done to the window that could not be detected through notifications.
 * Also attempt to tile any untiled window that is not marked as  floating. */
void RebalanceNodeTree(ax_display *Display)
{
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    if(!SpaceInfo->Initialized)
        return;

    if(SpaceInfo->Settings.Mode == SpaceModeBSP)
        RebalanceBSPTree(Display);
    else if(SpaceInfo->Settings.Mode == SpaceModeMonocle)
        RebalanceMonocleTree(Display);
}

void CreateInactiveWindowNodeTree(ax_display *Display, std::vector<uint32_t> *Windows)
{
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    if(!SpaceInfo->Initialized && !SpaceInfo->RootNode)
    {
        SpaceInfo->Initialized = true;
        LoadSpaceSettings(Display, SpaceInfo);
        CreateSpaceInfoWithWindowTree(Display, SpaceInfo, Windows);
    }
    else if(SpaceInfo->Initialized && !SpaceInfo->RootNode)
    {
        CreateSpaceInfoWithWindowTree(Display, SpaceInfo, Windows);
    }
}

void AddWindowToInactiveNodeTree(ax_display *Display, uint32_t WindowID)
{
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    if(!SpaceInfo->RootNode)
    {
        std::vector<uint32_t> Windows;
        Windows.push_back(WindowID);
        CreateInactiveWindowNodeTree(Display, &Windows);
    }
    else if(SpaceInfo->Settings.Mode == SpaceModeBSP)
    {
        DEBUG("AddWindowToInactiveNodeTree() BSP Space");
        tree_node *CurrentNode = FindFirstMinDepthLeafNode(SpaceInfo->RootNode);
        split_type SplitMode = KWMSettings.SplitMode == SPLIT_OPTIMAL ? GetOptimalSplitMode(CurrentNode) : KWMSettings.SplitMode;

        CreateLeafNodePair(Display, CurrentNode, CurrentNode->WindowID, WindowID, SplitMode);
        ApplyTreeNodeContainer(CurrentNode);
    }
    else if(SpaceInfo->Settings.Mode == SpaceModeMonocle)
    {
        DEBUG("AddWindowToInactiveNodeTree() Monocle Space");
        link_node *Link = SpaceInfo->RootNode->List;
        while(Link->Next)
            Link = Link->Next;

        link_node *NewLink = CreateLinkNode();
        SetLinkNodeContainer(Display, NewLink);

        NewLink->WindowID = WindowID;
        Link->Next = NewLink;
        NewLink->Prev = Link;
        ResizeWindowToContainerSize(NewLink);
    }
}

void ToggleWindowFloating(uint32_t WindowID, bool Center)
{
    ax_window *Window = GetWindowByID(WindowID);
    if(!Window)
        return;

    ax_display *Display = AXLibWindowDisplay(Window);
    if(AXLibHasFlags(Window, AXWindow_Floating))
    {
        AXLibClearFlags(Window, AXWindow_Floating);
        TileWindow(Display, Window);
        if((HasFlags(&KWMSettings, Settings_StandbyOnFloat)) &&
           (KWMSettings.Focus != FocusModeDisabled))
            KWMSettings.Focus = FocusModeAutoraise;
    }
    else
    {
        AXLibAddFlags(Window, AXWindow_Floating);
        RemoveWindowFromNodeTree(Display, Window->ID);

        if(Center && HasFlags(&KWMSettings, Settings_CenterOnFloat))
            CenterWindow(Display, Window);

        if((HasFlags(&KWMSettings, Settings_StandbyOnFloat)) &&
           (KWMSettings.Focus != FocusModeDisabled))
            KWMSettings.Focus = FocusModeStandby;
    }
}

void ToggleFocusedWindowFloating()
{
    if(FocusedApplication && FocusedApplication->Focus)
        ToggleWindowFloating(FocusedApplication->Focus->ID, true);
}

void ToggleFocusedWindowParentContainer()
{
    ax_window *Window = FocusedApplication->Focus;
    if(!Window)
        return;

    ax_display *Display = AXLibWindowDisplay(Window);
    space_info *Space = &WindowTree[Display->Space->Identifier];

    if(Space->Settings.Mode != SpaceModeBSP)
        return;

    tree_node *Node = GetTreeNodeFromWindowID(Space->RootNode, Window->ID);
    if(Node && Node->Parent)
    {
        if(IsLeafNode(Node) && Node->Parent->WindowID == 0)
        {
            DEBUG("ToggleFocusedWindowParentContainer() Set Parent Container");
            Node->Parent->WindowID = Node->WindowID;
            ResizeWindowToContainerSize(Node->Parent);
        }
        else
        {
            DEBUG("ToggleFocusedWindowParentContainer() Restore Window Container");
            Node->Parent->WindowID = 0;
            ResizeWindowToContainerSize(Node);
        }
    }
}

void ToggleFocusedWindowFullscreen()
{
    ax_window *Window = FocusedApplication->Focus;
    if(!Window)
        return;

    ax_display *Display = AXLibWindowDisplay(Window);
    space_info *Space = &WindowTree[Display->Space->Identifier];

    if(Space->Settings.Mode != SpaceModeBSP)
        return;

    tree_node *Node = NULL;
    if(Space->RootNode->WindowID == 0)
    {
        Node = GetTreeNodeFromWindowID(Space->RootNode, Window->ID);
        if(Node)
        {
            DEBUG("ToggleFocusedWindowFullscreen() Set fullscreen");
            Space->RootNode->WindowID = Node->WindowID;
            ResizeWindowToContainerSize(Space->RootNode);
        }
    }
    else
    {
        DEBUG("ToggleFocusedWindowFullscreen() Restore old size");
        Space->RootNode->WindowID = 0;
        Node = GetTreeNodeFromWindowID(Space->RootNode, Window->ID);
        if(Node)
        {
            ResizeWindowToContainerSize(Node);
        }
    }
}

bool IsWindowFullscreen(ax_window *Window)
{
    ax_display *Display = AXLibWindowDisplay(Window);
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];

    return SpaceInfo->RootNode && SpaceInfo->RootNode->WindowID == Window->ID;
}

bool IsWindowParentContainer(ax_window *Window)
{
    ax_display *Display = AXLibWindowDisplay(Window);
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];

    tree_node *Node = GetTreeNodeFromWindowID(SpaceInfo->RootNode, Window->ID);
    return Node && Node->Parent && Node->Parent->WindowID == Window->ID;
}

void LockWindowToContainerSize(ax_window *Window)
{
    if(Window)
    {
        ax_display *Display = AXLibWindowDisplay(Window);
        space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];

        tree_node *Node = GetTreeNodeFromWindowID(SpaceInfo->RootNode, Window->ID);
        if(Node)
        {
            if(IsWindowFullscreen(Window))
                ResizeWindowToContainerSize(SpaceInfo->RootNode);
            else if(IsWindowParentContainer(Window))
                ResizeWindowToContainerSize(Node->Parent);
            else
                ResizeWindowToContainerSize(Node);
        }
        else
        {
            link_node *Link = GetLinkNodeFromTree(SpaceInfo->RootNode, Window->ID);
            if(Link)
            {
                ResizeWindowToContainerSize(Link);
            }
        }
    }
}

void DetachAndReinsertWindow(unsigned int WindowID, int Degrees)
{
    if(WindowID == 0)
        return;

    if(MarkedWindow && MarkedWindow->ID == WindowID)
    {
        ax_window *FocusedWindow = FocusedApplication->Focus;
        if(MarkedWindow == FocusedWindow)
            return;

        ToggleWindowFloating(WindowID, false);
        ClearMarkedWindow();
        ToggleWindowFloating(WindowID, false);
        MoveCursorToCenterOfFocusedWindow();
    }
    else
    {
        ax_window *ClosestWindow = NULL;
        if(FindClosestWindow(Degrees, &ClosestWindow, false))
        {
            ToggleWindowFloating(WindowID, false);
            ax_window *PrevMarkedWindow = MarkedWindow;
            MarkedWindow = ClosestWindow;
            ToggleWindowFloating(WindowID, false);
            MoveCursorToCenterOfFocusedWindow();
            MarkedWindow = PrevMarkedWindow;
            UpdateBorder(&MarkedBorder, MarkedWindow);
        }
    }
}

void SwapFocusedWindowWithMarked()
{
    ax_window *FocusedWindow = FocusedApplication->Focus;
    if(!FocusedWindow || !MarkedWindow || (FocusedWindow == MarkedWindow))
        return;

    ax_display *Display = AXLibWindowDisplay(FocusedWindow);
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];

    tree_node *TreeNode = GetTreeNodeFromWindowIDOrLinkNode(SpaceInfo->RootNode, FocusedWindow->ID);
    if(TreeNode)
    {
        tree_node *NewFocusNode = GetTreeNodeFromWindowID(SpaceInfo->RootNode, MarkedWindow->ID);
        if(NewFocusNode)
        {
            SwapNodeWindowIDs(TreeNode, NewFocusNode);
            MoveCursorToCenterOfFocusedWindow();
        }
    }

    ClearMarkedWindow();
}

void SwapFocusedWindowWithNearest(int Shift)
{
    ax_window *Window = FocusedApplication->Focus;
    if(!Window)
        return;

    ax_display *Display = AXLibWindowDisplay(Window);
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    if(SpaceInfo->Settings.Mode == SpaceModeMonocle)
    {
        link_node *Link = GetLinkNodeFromTree(SpaceInfo->RootNode, Window->ID);
        if(Link)
        {
            link_node *ShiftNode = Shift == 1 ? Link->Next : Link->Prev;
            if(KWMSettings.Cycle == CycleModeScreen && !ShiftNode)
            {
                SpaceInfo->RootNode->Type = NodeTypeLink;
                if(Shift == 1)
                    GetFirstLeafNode(SpaceInfo->RootNode, (void**)&ShiftNode);
                else
                    GetLastLeafNode(SpaceInfo->RootNode, (void**)&ShiftNode);
                SpaceInfo->RootNode->Type = NodeTypeTree;
            }

            if(ShiftNode)
            {
                SwapNodeWindowIDs(Link, ShiftNode);
                MoveCursorToCenterOfWindow(Window);
            }
        }
    }
    else if(SpaceInfo->Settings.Mode == SpaceModeBSP)
    {
        tree_node *Node = GetTreeNodeFromWindowIDOrLinkNode(SpaceInfo->RootNode, Window->ID);
        if(Node)
        {
            tree_node *ClosestNode = NULL;;

            if(Shift == 1)
            {
                ClosestNode = GetNearestTreeNodeToTheRight(Node);
                if(KWMSettings.Cycle == CycleModeScreen && !ClosestNode)
                {
                    GetFirstLeafNode(SpaceInfo->RootNode, (void**)&ClosestNode);
                }
            }
            else if(Shift == -1)
            {
                ClosestNode = GetNearestTreeNodeToTheLeft(Node);
                if(KWMSettings.Cycle == CycleModeScreen && !ClosestNode)
                {
                    GetLastLeafNode(SpaceInfo->RootNode, (void**)&ClosestNode);
                }
            }

            if(ClosestNode)
            {
                SwapNodeWindowIDs(Node, ClosestNode);
                MoveCursorToCenterOfTreeNode(ClosestNode);
            }
        }
    }
}

void SwapFocusedWindowDirected(int Degrees)
{
    ax_window *Window = FocusedApplication->Focus;
    if(!Window)
        return;

    ax_display *Display = AXLibWindowDisplay(Window);
    space_info *Space = &WindowTree[Display->Space->Identifier];
    if(Space->Settings.Mode == SpaceModeBSP)
    {
        tree_node *Node = GetTreeNodeFromWindowIDOrLinkNode(Space->RootNode, Window->ID);
        if(Node)
        {
            tree_node *ClosestNode = NULL;
            ax_window *ClosestWindow = NULL;
            if(FindClosestWindow(Degrees, &ClosestWindow, KWMSettings.Cycle == CycleModeScreen))
                ClosestNode = GetTreeNodeFromWindowID(Space->RootNode, ClosestWindow->ID);

            if(ClosestNode)
            {
                SwapNodeWindowIDs(Node, ClosestNode);
                MoveCursorToCenterOfTreeNode(ClosestNode);
            }
        }
    }
    else if(Space->Settings.Mode == SpaceModeMonocle)
    {
        if(Degrees == 90)
            SwapFocusedWindowWithNearest(1);
        else if(Degrees == 270)
            SwapFocusedWindowWithNearest(-1);
    }
}

bool WindowIsInDirection(ax_window *WindowA, ax_window *WindowB, int Degrees)
{
    ax_display *Display = AXLibWindowDisplay(WindowA);
    space_info *Space = &WindowTree[Display->Space->Identifier];
    tree_node *NodeA = GetTreeNodeFromWindowIDOrLinkNode(Space->RootNode, WindowA->ID);
    tree_node *NodeB = GetTreeNodeFromWindowIDOrLinkNode(Space->RootNode, WindowB->ID);

    if(!NodeA || !NodeB || NodeA == NodeB)
        return false;

    node_container *A = &NodeA->Container;
    node_container *B = &NodeB->Container;

    if(Degrees == 0 || Degrees == 180)
        return A->Y != B->Y && fmax(A->X, B->X) < fmin(B->X + B->Width, A->X + A->Width);
    else if(Degrees == 90 || Degrees == 270)
        return A->X != B->X && fmax(A->Y, B->Y) < fmin(B->Y + B->Height, A->Y + A->Height);

    return false;
}

void GetCenterOfWindow(ax_window *Window, int *X, int *Y)
{
    ax_display *Display = AXLibWindowDisplay(Window);
    space_info *Space = &WindowTree[Display->Space->Identifier];
    tree_node *Node = GetTreeNodeFromWindowIDOrLinkNode(Space->RootNode, Window->ID);
    if(Node)
    {
        *X = Node->Container.X + Node->Container.Width / 2;
        *Y = Node->Container.Y + Node->Container.Height / 2;
    }
    else
    {
        *X = -1;
        *Y = -1;
    }
}

double GetWindowDistance(ax_window *A, ax_window *B, int Degrees, bool Wrap)
{
    ax_display *Display = AXLibWindowDisplay(A);
    double Rank = INT_MAX;

    int X1, Y1, X2, Y2;
    GetCenterOfWindow(A, &X1, &Y1);
    GetCenterOfWindow(B, &X2, &Y2);

    if(Wrap)
    {
        if(Degrees == 0 && Y1 < Y2)
            Y2 -= Display->Frame.size.height;
        else if(Degrees == 180 && Y1 > Y2)
            Y2 += Display->Frame.size.height;
        else if(Degrees == 90 && X1 > X2)
            X2 += Display->Frame.size.width;
        else if(Degrees == 270 && X1 < X2)
            X2 -= Display->Frame.size.width;
    }

    double DeltaX = X2 - X1;
    double DeltaY = Y2 - Y1;
    double Angle = std::atan2(DeltaY, DeltaX);
    double Distance = std::hypot(DeltaX, DeltaY);
    double DeltaA = 0;

    if((Degrees == 0 && DeltaY >= 0) ||
       (Degrees == 90 && DeltaX <= 0) ||
       (Degrees == 180 && DeltaY <= 0) ||
       (Degrees == 270 && DeltaX >= 0))
        return INT_MAX;

    if(Degrees == 0)
        DeltaA = -M_PI_2 - Angle;
    else if(Degrees == 180)
        DeltaA = M_PI_2 - Angle;
    else if(Degrees == 90)
        DeltaA = 0.0 - Angle;
    else if(Degrees == 270)
        DeltaA = M_PI - std::fabs(Angle);

    Rank = Distance / std::cos(DeltaA / 2.0);
    return Rank;
}

bool FindClosestWindow(int Degrees, ax_window **ClosestWindow, bool Wrap)
{
    ax_window *FocusedWindow = FocusedApplication->Focus;
    return FindClosestWindow(FocusedWindow, Degrees, ClosestWindow, Wrap);
}

bool FindClosestWindow(ax_window *Match, int Degrees, ax_window **ClosestWindow, bool Wrap)
{
    std::vector<ax_window*> Windows = AXLibGetAllVisibleWindows();

    int MatchX, MatchY;
    GetCenterOfWindow(Match, &MatchX, &MatchY);

    double MinDist = INT_MAX;
    for(int Index = 0; Index < Windows.size(); ++Index)
    {
        ax_window *Window = Windows[Index];
        if(Match->ID != Window->ID &&
           WindowIsInDirection(Match, Window, Degrees))
        {
            double Dist = GetWindowDistance(Match, Window, Degrees, Wrap);
            if(Dist < MinDist)
            {
                MinDist = Dist;
                *ClosestWindow = Window;
            }
        }
    }

    return MinDist != INT_MAX;
}

void ShiftWindowFocusDirected(int Degrees)
{
    ax_application *Application = AXLibGetFocusedApplication();
    ax_window *Window = FocusedApplication ? FocusedApplication->Focus : NULL;
    if(!Application || !Window)
    {
        if(Degrees == 90)
            FocusLastLeafNode(AXLibCursorDisplay());
        else if(Degrees == 270)
            FocusFirstLeafNode(AXLibCursorDisplay());

        return;
    }

    ax_display *Display = AXLibWindowDisplay(Window);
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    if(SpaceInfo->Settings.Mode == SpaceModeBSP)
    {
        ax_window *ClosestWindow = NULL;
        if((KWMSettings.Cycle == CycleModeDisabled &&
            FindClosestWindow(Degrees, &ClosestWindow, false)) ||
           (KWMSettings.Cycle == CycleModeScreen &&
            FindClosestWindow(Degrees, &ClosestWindow, true)))
        {
            AXLibSetFocusedWindow(ClosestWindow);
            MoveCursorToCenterOfWindow(ClosestWindow);
        }
    }
    else if(SpaceInfo->Settings.Mode == SpaceModeMonocle)
    {
        if(Degrees == 90)
            ShiftWindowFocus(1);
        else if(Degrees == 270)
            ShiftWindowFocus(-1);
    }
}

void ShiftWindowFocus(int Shift)
{
    ax_application *Application = AXLibGetFocusedApplication();
    ax_window *Window = FocusedApplication ? FocusedApplication->Focus : NULL;
    if(!Application || !Window)
    {
        if(Shift == 1)
            FocusLastLeafNode(AXLibCursorDisplay());
        else if(Shift == -1)
            FocusFirstLeafNode(AXLibCursorDisplay());

        return;
    }

    ax_display *Display = AXLibWindowDisplay(Window);
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    if(SpaceInfo->Settings.Mode == SpaceModeMonocle)
    {
        link_node *Link = GetLinkNodeFromTree(SpaceInfo->RootNode, Window->ID);
        if(Link)
        {
            link_node *FocusNode = Shift == 1 ? Link->Next : Link->Prev;
            if(KWMSettings.Cycle == CycleModeScreen && !FocusNode)
            {
                SpaceInfo->RootNode->Type = NodeTypeLink;
                if(Shift == 1)
                    GetFirstLeafNode(SpaceInfo->RootNode, (void**)&FocusNode);
                else
                    GetLastLeafNode(SpaceInfo->RootNode, (void**)&FocusNode);
                SpaceInfo->RootNode->Type = NodeTypeTree;
            }

            if(FocusNode)
            {
                SetWindowFocusByNode(FocusNode);
                MoveCursorToCenterOfLinkNode(FocusNode);
            }
        }
    }
    else if(SpaceInfo->Settings.Mode == SpaceModeBSP)
    {
        tree_node *TreeNode = GetTreeNodeFromWindowID(SpaceInfo->RootNode, Window->ID);
        if(TreeNode)
        {
            tree_node *FocusNode = NULL;

            if(Shift == 1)
            {
                FocusNode = GetNearestTreeNodeToTheRight(TreeNode);
                while(IsPseudoNode(FocusNode))
                    FocusNode = GetNearestTreeNodeToTheRight(FocusNode);

                if(KWMSettings.Cycle == CycleModeScreen && !FocusNode)
                {
                    GetFirstLeafNode(SpaceInfo->RootNode, (void**)&FocusNode);
                    while(IsPseudoNode(FocusNode))
                        FocusNode = GetNearestTreeNodeToTheRight(FocusNode);
                }
            }
            else if(Shift == -1)
            {
                FocusNode = GetNearestTreeNodeToTheLeft(TreeNode);
                while(IsPseudoNode(FocusNode))
                    FocusNode = GetNearestTreeNodeToTheLeft(FocusNode);

                if(KWMSettings.Cycle == CycleModeScreen && !FocusNode)
                {
                    GetLastLeafNode(SpaceInfo->RootNode, (void**)&FocusNode);
                    while(IsPseudoNode(FocusNode))
                        FocusNode = GetNearestTreeNodeToTheLeft(FocusNode);
                }
            }

            SetWindowFocusByNode(FocusNode);
            MoveCursorToCenterOfTreeNode(FocusNode);
        }
    }
}

void ShiftSubTreeWindowFocus(int Shift)
{
    ax_window *Window = FocusedApplication->Focus;
    if(!Window)
        return;

    ax_display *Display = AXLibWindowDisplay(Window);
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    if(SpaceInfo->Settings.Mode == SpaceModeBSP)
    {
        link_node *Link = GetLinkNodeFromWindowID(SpaceInfo->RootNode, Window->ID);
        if(Link)
        {
            if(Shift == 1)
            {
                link_node *FocusNode = Link->Next;
                if(FocusNode)
                {
                    SetWindowFocusByNode(FocusNode);
                    MoveCursorToCenterOfFocusedWindow();
                }
                else if(KWMSettings.Cycle == CycleModeScreen)
                {
                    tree_node *Root = GetTreeNodeFromLink(SpaceInfo->RootNode, Link);
                    SetWindowFocusByNode(Root);
                    MoveCursorToCenterOfFocusedWindow();
                }
            }
            else if(Shift == -1)
            {
                link_node *FocusNode = Link->Prev;
                if(FocusNode)
                {
                    SetWindowFocusByNode(FocusNode);
                }
                else
                {
                    tree_node *Root = GetTreeNodeFromLink(SpaceInfo->RootNode, Link);
                    SetWindowFocusByNode(Root);
                }
                MoveCursorToCenterOfFocusedWindow();
            }
        }
        else
        {
            tree_node *Root = GetTreeNodeFromWindowID(SpaceInfo->RootNode, Window->ID);
            if(Root)
            {
                if(Shift == 1)
                {
                    SetWindowFocusByNode(Root->List);
                    MoveCursorToCenterOfFocusedWindow();
                }
                else if(KWMSettings.Cycle == CycleModeScreen)
                {
                    link_node *FocusNode;
                    node_type PrevType = Root->Type;
                    if(Root->Type != NodeTypeLink)
                        Root->Type = NodeTypeLink;

                    GetLastLeafNode(Root, (void**)&FocusNode);
                    if(Root->Type != PrevType)
                        Root->Type = PrevType;

                    SetWindowFocusByNode(FocusNode);
                    MoveCursorToCenterOfFocusedWindow();
                }
            }
        }
    }
}

void FocusWindowByID(uint32_t WindowID)
{
    ax_window *Window = GetWindowByID(WindowID);
    if(Window)
    {
        AXLibSetFocusedWindow(Window);
    }
}

void ClearMarkedWindow()
{
    MarkedWindow = NULL;
    ClearBorder(&MarkedBorder);
}

void MarkWindowContainer(ax_window *Window)
{
    if(Window)
    {
        if(MarkedWindow && MarkedWindow->ID == Window->ID)
        {
            DEBUG("MarkWindowContainer() Unmarked " << Window->Name);
            ClearMarkedWindow();
        }
        else
        {
            DEBUG("MarkWindowContainer() Marked " << Window->Name);
            MarkedWindow = Window;
            UpdateBorder(&MarkedBorder, MarkedWindow);
        }
    }
}

void MarkFocusedWindowContainer()
{
    MarkWindowContainer(FocusedApplication->Focus);
}

void SetWindowFocusByNode(tree_node *Node)
{
    if(Node)
    {
        ax_window *Window = GetWindowByID(Node->WindowID);
        if(Window)
        {
            DEBUG("SetWindowFocusByNode()");
            AXLibSetFocusedWindow(Window);
        }
    }
}

void SetWindowFocusByNode(link_node *Link)
{
    if(Link)
    {
        ax_window *Window = GetWindowByID(Link->WindowID);
        if(Window)
        {
            DEBUG("SetWindowFocusByNode()");
            AXLibSetFocusedWindow(Window);
        }
    }
}

void CenterWindowInsideNodeContainer(ax_window *Window, int *Xptr, int *Yptr, int *Wptr, int *Hptr)
{
    CGPoint WindowOrigin = AXLibGetWindowPosition(Window->Ref);
    CGSize WindowOGSize = AXLibGetWindowSize(Window->Ref);

    int &X = *Xptr, &Y = *Yptr, &Width = *Wptr, &Height = *Hptr;
    int XDiff = (X + Width) - (WindowOrigin.x + WindowOGSize.width);
    int YDiff = (Y + Height) - (WindowOrigin.y + WindowOGSize.height);

    if(abs(XDiff) > 0 || abs(YDiff) > 0)
    {
        double XOff = XDiff / 2.0f;
        X += XOff > 0 ? XOff : 0;
        Width -= XOff > 0 ? XOff : 0;

        double YOff = YDiff / 2.0f;
        Y += YOff > 0 ? YOff : 0;
        Height -= YOff > 0 ? YOff : 0;

        AXLibAddFlags(Window, AXWindow_MoveIntrinsic);
        if(!AXLibSetWindowPosition(Window->Ref, X, Y))
            AXLibClearFlags(Window, AXWindow_MoveIntrinsic);

        AXLibAddFlags(Window, AXWindow_SizeIntrinsic);
        if(!AXLibSetWindowSize(Window->Ref, Width, Height))
            AXLibClearFlags(Window, AXWindow_SizeIntrinsic);
    }
}

void SetWindowDimensions(ax_window *Window, int X, int Y, int Width, int Height)
{
    if(!AXLibIsWindowFullscreen(Window->Ref))
    {
        bool Changed = false;

        if((Window->Position.x != X) ||
           (Window->Position.y != Y))
        {
            Changed = true;
            AXLibAddFlags(Window, AXWindow_MoveIntrinsic);
            if(!AXLibSetWindowPosition(Window->Ref, X, Y))
                AXLibClearFlags(Window, AXWindow_MoveIntrinsic);
        }

        if((Window->Size.width != Width) ||
           (Window->Size.height != Height))
        {
            Changed = true;
            AXLibAddFlags(Window, AXWindow_SizeIntrinsic);
            if(!AXLibSetWindowSize(Window->Ref, Width, Height))
                AXLibClearFlags(Window, AXWindow_SizeIntrinsic);
        }

        if(Changed)
            CenterWindowInsideNodeContainer(Window, &X, &Y, &Width, &Height);
    }
}

void CenterWindow(ax_display *Display, ax_window *Window)
{
    space_settings *SpaceSettings = GetSpaceSettingsForDisplay(Display->ArrangementID);
    CGRect Dimension = {};

    if((AXLibHasFlags(Window, AXWindow_Resizable)) &&
       (SpaceSettings) &&
       (SpaceSettings->FloatDim.width != 0) &&
       (SpaceSettings->FloatDim.height != 0))
    {
        Dimension.size.width = SpaceSettings->FloatDim.width;
        Dimension.size.height = SpaceSettings->FloatDim.height;
        Dimension.origin.x = Display->Frame.origin.x + ((Display->Frame.size.width / 2) - (Dimension.size.width / 2));
        Dimension.origin.y = Display->Frame.origin.y + ((Display->Frame.size.height / 2) - (Dimension.size.height / 2));
    }
    else
    {
        Dimension.origin.x = Display->Frame.origin.x + Display->Frame.size.width / 4;
        Dimension.origin.y = Display->Frame.origin.y + Display->Frame.size.height / 4;
        Dimension.size.width = Display->Frame.size.width / 2;
        Dimension.size.height = Display->Frame.size.height / 2;
    }

    SetWindowDimensions(Window, Dimension.origin.x, Dimension.origin.y,
                        Dimension.size.width, Dimension.size.height);
}

ax_window *GetWindowByID(uint32_t WindowID)
{
    ax_window *Result = NULL;

    ax_application_map *Applications = BeginAXLibApplications();
    for(ax_application_map_iter It = Applications->begin();
        It != Applications->end();
        ++It)
    {
        ax_application *Application = It->second;
        Result = AXLibFindApplicationWindow(Application, WindowID);
        if(Result)
        {
            break;
        }
    }

    EndAXLibApplications();
    return Result;
}

void MoveFloatingWindow(int X, int Y)
{
    ax_window *Window = FocusedApplication->Focus;
    if(!Window)
        return;

    if(AXLibHasFlags(Window, AXWindow_Floating))
    {
        AXLibSetWindowPosition(Window->Ref,
                               Window->Position.x + X,
                               Window->Position.y + Y);
    }
}
