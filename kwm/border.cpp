#include "border.h"
#include "window.h"
#include "keys.h"
#include "axlib/axlib.h"

extern ax_window *MarkedWindow;
extern kwm_border FocusedBorder;
extern kwm_border MarkedBorder;
extern kwm_hotkeys KWMHotkeys;

void UpdateBorder(kwm_border *Border, ax_window *Window)
{
    if(Border)
    {
        if(Window)
        {
            if((Border->Type == BORDER_FOCUSED) &&
               (!KWMHotkeys.ActiveMode->Color.Format.empty()))
                Border->Color = KWMHotkeys.ActiveMode->Color;

            OpenBorder(Border);
            if(!Border->Enabled)
                CloseBorder(Border);

            RefreshBorder(Border, Window);
        }
        else
        {
            ClearBorder(Border);
        }
    }
}
