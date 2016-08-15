#include "border.h"
#include "window.h"
#include "keys.h"
#include "axlib/axlib.h"

extern ax_window *MarkedWindow;
extern kwm_border FocusedBorder;
extern kwm_border MarkedBorder;
extern kwm_hotkeys KWMHotkeys;

void UpdateBorder(border_type Type)
{
    kwm_border *Border = NULL;
    ax_window *Window = NULL;

    if(Type == BORDER_FOCUSED)
    {
        Border = &FocusedBorder;
        ax_application *Application = AXLibGetFocusedApplication();
        if(Application)
            Window = Application->Focus;

        if(!KWMHotkeys.ActiveMode->Color.Format.empty())
            Border->Color = KWMHotkeys.ActiveMode->Color;
    }
    else if(Type == BORDER_MARKED)
    {
        Window = MarkedWindow;
        Border = &MarkedBorder;
    }

    OpenBorder(Border);
    if(!Border->Enabled)
        CloseBorder(Border);

    if(!Window)
        ClearBorder(Border);
    else if(Border->Enabled)
        RefreshBorder(Border, Window);
}
