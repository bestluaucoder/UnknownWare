#include "../imgui_internal.h"
#include "imgui_addons.h"
#include "colors/colors.h"

#include <map>
#include <string>

using namespace ImGui;

void Menu::DrawLabelShadow(ImDrawList* drawList, ImVec2 pos, ImU32 col, const char* text)
{
    drawList->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32_BLACK, text);
    drawList->AddText(ImVec2(pos.x - 1.0f, pos.y - 1.0f), IM_COL32_BLACK, text);
    drawList->AddText(ImVec2(pos.x - 1.0f, pos.y + 1.0f), IM_COL32_BLACK, text);
    drawList->AddText(ImVec2(pos.x + 1.0f, pos.y - 1.0f), IM_COL32_BLACK, text);
    drawList->AddText(ImVec2(pos.x, pos.y + 1.0f), IM_COL32_BLACK, text);
    drawList->AddText(ImVec2(pos.x + 1.0f, pos.y), IM_COL32_BLACK, text);
    drawList->AddText(ImVec2(pos.x, pos.y - 1.0f), IM_COL32_BLACK, text);
    drawList->AddText(ImVec2(pos.x - 1.0f, pos.y), IM_COL32_BLACK, text);
    drawList->AddText(pos, col, text);
}

int Menu::Tabs(ImDrawList* draw, ImVec2 pos, float textY, int& section, const char* labels[], int count, float gap, float rightMargin)
{
    static std::map<int, ImColor> hoverColors;
    static std::map<int, ImColor> textColors;

    float totalTabsWidth = 0.0f;
    for (int i = 0; i < count; i++)
    {
        totalTabsWidth += ImGui::CalcTextSize(labels[i]).x;
        if (i < count - 1)
            totalTabsWidth += gap;
    }

    ImVec2 cursor = ImVec2(pos.x + ImGui::GetWindowSize().x - totalTabsWidth - rightMargin, textY);
    ImGuiContext& g = *GImGui;

    for (int i = 0; i < count; i++)
    {
        ImVec2 textSize = ImGui::CalcTextSize(labels[i]);
        bool selected = (section == i);
        ImRect textRect(cursor, ImVec2(cursor.x + textSize.x, cursor.y + textSize.y));
        bool hovered = ImGui::IsMouseHoveringRect(textRect.Min, textRect.Max);

        ImColor targetHover = hovered ? Menu::Text : Menu::Bg;
        ImColor targetText = selected ? Menu::Accent : (hovered ? Menu::Text : Menu::TextDim);

        ImColor& hoverCol = hoverColors[i];
        if (hoverCol.Value.w == 0.0f)
            hoverCol = targetHover;

        hoverCol.Value.x = ImLerp(hoverCol.Value.x, targetHover.Value.x, g.IO.DeltaTime * 10.0f);
        hoverCol.Value.y = ImLerp(hoverCol.Value.y, targetHover.Value.y, g.IO.DeltaTime * 10.0f);
        hoverCol.Value.z = ImLerp(hoverCol.Value.z, targetHover.Value.z, g.IO.DeltaTime * 10.0f);
        hoverCol.Value.w = ImLerp(hoverCol.Value.w, targetHover.Value.w, g.IO.DeltaTime * 10.0f);

        ImColor& txtCol = textColors[i];
        if (txtCol.Value.w == 0.0f)
            txtCol = targetText;

        txtCol.Value.x = ImLerp(txtCol.Value.x, targetText.Value.x, g.IO.DeltaTime * 10.0f);
        txtCol.Value.y = ImLerp(txtCol.Value.y, targetText.Value.y, g.IO.DeltaTime * 10.0f);
        txtCol.Value.z = ImLerp(txtCol.Value.z, targetText.Value.z, g.IO.DeltaTime * 10.0f);
        txtCol.Value.w = ImLerp(txtCol.Value.w, targetText.Value.w, g.IO.DeltaTime * 10.0f);

        Menu::DrawLabelShadow(draw, cursor, txtCol, labels[i]);

        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            section = i;

        cursor.x += textSize.x + gap;
    }

    return section;
}

