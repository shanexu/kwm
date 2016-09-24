#ifndef CURSOR_H
#define CURSOR_H

#include "types.h"
#include "../axlib/window.h"
#include <Carbon/Carbon.h>

void FocusWindowBelowCursor();
void MoveCursorToCenterOfWindow(ax_window *Window);
void MoveCursorToCenterOfTreeNode(tree_node *Node);
void MoveCursorToCenterOfLinkNode(link_node *Link);
void MoveCursorToCenterOfFocusedWindow();

#endif
