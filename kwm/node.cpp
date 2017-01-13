#include "node.h"
#include "container.h"
#include "tree.h"
#include "space.h"
#include "window.h"
#include "../axlib/axlib.h"

#define internal static

extern std::map<std::string, space_info> WindowTree;
extern ax_application *FocusedApplication;
extern kwm_settings KWMSettings;

tree_node *CreateRootNode()
{
    tree_node *RootNode = (tree_node*) malloc(sizeof(tree_node));
    memset(RootNode, 0, sizeof(tree_node));

    RootNode->WindowID = 0;
    RootNode->Type = NodeTypeTree;
    RootNode->Parent = NULL;
    RootNode->LeftChild = NULL;
    RootNode->RightChild = NULL;
    RootNode->SplitRatio = KWMSettings.SplitRatio;
    RootNode->SplitMode = SPLIT_OPTIMAL;

    return RootNode;
}

link_node *CreateLinkNode()
{
    link_node *Link = (link_node*) malloc(sizeof(link_node));
    memset(Link, 0, sizeof(link_node));

    Link->WindowID = 0;
    Link->Prev = NULL;
    Link->Next = NULL;

    return Link;
}

tree_node *CreateLeafNode(ax_display *Display, tree_node *Parent, uint32_t WindowID, container_type Type)
{
    tree_node *Leaf = (tree_node*) malloc(sizeof(tree_node));
    memset(Leaf, 0, sizeof(tree_node));

    Leaf->Parent = Parent;
    Leaf->WindowID = WindowID;
    Leaf->Type = NodeTypeTree;

    CreateNodeContainer(Display, Leaf, Type);

    Leaf->LeftChild = NULL;
    Leaf->RightChild = NULL;

    return Leaf;
}

void CreateLeafNodePair(ax_display *Display, tree_node *Parent, uint32_t FirstWindowID, uint32_t SecondWindowID, split_type SplitMode)
{
    Parent->WindowID = 0;
    Parent->SplitMode = SplitMode;
    Parent->SplitRatio = KWMSettings.SplitRatio;

    node_type ParentType = Parent->Type;
    link_node *ParentList = Parent->List;
    Parent->Type = NodeTypeTree;
    Parent->List = NULL;

    uint32_t LeftWindowID;
    uint32_t RightWindowID;

    if(HasFlags(&KWMSettings, Settings_SpawnAsLeftChild))
    {
        LeftWindowID = SecondWindowID;
        RightWindowID = FirstWindowID;
    }
    else
    {
        LeftWindowID = FirstWindowID;
        RightWindowID = SecondWindowID;
    }

    if(SplitMode == SPLIT_VERTICAL)
    {
        Parent->LeftChild = CreateLeafNode(Display, Parent, LeftWindowID, CONTAINER_LEFT);
        Parent->RightChild = CreateLeafNode(Display, Parent, RightWindowID, CONTAINER_RIGHT);

        tree_node *Node;
        if(HasFlags(&KWMSettings, Settings_SpawnAsLeftChild))
            Node = Parent->RightChild;
        else
            Node = Parent->LeftChild;

        Node->Type = ParentType;
        Node->List = ParentList;
        ResizeLinkNodeContainers(Node);
    }
    else if(SplitMode == SPLIT_HORIZONTAL)
    {
        Parent->LeftChild = CreateLeafNode(Display, Parent, LeftWindowID, CONTAINER_UPPER);
        Parent->RightChild = CreateLeafNode(Display, Parent, RightWindowID, CONTAINER_LOWER);

        tree_node *Node;
        if(HasFlags(&KWMSettings, Settings_SpawnAsLeftChild))
            Node = Parent->RightChild;
        else
            Node = Parent->LeftChild;

        Node->Type = ParentType;
        Node->List = ParentList;
        ResizeLinkNodeContainers(Node);
    }
    else
    {
        Parent->Parent = NULL;
        Parent->LeftChild = NULL;
        Parent->RightChild = NULL;
        Parent = NULL;
    }
}

void CreatePseudoNode()
{
    ax_display *Display = AXLibMainDisplay();
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    if(!FocusedApplication)
        return;

    ax_window *Window = FocusedApplication->Focus;
    if(!Window)
        return;

    tree_node *Node = GetTreeNodeFromWindowID(SpaceInfo->RootNode, Window->ID);
    if(Node)
    {
        split_type SplitMode = KWMSettings.SplitMode == SPLIT_OPTIMAL ? GetOptimalSplitMode(Node) : KWMSettings.SplitMode;
        CreateLeafNodePair(Display, Node, Node->WindowID, 0, SplitMode);
        ApplyTreeNodeContainer(Node);
    }
}

