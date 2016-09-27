#include "border.h"
#include "window.h"

#define internal static
#define local_persist static

extern kwm_path KWMPath;

internal void
OpenBorder(kwm_border *Border)
{
    local_persist std::string OverlayBin = KWMPath.FilePath + "/kwm-overlay";
    DEBUG("Kwm: popen " << OverlayBin);

    Border->Handle = popen(OverlayBin.c_str(), "w");
    if(!Border->Handle)
        Border->Enabled = false;
}

void CloseBorder(kwm_border *Border)
{
    if(Border->Handle)
    {
        local_persist std::string Terminate = "quit\n";
        fwrite(Terminate.c_str(), Terminate.size(), 1, Border->Handle);
        fflush(Border->Handle);
        pclose(Border->Handle);
        Border->Handle = NULL;
    }
}

void ClearBorder(kwm_border *Border)
{
    if(Border->Handle)
    {
        local_persist std::string Command = "clear\n";
        fwrite(Command.c_str(), Command.size(), 1, Border->Handle);
        fflush(Border->Handle);
    }
}

internal inline void
RefreshBorder(kwm_border *Border, ax_window *Window)
{
    std::string Command = "x:" + std::to_string(Window->Position.x) + \
                          " y:" + std::to_string(Window->Position.y) + \
                          " w:" + std::to_string(Window->Size.width) + \
                          " h:" + std::to_string(Window->Size.height) + \
                          " " + Border->Color.Format + \
                          " s:" + std::to_string(Border->Width);

    Command += (Border->Radius != -1 ? " rad:" + std::to_string(Border->Radius) : "") + "\n";
    fwrite(Command.c_str(), Command.size(), 1, Border->Handle);
    fflush(Border->Handle);
}

internal inline void
RefreshBorder(kwm_border *Border, tree_node *Node)
{
    std::string Command = "x:" + std::to_string(Node->Container.X) + \
                          " y:" + std::to_string(Node->Container.Y) + \
                          " w:" + std::to_string(Node->Container.Width) + \
                          " h:" + std::to_string(Node->Container.Height) + \
                          " " + Border->Color.Format + \
                          " s:" + std::to_string(Border->Width);

    Command += (Border->Radius != -1 ? " rad:" + std::to_string(Border->Radius) : "") + "\n";
    fwrite(Command.c_str(), Command.size(), 1, Border->Handle);
    fflush(Border->Handle);
}

void UpdateBorder(kwm_border *Border, ax_window *Window)
{
    if(Border && Border->Enabled)
    {
        if(Window)
        {
            if(!Border->Handle)
                OpenBorder(Border);

            if(Border->Handle)
                RefreshBorder(Border, Window);
        }
        else
        {
            ClearBorder(Border);
        }
    }
}

void UpdateBorder(kwm_border *Border, tree_node *Node)
{
    if(Border && Border->Enabled)
    {
        if(Node)
        {
            if(!Border->Handle)
                OpenBorder(Border);

            if(Border->Handle)
                RefreshBorder(Border, Node);
        }
        else
        {
            ClearBorder(Border);
        }
    }
}