bool Menu::BeginChild(const char* Label, const ImVec2& SizeArg)
{
    ImGuiWindow* Window = ImGui::GetCurrentWindow();
    if (Window->SkipItems) return false;

    ImGuiContext& G = *GImGui;
    const ImGuiStyle& Style = G.Style;

    std::string StrLabel = std::string(Label ? Label : "");
    const ImVec2 LabelSize = CalcTextSize(StrLabel.c_str(), nullptr, true);
    bool HasHeader = LabelSize.x > 0;

    bool Success = ImGui::BeginChild(StrLabel.c_str(), SizeArg, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
    if (Success)
    {
        ImVec2 Pos = ImGui::GetWindowPos();
        ImVec2 Size = ImGui::GetWindowSize();
        ImDrawList* DrawList = ImGui::GetWindowDrawList();

        DrawList->AddRectFilled(Pos + ImVec2(1, 1), Pos + Size - ImVec2(1, 1), Menu::Bg, 0.0f);

        float HeaderPadding = Style.CellPadding.x * 2.0f;
        float HeaderHeight = G.FontSize + HeaderPadding * 2.0f;
        float Border = Style.ChildBorderSize > 0 ? 3.0f : 0.0f;

        if (HasHeader) Menu::DrawLabelShadow(DrawList, ImVec2(Pos.x + HeaderPadding + Style.FrameBorderSize * 2.0f, Pos.y + HeaderPadding + Style.FrameBorderSize), GetColorU32(ImGuiCol_Text), StrLabel.c_str());

        if (Style.ChildBorderSize > 0)
        {
            DrawList->AddRect(Pos + ImVec2(1.0f, 1.0f), Pos + Size - ImVec2(1.0f, 1.0f), Menu::DarkAccent, 0, 0, 1.0f);
            DrawList->AddRect(Pos + ImVec2(2.0f, 2.0f), Pos + Size - ImVec2(2.0f, 2.0f), Menu::Outline, 0, 0, 1.0f);
            DrawList->AddLine(Pos + ImVec2(3.0f, 3.0f), ImVec2(Pos.x + Size.x - 3.0f, Pos.y + 3.0f), Menu::Accent, 1.0f);
            DrawList->AddLine(Pos + ImVec2(3.0f, 4.0f), ImVec2(Pos.x + Size.x - 3.0f, Pos.y + 4.0f), Menu::DarkAccent, 1.0f);
        }

        ImGui::SetCursorScreenPos(ImVec2(Pos.x + Border, Pos.y + (HasHeader ? HeaderHeight : 0.0f)));
        PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(3.0f, 6.0f));
        ImGui::BeginChild((StrLabel + "##Body").c_str(), ImVec2(Size.x - Border * 2.0f, Size.y - (HasHeader ? HeaderHeight : 0.0f) - Border), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
        PushItemWidth(ImGui::GetContentRegionAvail().x);
    }

    return Success;
}

void Menu::EndChild()
{
    PopItemWidth();
    ImGui::EndChild();
    PopStyleVar();
    ImGui::EndChild();
}

bool Menu::CheckBox(const char* label, bool* checked)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 labelSize = CalcTextSize(label, nullptr, true);
    const float squareSize = g.FontSize + style.CellPadding.y * 2.0f - 2.0f;

    ImVec2 pos = ImVec2(window->DC.CursorPos.x + 1.0f, window->DC.CursorPos.y);
    ImVec2 size = ImVec2(squareSize + style.ItemInnerSpacing.x + labelSize.x, squareSize);

    const ImRect checkBB(pos, pos + ImVec2(squareSize, squareSize));
    const ImRect totalBB(pos, pos + size);

    ItemSize(totalBB);
    if (!ItemAdd(totalBB, id)) return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(totalBB, id, &hovered, &held);
    if (pressed) *checked = !*checked;

    static std::map<ImGuiID, ImColor> checkboxAnim;
    static std::map<ImGuiID, ImColor> textAnim;

    bool activeBox = *checked;
    ImColor targetBox = activeBox ? Menu::Accent : Menu::ChildBg;

    bool activeText = *checked || hovered;
    ImColor targetText = activeText ? Menu::Text : Menu::TextDim;

    ImColor& boxCol = checkboxAnim[id];
    if (boxCol.Value.w == 0.0f) boxCol = targetBox;
    boxCol.Value = ImLerp(boxCol.Value, targetBox.Value, g.IO.DeltaTime * 10.0f);

    ImColor& txtCol = textAnim[id];
    if (txtCol.Value.w == 0.0f) txtCol = targetText;
    txtCol.Value = ImLerp(txtCol.Value, targetText.Value, g.IO.DeltaTime * 10.0f);

    RenderNavCursor(totalBB, id);

    if (*checked)
    {
        window->DrawList->AddRectFilled(checkBB.Min, checkBB.Max, boxCol, style.FrameRounding);
        window->DrawList->AddRectFilledMultiColorRounded(checkBB.Min, checkBB.Max, ImColor(0, 0, 0, 0), ImColor(0, 0, 0, 0), ImColor(0, 0, 0, 140), ImColor(0, 0, 0, 140), style.FrameRounding);
    }
    else
    {
        window->DrawList->AddRectFilled(checkBB.Min, checkBB.Max, boxCol, style.FrameRounding);
        window->DrawList->AddRectFilledMultiColorRounded(checkBB.Min, checkBB.Max, IM_COL32(32, 32, 32, 255), IM_COL32(32, 32, 32, 255), IM_COL32(24, 24, 24, 255), IM_COL32(24, 24, 24, 255), style.FrameRounding);
    }

    window->DrawList->AddRect(checkBB.Min, checkBB.Max, Menu::Outline, style.FrameRounding);
    window->DrawList->AddRect(checkBB.Min + ImVec2(1, 1), checkBB.Max - ImVec2(1, 1), activeBox ? Menu::DarkAccent : IM_COL32(44, 44, 44, 255), style.FrameRounding);

    Menu::DrawLabelShadow(window->DrawList, ImVec2(pos.x + squareSize + style.ItemInnerSpacing.x + 1.0f, pos.y + (squareSize - labelSize.y) * 0.5f), txtCol, label);

    return pressed;
}

bool Menu::SliderAlpha(const char* str_id, ImVec4& col)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    ImGuiIO& io = g.IO;
    const ImGuiID id = window->GetID(str_id);

    float width = CalcItemWidth();
    float height = GetFrameHeight();
    float half_height = ImTrunc(height / 2.0f);

    ImVec2 pos = window->DC.CursorPos;
    const ImVec2 size(width, height);
    const ImRect bb(pos, pos + size);

    ItemSize(bb);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    const float slider_min_x = bb.Min.x + half_height;
    const float slider_max_x = bb.Max.x - half_height;

    static std::map<ImGuiID, float> AlphaAnim;
    float& alpha_lerp = AlphaAnim[id];
    if (alpha_lerp == 0.0f)
        alpha_lerp = col.w;

    if (held)
    {
        float slider_width = slider_max_x - slider_min_x;
        if (slider_width > 0.0f)
        {
            float mouse_x = ImClamp(io.MousePos.x, slider_min_x, slider_max_x);
            alpha_lerp = 1.0f - ((mouse_x - slider_min_x) / slider_width);
            alpha_lerp = ImClamp(alpha_lerp, 0.0f, 1.0f);
        }
    }

    col.w = col.w + (alpha_lerp - col.w) * 0.07f;

    ImColor col_rgb = ImVec4(col.x, col.y, col.z, 1.0f);
    ImColor col_rgb_width_alpha = ImVec4(col.x, col.y, col.z, col.w);

    window->DrawList->AddRectFilled(pos, pos + ImVec2(half_height, height), col_rgb, style.FrameRounding, ImDrawFlags_RoundCornersLeft);
    RenderColorRectWithAlphaCheckerboard(window->DrawList, pos + ImVec2(half_height, 0), pos + size, 0, half_height, ImVec2(0.0f, 0.0f), style.FrameRounding, ImDrawFlags_RoundCornersRight);
    window->DrawList->AddRectFilledMultiColorRounded(pos + ImVec2(half_height, 0), pos + ImVec2(width - half_height, height), col_rgb, col_rgb & ~IM_COL32_A_MASK, col_rgb & ~IM_COL32_A_MASK, col_rgb);

    if (style.FrameBorderSize > 0)
        window->DrawList->AddRect(pos, pos + size, GetColorU32(ImGuiCol_Border), style.FrameRounding, 0, style.FrameBorderSize);

    const float circle_radius = ImMax(ImTrunc(g.FontSize / 2.0f) - 2.0f, 1.0f);
    float slider_width = slider_max_x - slider_min_x;
    float circle_x = slider_min_x + slider_width * (1.0f - col.w);

    RenderColorRectWithAlphaCheckerboard(window->DrawList,
        ImVec2(circle_x - circle_radius, pos.y + half_height - circle_radius),
        ImVec2(circle_x + circle_radius, pos.y + half_height + circle_radius),
        col_rgb_width_alpha, circle_radius, ImVec2(0.0f, 0.0f), circle_radius + 1.0f);

    window->DrawList->AddCircle(ImVec2(circle_x, pos.y + half_height), circle_radius + 1.0f, IM_COL32_BLACK, 0, 1.0f);

    return pressed;
}