void RemovePseudoNode()
{
    ax_display *Display = AXLibMainDisplay();
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    if(!FocusedApplication)
        return;

    ax_window *Window = FocusedApplication->Focus;
    if(!Window)
        return;

    tree_node *Node = GetTreeNodeFromWindowID(SpaceInfo->RootNode, Window->ID);
    if(Node && Node->Parent)
    {
        tree_node *Parent = Node->Parent;
        tree_node *PseudoNode = IsLeftChild(Node) ? Parent->RightChild : Parent->LeftChild;
        if(!PseudoNode || !IsLeafNode(PseudoNode) || PseudoNode->WindowID != 0)
            return;

        Parent->WindowID = Node->WindowID;
        Parent->LeftChild = NULL;
        Parent->RightChild = NULL;
        free(Node);
        free(PseudoNode);
        ApplyTreeNodeContainer(Parent);
    }
}

bool IsLeafNode(tree_node *Node)
{
    return Node->LeftChild == NULL && Node->RightChild == NULL ? true : false;
}

bool IsPseudoNode(tree_node *Node)
{
    return Node && Node->WindowID == 0 && IsLeafNode(Node);
}

bool IsLeftChild(tree_node *Node)
{
    if(Node && IsLeafNode(Node))
    {
        tree_node *Parent = Node->Parent;
        return Parent->LeftChild == Node;
    }

    return false;
}

bool IsRightChild(tree_node *Node)
{
    if(Node && IsLeafNode(Node))
    {
        tree_node *Parent = Node->Parent;
        return Parent->RightChild == Node;
    }

    return false;
}

void ToggleFocusedNodeSplitMode()
{
    ax_display *Display = AXLibMainDisplay();
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    if(!FocusedApplication)
        return;

    ax_window *Window = FocusedApplication->Focus;
    if(!Window)
        return;

    tree_node *Node = GetTreeNodeFromWindowID(SpaceInfo->RootNode, Window->ID);
    if(!Node)
        return;

    tree_node *Parent = Node->Parent;
    if(!Parent || IsLeafNode(Parent))
        return;

    Parent->SplitMode = Parent->SplitMode == SPLIT_VERTICAL ? SPLIT_HORIZONTAL : SPLIT_VERTICAL;
    CreateNodeContainers(Display, Parent, false);
    ApplyTreeNodeContainer(Parent);
}

void ToggleTypeOfFocusedNode()
{
    ax_window *Window = FocusedApplication ? FocusedApplication->Focus : NULL;
    if(!Window)
        return;

    ax_display *Display = AXLibWindowDisplay(Window);
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];

    tree_node *TreeNode = GetTreeNodeFromWindowIDOrLinkNode(SpaceInfo->RootNode, Window->ID);
    if(TreeNode && TreeNode != SpaceInfo->RootNode)
        TreeNode->Type = TreeNode->Type == NodeTypeTree ? NodeTypeLink : NodeTypeTree;
}

void ChangeTypeOfFocusedNode(node_type Type)
{
    ax_window *Window = FocusedApplication ? FocusedApplication->Focus : NULL;
    if(!Window)
        return;

    ax_display *Display = AXLibWindowDisplay(Window);
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];

    tree_node *TreeNode = GetTreeNodeFromWindowIDOrLinkNode(SpaceInfo->RootNode, Window->ID);
    if(TreeNode && TreeNode != SpaceInfo->RootNode)
        TreeNode->Type = Type;
}

void SwapNodeWindowIDs(tree_node *A, tree_node *B)
{
    if(A && B)
    {
        DEBUG("SwapNodeWindowIDs() " << A->WindowID << " with " << B->WindowID);
        int TempWindowID = A->WindowID;
        A->WindowID = B->WindowID;
        B->WindowID = TempWindowID;

        node_type TempNodeType = A->Type;
        A->Type = B->Type;
        B->Type = TempNodeType;

        link_node *TempLinkList = A->List;
        A->List = B->List;
        B->List = TempLinkList;

        ResizeLinkNodeContainers(A);
        ResizeLinkNodeContainers(B);
        ApplyTreeNodeContainer(A);
        ApplyTreeNodeContainer(B);
    }
}

void SwapNodeWindowIDs(link_node *A, link_node *B)
{
    if(A && B)
    {
        DEBUG("SwapNodeWindowIDs() " << A->WindowID << " with " << B->WindowID);
        int TempWindowID = A->WindowID;
        A->WindowID = B->WindowID;
        B->WindowID = TempWindowID;
        ResizeWindowToContainerSize(A);
        ResizeWindowToContainerSize(B);
    }
}

split_type GetOptimalSplitMode(tree_node *Node)
{
    return (Node->Container.Width / Node->Container.Height) >= KWMSettings.OptimalRatio ? SPLIT_VERTICAL : SPLIT_HORIZONTAL;
}

void ResizeWindowToContainerSize(tree_node *Node)
{
    ax_window *Window = GetWindowByID((unsigned int)Node->WindowID);
    if(Window)
    {
        SetWindowDimensions(Window, Node->Container.X, Node->Container.Y,
                            Node->Container.Width, Node->Container.Height);
    }
}

