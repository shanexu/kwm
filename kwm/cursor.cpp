#include "cursor.h"
#include "window.h"
#include "node.h"
#include "tree.h"
#include "space.h"
#include "border.h"
#include "../axlib/axlib.h"
#include "display.h"
#include "helpers.h"

#define internal static
extern std::map<std::string, space_info> WindowTree;
extern ax_state AXState;
extern ax_application *FocusedApplication;
extern kwm_settings KWMSettings;
extern kwm_border MarkedBorder;

internal bool DragMoveWindow = false;
internal tree_node *MarkedNode = NULL;

internal bool DragResizeNode = false;

struct ResizeStateStruct {
    tree_node *Node;
    tree_node *HorizontalAncestor;
    tree_node *VerticalAncestor;
    ax_display *Display;
};

struct ResizeIndicatorBorder {
    kwm_border *Border;
    tree_node *Node;
};

internal std::vector<ResizeIndicatorBorder> ResizeIndicatorBorders;

internal ResizeStateStruct ResizeState = ResizeStateStruct();

internal inline CGPoint
GetCursorPos()
{
    CGEventRef Event = CGEventCreate(NULL);
    CGPoint Cursor = CGEventGetLocation(Event);
    CFRelease(Event);

    return Cursor;
}

internal bool
IsCursorInsideRect(double X, double Y, double Width, double Height)
{
    CGPoint Cursor = GetCursorPos();
    if(Cursor.x >= X &&
       Cursor.x <= X + Width &&
       Cursor.y >= Y &&
       Cursor.y <= Y + Height)
        return true;

    return false;
}

internal inline void
ClearMarkedNode()
{
    MarkedNode = NULL;
    ClearBorder(&MarkedBorder);
}

EVENT_CALLBACK(Callback_AXEvent_MouseMoved)
{
    FocusWindowBelowCursor();
}

EVENT_CALLBACK(Callback_AXEvent_LeftMouseDown)
{
    if((FocusedApplication && FocusedApplication->Focus) &&
       (IsCursorInsideRect(FocusedApplication->Focus->Position.x,
                           FocusedApplication->Focus->Position.y,
                           FocusedApplication->Focus->Size.width,
                           FocusedApplication->Focus->Size.height)))
    {
        DEBUG("AXEvent_LeftMouseDown");
        DragMoveWindow = true;
    }
}

EVENT_CALLBACK(Callback_AXEvent_LeftMouseUp)
{
    if(DragMoveWindow)
    {
        DEBUG("AXEvent_LeftMouseUp");
        DragMoveWindow = false;

        /* NOTE(koekeishiya): DragMoveWindow can only be true if the LeftMouseDown event
         * was triggered on top of a window. Thus, we assume that the FocusedApplication
         * and its Focus can never be NULL here. */

        ax_window *Window = FocusedApplication->Focus;
        ax_display *WindowDisplay = AXLibWindowDisplay(Window);
        ax_display *CursorDisplay = AXLibCursorDisplay();
        
        if (WindowDisplay != CursorDisplay) {
            MoveWindowToDisplay(Window, CursorDisplay);
        } else {
            if(MarkedNode && MarkedNode->WindowID != Window->ID)
            {
                tree_node *Node = GetTreeNodeFromWindowIDOrLinkNode(WindowTree[WindowDisplay->Space->Identifier].RootNode, Window->ID);
                if(Node)
                {
                    SwapNodeWindowIDs(Node, MarkedNode);
                }
            }
        }

        ClearMarkedNode();
    }
}

EVENT_CALLBACK(Callback_AXEvent_LeftMouseDragged)
{
    CGPoint *Cursor = (CGPoint *) Event->Context;

    if(DragMoveWindow)
    {
        DEBUG("AXEvent_LeftMouseDragged");

        /* NOTE(koekeishiya): DragMoveWindow can only be true if the LeftMouseDown event
         * was triggered on top of a window. Thus, we assume that the FocusedApplication
         * and its Focus can never be NULL here. */

        ax_window *Window = FocusedApplication->Focus;
        if(AXLibHasFlags(Window, AXWindow_Floating))
        {
            double X = Cursor->x - Window->Size.width / 2;
            double Y = Cursor->y - Window->Size.height / 2;
            AXLibSetWindowPosition(Window->Ref, X, Y);
        }
        else
        {
            ax_display *CursorDisplay = AXLibCursorDisplay();
            ax_display *WindowDisplay = AXLibWindowDisplay(Window);
            tree_node *NewNode;
            if (WindowDisplay != CursorDisplay)
            {
                NewNode = WindowTree[CursorDisplay->Space->Identifier].RootNode;
            }
            else
            {
                NewNode = GetTreeNodeForPoint(WindowTree[WindowDisplay->Space->Identifier].RootNode, Cursor);
            }
            if(NewNode && NewNode != MarkedNode)
            {
                MarkedNode = NewNode;
                UpdateBorder(&MarkedBorder, MarkedNode);
            }
        }
    }

    free(Cursor);
}

