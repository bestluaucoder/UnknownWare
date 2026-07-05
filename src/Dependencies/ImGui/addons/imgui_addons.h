#pragma once

#include "../imgui.h"

#define IMADD_ANIMATIONS_SPEED	0.07f

#include <Windows.h>
#include <thread>
#include <cmath>
#include <string>
#include <algorithm>
#include <vector>

enum ImKeyBindMode : int
{
	ImKeyBindMode_None = 0,

	ImKeyBindMode_Toggle = ImKeyBindMode_None,
	ImKeyBindMode_Hold
};

inline int ImGuiKeyToVK(ImGuiKey key)
{
    if (key >= ImGuiKey_A && key <= ImGuiKey_Z)
        return 'A' + (key - ImGuiKey_A);

    if (key >= ImGuiKey_0 && key <= ImGuiKey_9)
        return '0' + (key - ImGuiKey_0);

    switch (key)
    {
    case ImGuiKey_Tab: return VK_TAB;
    case ImGuiKey_LeftArrow: return VK_LEFT;
    case ImGuiKey_RightArrow: return VK_RIGHT;
    case ImGuiKey_UpArrow: return VK_UP;
    case ImGuiKey_DownArrow: return VK_DOWN;
    case ImGuiKey_PageUp: return VK_PRIOR;
    case ImGuiKey_PageDown: return VK_NEXT;
    case ImGuiKey_Home: return VK_HOME;
    case ImGuiKey_End: return VK_END;
    case ImGuiKey_Insert: return VK_INSERT;
    case ImGuiKey_Delete: return VK_DELETE;
    case ImGuiKey_Backspace: return VK_BACK;
    case ImGuiKey_Space: return VK_SPACE;
    case ImGuiKey_Enter: return VK_RETURN;
    case ImGuiKey_Escape: return VK_ESCAPE;

    case ImGuiKey_LeftCtrl: return VK_LCONTROL;
    case ImGuiKey_LeftShift: return VK_LSHIFT;
    case ImGuiKey_LeftAlt: return VK_LMENU;
    case ImGuiKey_RightCtrl: return VK_RCONTROL;
    case ImGuiKey_RightShift: return VK_RSHIFT;
    case ImGuiKey_RightAlt: return VK_RMENU;

    case ImGuiKey_F1: return VK_F1;
    case ImGuiKey_F2: return VK_F2;
    case ImGuiKey_F3: return VK_F3;
    case ImGuiKey_F4: return VK_F4;
    case ImGuiKey_F5: return VK_F5;
    case ImGuiKey_F6: return VK_F6;
    case ImGuiKey_F7: return VK_F7;
    case ImGuiKey_F8: return VK_F8;
    case ImGuiKey_F9: return VK_F9;
    case ImGuiKey_F10: return VK_F10;
    case ImGuiKey_F11: return VK_F11;
    case ImGuiKey_F12: return VK_F12;

    case ImGuiKey_MouseLeft: return VK_LBUTTON;
    case ImGuiKey_MouseRight: return VK_RBUTTON;
    case ImGuiKey_MouseMiddle: return VK_MBUTTON;
    case ImGuiKey_MouseX1: return VK_XBUTTON1;
    case ImGuiKey_MouseX2: return VK_XBUTTON2;
    }

    return 0;
}

namespace Menu
{
    void DrawLabelShadow(ImDrawList* drawList, ImVec2 pos, ImU32 col, const char* text);

    int Tabs(ImDrawList* draw, ImVec2 pos, float textY, int& section, const char* labels[], int count, float gap = 5.0f, float rightMargin = 5.0f);

    bool BeginChild(const char* label, const ImVec2& sizeArg);
    void EndChild();

    bool CheckBox(const char* label, bool* checked);

    bool SliderAlpha(const char* strId, ImVec4& col);
    bool SliderHue(const char* strId, ImVec4& col);

    bool ColorPalette(const char* strId, ImVec4& col, const ImVec2& sizeArg = ImVec2(200, 100));
    bool ColorButton(const char* descId, const ImVec4& col, bool hasAlpha = true, const ImVec2& sizeArg = ImVec2(20, 20));

    void ColorPicker4(const char* label, float col[4], bool hasAlpha = true);
    bool ColorEdit4(const char* name, float col[4]);

    bool SliderScalar(const char* Label, ImGuiDataType DataType, void* PData, const void* PMin, const void* PMax, const char* Format = nullptr);
    bool SliderFloat(const char* Label, float* V, float VMin, float VMax, const char* Format = nullptr);
    bool SliderInt(const char* Label, int* V, int VMin, int VMax, const char* Format = nullptr);

    bool SelectableLabel(const char* Label, bool Selected, const ImVec2& SizeArg = ImVec2(0, 0));
    bool Combo(const char* Label, int* SelectedIndex, std::vector<const char*> Items);

    bool KeyBind(const char* StrId, ImGuiKey* K);
    bool KeyBindEx(const char* StrId, ImGuiKey* K, ImKeyBindMode* Mode, const ImVec2& SizeArg = ImVec2(0, 0));

    float GetColorPickerWidth();
}