void ResizeWindowToContainerSize(link_node *Link)
{
    ax_window *Window = GetWindowByID((unsigned int)Link->WindowID);
    if(Window)
    {
        SetWindowDimensions(Window, Link->Container.X, Link->Container.Y,
                            Link->Container.Width, Link->Container.Height);
    }
}

void ResizeWindowToContainerSize(ax_window *Window)
{
    if(Window)
    {
        ax_window *Window = FocusedApplication ? FocusedApplication->Focus : NULL;
        if(!Window)
            return;

        ax_display *Display = AXLibWindowDisplay(Window);
        space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];

        tree_node *Node = GetTreeNodeFromWindowID(SpaceInfo->RootNode, Window->ID);
        if(Node)
            ResizeWindowToContainerSize(Node);

        if(!Node)
        {
            link_node *Link = GetLinkNodeFromWindowID(SpaceInfo->RootNode, Window->ID);
            if(Link)
                ResizeWindowToContainerSize(Link);
        }
    }
}

void ResizeWindowToContainerSize()
{
    ax_window *Window = FocusedApplication ? FocusedApplication->Focus : NULL;
    if(!Window)
        return;

    ResizeWindowToContainerSize(Window);
}

void ModifyContainerSplitRatio(double Offset)
{
    if(!FocusedApplication)
        return;

    ax_window *Window = FocusedApplication->Focus;
    if(!Window)
        return;

    ax_display *Display = AXLibWindowDisplay(Window);
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];

    tree_node *Root = SpaceInfo->RootNode;
    if(!Root || IsLeafNode(Root) || Root->WindowID != 0)
        return;

    tree_node *Node = GetTreeNodeFromWindowIDOrLinkNode(Root, Window->ID);
    if(Node && Node->Parent)
    {
        if(Node->Parent->SplitRatio + Offset > 0.0 &&
           Node->Parent->SplitRatio + Offset < 1.0)
        {
            Node->Parent->SplitRatio += Offset;
            ResizeNodeContainer(Display, Node->Parent);
            ApplyTreeNodeContainer(Node->Parent);
        }
    }
}

internal inline bool
IsLeftChildInSubTree(tree_node *Root, tree_node *Target)
{
    bool Result = ((Root->LeftChild == Target) ||
                   (Root->RightChild == Target)) ||
                   (Root->LeftChild && IsLeftChildInSubTree(Root->LeftChild, Target)) ||
                   (Root->RightChild && IsLeftChildInSubTree(Root->RightChild, Target));

    return Result;
}

tree_node *FindLowestCommonAncestor(tree_node *A, tree_node *B)
{
    if(!A || !B)
        return NULL;

    std::map<tree_node *, bool> Ancestors;
    while(A != NULL)
    {
        Ancestors[A] = true;
        A = A->Parent;
    }

    while(B != NULL)
    {
        if(Ancestors.find(B) != Ancestors.end())
        {
           return B;
        }

        B = B->Parent;
    }

    return NULL;
}

void ModifyContainerSplitRatio(double Offset, int Degrees)
{
    if(!FocusedApplication)
        return;

    ax_window *Window = FocusedApplication->Focus;
    if(!Window)
        return;

    ax_display *Display = AXLibWindowDisplay(Window);
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];

    tree_node *Root = SpaceInfo->RootNode;
    if(!Root || IsLeafNode(Root) || Root->WindowID != 0)
        return;

    tree_node *Node = GetTreeNodeFromWindowIDOrLinkNode(Root, Window->ID);
    if(Node)
    {
        ax_window *ClosestWindow = NULL;
        if(FindClosestWindow(Degrees, &ClosestWindow, false))
        {
            tree_node *Target = GetTreeNodeFromWindowIDOrLinkNode(Root, ClosestWindow->ID);
            tree_node *Ancestor = FindLowestCommonAncestor(Node, Target);

            if(Ancestor)
            {
                if(!(Node == Ancestor->LeftChild || IsLeftChildInSubTree(Ancestor->LeftChild, Node)))
                    Offset = -Offset;

                double NewSplitRatio = Ancestor->SplitRatio + Offset;
                SetContainerSplitRatio(NewSplitRatio, Node, Ancestor, Display, true);
            }
        }
    }
}

void SetContainerSplitRatio(double SplitRatio, tree_node *Node, tree_node *Ancestor, ax_display *Display, bool ResizeWindows)
{
    if(SplitRatio > 0.0 &&
       SplitRatio < 1.0)
    {
        Ancestor->SplitRatio = SplitRatio;
        ResizeNodeContainer(Display, Ancestor);
        if(ResizeWindows)
            ApplyTreeNodeContainer(Ancestor);
    }
}