bool Menu::SliderHue(const char* str_id, ImVec4& col)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    ImGuiIO& io = g.IO;
    const ImGuiID id = window->GetID(str_id);

    float width = CalcItemWidth();
    float height = GetFrameHeight();
    float half_height = ImTrunc(height / 2.0f);

    ImVec2 pos = window->DC.CursorPos;
    const ImVec2 size(width, height);
    const ImRect bb(pos, pos + size);

    ItemSize(bb);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    const float slider_min_x = bb.Min.x + half_height;
    const float slider_max_x = bb.Max.x - half_height;

    float h, s, v;
    ImGui::ColorConvertRGBtoHSV(col.x, col.y, col.z, h, s, v);

    static std::map<ImGuiID, float> HueAnim;
    float& hue_lerp = HueAnim[id];
    if (hue_lerp == 0.0f)
        hue_lerp = h;

    if (held)
    {
        float slider_width = slider_max_x - slider_min_x;
        if (slider_width > 0.0f)
        {
            float mouse_x = ImClamp(io.MousePos.x, slider_min_x, slider_max_x);
            hue_lerp = (mouse_x - slider_min_x) / slider_width;
            hue_lerp = ImClamp(hue_lerp, 0.0f, 0.996f);
        }
    }

    h = h + (hue_lerp - h) * 0.07f;
    ImGui::ColorConvertHSVtoRGB(h, s, v, col.x, col.y, col.z);

    const ImVec2 gradient_start = pos + ImVec2(half_height, 0);
    const ImVec2 gradient_end = pos + ImVec2(width - half_height, height);

    const int num_segments = 6;
    const float segment_width = (width - height) / num_segments;

    for (int i = 0; i < num_segments; i++)
    {
        float hue_start = i / (float)num_segments;
        float hue_end = (i + 1) / (float)num_segments;

        ImVec4 color_start, color_end;
        ImGui::ColorConvertHSVtoRGB(hue_start, 1.0f, 1.0f, color_start.x, color_start.y, color_start.z);
        ImGui::ColorConvertHSVtoRGB(hue_end, 1.0f, 1.0f, color_end.x, color_end.y, color_end.z);
        color_start.w = color_end.w = 1.0f;

        ImVec2 seg_start = gradient_start + ImVec2(segment_width * i, 0);
        ImVec2 seg_end = gradient_start + ImVec2(segment_width * (i + 1), height);

        window->DrawList->AddRectFilledMultiColor(seg_start, seg_end,
            ImColor(color_start), ImColor(color_end), ImColor(color_end), ImColor(color_start));
    }

    ImVec4 left_color, right_color;
    ImGui::ColorConvertHSVtoRGB(0.0f, 1.0f, 1.0f, left_color.x, left_color.y, left_color.z);
    ImGui::ColorConvertHSVtoRGB(1.0f, 1.0f, 1.0f, right_color.x, right_color.y, right_color.z);
    left_color.w = right_color.w = 1.0f;

    window->DrawList->AddRectFilled(pos, pos + ImVec2(half_height, height), ImColor(left_color), style.FrameRounding, ImDrawFlags_RoundCornersLeft);
    window->DrawList->AddRectFilled(pos + ImVec2(width - half_height - ImFmod(g.FontSize, 2.0f), 0), pos + size, ImColor(right_color), style.FrameRounding, ImDrawFlags_RoundCornersRight);

    if (style.FrameBorderSize > 0)
        window->DrawList->AddRect(pos, pos + size, GetColorU32(ImGuiCol_Border), style.FrameRounding, 0, style.FrameBorderSize);

    const float circle_radius = ImMax(ImTrunc(g.FontSize / 2.0f) - 2.0f, 1.0f);
    float slider_width = slider_max_x - slider_min_x;
    float circle_x = slider_min_x + slider_width * h;

    ImVec4 hue_color;
    ImGui::ColorConvertHSVtoRGB(h, 1.0f, 1.0f, hue_color.x, hue_color.y, hue_color.z);
    hue_color.w = 1.0f;

    window->DrawList->AddCircleFilled(ImVec2(circle_x, pos.y + half_height), circle_radius, GetColorU32(hue_color));
    window->DrawList->AddCircle(ImVec2(circle_x, pos.y + half_height), circle_radius + 1.0f, IM_COL32_BLACK, 0, 1.0f);

    return pressed;
}

