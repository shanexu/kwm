#include "tree.h"
#include "node.h"
#include "container.h"
#include "helpers.h"
#include "display.h"
#include "space.h"
#include "window.h"
#include "border.h"
#include "cursor.h"
#include "../axlib/axlib.h"

#define internal static
extern std::map<std::string, space_info> WindowTree;

/* NOTE(koekeishiya): Should not be able to return null as the binary-tree is always proper. */
tree_node * FindFirstMinDepthLeafNode(tree_node *Root)
{
    std::queue<tree_node *> Queue;
    Queue.push(Root);

    while(!Queue.empty())
    {
        tree_node *Node = Queue.front();
        Queue.pop();

        if(IsLeafNode(Node))
            return Node;

        if(Node->LeftChild)
            Queue.push(Node->LeftChild);

        if(Node->RightChild)
            Queue.push(Node->RightChild);
    }

    return NULL;
}

internal bool
CreateBSPTree(tree_node *RootNode, ax_display *Display, std::vector<uint32_t> *WindowsPtr)
{
    bool Result = false;
    std::vector<uint32_t> &Windows = *WindowsPtr;

    if(!Windows.empty())
    {
        tree_node *Root = RootNode;
        Root->WindowID = Windows[0];
        for(std::size_t Index = 1; Index < Windows.size(); ++Index)
        {
            Root = FindFirstMinDepthLeafNode(RootNode);
            Assert(Root != NULL);

            DEBUG("CreateBSPTree() Create pair of leafs");
            CreateLeafNodePair(Display, Root, Root->WindowID, Windows[Index], GetOptimalSplitMode(Root));
        }

        Result = true;
    }

    return Result;
}

internal bool
CreateMonocleTree(tree_node *RootNode, ax_display *Display, std::vector<uint32_t> *WindowsPtr)
{
    bool Result = false;
    std::vector<uint32_t> &Windows = *WindowsPtr;

    if(!Windows.empty())
    {
        tree_node *Root = RootNode;
        Root->List = CreateLinkNode();

        SetLinkNodeContainer(Display, Root->List);
        Root->List->WindowID = Windows[0];

        link_node *Link = Root->List;
        for(std::size_t Index = 1; Index < Windows.size(); ++Index)
        {
            link_node *Next = CreateLinkNode();
            SetLinkNodeContainer(Display, Next);
            Next->WindowID = Windows[Index];

            Link->Next = Next;
            Next->Prev = Link;
            Link = Next;
        }

        Result = true;
    }

    return Result;
}

tree_node *CreateTreeFromWindowIDList(ax_display *Display, std::vector<uint32_t> *Windows)
{
    tree_node *RootNode = CreateRootNode();
    SetRootNodeContainer(Display, RootNode);
    bool Result = false;

    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    if(SpaceInfo->Settings.Mode == SpaceModeBSP)
        Result = CreateBSPTree(RootNode, Display, Windows);
    else if(SpaceInfo->Settings.Mode == SpaceModeMonocle)
        Result = CreateMonocleTree(RootNode, Display, Windows);

    if(!Result)
    {
        free(RootNode);
        RootNode = NULL;
    }

    return RootNode;
}

tree_node *GetNearestLeafNodeNeighbour(tree_node *Node)
{
    if(Node && IsLeafNode(Node))
        return IsLeftChild(Node) ? GetNearestTreeNodeToTheRight(Node) : GetNearestTreeNodeToTheLeft(Node);

    return NULL;
}

/* NOTE(koekeishiya): This function can probably be optimized by checking
 * containers in a Root->Child approach, rather than iterating through
 * every leaf node. */
tree_node *GetTreeNodeForPoint(tree_node *Node, CGPoint *Point)
{
    if(Node)
    {
        tree_node *CurrentNode = NULL;
        GetFirstLeafNode(Node, (void**)&CurrentNode);
        while(CurrentNode)
        {
            if(Point->x >= CurrentNode->Container.X &&
               Point->x <= CurrentNode->Container.X + CurrentNode->Container.Width &&
               Point->y >= CurrentNode->Container.Y &&
               Point->y <= CurrentNode->Container.Y + CurrentNode->Container.Height)
                return CurrentNode;

            CurrentNode = GetNearestTreeNodeToTheRight(CurrentNode);
        }
    }

    return NULL;
}

tree_node *GetTreeNodeFromWindowID(tree_node *Node, uint32_t WindowID)
{
    if(Node)
    {
        tree_node *CurrentNode = NULL;
        GetFirstLeafNode(Node, (void**)&CurrentNode);
        while(CurrentNode)
        {
            if(CurrentNode->WindowID == WindowID)
                return CurrentNode;

            CurrentNode = GetNearestTreeNodeToTheRight(CurrentNode);
        }
    }

    return NULL;
}