internal ResizeIndicatorBorder
InitializeResizedNodeBorder(tree_node *Node) {
    DEBUG("Making resize border");
    kwm_border *Border = (kwm_border *) malloc( sizeof(*Border) );
    Border->Type = BORDER_MARKED;
    Border->BorderId = 0;
    Border->Enabled = true;
    Border->Radius = 6;
    Border->Width = 2;
    Border->Color = (color) {0.6, 0.5, 1.0, 0.4};
    
    return (ResizeIndicatorBorder) {Border, Node};
}

internal void
InitializeResizedNodeBorders(tree_node *Node) {
    DEBUG("Initing borders");
    if(Node)
    {
        if (Node->WindowID)
            ResizeIndicatorBorders.push_back(InitializeResizedNodeBorder(Node));
        if (Node->LeftChild)
            InitializeResizedNodeBorders(Node->LeftChild);
        if (Node->RightChild)
            InitializeResizedNodeBorders(Node->RightChild);
    }
}

internal void
UpdateResizedNodeBorders()
{
    for(std::vector<ResizeIndicatorBorder>::iterator it = ResizeIndicatorBorders.begin();
        it != ResizeIndicatorBorders.end();
        ++it)
    {
        UpdateBorder(it->Border, it->Node);
    }
}

internal void
FreeResizedNodeBorders()
{
    while (!ResizeIndicatorBorders.empty())
    {
        kwm_border *Border = ResizeIndicatorBorders.back().Border;
        CloseBorder(Border);
        free(Border);
        ResizeIndicatorBorders.pop_back();
    }
    
}

EVENT_CALLBACK(Callback_AXEvent_RightMouseDown)
{
    CGPoint CursorPos = GetCursorPos();
    ax_display *CursorDisplay = AXLibCursorDisplay();
    tree_node *Root = WindowTree[CursorDisplay->Space->Identifier].RootNode;
    tree_node *NodeBelowCursor = GetTreeNodeForPoint(Root, &CursorPos);
    
    if(!NodeBelowCursor)
        return;

    ax_window *WindowBelowCursor = GetWindowByID(NodeBelowCursor->WindowID);

    if (!WindowBelowCursor)
        return;
    
    DragResizeNode = true;
    
    ax_window *NorthNeighbour, *EastNeighbour, *SouthNeighbour, *WestNeighbour;
    bool HaveNorth = FindClosestWindow(WindowBelowCursor, 0, &NorthNeighbour, false);
    bool HaveEast = FindClosestWindow(WindowBelowCursor, 90, &EastNeighbour, false);
    bool HaveSouth = FindClosestWindow(WindowBelowCursor, 180, &SouthNeighbour, false);
    bool HaveWest = FindClosestWindow(WindowBelowCursor, 270, &WestNeighbour, false);
    
    ax_window *VerticalNeighbour;
    if (HaveNorth && HaveSouth) {
        //find whether north or south bound is closest to cursor
        VerticalNeighbour = NorthNeighbour;
    } else if (HaveNorth) {
        VerticalNeighbour = NorthNeighbour;
    } else if (HaveSouth) {
        VerticalNeighbour = SouthNeighbour;
    } else {
        VerticalNeighbour = NULL;
    }
    
    ax_window *HorizontalNeighbour;
    if (HaveEast && HaveWest) {
        //find whether north or south bound is closest to cursor
        HorizontalNeighbour = EastNeighbour;
    } else if (HaveEast) {
        HorizontalNeighbour = EastNeighbour;
    } else if (HaveWest) {
        HorizontalNeighbour = WestNeighbour;
    } else {
        HorizontalNeighbour = NULL;
    }
    
    tree_node *VerticalTarget = (VerticalNeighbour)
        ? GetTreeNodeFromWindowIDOrLinkNode(Root, VerticalNeighbour->ID)
        : NULL;
    ResizeState.VerticalAncestor = FindLowestCommonAncestor(NodeBelowCursor, VerticalTarget);
    
    tree_node *HorizontalTarget = (HorizontalNeighbour)
        ? GetTreeNodeFromWindowIDOrLinkNode(Root, HorizontalNeighbour->ID)
        : NULL;
    ResizeState.HorizontalAncestor = FindLowestCommonAncestor(NodeBelowCursor, HorizontalTarget);
    
    ResizeState.Node = NodeBelowCursor;
    ResizeState.Display = CursorDisplay;
    
    tree_node *AbsoluteAncestor;
    if (ResizeState.VerticalAncestor && ResizeState.HorizontalAncestor)
        AbsoluteAncestor = FindLowestCommonAncestor(ResizeState.VerticalAncestor, ResizeState.HorizontalAncestor);
    else if (ResizeState.VerticalAncestor)
        AbsoluteAncestor = ResizeState.VerticalAncestor;
    else if (ResizeState.HorizontalAncestor)
        AbsoluteAncestor = ResizeState.HorizontalAncestor;
    else
        AbsoluteAncestor = NULL;

    if (AbsoluteAncestor == NULL)
        AbsoluteAncestor = Root;
    
    InitializeResizedNodeBorders(AbsoluteAncestor);
    
    DEBUG("AXEvent_RightMouseDown");
}

