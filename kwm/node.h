#ifndef NODE_H
#define NODE_H

#include "types.h"
#include "../axlib/display.h"
#include "../axlib/window.h"

tree_node *CreateRootNode();
link_node *CreateLinkNode();
tree_node *CreateLeafNode(ax_display *Display, tree_node *Parent, uint32_t WindowID, container_type Type);
void CreateLeafNodePair(ax_display *Display, tree_node *Parent, uint32_t FirstWindowID, uint32_t SecondWindowID, split_type SplitMode);
void CreatePseudoNode();
void RemovePseudoNode();
bool IsLeafNode(tree_node *Node);
bool IsPseudoNode(tree_node *Node);
bool IsLeftChild(tree_node *Node);
bool IsRightChild(tree_node *Node);
void ToggleFocusedNodeSplitMode();
void SwapNodeWindowIDs(tree_node *A, tree_node *B);
void SwapNodeWindowIDs(link_node *A, link_node *B);
split_type GetOptimalSplitMode(tree_node *Node);
void ResizeWindowToContainerSize(tree_node *Node);
void ResizeWindowToContainerSize(link_node *Node);
void ResizeWindowToContainerSize(ax_window *Window);
void ResizeWindowToContainerSize();
tree_node *FindLowestCommonAncestor(tree_node *A, tree_node *B);
void ModifyContainerSplitRatio(double Offset);
void ModifyContainerSplitRatio(double Offset, int Degrees);
void SetContainerSplitRatio(double SplitRatio, tree_node *Node, tree_node *Ancestor, ax_display *Display);

void ToggleTypeOfFocusedNode();
void ChangeTypeOfFocusedNode(node_type Type);

#endif
