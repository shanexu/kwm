#ifndef CONTAINER_H
#define CONTAINER_H

#include "types.h"
#include "../axlib/display.h"

void SetRootNodeContainer(ax_display *Display, tree_node *Node);
void SetLinkNodeContainer(ax_display *Display, link_node *Link);
void CreateNodeContainer(ax_display *Display, tree_node *Node, container_type Type);
void CreateNodeContainerPair(ax_display *Display, tree_node *LeftNode, tree_node *RightNode, split_type SplitMode);
void ResizeNodeContainer(ax_display *Display, tree_node *Node);
void ResizeLinkNodeContainers(tree_node *Root);
void CreateNodeContainers(ax_display *Display, tree_node *Node, bool OptimalSplit);
void CreateDeserializedNodeContainer(ax_display *Display, tree_node *Node);

#endif
