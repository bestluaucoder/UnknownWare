#pragma once

using BYTE = unsigned char;

namespace FontsData
{
    extern BYTE KeyBindsFont[];
    extern BYTE LegitTabIcons[];
    extern BYTE TabIcons[];

    inline constexpr unsigned int TabIconsSize = 5192;
    inline constexpr unsigned int LegitTabIconsSize = 47320;
}

namespace TexturesData
{
    extern BYTE BgTexture[];

    inline constexpr unsigned int BgTextureSize = 424852;
}