bool Menu::ColorPalette(const char* str_id, ImVec4& col, const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    ImGuiIO& io = g.IO;
    const ImGuiID id = window->GetID(str_id);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, CalcItemWidth(), CalcItemWidth());

    const ImRect bb(pos, pos + size);

    ItemSize(bb);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    float h, s, v;
    ImGui::ColorConvertRGBtoHSV(col.x, col.y, col.z, h, s, v);

    struct SVState { float s; float v; };
    static std::map<ImGuiID, SVState> SVAnim;
    SVState& sv = SVAnim[id];

    if (sv.s == 0.0f && sv.v == 0.0f)
    {
        sv.s = s;
        sv.v = v;
    }

    if (held)
    {
        sv.s = ImClamp((io.MousePos.x - bb.Min.x) / size.x, 0.0f, 1.0f);
        sv.v = 1.0f - ImClamp((io.MousePos.y - bb.Min.y) / size.y, 0.0f, 0.996f);
    }

    s = s + (sv.s - s) * 0.07f;
    v = v + (sv.v - v) * 0.07f;

    ImGui::ColorConvertHSVtoRGB(h, s, v, col.x, col.y, col.z);

    ImVec4 hue_color;
    ImGui::ColorConvertHSVtoRGB(h, 1.0f, 1.0f, hue_color.x, hue_color.y, hue_color.z);
    hue_color.w = 1.0f;
    ImU32 hue_color32 = ImGui::ColorConvertFloat4ToU32(hue_color);

    window->DrawList->AddRectFilledMultiColorRounded(
        bb.Min, bb.Max,
        IM_COL32_WHITE, hue_color32, hue_color32, IM_COL32_WHITE,
        style.FrameRounding + (style.FrameRounding > 0 ? 1.5f : 0.0f)
    );

    window->DrawList->AddRectFilledMultiColorRounded(
        bb.Min, bb.Max,
        IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), IM_COL32_BLACK, IM_COL32_BLACK,
        style.FrameRounding
    );

    if (style.FrameBorderSize > 0)
        window->DrawList->AddRect(bb.Min, bb.Max, GetColorU32(ImGuiCol_Border), style.FrameRounding, 0, style.FrameBorderSize);

    float circle_x = bb.Min.x + s * size.x;
    float circle_y = bb.Min.y + (1.0f - v) * size.y;
    const float circle_radius = ImMax(ImTrunc(g.FontSize / 2.0f) - 2.0f, 1.0f);

    ImVec4 col_rgb_no_alpha(col.x, col.y, col.z, 1.0f);

    window->DrawList->AddCircleFilled(ImVec2(circle_x, circle_y), circle_radius, GetColorU32(col_rgb_no_alpha));
    window->DrawList->AddCircle(ImVec2(circle_x, circle_y), circle_radius + 1.0f, IM_COL32_BLACK, 0, 1.0f);
    window->DrawList->AddCircle(ImVec2(circle_x, circle_y), circle_radius, IM_COL32_WHITE, 0, 1.0f);

    return pressed;
}

bool Menu::ColorButton(const char* desc_id, const ImVec4& col, bool has_alpha, const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(desc_id);

    ImVec2 pos = window->DC.CursorPos;
    const ImVec2 size(CalcItemSize(size_arg, g.FontSize, g.FontSize));
    const ImRect bb(pos, pos + size);

    ItemSize(bb);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    ImVec4 col_rgb = col;
    ImVec4 col_rgb_without_alpha(col_rgb.x, col_rgb.y, col_rgb.z, 1.0f);

    ImVec4 col_source = has_alpha ? col_rgb : col_rgb_without_alpha;

    if (col_source.w < 1.0f && has_alpha)
        RenderColorRectWithAlphaCheckerboard(window->DrawList, pos, pos + size, GetColorU32(col_source), size.y / 2, ImVec2(0, 0), style.FrameRounding);
    else
        window->DrawList->AddRectFilled(pos, pos + size, GetColorU32(col_source), style.FrameRounding);

    if (style.FrameBorderSize > 0)
    {
        window->DrawList->AddRect(pos, pos + size, GetColorU32(ImGuiCol_BorderShadow), style.FrameRounding, 0, style.FrameBorderSize);
        window->DrawList->AddRect(pos + ImVec2(style.FrameBorderSize, style.FrameBorderSize), pos + size - ImVec2(style.FrameBorderSize, style.FrameBorderSize), IM_COL32(44 ,44 ,44 , 255), style.FrameRounding, 0, style.FrameBorderSize);
    }

    RenderNavCursor(bb, id);

    return pressed;
}

void Menu::ColorPicker4(const char* label, float col[4], bool has_alpha)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    ImGuiStyle& style = g.Style;

    ImVec4 col_v4(col[0], col[1], col[2], has_alpha ? col[3] : 1.0F);

    float width = 200;

    PushID(label);
    BeginGroup();

    std::string str_label = label;
    std::string str_colp_id = str_label + "::color_palette";
    std::string str_hue_id = str_label + "::hue";

    Menu::ColorPalette(str_colp_id.c_str(), col_v4, ImVec2(width, ImTrunc(width / 2)));

    PushItemWidth(width);
    PushStyleVar(ImGuiStyleVar_FramePadding, style.CellPadding);

    Menu::SliderHue(str_hue_id.c_str(), col_v4);

    if (has_alpha)
    {
        std::string str_alpha_id = str_label + "::alpha";
        Menu::SliderAlpha(str_alpha_id.c_str(), col_v4);

    }

    PopStyleVar();
    PopItemWidth();

    EndGroup();
    PopID();

    col[0] = col_v4.x;
    col[1] = col_v4.y;
    col[2] = col_v4.z;
    col[3] = col_v4.w;
}