tree_node *GetTreeNodeFromWindowIDOrLinkNode(tree_node *Node, uint32_t WindowID)
{
    tree_node *Result = NULL;
    Result = GetTreeNodeFromWindowID(Node, WindowID);
    if(!Result)
    {
        link_node *Link = GetLinkNodeFromWindowID(Node, WindowID);
        Result = GetTreeNodeFromLink(Node, Link);
    }

    return Result;
}

link_node *GetLinkNodeFromWindowID(tree_node *Root, uint32_t WindowID)
{
    if(Root)
    {
        tree_node *Node = NULL;
        GetFirstLeafNode(Root, (void**)&Node);
        while(Node)
        {
            link_node *Link = GetLinkNodeFromTree(Node, WindowID);
            if(Link)
                return Link;

            Node = GetNearestTreeNodeToTheRight(Node);
        }
    }

    return NULL;
}

link_node *GetLinkNodeFromTree(tree_node *Root, uint32_t WindowID)
{
    if(Root)
    {
        link_node *Link = Root->List;
        while(Link)
        {
            if(Link->WindowID == WindowID)
                return Link;

            Link = Link->Next;
        }
    }

    return NULL;
}

tree_node *GetTreeNodeFromLink(tree_node *Root, link_node *Link)
{
    if(Root && Link)
    {
        tree_node *Node = NULL;
        GetFirstLeafNode(Root, (void**)&Node);
        while(Node)
        {
            if(GetLinkNodeFromTree(Node, Link->WindowID) == Link)
                return Node;

            Node = GetNearestTreeNodeToTheRight(Node);
        }
    }

    return NULL;
}

tree_node *GetNearestTreeNodeToTheLeft(tree_node *Node)
{
    if(Node)
    {
        if(Node->Parent)
        {
            tree_node *Root = Node->Parent;
            if(Root->LeftChild == Node)
                return GetNearestTreeNodeToTheLeft(Root);

            if(IsLeafNode(Root->LeftChild))
                return Root->LeftChild;

            Root = Root->LeftChild;
            while(!IsLeafNode(Root->RightChild))
                Root = Root->RightChild;

            return Root->RightChild;
        }
    }

    return NULL;
}

tree_node *GetNearestTreeNodeToTheRight(tree_node *Node)
{
    if(Node)
    {
        if(Node->Parent)
        {
            tree_node *Root = Node->Parent;
            if(Root->RightChild == Node)
                return GetNearestTreeNodeToTheRight(Root);

            if(IsLeafNode(Root->RightChild))
                return Root->RightChild;

            Root = Root->RightChild;
            while(!IsLeafNode(Root->LeftChild))
                Root = Root->LeftChild;

            return Root->LeftChild;
        }
    }

    return NULL;
}

void GetFirstLeafNode(tree_node *Node, void **Result)
{
    if(Node)
    {
        if(Node->Type == NodeTypeLink)
            *Result = Node->List;

        else if(Node->Type == NodeTypeTree)
        {
            while(Node->LeftChild)
                Node = Node->LeftChild;

            *Result = Node;
        }
    }
}

void GetLastLeafNode(tree_node *Node, void **Result)
{
    if(Node)
    {
        if(Node->Type == NodeTypeLink)
        {
            link_node *Link = Node->List;
            while(Link->Next)
                Link = Link->Next;

            *Result = Link;
        }
        else if(Node->Type == NodeTypeTree)
        {
            while(Node->RightChild)
                Node = Node->RightChild;

            *Result = Node;
        }
    }
}

void FocusFirstLeafNode(ax_display *Display)
{
    if(Display)
    {
        space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
        if(!SpaceInfo->RootNode)
            return;

        if(SpaceInfo->Settings.Mode == SpaceModeBSP)
        {
            tree_node *Node = NULL;
            GetFirstLeafNode(SpaceInfo->RootNode, (void**)&Node);
            SetWindowFocusByNode(Node);
            MoveCursorToCenterOfTreeNode(Node);
        }
        else if(SpaceInfo->Settings.Mode == SpaceModeMonocle)
        {
            link_node *Node = SpaceInfo->RootNode->List;
            SetWindowFocusByNode(Node);
            MoveCursorToCenterOfLinkNode(Node);
        }
    }
}

