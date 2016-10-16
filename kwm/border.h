#ifndef BORDER_H
#define BORDER_H

#include "types.h"
#include "../axlib/axlib.h"

void OverlayLibInitialize();

void CloseBorder(kwm_border *Border);
void ClearBorder(kwm_border *Border);
void UpdateBorder(kwm_border *Border, ax_window *Window);
void UpdateBorder(kwm_border *Border, tree_node *Node);

#endif