bool Menu::ColorEdit4(const char* name, float col[4])
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImVec4 col_v4(col[0], col[1], col[2], col[3]);

    ImVec2 pos = window->DC.CursorPos + ImVec2(1.0f, 0.0f);

    std::string popup_str_id = std::string(std::string(name) + "::color_edit_4");

    PushID(GetID(name));
    BeginGroup();

    float square_sz = GetColorPickerWidth();

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 1.0f - 8.0f);

    bool result = Menu::ColorButton(name, col_v4, true, ImVec2(square_sz + 8.0f, square_sz));

    ImVec2 bb_min = pos;
    ImVec2 bb_max = pos + ImVec2(square_sz, square_sz);
    bb_min.x -= 8.0f;

    window->DrawList->AddRectFilledMultiColorRounded(bb_min, bb_max, ImColor(0, 0, 0, 0), ImColor(0, 0, 0, 0), ImColor(0, 0, 0, 140), ImColor(0, 0, 0, 140), style.FrameRounding);

    window->DrawList->AddRect(bb_min, bb_max, Menu::Outline, 0.0f, 0, 1.0f);
    window->DrawList->AddRect(bb_min + ImVec2(1.0f, 1.0f), bb_max - ImVec2(1.0f, 1.0f), IM_COL32(44, 44, 44, 255), 0.0f, 0, 1.0f);

    EndGroup();
    PopID();

    if (result)
    {
        OpenPopup(popup_str_id.c_str());
    }

    PushStyleVar(ImGuiStyleVar_WindowPadding, style.FramePadding);
    PushStyleVar(ImGuiStyleVar_ItemSpacing, style.FramePadding);

    if (BeginPopupEx(GetID(popup_str_id.c_str()), ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        SetWindowPos(ImVec2(pos.x - style.WindowPadding.x - 5.0f, pos.y - style.WindowPadding.y));

        ImVec4 col_rgba = ImVec4(0, 0, 0, 0);
        col_rgba.x = col[0];
        col_rgba.y = col[1];
        col_rgba.z = col[2];
        col_rgba.w = col[3];

        char hex_buf[64];
        int i[4] = { IM_F32_TO_INT8_UNBOUND(col[0]), IM_F32_TO_INT8_UNBOUND(col[1]), IM_F32_TO_INT8_UNBOUND(col[2]), IM_F32_TO_INT8_UNBOUND(col[3]) };
        ImFormatString(hex_buf, IM_ARRAYSIZE(hex_buf), "#%02X%02X%02X%02X", ImClamp(i[0], 0, 255), ImClamp(i[1], 0, 255), ImClamp(i[2], 0, 255), ImClamp(i[3], 0, 255));

        ImGui::SameLine();
        ImGui::SetCursorPosY(style.WindowPadding.y + style.CellPadding.y);
        ImGui::Text("%s", name);

        ImGui::SameLine(ImGui::CalcItemSize(ImVec2(-0.1f, 0), 0, 0).x - CalcTextSize(hex_buf).x + style.WindowPadding.x);
        ImGui::SetCursorPosY(style.WindowPadding.y + style.CellPadding.y);
        ImGui::TextDisabled("%s", hex_buf);

        Menu::ColorPicker4(name, col, true);

        EndPopup();
    }

    PopStyleVar(2);

    return result;
}

float Menu::GetColorPickerWidth()
{
    return (GetFontSize() + ImGui::GetStyle().CellPadding.y * 2.0f) - ImFmod(GetFontSize(), 2.0f) - 1.0f;
}

bool Menu::SliderScalar(const char* Label, ImGuiDataType DataType, void* PData, const void* PMin, const void* PMax, const char* Format)
{
    ImGuiWindow* Window = GetCurrentWindow();
    if (Window->SkipItems) return false;

    ImGuiContext& G = *GImGui;
    const ImGuiStyle& Style = G.Style;
    const ImGuiID Id = Window->GetID(Label);
    const float Width = CalcItemWidth();
    ImVec2 Pos = Window->DC.CursorPos; Pos.x += 1.0f;

    const ImVec2 LabelSize = CalcTextSize(Label, nullptr, true);
    const bool HasLabel = LabelSize.x > 0;
    const float FramePosY = HasLabel ? (G.FontSize + Style.ItemInnerSpacing.y) : 0.0f;
    const float FrameHeight = Style.GrabMinSize;
    const ImRect FrameBB(Pos + ImVec2(0, FramePosY - 3.0f), Pos + ImVec2(Width, FramePosY + FrameHeight + 1.0f));
    const ImRect TotalBB(Pos, FrameBB.Max);

    ItemSize(TotalBB);
    if (!ItemAdd(TotalBB, Id, &FrameBB, 0)) return false;

    if (!Format) Format = DataTypeGetInfo(DataType)->PrintFmt;

    const bool Hovered = ItemHoverable(FrameBB, Id, G.LastItemData.ItemFlags);
    const bool Clicked = Hovered && IsMouseClicked(0, ImGuiInputFlags_None, Id);
    const bool Held = G.ActiveId == Id;
    const bool MakeActive = Clicked || G.NavActivateId == Id;

    if (MakeActive)
    {
        SetActiveID(Id, Window);
        SetFocusID(Id, Window);
        FocusWindow(Window);
        G.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
    }

    ImRect GrabBB;
    const bool ValueChanged = SliderBehavior(FrameBB, Id, DataType, PData, PMin, PMax, Format, 0, &GrabBB);
    if (ValueChanged) MarkItemEdited(Id);

    float TargetValue = 0.0f;
    if (DataType == ImGuiDataType_Float)
        TargetValue = (*(float*)PData - *(float*)PMin) / (*(float*)PMax - *(float*)PMin);
    else if (DataType == ImGuiDataType_S32)
        TargetValue = (*(int*)PData - *(int*)PMin) / float(*(int*)PMax - *(int*)PMin);
    TargetValue = ImClamp(TargetValue, 0.0f, 1.0f);

    static std::map<ImGuiID, float> SliderAnim;
    float& AnimValue = SliderAnim[Id];
    if (AnimValue == 0.0f && !Held) AnimValue = TargetValue;
    AnimValue = ImLerp(AnimValue, TargetValue, G.IO.DeltaTime * (Held ? 18.0f : 10.0f));

    Window->DrawList->AddRectFilledMultiColor(FrameBB.Min, FrameBB.Max, IM_COL32(24, 24, 24, 255), IM_COL32(24, 24, 24, 255), IM_COL32(14, 14, 14, 255), IM_COL32(14, 14, 14, 255));

    ImVec2 FillEnd = ImTrunc(ImVec2(FrameBB.Min.x + AnimValue * FrameBB.GetWidth(), FrameBB.Max.y));
    ImRect SliderBB(FrameBB.Min + ImVec2(1.0f, 1.0f), FillEnd - ImVec2(1.0f, 1.0f));
    if (SliderBB.Max.x > SliderBB.Min.x)
        Window->DrawList->AddRectFilled(SliderBB.Min, SliderBB.Max, Menu::Accent, Style.FrameRounding);
    Window->DrawList->AddRectFilledMultiColorRounded(SliderBB.Min, SliderBB.Max, ImColor(0, 0, 0, 0), ImColor(0, 0, 0, 0), ImColor(0, 0, 0, 140), ImColor(0, 0, 0, 140), Style.FrameRounding);

    Window->DrawList->AddRect(FrameBB.Min, FrameBB.Max, IM_COL32(0, 0, 0, 255), Style.FrameRounding, 0, 1);
    Window->DrawList->AddRect(FrameBB.Min + ImVec2(1.0f, 1.0f), FrameBB.Max - ImVec2(1.0f, 1.0f), IM_COL32(44, 44, 44, 255), Style.FrameRounding, 0, 1);

    char ValueBuf[64];
    DataTypeFormatString(ValueBuf, IM_ARRAYSIZE(ValueBuf), DataType, PData, Format);

    if (HasLabel)
        Menu::DrawLabelShadow(Window->DrawList, TotalBB.Min + ImVec2(1.0f, 0.0f), GetColorU32(ImGuiCol_Text), Label);

    ImVec2 TextSize = CalcTextSize(ValueBuf);
    ImVec2 TextPos = ImVec2(FrameBB.Min.x + 1.0f + (FrameBB.GetWidth() - TextSize.x) * 0.5f, FrameBB.Min.y + (FrameBB.GetHeight() - TextSize.y) * 0.5f);
    static std::map<ImGuiID, float> TextAnim;
    float& TextT = TextAnim[Id];

    float TargetT = (Hovered || Held) ? 1.0f : 0.0f;
    TextT = ImLerp(TextT, TargetT, G.IO.DeltaTime * 12.0f);

    ImVec4 Dim = ImGui::ColorConvertU32ToFloat4(Menu::TextDim);
    ImVec4 Bright = ImGui::ColorConvertU32ToFloat4(Menu::Text);

    ImVec4 LerpCol = ImLerp(Dim, Bright, TextT);
    ImU32 ValueTextColor = ImGui::ColorConvertFloat4ToU32(LerpCol);

    Menu::DrawLabelShadow(Window->DrawList, TextPos, ValueTextColor, ValueBuf);



    return ValueChanged;
}

