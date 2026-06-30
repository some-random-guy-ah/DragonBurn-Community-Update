#pragma once

#include "imgui.h"

namespace colors
{
    inline ImVec4 rgba(int r, int g, int b, int a = 255)
    {
        return ImVec4(r / 255.f, g / 255.f, b / 255.f, a / 255.f);
    }

    inline ImVec4 ShellTint      = rgba(16, 14, 20, 175);
    inline ImVec4 Bg             = rgba(16, 14, 20, 210);
    inline ImVec4 SidebarBg      = rgba(18, 16, 22, 220);
    inline ImVec4 PanelBg        = rgba(22, 20, 26, 235);
    inline ImVec4 PanelHeader    = rgba(26, 24, 30, 245);
    inline ImVec4 WidgetBg       = rgba(32, 30, 38);
    inline ImVec4 WidgetHover    = rgba(42, 38, 48);
    inline ImVec4 Outline        = rgba(48, 44, 58);
    inline ImVec4 OutlineLight   = rgba(58, 52, 68);
    inline ImVec4 Accent         = rgba(255, 130, 160);
    inline ImVec4 AccentHover    = rgba(255, 150, 175);
    inline ImVec4 AccentDark     = rgba(210, 95, 125);
    inline ImVec4 AccentGlow     = rgba(255, 130, 160, 90);
    inline ImVec4 Text           = rgba(210, 205, 220);
    inline ImVec4 TextDisabled   = rgba(130, 125, 145);
    inline ImVec4 TextMuted      = rgba(95, 90, 108);
    inline ImVec4 TabBox         = rgba(28, 26, 34);
    inline ImVec4 TabStripBg     = rgba(24, 24, 28);
    inline ImVec4 TabPill        = rgba(33, 33, 39);
    inline ImVec4 TabTextActive  = rgba(255, 255, 255);
    inline ImVec4 TabTextIdle    = rgba(79, 79, 96);
    inline ImVec4 ActiveTab      = rgba(36, 32, 44);
    inline ImVec4 SubTabActive   = rgba(40, 34, 48);
    inline ImVec4 PopupBg       = rgba(22, 20, 28, 255);
    inline ImVec4 DropdownBg    = rgba(22, 20, 26, 255);
    inline ImVec4 PanelHeaderText = rgba(175, 170, 190);
    inline ImVec4 CheckBg        = PanelBg;
    inline ImVec4 CheckboxBg     = rgba(14, 12, 18);
    inline ImVec4 WhiteGlow      = rgba(255, 255, 255, 40);

    inline ImVec4 KeybindBg      = PanelHeader;
    inline ImVec4 KeybindIconBg  = rgba(22, 20, 26);
    inline ImVec4 KeybindIconHover = rgba(32, 30, 38);
    inline ImVec4 KeybindListen  = rgba(255, 0, 127);
    inline ImVec4 KeybindCog     = rgba(136, 136, 136);
    inline ImVec4 KeybindText    = rgba(255, 255, 255);
}