void FocusLastLeafNode(ax_display *Display)
{
    if(Display)
    {
        space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
        if(!SpaceInfo->RootNode)
            return;

        if(SpaceInfo->Settings.Mode == SpaceModeBSP)
        {
            tree_node *Node = NULL;
            GetLastLeafNode(SpaceInfo->RootNode, (void **)&Node);
            SetWindowFocusByNode(Node);
            MoveCursorToCenterOfTreeNode(Node);
        }
        else if(SpaceInfo->Settings.Mode == SpaceModeMonocle)
        {
            link_node *Node = SpaceInfo->RootNode->List;
            while(Node && Node->Next)
                Node = Node->Next;

            SetWindowFocusByNode(Node);
            MoveCursorToCenterOfLinkNode(Node);
        }
    }
}

tree_node *GetFirstPseudoLeafNode(tree_node *Node)
{
    tree_node *Leaf = NULL;
    GetFirstLeafNode(Node, (void**)&Leaf);
    while(Leaf && Leaf->WindowID != 0)
        Leaf = GetNearestTreeNodeToTheRight(Leaf);

    return Leaf;
}

void ApplyLinkNodeContainer(link_node *Link)
{
    if(Link)
    {
        ResizeWindowToContainerSize(Link);
        if(Link->Next)
            ApplyLinkNodeContainer(Link->Next);
    }
}

void ApplyTreeNodeContainer(tree_node *Node)
{
    if(Node)
    {
        if(Node->WindowID != 0)
            ResizeWindowToContainerSize(Node);

        if(Node->List)
            ApplyLinkNodeContainer(Node->List);

        if(Node->LeftChild)
            ApplyTreeNodeContainer(Node->LeftChild);

        if(Node->RightChild)
            ApplyTreeNodeContainer(Node->RightChild);
    }
}

internal void
DestroyLinkList(link_node *Link)
{
    if(Link)
    {
        if(Link->Next)
            DestroyLinkList(Link->Next);

        free(Link);
        Link = NULL;
    }
}

void DestroyNodeTree(tree_node *Node)
{
    if(Node)
    {
        if(Node->List)
            DestroyLinkList(Node->List);

        if(Node->LeftChild)
            DestroyNodeTree(Node->LeftChild);

        if(Node->RightChild)
            DestroyNodeTree(Node->RightChild);

        free(Node);
        Node = NULL;
    }
}

internal void
RotateTree(tree_node *Node, int Deg)
{
    if (Node == NULL || IsLeafNode(Node))
        return;

    DEBUG("RotateTree() " << Deg << " degrees");

    if((Deg == 90 && Node->SplitMode == SPLIT_VERTICAL) ||
       (Deg == 270 && Node->SplitMode == SPLIT_HORIZONTAL) ||
       Deg == 180)
    {
        tree_node *Temp = Node->LeftChild;
        Node->LeftChild = Node->RightChild;
        Node->RightChild = Temp;
        Node->SplitRatio = 1 - Node->SplitRatio;
    }

    if(Deg != 180)
        Node->SplitMode = Node->SplitMode == SPLIT_HORIZONTAL ? SPLIT_VERTICAL : SPLIT_HORIZONTAL;

    RotateTree(Node->LeftChild, Deg);
    RotateTree(Node->RightChild, Deg);
}

void RotateBSPTree(int Deg)
{
    ax_display *Display = AXLibMainDisplay();
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    if(SpaceInfo->Settings.Mode == SpaceModeBSP)
    {
        RotateTree(SpaceInfo->RootNode, Deg);
        CreateNodeContainers(Display, SpaceInfo->RootNode, false);
        ApplyTreeNodeContainer(SpaceInfo->RootNode);
    }
}

void FillDeserializedTree(tree_node *RootNode, ax_display *Display, std::vector<uint32_t> *WindowsPtr)
{
    std::vector<uint32_t> &Windows = *WindowsPtr;
    tree_node *Current = NULL;
    GetFirstLeafNode(RootNode, (void**)&Current);

    std::size_t Counter = 0, Leafs = 0;
    while(Current)
    {
        if(Counter < Windows.size())
            Current->WindowID = Windows[Counter++];

        Current = GetNearestTreeNodeToTheRight(Current);
        ++Leafs;
    }

    if(Leafs < Windows.size() && Counter < Windows.size())
    {
        tree_node *Root = RootNode;
        for(; Counter < Windows.size(); ++Counter)
        {
            while(!IsLeafNode(Root))
            {
                if(!IsLeafNode(Root->LeftChild) && IsLeafNode(Root->RightChild))
                    Root = Root->RightChild;
                else
                    Root = Root->LeftChild;
            }

            DEBUG("FillDeserializedTree() Create pair of leafs");
            CreateLeafNodePair(Display, Root, Root->WindowID, Windows[Counter], GetOptimalSplitMode(Root));
            Root = RootNode;
        }
    }
}