bool Menu::SliderFloat(const char* Label, float* V, float VMin, float VMax, const char* Format)
{
    return Menu::SliderScalar(Label, ImGuiDataType_Float, V, &VMin, &VMax, Format);
}

bool Menu::SliderInt(const char* Label, int* V, int VMin, int VMax, const char* Format)
{
    return Menu::SliderScalar(Label, ImGuiDataType_S32, V, &VMin, &VMax, Format);
}

bool Menu::SelectableLabel(const char* Label, bool Selected, const ImVec2& SizeArg)
{
    ImGuiWindow* Window = GetCurrentWindow();
    if (Window->SkipItems) return false;

    ImGuiContext& G = *GImGui;
    const ImGuiStyle& Style = G.Style;
    const ImGuiID Id = Window->GetID(Label);
    const ImVec2 LabelSize = CalcTextSize(Label, NULL, true);

    ImVec2 Pos = Window->DC.CursorPos;
    ImVec2 Size = CalcItemSize(SizeArg, LabelSize.x, LabelSize.y);

    const ImRect TotalBB(Pos, Pos + Size);
    ItemSize(Size);
    if (!ItemAdd(TotalBB, Id)) return false;

    bool Hovered, Held;
    bool Pressed = ButtonBehavior(TotalBB, Id, &Hovered, &Held);

    ImVec4 ColLabel = Selected ? ColorConvertU32ToFloat4(Menu::Accent) : Hovered ? ColorConvertU32ToFloat4(Menu::Text) : ColorConvertU32ToFloat4(Menu::TextDim);

    struct StateColors { ImColor Label; };
    static std::map<ImGuiID, StateColors> Anim;
    auto ItAnim = Anim.find(Id);
    if (ItAnim == Anim.end())
    {
        Anim.insert({ Id, StateColors() });
        ItAnim = Anim.find(Id);
        ItAnim->second.Label = ColLabel;
    }

    ItAnim->second.Label.Value = ImLerp(ItAnim->second.Label.Value, ColLabel, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);

    RenderNavCursor(TotalBB, Id);

    Window->DrawList->AddText(Pos + ImVec2(1.0f, 0.0f), ItAnim->second.Label, Label);

    return Pressed;
}