EVENT_CALLBACK(Callback_AXEvent_RightMouseUp) {
    if(DragResizeNode)
    {
        DEBUG("AXEvent_RightMouseUp");
        
        FreeResizedNodeBorders();
        ApplyTreeNodeContainer(ResizeState.HorizontalAncestor);
        ApplyTreeNodeContainer(ResizeState.VerticalAncestor);
        ResizeState = ResizeStateStruct();
        DragResizeNode = false;
    }
}

EVENT_CALLBACK(Callback_AXEvent_RightMouseDragged) {
    CGPoint CursorPos = GetCursorPos();
    static const double SplitRatioMinDifference = 0.002;
    if (DragResizeNode)
    {
        DEBUG("AXEvent_RightMouseDragged");
        if (ResizeState.VerticalAncestor) {
            double ContainerTop = ResizeState.VerticalAncestor->Container.Y;
            double ContainerHeight = ResizeState.VerticalAncestor->Container.Height;
            double SplitRatio = (CursorPos.y - ContainerTop) / ContainerHeight;
            if (fabs(SplitRatio - ResizeState.VerticalAncestor->SplitRatio) > SplitRatioMinDifference)
                SetContainerSplitRatio(SplitRatio, ResizeState.Node, ResizeState.VerticalAncestor, ResizeState.Display, false);
        }
        
        if (ResizeState.HorizontalAncestor) {
            double ContainerLeft = ResizeState.HorizontalAncestor->Container.X;
            double ContainerWidth = ResizeState.HorizontalAncestor->Container.Width;
            double SplitRatio = (CursorPos.x - ContainerLeft) / ContainerWidth;
            if (fabs(SplitRatio - ResizeState.HorizontalAncestor->SplitRatio) > SplitRatioMinDifference)
                SetContainerSplitRatio(SplitRatio, ResizeState.Node, ResizeState.HorizontalAncestor, ResizeState.Display, false);
        }
        UpdateResizedNodeBorders();
    }
    
    CGPoint *EventCursorPos = (CGPoint *) Event->Context;
    free(EventCursorPos);
}


void MoveCursorToCenterOfTreeNode(tree_node *Node)
{
    if((HasFlags(&KWMSettings, Settings_MouseFollowsFocus)) &&
       (!IsCursorInsideRect(Node->Container.X, Node->Container.Y,
                            Node->Container.Width, Node->Container.Height)))
    {
        CGWarpMouseCursorPosition(CGPointMake(Node->Container.X + Node->Container.Width / 2,
                                              Node->Container.Y + Node->Container.Height / 2));
    }
}

void MoveCursorToCenterOfLinkNode(link_node *Link)
{
    if((HasFlags(&KWMSettings, Settings_MouseFollowsFocus)) &&
       (!IsCursorInsideRect(Link->Container.X, Link->Container.Y,
                            Link->Container.Width, Link->Container.Height)))
    {
        CGWarpMouseCursorPosition(CGPointMake(Link->Container.X + Link->Container.Width / 2,
                                              Link->Container.Y + Link->Container.Height / 2));
    }
}

void MoveCursorToCenterOfWindow(ax_window *Window)
{
    if((HasFlags(&KWMSettings, Settings_MouseFollowsFocus)) &&
       (!IsCursorInsideRect(Window->Position.x, Window->Position.y,
                            Window->Size.width, Window->Size.height)))
    {
        CGWarpMouseCursorPosition(CGPointMake(Window->Position.x + Window->Size.width / 2,
                                              Window->Position.y + Window->Size.height / 2));
    }
}

void MoveCursorToCenterOfFocusedWindow()
{
    if(FocusedApplication && FocusedApplication->Focus)
        MoveCursorToCenterOfWindow(FocusedApplication->Focus);
}

void FocusWindowBelowCursor()
{
    ax_application *Application = AXLibGetFocusedApplication();
    ax_window *FocusedWindow = NULL;
    if(Application)
    {
        FocusedWindow = Application->Focus;
         if((FocusedWindow) && (IsCursorInsideRect(FocusedWindow->Position.x, FocusedWindow->Position.y,
                                                   FocusedWindow->Size.width, FocusedWindow->Size.height)))
             return;
    }

    uint32_t WindowID = AXLibGetWindowBelowCursor();
    if(WindowID == 0)
        return;

    std::map<pid_t, ax_application>::iterator It;
    for(It = AXState.Applications.begin(); It != AXState.Applications.end(); ++It)
    {
        ax_application *Application = &It->second;
        ax_window *Window = AXLibFindApplicationWindow(Application, WindowID);
        if(Window)
        {
            if((AXLibIsWindowStandard(Window)) ||
               (AXLibIsWindowCustom(Window)))
            {
                if(Application == Window->Application)
                {
                    if(FocusedWindow != Window)
                        AXLibSetFocusedWindow(Window);
                }
                else
                {
                    AXLibSetFocusedWindow(Window);
                }
            }
            return;
        }
    }
}