bool Menu::Combo(const char* Label, int* SelectedIndex, std::vector<const char*> Items)
{
    ImGuiWindow* Window = GetCurrentWindow();
    if (Window->SkipItems)
        return false;

    ImGuiContext& G = *GImGui;
    const ImGuiStyle& Style = G.Style;
    const ImGuiID Id = Window->GetID(Label);
    const ImVec2 LabelSize = CalcTextSize(Label, NULL, true);
    const float Height = GetFrameHeight();
    const float Width = CalcItemWidth();

    ImVec2 Pos = Window->DC.CursorPos;
    ImVec2 Size = ImVec2(Width, Height + (LabelSize.x > 0.0f ? (LabelSize.y + Style.ItemInnerSpacing.y) : 0.0f));

    const ImRect FrameBB(Pos + ImVec2(2.0f, LabelSize.x > 0.0f ? (LabelSize.y + Style.ItemInnerSpacing.y) : 0.0f), Pos + Size);
    const ImRect TotalBB(Pos, Pos + Size);

    ItemSize(Size);
    if (!ItemAdd(TotalBB, Id))
        return false;

    bool Hovered, Held;
    bool Pressed = ButtonBehavior(FrameBB, Id, &Hovered, &Held);
    bool PopupOpen = IsPopupOpen(std::string(std::string(Label) + "::combo_popup").c_str());

    struct StColorsState { ImColor Frame; ImColor Text; };
    static std::map<ImGuiID, StColorsState> Anim;
    auto& State = Anim[Id];

    ImColor TargetText = PopupOpen ? Menu::Accent : Hovered ? Menu::Text : Menu::TextDim;
    ImColor TargetFrame = GetStyleColorVec4((Hovered && Held) ? ImGuiCol_FrameBgActive : Hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);

    if (State.Text.Value.w == 0.0f) {
        State.Text = TargetText;
        State.Frame = TargetFrame;
    }

    float LerpSpeed = 1.0f / IMADD_ANIMATIONS_SPEED * G.IO.DeltaTime;
    State.Frame.Value = ImLerp(State.Frame.Value, TargetFrame.Value, LerpSpeed);
    State.Text.Value = ImLerp(State.Text.Value, TargetText.Value, LerpSpeed);

    std::string PopupStrId = std::string(std::string(Label) + "::combo_popup");
    if (Pressed) OpenPopup(PopupStrId.c_str());

    PushStyleVar(ImGuiStyleVar_WindowPadding, Style.FramePadding);
    PushStyleVar(ImGuiStyleVar_ItemSpacing, Style.FramePadding);

    if (BeginPopupEx(GetID(PopupStrId.c_str()), ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground))
    {
        ImDrawList* DL = ImGui::GetWindowDrawList();
        ImVec2 PMin = GetWindowPos();
        ImVec2 PMax = PMin + GetWindowSize();

        DL->AddRectFilledMultiColor(PMin, PMax, IM_COL32(24, 24, 24, 255), IM_COL32(24, 24, 24, 255), IM_COL32(14, 14, 14, 255), IM_COL32(14, 14, 14, 255));
        DL->AddRect(PMin + ImVec2(1, 1), PMax - ImVec2(1, 1), Menu::Outline);
        DL->AddRect(PMin + ImVec2(2, 2), PMax - ImVec2(2, 2), IM_COL32(44, 44, 44, 255));

        SetWindowPos(FrameBB.Min + ImVec2(-2.0f, Height + Style.FramePadding.y - 2.0f), ImGuiCond_Always);
        SetWindowSize(ImVec2(Width + 2.0f, GetFontSize() * Items.size() + Style.FramePadding.y * (Items.size() + 1) + 1.0f), ImGuiCond_Always);

        PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        for (int i = 0; i < (int)Items.size(); i++)
        {
            ImVec2 ItemPos = GetCursorScreenPos();
            ImGuiID ItemId = Window->GetID((std::string(Label) + Items[i] + std::to_string(i)).c_str());
            bool IsSelected = (i == *SelectedIndex);

            if (Menu::SelectableLabel(Items[i], IsSelected, ImVec2(-0.1f, GetFontSize())))
            {
                *SelectedIndex = i;
                CloseCurrentPopup();
            }

            bool ItemHovered = IsItemHovered();
            static std::map<ImGuiID, float> ItemAnims;
            float& AnimV = ItemAnims[ItemId];
            AnimV = ImLerp(AnimV, ItemHovered ? 1.0f : 0.0f, LerpSpeed);

            ImVec4 ColDim = ColorConvertU32ToFloat4(Menu::TextDim);
            ImVec4 ColBright = ColorConvertU32ToFloat4(Menu::Text);
            ImU32 CurrentItemCol = ColorConvertFloat4ToU32(ImLerp(ColDim, ColBright, AnimV));
            if (IsSelected) CurrentItemCol = Menu::Accent;

            Menu::DrawLabelShadow(DL, ItemPos + ImVec2(2.0f, 0.0f), CurrentItemCol, Items[i]);
        }
        PopStyleVar();
        EndPopup();
    }
    PopStyleVar(2);

    RenderNavCursor(FrameBB, Id);

    Menu::DrawLabelShadow(Window->DrawList, Pos + ImVec2(2.0f, 0.0f), Menu::Text, Label);
    Window->DrawList->AddRectFilledMultiColor(FrameBB.Min, FrameBB.Max, IM_COL32(24, 24, 24, 255), IM_COL32(24, 24, 24, 255), IM_COL32(14, 14, 14, 255), IM_COL32(14, 14, 14, 255));
    Window->DrawList->AddRect(FrameBB.Min - ImVec2(1, 1), FrameBB.Max + ImVec2(1, 1), Menu::Outline, Style.FrameRounding);
    Window->DrawList->AddRect(FrameBB.Min, FrameBB.Max, IM_COL32(44, 44, 44, 255), Style.FrameRounding);

    const char* PreviewItem = (*SelectedIndex < (int)Items.size()) ? Items[*SelectedIndex] : "*unknown*";
    Menu::DrawLabelShadow(Window->DrawList, FrameBB.Min + Style.FramePadding + ImVec2(1.0f, 0.0f), State.Text, PreviewItem);

    const char* ArrowText = PopupOpen ? "-" : "+";
    ImVec2 ArrowSize = CalcTextSize(ArrowText);
    ImVec2 ArrowPos = ImVec2(FrameBB.Max.x - Height * 0.5f - ArrowSize.x * 0.5f, FrameBB.Min.y + Height * 0.5f - ArrowSize.y * 0.5f - 1.0f);

    for (int dx = -1; dx <= 1; dx++)
        for (int dy = -1; dy <= 1; dy++)
            if (dx || dy)
                Window->DrawList->AddText(ArrowPos + ImVec2((float)dx, (float)dy), IM_COL32_BLACK, ArrowText);

    Window->DrawList->AddText(ArrowPos, GetColorU32(ImGuiCol_Text), ArrowText);

    return Pressed;
}

bool Menu::KeyBind(const char* StrId, ImGuiKey* Key)
{
    ImGuiWindow* Window = GetCurrentWindow();
    if (Window->SkipItems) return false;

    ImGuiContext& G = *GImGui;
    ImGuiIO& Io = G.IO;
    ImGuiID Id = Window->GetID(StrId);

    ImVec2 BoxSize(40.0f, 15.0f);
    ImVec2 Pos = Window->DC.CursorPos - ImVec2(37.0f, 0.0f);
    ImRect FrameBb(Pos, Pos + BoxSize);

    ItemSize(FrameBb);
    if (!ItemAdd(FrameBb, Id)) return false;

    bool Hovered = ItemHoverable(FrameBb, Id, 0);
    bool IsSelecting = (G.ActiveId == Id);

    struct ColorState { ImColor Label; };
    static std::map<ImGuiID, ColorState> Anim;
    auto ItAnim = Anim.find(Id);
    if (ItAnim == Anim.end()) { Anim[Id] = { ImColor(Menu::TextDim) }; ItAnim = Anim.find(Id); }

    ImColor TargetColor = IsSelecting ? ImColor(Menu::Accent) : (Hovered ? ImColor(Menu::Text) : ImColor(Menu::TextDim));
    float T = 1.0f / IMADD_ANIMATIONS_SPEED * Io.DeltaTime;
    ItAnim->second.Label.Value = ImLerp(ItAnim->second.Label.Value, TargetColor.Value, T);

    if (Hovered) { SetHoveredID(Id); G.MouseCursor = ImGuiMouseCursor_Hand; }
    if (Hovered && IsMouseClicked(ImGuiMouseButton_Left)) { SetActiveID(Id, Window); FocusWindow(Window); }
    else if (IsMouseClicked(ImGuiMouseButton_Left) && G.ActiveId == Id && !Hovered) ClearActiveID();

    bool ValueChanged = false;
    if (IsSelecting)
    {
        for (int btn = 0; btn < 5; btn++)
        {
            if (IsMouseClicked(btn))
            {
                *Key = (ImGuiKey)(ImGuiKey_MouseLeft + btn);
                ValueChanged = true;
                ClearActiveID();
                break;
            }
        }

        if (!ValueChanged)
        {
            for (int I = ImGuiKey_NamedKey_BEGIN; I < ImGuiKey_NamedKey_END; I++)
            {
                ImGuiKey KeyTest = (ImGuiKey)I;

                if (KeyTest >= ImGuiKey_MouseLeft && KeyTest <= ImGuiKey_MouseWheelY)
                    continue;

                if (IsKeyPressed(KeyTest))
                {
                    *Key = KeyTest;
                    ValueChanged = true;
                    ClearActiveID();
                    break;
                }
            }
        }

        if (IsKeyPressed(ImGuiKey_Escape))
        {
            *Key = ImGuiKey_None;
            ClearActiveID();
        }
    }

    ImDrawList* DrawList = Window->DrawList;
    DrawList->AddRectFilledMultiColor(FrameBb.Min, FrameBb.Max, IM_COL32(34, 34, 34, 255), IM_COL32(34, 34, 34, 255), IM_COL32(24, 24, 24, 255), IM_COL32(24, 24, 24, 255));
    DrawList->AddRect(FrameBb.Min, FrameBb.Max, Menu::Outline, 0.0f, 0, 1.0f);
    DrawList->AddRect(FrameBb.Min + ImVec2(1, 1), FrameBb.Max - ImVec2(1, 1), IM_COL32(44, 44, 44, 255), 0.0f, 0, 1.0f);

    const char* KeyName = *Key ? GetKeyName(*Key) : "Unbound";
    ImVec2 TextSize = CalcTextSize(KeyName);
    ImVec2 TextPos = FrameBb.Min + ImVec2((BoxSize.x - TextSize.x) / 2.0f, (BoxSize.y - TextSize.y) / 2.0f - 1.0f);

    Menu::DrawLabelShadow(DrawList, TextPos, ItAnim->second.Label, KeyName);

    return ValueChanged;
}

bool Menu::KeyBindEx(const char* StrId, ImGuiKey* Key, ImKeyBindMode* Mode, const ImVec2& SizeArg)
{
    ImGuiWindow* Window = GetCurrentWindow();
    if (Window->SkipItems) return false;

    ImGuiContext& G = *GImGui;
    ImGuiIO& Io = G.IO;
    const ImGuiStyle& Style = G.Style;

    bool Result = Menu::KeyBind(StrId, Key);

    std::string PopupName = std::string(StrId ? StrId : "") + "##popup";
    if (IsItemHovered() && IsMouseClicked(ImGuiMouseButton_Right, false))
        OpenPopup(PopupName.c_str());

    if (BeginPopup(PopupName.c_str()))
    {
        ImGui::BeginChild("popup_rect", ImVec2(CalcTextSize("ToggleHold").x + Style.ItemSpacing.x * 2.0f - 8.0f, GetFontSize()), ImGuiActivateFlags_None, ImGuiWindowFlags_NoBackground);

        auto DrawTabColor = [&](bool Selected) {
            static std::map<int, ImColor> AnimColors;
            ImU32 Col = Selected ? Menu::Accent : Menu::TextDim;
            ImColor& C = AnimColors[GetID(&Col)];
            if (C.Value.w == 0.0f) C = Col;
            C.Value = ImLerp(C.Value, ImColor(Col).Value, G.IO.DeltaTime * 10.0f);
            return C;
            };

        if (Menu::SelectableLabel("Toggle", *Mode == ImKeyBindMode_Toggle)) *Mode = ImKeyBindMode_Toggle;
        SameLine();
        if (Menu::SelectableLabel("Hold", *Mode == ImKeyBindMode_Hold)) *Mode = ImKeyBindMode_Hold;

        EndChild();
        EndPopup();
    }

    return Result;
}
