#include <iostream>
#include "feature/esp.h"
#include "engine/engine.h"
#include <cfloat>
#include <cmath>
#include <clipper2/clipper.h>
#include <imgui.h>
#include "global.h"
#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <imgui_internal.h>
#include "ui/graphic.h"
#include "feature/wallcheck.h"

#define IMGUI_DEFINE_MATH_OPERATORS

#define FlotationDevice(c) ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3]))

static inline bool validscreen(float x, float y)
{
    return x > -0.5f && y > -0.5f;
}

static inline float dot3(const sdk::vector3& a, const sdk::vector3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

namespace visual_frame
{
    static HWND Window = nullptr;
    static RECT ClientRect{};
    static bool HasClientRect = false;
    static sdk::vector3 CameraPos{};
    static bool HasCamera = false;

    static void begin()
    {
        if (!Window || !IsWindow(Window))
            Window = FindWindowA(nullptr, "Roblox");

        HasClientRect = Window && GetClientRect(Window, &ClientRect);
        HasCamera = false;

        if (global::camera.Address)
        {
            sdk::camera cam(global::camera.Address);
            CameraPos = cam.position();
            HasCamera = !(std::isnan(CameraPos.x) || std::isnan(CameraPos.y) || std::isnan(CameraPos.z));
        }
    }
}

static bool visiblecheck(const sdk::vector3& target, const sdk::vector2& screen)
{
    if (!visual_frame::HasClientRect)
        return false;

    if (!(screen.x >= 0.f && screen.y >= 0.f &&
        screen.x <= (float)visual_frame::ClientRect.right &&
        screen.y <= (float)visual_frame::ClientRect.bottom))
        return false;

    if (!visual_frame::HasCamera)
        return false;

    return wallcheck::can_see(visual_frame::CameraPos, target);
}

namespace style
{
    static constexpr ImU32 Surface = IM_COL32(3, 8, 14, 188);
    static constexpr ImU32 SurfaceSoft = IM_COL32(5, 13, 22, 132);
    static constexpr ImU32 Border = IM_COL32(38, 72, 94, 150);
    static constexpr ImU32 Accent = IM_COL32(0, 174, 255, 255);
    static constexpr ImU32 Purple = IM_COL32(100, 117, 255, 255);
    static constexpr ImU32 text = IM_COL32(232, 242, 249, 255);
    static constexpr ImU32 TextDim = IM_COL32(146, 166, 178, 255);
    static bool VisibilityTint = false;
    static bool TargetVisible = true;

    static ImU32 alpha(ImU32 c, float a)
    {
        const int alpha = (int)(((c >> 24) & 0xFF) * ImClamp(a, 0.f, 1.f));
        return (c & 0x00FFFFFF) | ((ImU32)alpha << 24);
    }

    static ImU32 fromfloat(const float col[4], float a = 1.f)
    {
        if (VisibilityTint)
            col = TargetVisible ? global::esp::color::Visible : global::esp::color::NotVisible;

        return IM_COL32(
            (int)(ImClamp(col[0], 0.f, 1.f) * 255.f),
            (int)(ImClamp(col[1], 0.f, 1.f) * 255.f),
            (int)(ImClamp(col[2], 0.f, 1.f) * 255.f),
            (int)(ImClamp(col[3] * a, 0.f, 1.f) * 255.f));
    }

    static ImU32 lerp(ImU32 a, ImU32 b, float t)
    {
        auto ch = [](ImU32 c, int s) { return (int)((c >> s) & 0xFF); };
        auto mix = [&](int s) { return (int)(ch(a, s) * (1.f - t) + ch(b, s) * t); };
        return IM_COL32(mix(0), mix(8), mix(16), mix(24));
    }

    static void shadow(ImDrawList* draw, ImVec2 min, ImVec2 max, float rounding, float strength = 1.f)
    {
        for (int i = 0; i < 5; i++)
        {
            const float spread = 4.f + i * 3.5f;
            const int alpha = (int)((32.f - i * 4.5f) * strength);
            draw->AddRectFilled(min - ImVec2(spread * .45f, spread * .18f) + ImVec2(0.f, 3.f + i),
                max + ImVec2(spread * .45f, spread * .62f),
                IM_COL32(0, 0, 0, alpha), rounding + spread);
        }
    }

    static void frame(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 accent, bool fill)
    {
        const float rounding = 0.f;
        shadow(draw, min, max, rounding, .75f);
        draw->AddRectFilled(min + ImVec2(1.f, 1.f), max - ImVec2(1.f, 1.f),
            fill ? IM_COL32(3, 8, 14, 34) : IM_COL32(3, 8, 14, 36), rounding);
        draw->AddRect(min - ImVec2(1.f, 1.f), max + ImVec2(1.f, 1.f), IM_COL32(0, 0, 0, 92), rounding, 0, 1.4f);
        draw->AddRect(min, max, alpha(accent, .88f), rounding, 0, 1.35f);
        draw->AddRect(min + ImVec2(1.f, 1.f), max - ImVec2(1.f, 1.f), IM_COL32(255, 255, 255, 24), rounding, 0, 1.f);
    }

    static void fillbox(ImDrawList* draw, ImVec2 min, ImVec2 max)
    {
        if (global::esp::Box_Fill_Gradient)
        {
            const float time = global::esp::Box_Fill_Gradient_Rotate ? (float)ImGui::GetTime() * global::esp::BoxFillSpeed : 0.f;
            const ImU32 col1 = fromfloat(global::esp::color::BoxFill_Top, .75f);
            const ImU32 col2 = fromfloat(global::esp::color::BoxFill_Bottom, .75f);
            const float s = sinf(time);
            const float c = cosf(time);
            ImU32 c_tl;
            ImU32 c_tr;
            ImU32 c_br;
            ImU32 c_bl;

            if (global::esp::Box_Fill_Type == 0)
            {
                c_tl = c_bl = ImGui::ColorConvertFloat4ToU32(ImLerp(ImGui::ColorConvertU32ToFloat4(col1), ImGui::ColorConvertU32ToFloat4(col2), (s + 1.f) * .5f));
                c_tr = c_br = ImGui::ColorConvertFloat4ToU32(ImLerp(ImGui::ColorConvertU32ToFloat4(col1), ImGui::ColorConvertU32ToFloat4(col2), (c + 1.f) * .5f));
            }
            else if (global::esp::Box_Fill_Type == 1)
            {
                c_tl = c_tr = ImGui::ColorConvertFloat4ToU32(ImLerp(ImGui::ColorConvertU32ToFloat4(col1), ImGui::ColorConvertU32ToFloat4(col2), (s + 1.f) * .5f));
                c_bl = c_br = ImGui::ColorConvertFloat4ToU32(ImLerp(ImGui::ColorConvertU32ToFloat4(col1), ImGui::ColorConvertU32ToFloat4(col2), (c + 1.f) * .5f));
            }
            else
            {
                c_tl = ImGui::ColorConvertFloat4ToU32(ImLerp(ImGui::ColorConvertU32ToFloat4(col1), ImGui::ColorConvertU32ToFloat4(col2), (s + 1.f) * .5f));
                c_tr = ImGui::ColorConvertFloat4ToU32(ImLerp(ImGui::ColorConvertU32ToFloat4(col1), ImGui::ColorConvertU32ToFloat4(col2), (c + 1.f) * .5f));
                c_br = ImGui::ColorConvertFloat4ToU32(ImLerp(ImGui::ColorConvertU32ToFloat4(col1), ImGui::ColorConvertU32ToFloat4(col2), (-s + 1.f) * .5f));
                c_bl = ImGui::ColorConvertFloat4ToU32(ImLerp(ImGui::ColorConvertU32ToFloat4(col1), ImGui::ColorConvertU32ToFloat4(col2), (-c + 1.f) * .5f));
            }

            draw->AddRectFilledMultiColor(min, max, c_tl, c_tr, c_br, c_bl);
            return;
        }

        draw->AddRectFilled(min, max, fromfloat(global::esp::color::BoxFill_Top, .72f));
    }

    static void fullbox(ImDrawList* draw, ImVec2 min, ImVec2 max)
    {
        const ImU32 accent = fromfloat(global::esp::color::Box);
        if (global::esp::Box_Fill)
            fillbox(draw, min + ImVec2(2.f, 2.f), max - ImVec2(2.f, 2.f));
        frame(draw, min, max, accent, global::esp::Box_Fill);
    }

    static void cornerline(ImDrawList* draw, ImVec2 a, ImVec2 b, ImU32 accent)
    {
        draw->AddLine(a, b, IM_COL32(0, 0, 0, 142), 4.2f);
        draw->AddLine(a, b, alpha(Purple, .28f), 2.8f);
        draw->AddLine(a, b, accent, 1.55f);
    }

    static void cornerbox(ImDrawList* draw, ImVec2 min, ImVec2 max)
    {
        const ImU32 accent = fromfloat(global::esp::color::Box);
        const float w = max.x - min.x;
        const float h = max.y - min.y;
        const float len = ImClamp(ImMin(w, h) * .27f, 9.f, 48.f);

        shadow(draw, min, max, 0.f, .52f);
        if (global::esp::Box_Fill)
            fillbox(draw, min + ImVec2(2.f, 2.f), max - ImVec2(2.f, 2.f));
        else
            draw->AddRectFilled(min + ImVec2(1.f, 1.f), max - ImVec2(1.f, 1.f), IM_COL32(3, 8, 14, 24));

        draw->AddRect(min, max, IM_COL32(255, 255, 255, 18), 0.f, 0, 1.f);
        cornerline(draw, min, ImVec2(min.x + len, min.y), accent);
        cornerline(draw, min, ImVec2(min.x, min.y + len), accent);
        cornerline(draw, ImVec2(max.x - len, min.y), ImVec2(max.x, min.y), accent);
        cornerline(draw, ImVec2(max.x, min.y), ImVec2(max.x, min.y + len), accent);
        cornerline(draw, ImVec2(min.x, max.y), ImVec2(min.x + len, max.y), accent);
        cornerline(draw, ImVec2(min.x, max.y - len), ImVec2(min.x, max.y), accent);
        cornerline(draw, ImVec2(max.x - len, max.y), max, accent);
        cornerline(draw, ImVec2(max.x, max.y - len), max, accent);
    }

    static void healthbar(ImDrawList* draw, ImVec2 pos, ImVec2 size, float ratio)
    {
        ratio = ImClamp(ratio, 0.f, 1.f);
        const float gap = (float)global::esp::gap;
        const float thick = ImMax(4.f, (float)global::esp::Thickness + 2.f);
        const float x = pos.x - gap - thick - 5.f;
        const float y0 = pos.y;
        const float y1 = pos.y + size.y;
        const ImVec2 bgMin(x, y0);
        const ImVec2 bgMax(x + thick, y1);
        const float rounding = thick * .5f;

        shadow(draw, bgMin, bgMax, rounding, .45f);
        draw->AddRectFilled(bgMin, bgMax, IM_COL32(3, 8, 14, 178), rounding);
        draw->AddRect(bgMin, bgMax, Border, rounding, 0, 1.f);

        const float fillH = (y1 - y0 - 4.f) * ratio;
        const ImVec2 fillMin(x + 2.f, y1 - 2.f - fillH);
        const ImVec2 fillMax(x + thick - 2.f, y1 - 2.f);

        if (fillH > 1.f)
        {
            if (global::esp::Healthbar_Type == 1)
                draw->AddRectFilledMultiColor(fillMin, fillMax,
                    fromfloat(global::esp::color::Healthbar_Top),
                    fromfloat(global::esp::color::Healthbar_Top),
                    fromfloat(global::esp::color::Healthbar_Bottom),
                    fromfloat(global::esp::color::Healthbar_Bottom));
            else
                draw->AddRectFilled(fillMin, fillMax, fromfloat(global::esp::color::Healthbar), rounding - 1.f);
        }
    }

    static void label(ImDrawList* draw, ImVec2 pos, const char* text, const float col[4])
    {
        if (!text || !*text)
            return;

        ImFont* font = Tahoma_BoldXP ? Tahoma_BoldXP : ImGui::GetFont();
        ImGui::PushFont(font);
        const float fontSize = ImGui::GetFontSize();
        ImGui::PopFont();

        pos.x = std::roundf(pos.x);
        pos.y = std::roundf(pos.y);
        const ImU32 accent = fromfloat(col);

        draw->AddText(font, fontSize, pos + ImVec2(0.f, 2.f), IM_COL32(0, 0, 0, 185), text);
        draw->AddText(font, fontSize, pos + ImVec2(2.f, 2.f), IM_COL32(0, 0, 0, 122), text);
        draw->AddText(font, fontSize, pos + ImVec2(-1.f, 1.f), IM_COL32(0, 0, 0, 135), text);
        draw->AddText(font, fontSize, pos + ImVec2(1.f, -1.f), style::alpha(accent, .22f), text);
        draw->AddText(font, fontSize, pos, lerp(style::text, accent, .32f), text);
    }

    static void fov(ImDrawList* draw, ImVec2 center, float radius, const float color[4], bool fill, bool spin, int speed)
    {
        static float rotation = 0.f;
        if (spin)
            rotation += ImGui::GetIO().DeltaTime * ImMax(1, speed) * 1.4f;

        const ImU32 accent = fromfloat(color);
        if (fill)
            draw->AddCircleFilled(center, radius, alpha(accent, .10f), 96);

        for (int i = 0; i < 4; i++)
            draw->AddCircle(center, radius + 2.f + i * 2.5f, IM_COL32(0, 0, 0, 38 - i * 7), 96, 1.3f);

        draw->AddCircle(center, radius, alpha(accent, .94f), 96, 1.65f);
        draw->AddCircle(center, radius - 2.f, IM_COL32(255, 255, 255, 24), 96, 1.f);

        if (spin)
        {
            const float arc = IM_PI * .34f;
            for (int i = 0; i < 3; i++)
            {
                const float start = rotation + i * IM_PI * .666f;
                draw->PathArcTo(center, radius + 4.f, start, start + arc, 18);
                draw->PathStroke(i == 1 ? alpha(Purple, .72f) : alpha(accent, .78f), false, 2.1f);
            }
        }
    }

    static bool project(const sdk::vector3& world, ImVec2& out)
    {
        auto screen = global::render.screen(world);
        if (!validscreen(screen.x, screen.y))
            return false;

        out.x = std::roundf(screen.x);
        out.y = std::roundf(screen.y);
        return true;
    }

    static float dot(const sdk::vector3& a, const sdk::vector3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    static bool normalize(sdk::vector3& v)
    {
        const float mag = v.magnitude();
        if (mag < .001f || std::isnan(mag))
            return false;

        v = v / mag;
        return true;
    }

    static bool partpose(const sdk::instance& inst, sdk::vector3& position, sdk::matrix3& rotation)
    {
        if (!inst.Address)
            return false;

        sdk::part part(inst.Address);
        sdk::part primitive = part.primitive();
        if (!primitive.Address)
            return false;

        position = primitive.position();
        rotation = primitive.rotation();
        return !(std::isnan(position.x) || std::isnan(position.y) || std::isnan(position.z));
    }

    static bool playerposition(const sdk::player& player, sdk::vector3& out)
    {
        sdk::matrix3 ignored{};
        if (partpose(player.HumanoidRootPart, out, ignored))
            return true;
        if (partpose(player.LowerTorso, out, ignored))
            return true;
        if (partpose(player.Torso, out, ignored))
            return true;
        return partpose(player.Head, out, ignored);
    }

    static bool aimray(const sdk::player& player, sdk::vector3& origin, sdk::vector3& direction)
    {
        sdk::matrix3 rotation{};
        if (!partpose(player.Head, origin, rotation) &&
            !partpose(player.HumanoidRootPart, origin, rotation) &&
            !partpose(player.Torso, origin, rotation))
            return false;

        direction = rotation * sdk::vector3{ 0.f, 0.f, -1.f };
        return normalize(direction);
    }

    static bool aimingat(const sdk::player& player, const sdk::vector3& target, float* closestDistance = nullptr)
    {
        sdk::vector3 origin{};
        sdk::vector3 direction{};
        if (!aimray(player, origin, direction))
            return false;

        const sdk::vector3 toTarget = target - origin;
        const float along = dot(toTarget, direction);
        const float maxLength = ImClamp(global::overlay::AimView_MaxLength, 50.f, 1000.f);
        if (along < 0.f || along > maxLength)
            return false;

        const sdk::vector3 closest = origin + direction * along;
        const float miss = closest.distance(target);
        if (closestDistance)
            *closestDistance = miss;

        const float tolerance = ImClamp(toTarget.magnitude() * .018f, 4.0f, 10.0f);
        return miss <= tolerance;
    }

    static void aimline(ImDrawList* draw, const sdk::player& player, const sdk::vector3& localPos)
    {
        sdk::vector3 origin{};
        sdk::vector3 direction{};
        if (!aimray(player, origin, direction))
            return;

        const bool threat = aimingat(player, localPos);
        const float maxLength = ImClamp(global::overlay::AimView_MaxLength, 50.f, 1000.f);
        const sdk::vector3 end = origin + direction * maxLength;

        ImVec2 a{};
        ImVec2 b{};
        if (!project(origin, a) || !project(end, b))
            return;

        const ImU32 accent = threat
            ? IM_COL32(255, 80, 104, 245)
            : fromfloat(global::esp::color::aimline, .88f);

        draw->AddLine(a, b, IM_COL32(0, 0, 0, 150), threat ? 5.2f : 4.1f);
        draw->AddLine(a, b, alpha(Purple, threat ? .42f : .26f), threat ? 3.0f : 2.2f);
        draw->AddLine(a, b, accent, threat ? 1.85f : 1.35f);
        draw->AddCircleFilled(b, threat ? 3.4f : 2.6f, alpha(accent, .78f), 16);
    }

    struct trailpoint
    {
        sdk::vector3 world;
        float Time;
    };

    static std::unordered_map<std::uint64_t, std::deque<trailpoint>> PlayerTrails;
    static constexpr float TrailLifetime = 1.45f;
    static constexpr float TrailMinStep = 0.28f;
    static constexpr size_t TrailMaxPoints = 34;

    static void trimtrail(std::deque<trailpoint>& trail, float now)
    {
        while (!trail.empty() && now - trail.front().Time > TrailLifetime)
            trail.pop_front();

        while (trail.size() > TrailMaxPoints)
            trail.pop_front();
    }

    static void cleartrail()
    {
        PlayerTrails.clear();
    }

    static void prunetrail()
    {
        const float now = (float)ImGui::GetTime();
        for (auto it = PlayerTrails.begin(); it != PlayerTrails.end();)
        {
            trimtrail(it->second, now);
            if (it->second.empty())
                it = PlayerTrails.erase(it);
            else
                ++it;
        }
    }

    static bool bottom(const sdk::instance& instance, sdk::vector3& out)
    {
        if (!instance.Address)
            return false;

        const sdk::part part(instance.Address);
        const sdk::part primitive = part.primitive();
        if (!primitive.Address)
            return false;

        const sdk::vector3 position = primitive.position();
        const sdk::vector3 size = primitive.size();
        const sdk::matrix3 rotation = primitive.rotation();
        if (size.x == 0.f && size.y == 0.f && size.z == 0.f)
            return false;

        out = position - rotation * sdk::vector3{ 0.f, size.y * .5f, 0.f };
        return !(out.x == 0.f && out.y == 0.f && out.z == 0.f);
    }

    static bool trailanchor(const sdk::player& player, const std::vector<const sdk::instance*>& bones, sdk::vector3& out)
    {
        auto AveragePair = [&](const sdk::instance& left, const sdk::instance& right) -> bool {
            sdk::vector3 sum{};
            int count = 0;
            sdk::vector3 point{};

            if (bottom(left, point)) {
                sum += point;
                ++count;
            }
            if (bottom(right, point)) {
                sum += point;
                ++count;
            }

            if (count == 0)
                return false;

            out = sum / (float)count;
            return true;
            };

        if (AveragePair(player.LeftFoot, player.RightFoot))
            return true;

        if (AveragePair(player.LeftLeg, player.RightLeg))
            return true;

        sdk::vector3 lowest{};
        bool found = false;
        for (auto* part : bones)
        {
            sdk::vector3 point{};
            if (!bottom(*part, point))
                continue;

            if (!found || point.y < lowest.y)
            {
                lowest = point;
                found = true;
            }
        }

        if (found)
        {
            out = lowest;
            return true;
        }

        return bottom(player.HumanoidRootPart, out);
    }

    static bool trailanchor(const sdk::player& player, sdk::vector3& out)
    {
        return trailanchor(player, esp::bone(player), out);
    }

    static void trail(ImDrawList* draw, std::uint64_t key, const sdk::vector3& world)
    {
        if (!key)
            return;

        const float now = (float)ImGui::GetTime();
        auto& trail = PlayerTrails[key];
        trimtrail(trail, now);

        if (trail.empty() || trail.back().world.distance(world) >= TrailMinStep)
            trail.push_back({ world, now });
        else
            trail.back().Time = now;

        trimtrail(trail, now);
        if (trail.size() < 2)
            return;

        ImVec2 prev;
        bool prevValid = project(trail.front().world, prev);
        const float denom = ImMax(1.f, (float)trail.size() - 1.f);

        for (size_t i = 1; i < trail.size(); ++i)
        {
            ImVec2 current;
            const bool currentValid = project(trail[i].world, current);
            if (prevValid && currentValid)
            {
                const float order = (float)i / denom;
                const float life = 1.f - ImClamp((now - trail[i].Time) / TrailLifetime, 0.f, 1.f);
                const float alpha = life * (.18f + order * .82f);
                if (alpha > .035f)
                {
                    const float width = 1.05f + order * 2.35f;
                    draw->AddLine(prev, current, IM_COL32(0, 0, 0, (int)(142.f * alpha)), width + 3.0f);
                    draw->AddLine(prev, current, style::alpha(style::Purple, .35f * alpha), width + 1.15f);
                    draw->AddLine(prev, current, fromfloat(global::esp::color::Trails, .92f * alpha), width);

                    if (i == trail.size() - 1 || (i % 4) == 0)
                        draw->AddCircleFilled(current, width * .85f, fromfloat(global::esp::color::Trails, .45f * alpha), 12);
                }
            }

            prev = current;
            prevValid = currentValid;
        }
    }

    static void hat(ImDrawList* draw, const sdk::part& head)
    {
        if (!head.Address)
            return;

        const sdk::part primitive = head.primitive();
        if (!primitive.Address)
            return;

        const sdk::vector3 position = primitive.position();
        const sdk::vector3 size = primitive.size();
        const sdk::matrix3 rotation = primitive.rotation();
        if (size.x == 0.f && size.y == 0.f && size.z == 0.f)
            return;

        const float headWidth = ImMax(size.x, size.z);
        const float radius = ImClamp(headWidth * .96f, .58f, 2.35f);
        const float brimLift = ImMax(size.y * .54f, .38f);
        const float coneLift = brimLift + ImClamp(size.y * .72f, .42f, 1.15f);

        const sdk::vector3 brimCenter = position + rotation * sdk::vector3{ 0.f, brimLift, 0.f };
        const sdk::vector3 apex = position + rotation * sdk::vector3{ 0.f, coneLift, 0.f };

        ImVec2 apexScreen;
        if (!project(apex, apexScreen))
            return;

        constexpr int Segments = 40;
        ImVec2 brim[Segments]{};

        const float spin = (float)ImGui::GetTime() * .95f;
        for (int i = 0; i < Segments; ++i)
        {
            const float angle = spin + ((float)i / (float)Segments) * IM_PI * 2.f;
            const sdk::vector3 local{ cosf(angle) * radius, 0.f, sinf(angle) * radius };
            ImVec2 screen;
            if (!project(brimCenter + rotation * local, screen))
                return;
            brim[i] = screen;
        }

        const ImU32 accent = fromfloat(global::esp::color::hat, .94f);
        const ImU32 shadow = IM_COL32(0, 0, 0, 118);

        for (int i = 0; i < Segments; ++i)
        {
            const ImVec2& a = brim[i];
            const ImVec2& b = brim[(i + 1) % Segments];
            draw->AddTriangleFilled(apexScreen, a, b,
                fromfloat(global::esp::color::hat, (i & 1) ? .085f : .14f));
        }

        draw->AddConvexPolyFilled(brim, Segments, fromfloat(global::esp::color::hat, .075f));

        for (int i = 0; i < Segments; i += 5)
        {
            draw->AddLine(apexScreen, brim[i], IM_COL32(0, 0, 0, 78), 2.4f);
            draw->AddLine(apexScreen, brim[i], alpha(accent, .38f), 1.05f);
        }

        draw->AddPolyline(brim, Segments, shadow, true, 3.4f);
        draw->AddPolyline(brim, Segments, alpha(Purple, .30f), true, 2.05f);
        draw->AddPolyline(brim, Segments, accent, true, 1.2f);
        draw->AddCircleFilled(apexScreen, 2.25f, alpha(accent, .86f), 12);
    }
}

static void outline(const ImVec2& Pos, const char* text, const float Col[4]) {

    if (!text || !*text)
        return;

    ImDrawList* Draw = ImGui::GetBackgroundDrawList();
    Draw->Flags |= ImDrawListFlags_AntiAliasedLines | ImDrawListFlags_AntiAliasedFill;
    style::label(Draw, Pos, text, Col);
}

static const sdk::vector3 Corners[8] = {
    {-1,-1,-1}, {1,-1,-1}, {-1,1,-1}, {1,1,-1},
    {-1,-1, 1}, {1,-1, 1}, {-1,1, 1}, {1,1, 1}
};

namespace esp {

    std::vector<const sdk::instance*> bone(const sdk::player& player) {

        std::vector<const sdk::instance*> Parts;
        Parts.reserve(player.UpperTorso.Address && player.LowerTorso.Address ? 15 : 8);

        const bool R15 = player.UpperTorso.Address && player.LowerTorso.Address;
        const bool R6 = player.Torso.Address;

        if (R15) {
            if (player.Head.Address) Parts.push_back(&player.Head);
            if (player.UpperTorso.Address) Parts.push_back(&player.UpperTorso);
            if (player.LowerTorso.Address) Parts.push_back(&player.LowerTorso);
            if (player.LeftUpperArm.Address) Parts.push_back(&player.LeftUpperArm);
            if (player.LeftLowerArm.Address) Parts.push_back(&player.LeftLowerArm);
            if (player.LeftHand.Address)     Parts.push_back(&player.LeftHand);
            if (player.RightUpperArm.Address) Parts.push_back(&player.RightUpperArm);
            if (player.RightLowerArm.Address) Parts.push_back(&player.RightLowerArm);
            if (player.RightHand.Address)     Parts.push_back(&player.RightHand);
            if (player.LeftUpperLeg.Address) Parts.push_back(&player.LeftUpperLeg);
            if (player.LeftLowerLeg.Address) Parts.push_back(&player.LeftLowerLeg);
            if (player.LeftFoot.Address)     Parts.push_back(&player.LeftFoot);
            if (player.RightUpperLeg.Address) Parts.push_back(&player.RightUpperLeg);
            if (player.RightLowerLeg.Address) Parts.push_back(&player.RightLowerLeg);
            if (player.RightFoot.Address)     Parts.push_back(&player.RightFoot);
        }
        else if (R6) {
            if (player.Head.Address)  Parts.push_back(&player.Head);
            if (player.Torso.Address) Parts.push_back(&player.Torso);
            if (player.LeftArm.Address)  Parts.push_back(&player.LeftArm);
            if (player.RightArm.Address) Parts.push_back(&player.RightArm);
            if (player.LeftLeg.Address)  Parts.push_back(&player.LeftLeg);
            if (player.RightLeg.Address) Parts.push_back(&player.RightLeg);
        }
        else {
            for (const auto& Bone : player.Bones) {
                if (Bone.Address)
                    Parts.push_back(&Bone);
            }
            if (Parts.empty()) {
                if (player.HumanoidRootPart.Address) Parts.push_back(&player.HumanoidRootPart);
                if (player.Head.Address)             Parts.push_back(&player.Head);
                if (player.Torso.Address)            Parts.push_back(&player.Torso);
                if (player.UpperTorso.Address)       Parts.push_back(&player.UpperTorso);
                if (player.LowerTorso.Address)       Parts.push_back(&player.LowerTorso);
            }
        }
        return Parts;
    }

    void run()
    {
        if (global::render.Address == 0 || global::model.Address == 0)
            return;

        visual_frame::begin();

        ImDrawList* Draw = ImGui::GetBackgroundDrawList();
        Draw->Flags |= ImDrawListFlags_AntiAliasedLines | ImDrawListFlags_AntiAliasedFill;

        if (!global::esp::Enabled || !global::esp::Trails)
            style::cleartrail();
        else
            style::prunetrail();

        const auto Snapshot = cache::snapshot();
        const auto& Local = global::LocalPlayer;
        sdk::vector3 LocalPos{};
        const bool HasLocalPos = style::playerposition(Local, LocalPos);
        style::VisibilityTint = false;

        for (auto& player : Snapshot)
        {
            if (!global::esp::Enabled || !player.character.Address)
                continue;

            if (Local.character.Address && player.character.Address == Local.character.Address)
                continue;

            if (global::esp::Render_Distance > 0.f && player.Distance > global::esp::Render_Distance)
                continue;

            if (global::aim::DeadCheck)
            {
                sdk::instance BodyEffects = player.character.child("BodyEffects");
                if (BodyEffects.Address)
                {
                    sdk::instance Ko = BodyEffects.child("K.O");
                    if (Ko.Address && drive->read<bool>(Ko.Address + offset::misc::Value))
                        continue;
                }
            }

            sdk::part Head(player.Head.Address);
            if (!Head.Address) continue;

            const sdk::part HeadPrimitive = Head.primitive();
            if (!HeadPrimitive.Address)
                continue;

            const sdk::vector3 HeadPosition = HeadPrimitive.position();
            auto Head_W2S = global::render.screen(HeadPosition);

            if (!validscreen(Head_W2S.x, Head_W2S.y))
                continue;

            const bool PlayerVisible = !global::esp::VisibleCheck || visiblecheck(HeadPosition, Head_W2S);
            style::VisibilityTint = global::esp::VisibleCheck;
            style::TargetVisible = PlayerVisible;

            float Left = FLT_MAX, Top = FLT_MAX, Right = -FLT_MAX, Bottom = -FLT_MAX;
            bool valid = false;

            auto Bones = esp::bone(player);
            if (Bones.empty()) continue;

            for (auto* Inst : Bones) {
                if (!Inst || !Inst->Address) continue;

                const auto part = sdk::part(Inst->Address);
                const auto primitive = part.primitive();

                sdk::vector3 Size = primitive.size();
                const auto Position = primitive.position();
                const auto Rotation = primitive.rotation();

                if (global::GameID == 292439477)
                {
                    std::string name = part.name();
                    if (name.find("Other_") != std::string::npos) {
                        Size = { 1.f, 2.f, 1.f };
                    }
                    else if (name == "Head") {
                        Size = { 1.f, 1.f, 1.f };
                    }
                    else if (name == "Torso") {
                        Size = { 2.f, 2.f, 1.f };
                    }
                }

                if (Size.x == 0.f && Size.y == 0.f && Size.z == 0.f)
                    continue;

                for (const auto& LocalCorners : Corners) {
                    sdk::vector3 Offset{
                        LocalCorners.x * Size.x * 0.5f,
                        LocalCorners.y * Size.y * 0.5f,
                        LocalCorners.z * Size.z * 0.5f
                    };

                    sdk::vector3 world = Position + Rotation * Offset;
                    auto W2S = global::render.screen(world);

                    if (!validscreen(W2S.x, W2S.y)) continue;

                    valid = true;
                    Left = min(Left, W2S.x);
                    Top = min(Top, W2S.y);
                    Right = max(Right, W2S.x);
                    Bottom = max(Bottom, W2S.y);
                }
            }

            if (!valid || Left >= Right || Top >= Bottom) continue;

            ImVec2 Pos(Left - 1.f, Top - 1.f);
            ImVec2 Size((Right - Left) + 2.f, (Bottom - Top) + 2.f);

            if (global::esp::aimline && HasLocalPos)
                style::aimline(Draw, player, LocalPos);

            if (global::esp::Trails)
            {
                sdk::vector3 TrailAnchor;
                if (style::trailanchor(player, Bones, TrailAnchor))
                    style::trail(Draw, player.character.Address, TrailAnchor);
            }

            if (global::esp::Box)
            {
                if (global::esp::Box_Type == 0) {

                    Pos.x = std::round(Pos.x);
                    Pos.y = std::round(Pos.y);
                    Size.x = std::round(Size.x);
                    Size.y = std::round(Size.y);

                    ImVec2 Min = Pos;
                    ImVec2 Max = ImVec2(Pos.x + Size.x, Pos.y + Size.y);

                    style::fullbox(Draw, Min, Max);
                }
                else if (global::esp::Box_Type == 1) {

                    float X1 = Pos.x - 1.f;
                    float Y1 = Pos.y - 1.f;
                    float X2 = Pos.x + Size.x + 1.f;
                    float Y2 = Pos.y + Size.y + 1.f;

                    style::cornerbox(Draw, ImVec2(X1, Y1), ImVec2(X2, Y2));
                }
            }

            if (global::esp::Chinese_Hat)
                style::hat(Draw, Head);

            if (global::esp::Healthbar) {
                float Ratio = (player.MaxHealth > 0.f) ? player.Health / player.MaxHealth : 0.f;
                style::healthbar(Draw, Pos, Size, Ratio);
            }

            if (global::esp::Health)
            {
                std::string HealthStr = "[" + std::to_string(static_cast<int>(player.Health)) + "]";
                float X_Text = Pos.x - 6.0f;
                if (global::esp::Healthbar)
                    X_Text -= global::esp::Thickness + global::esp::gap;
                float Y_text = Pos.y - 3.0f;
                ImGui::PushFont(Tahoma_BoldXP);
                ImVec2 Text_Size = ImGui::CalcTextSize(HealthStr.c_str());
                ImGui::PopFont();
                ImVec2 Text_Pos(X_Text - Text_Size.x, Y_text);
                outline(Text_Pos, HealthStr.c_str(), global::esp::color::Health);
            }

            if (global::esp::name) {
                if (global::esp::Name_Type == 0) {
                    ImGui::PushFont(Tahoma_BoldXP);
                    ImVec2 Text_Size = ImGui::CalcTextSize(player.name.c_str());
                    ImGui::PopFont();
                    ImVec2 Text_Position(Pos.x + (Size.x * 0.5f) - (Text_Size.x * 0.5f), Pos.y - Text_Size.y - 3.f);
                    outline(Text_Position, player.name.c_str(), global::esp::color::name);
                }
                else if (global::esp::Name_Type == 1) {
                    ImGui::PushFont(Tahoma_BoldXP);
                    ImVec2 Text_Size = ImGui::CalcTextSize(player.Display_Name.c_str());
                    ImGui::PopFont();
                    ImVec2 Text_Position(Pos.x + (Size.x * 0.5f) - (Text_Size.x * 0.5f), Pos.y - Text_Size.y - 3.f);
                    outline(Text_Position, player.Display_Name.c_str(), global::esp::color::name);
                }
                else if (global::esp::Name_Type == 2) {
                    std::string text = player.name + " [" + player.Display_Name + "]";
                    ImGui::PushFont(Tahoma_BoldXP);
                    ImVec2 Text_Size = ImGui::CalcTextSize(text.c_str());
                    float NameW = ImGui::CalcTextSize((player.name + " ").c_str()).x;
                    float BracketW = ImGui::CalcTextSize("[").x;
                    float DisplayW = ImGui::CalcTextSize(player.Display_Name.c_str()).x;
                    ImGui::PopFont();
                    ImVec2 Position(Pos.x + (Size.x * 0.5f) - (Text_Size.x * 0.5f), Pos.y - Text_Size.y - 3.f);
                    outline(Position, player.name.c_str(), global::esp::color::name);
                    outline(ImVec2(Position.x + NameW, Position.y - 2.f), "[", global::esp::color::name);
                    static float white[4] = { 1.f, 1.f, 1.f, 1.f };
                    outline(ImVec2(Position.x + NameW + BracketW, Position.y - 1.f), player.Display_Name.c_str(), white);
                    outline(ImVec2(Position.x + NameW + BracketW + DisplayW, Position.y - 2.f), "]", global::esp::color::name);
                }
            }

            if (global::esp::Distance) {
                ImGui::PushFont(Tahoma_BoldXP);
                char Buffer[16];
                snprintf(Buffer, sizeof(Buffer), "[%dm]", static_cast<int>(player.Distance));
                ImVec2 Text_Size = ImGui::CalcTextSize(Buffer);
                ImGui::PopFont();
                ImVec2 Text_Position(Pos.x + (Size.x * 0.5f) - (Text_Size.x * 0.5f), Pos.y + Size.y + 3.0f);
                outline(Text_Position, Buffer, global::esp::color::Distance);
            }

            if (global::esp::Rig_Type)
            {
                ImGui::PushFont(Tahoma_BoldXP);
                const char* Rig_Type = nullptr;
                if (player.Rig_Type == 1)
                    Rig_Type = "[R15]";
                else if (player.Rig_Type == 0)
                    Rig_Type = "[R6]";
                else { ImGui::PopFont(); continue; }
                ImVec2 Text_Size = ImGui::CalcTextSize(Rig_Type);
                ImVec2 Text_Position(std::round(Pos.x + Size.x + 5.0f), std::round(Pos.y - Text_Size.y + 10.0f));
                outline(Text_Position, Rig_Type, global::esp::color::Rig_Type);
                ImGui::PopFont();
            }

            if (global::esp::tool)
            {
                std::string Cl_Name;
                const std::string& Tool_Name = player.Tool_Name;
                if (Tool_Name.empty()) {
                    Cl_Name = "[None]";
                }
                else {
                    Cl_Name.reserve(Tool_Name.size());
                    for (char c : Tool_Name) {
                        if (c != '[' && c != ']')
                            Cl_Name.push_back(c);
                    }
                    if (!Cl_Name.empty()) {
                        Cl_Name.insert(Cl_Name.begin(), '[');
                        Cl_Name.push_back(']');
                    }
                }
                ImGui::PushFont(Tahoma_BoldXP);
                ImVec2 Text_Size = ImGui::CalcTextSize(Cl_Name.c_str());
                ImGui::PopFont();
                float Offset = global::esp::Distance ? 18.0f : 3.0f;
                ImVec2 Text_Position(std::round(Pos.x + (Size.x * 0.5f) - (Text_Size.x * 0.5f)), std::round(Pos.y + Size.y + Offset));
                outline(Text_Position, Cl_Name.c_str(), global::esp::color::tool);
            }

            if (global::esp::Chams) {
                ImDrawList* Draw = ImGui::GetBackgroundDrawList();
                Draw->Flags |= ImDrawListFlags_AntiAliasedLines | ImDrawListFlags_AntiAliasedFill;

                auto ProjectPart = [&](const sdk::part& part) -> std::vector<ImVec2> {
                    std::vector<ImVec2> Projected;
                    if (!part.Address) return Projected;
                    const auto Prim = part.primitive();
                    const auto Size = Prim.size();
                    const auto Pos = Prim.position();
                    const auto Rot = Prim.rotation();
                    Projected.reserve(8);
                    for (int i = 0; i < 8; ++i) {
                        const auto& Corner = Corners[i];
                        sdk::vector3 world = Pos + Rot * sdk::vector3{ Corner.x * Size.x * 0.5f, Corner.y * Size.y * 0.5f, Corner.z * Size.z * 0.5f };
                        sdk::vector2 Screen = global::render.screen(world);
                        if (Screen.x > -0.5f && Screen.y > -0.5f)
                            Projected.emplace_back(Screen.x, Screen.y);
                    }
                    if (Projected.size() < 3) return {};
                    std::sort(Projected.begin(), Projected.end(), [](const ImVec2& A, const ImVec2& B) {
                        return A.x < B.x || (A.x == B.x && A.y < B.y);
                        });
                    std::vector<ImVec2> Hull;
                    Hull.reserve(Projected.size() * 2);
                    auto cross2 = [](const ImVec2& O, const ImVec2& A, const ImVec2& B) {
                        return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);
                        };
                    for (auto& P : Projected) {
                        while (Hull.size() >= 2 && cross2(Hull[Hull.size() - 2], Hull.back(), P) <= 0)
                            Hull.pop_back();
                        Hull.push_back(P);
                    }
                    size_t T = Hull.size() + 1;
                    for (int i = (int)Projected.size() - 1; i >= 0; --i) {
                        auto& P = Projected[i];
                        while (Hull.size() >= T && cross2(Hull[Hull.size() - 2], Hull.back(), P) <= 0)
                            Hull.pop_back();
                        Hull.push_back(P);
                    }
                    Hull.pop_back();
                    return Hull;
                    };

                Clipper2Lib::Paths64 AllParts;
                AllParts.reserve(Bones.size());
                for (auto* Inst : Bones) {
                    const sdk::part part(Inst->Address);
                    auto Hull = ProjectPart(part);
                    if (Hull.size() < 3) continue;
                    Clipper2Lib::Path64 Path;
                    Path.reserve(Hull.size());
                    for (auto& Pt : Hull) Path.emplace_back(static_cast<int64_t>(Pt.x * 1000.0), static_cast<int64_t>(Pt.y * 1000.0));
                    AllParts.push_back(std::move(Path));
                }

                if (!AllParts.empty()) {
                    auto UnifiedSolution = Clipper2Lib::Union(AllParts, Clipper2Lib::FillRule::NonZero);
                    std::vector<std::vector<ImVec2>> AllPolys;
                    AllPolys.reserve(UnifiedSolution.size());
                    for (auto& Sp : UnifiedSolution) {
                        std::vector<ImVec2> Poly;
                        Poly.reserve(Sp.size());
                        for (auto& Pt : Sp) Poly.emplace_back(Pt.x / 1000.0f, Pt.y / 1000.0f);
                        if (Poly.size() >= 3) AllPolys.push_back(std::move(Poly));
                    }
                    ImU32 FillColor = style::fromfloat(global::esp::color::Chams, .52f);
                    ImU32 OutlineColor = style::fromfloat(global::esp::color::ChamsOutline, .72f);
                    if (global::esp::ChamsFade) {
                        float time = (float)ImGui::GetTime() * global::esp::ChamsFadeSpeed;
                        float t = (sinf(time) + 1.0f) * 0.5f;
                        ImVec4 c = ImGui::ColorConvertU32ToFloat4(FillColor);
                        c.w *= t;
                        FillColor = ImGui::ColorConvertFloat4ToU32(c);
                    }
                    for (auto& Poly : AllPolys)
                        Draw->AddConcavePolyFilled(Poly.data(), (int)Poly.size(), FillColor);
                    for (auto& Poly : AllPolys)
                    {
                        Draw->AddPolyline(Poly.data(), (int)Poly.size(), IM_COL32(0, 0, 0, 96), true, 3.2f);
                        Draw->AddPolyline(Poly.data(), (int)Poly.size(), OutlineColor, true, 1.35f);
                    }
                }
            }

            if (global::esp::Skeleton) {
                const ImU32 SkelCol = style::fromfloat(global::esp::color::Skeleton);
                const ImU32 OutlineCol = IM_COL32(0, 0, 0, 132);
                const float Thickness = 1.45f;

                auto W2S = [&](const sdk::vector3& WorldPos, ImVec2& Out) -> bool {
                    auto ScreenPos = global::render.screen(WorldPos);
                    if (!validscreen(ScreenPos.x, ScreenPos.y)) return false;
                    Out.x = std::roundf(ScreenPos.x);
                    Out.y = std::roundf(ScreenPos.y);
                    return true;
                    };

                auto DrawPoly = [&](const ImVec2* Points, int Count) {
                    if (Count < 2) return;
                    Draw->AddPolyline(Points, Count, OutlineCol, false, Thickness + 3.f);
                    Draw->AddPolyline(Points, Count, style::alpha(SkelCol, .35f), false, Thickness + 1.2f);
                    Draw->AddPolyline(Points, Count, SkelCol, false, Thickness);
                    };

                if (player.UpperTorso.Address && player.LowerTorso.Address)
                {
                    auto ProcessR15Chain = [&](const sdk::instance* Instances, int Count) {
                        ImVec2 ScreenPoints[4];
                        int ValidCount = 0;
                        for (int i = 0; i < Count; ++i) {
                            if (!Instances[i].Address) { DrawPoly(ScreenPoints, ValidCount); ValidCount = 0; continue; }
                            sdk::part part(Instances[i].Address);
                            if (!part.Address) { DrawPoly(ScreenPoints, ValidCount); ValidCount = 0; continue; }
                            ImVec2 ScreenPos;
                            if (!W2S(part.primitive().position(), ScreenPos)) { DrawPoly(ScreenPoints, ValidCount); ValidCount = 0; continue; }
                            ScreenPoints[ValidCount++] = ScreenPos;
                        }
                        DrawPoly(ScreenPoints, ValidCount);
                        };

                    const sdk::instance Spine[] = { player.Head, player.UpperTorso, player.LowerTorso };
                    const sdk::instance LeftArm[] = { player.UpperTorso, player.LeftUpperArm,  player.LeftLowerArm,  player.LeftHand };
                    const sdk::instance RightArm[] = { player.UpperTorso, player.RightUpperArm, player.RightLowerArm, player.RightHand };
                    const sdk::instance LeftLeg[] = { player.LowerTorso, player.LeftUpperLeg,  player.LeftLowerLeg,  player.LeftFoot };
                    const sdk::instance RightLeg[] = { player.LowerTorso, player.RightUpperLeg, player.RightLowerLeg, player.RightFoot };

                    ProcessR15Chain(Spine, 3);
                    ProcessR15Chain(LeftArm, 4);
                    ProcessR15Chain(RightArm, 4);
                    ProcessR15Chain(LeftLeg, 4);
                    ProcessR15Chain(RightLeg, 4);
                }
                else if (player.Torso.Address && player.Head.Address)
                {
                    sdk::part TorsoPart(player.Torso.Address);
                    sdk::part HeadPart(player.Head.Address);
                    const auto& TorsoPrim = TorsoPart.primitive();
                    const auto& HeadPrim = HeadPart.primitive();
                    const sdk::vector3 TorsoPos = TorsoPrim.position();
                    const sdk::vector3 TorsoSize = TorsoPrim.size();
                    const auto TorsoRot = TorsoPrim.rotation();
                    const sdk::vector3 HeadPos = HeadPrim.position();
                    const sdk::vector3 HeadSize = HeadPrim.size();
                    const sdk::vector3 ShoulderCenter = TorsoPos + TorsoRot * sdk::vector3{ 0, TorsoSize.y * 0.2f,  0 };
                    const sdk::vector3 HipCenter = TorsoPos - TorsoRot * sdk::vector3{ 0, TorsoSize.y * 0.4f,  0 };
                    const sdk::vector3 HeadBottom = HeadPos - sdk::vector3{ 0, HeadSize.y * 0.5f, 0 };
                    const sdk::vector3 ShoulderLeft = ShoulderCenter + TorsoRot * sdk::vector3{ -TorsoSize.x * 0.5f, 0, 0 };
                    const sdk::vector3 ShoulderRight = ShoulderCenter + TorsoRot * sdk::vector3{ TorsoSize.x * 0.5f, 0, 0 };

                    auto ProcessR6Chain = [&](const sdk::vector3* Points, int Count) {
                        ImVec2 ScreenPoints[8];
                        int ValidCount = 0;
                        for (int i = 0; i < Count; ++i) {
                            ImVec2 ScreenPos;
                            if (W2S(Points[i], ScreenPos))
                                ScreenPoints[ValidCount++] = ScreenPos;
                        }
                        DrawPoly(ScreenPoints, ValidCount);
                        };

                    { const sdk::vector3 SpinePts[] = { HeadPos, HeadBottom, ShoulderCenter, HipCenter }; ProcessR6Chain(SpinePts, 4); }

                    auto BuildArmChain = [&](const sdk::vector3& Shoulder, const sdk::instance& ArmInst, sdk::vector3* Out, int& Count) {
                        Out[Count++] = ShoulderCenter;
                        Out[Count++] = Shoulder;
                        if (ArmInst.Address) {
                            sdk::part Arm(ArmInst.Address);
                            const auto& P = Arm.primitive();
                            const sdk::vector3 AP = P.position();
                            const sdk::vector3 AS = P.size();
                            const auto         AR = P.rotation();
                            Out[Count++] = AP + AR * sdk::vector3{ 0, AS.y * 0.2f,  0 };
                            Out[Count++] = AP - AR * sdk::vector3{ 0, AS.y * 0.5f,  0 };
                        }
                        };

                    { sdk::vector3 Pts[4]; int C = 0; BuildArmChain(ShoulderLeft, player.LeftArm, Pts, C); ProcessR6Chain(Pts, C); }
                    { sdk::vector3 Pts[4]; int C = 0; BuildArmChain(ShoulderRight, player.RightArm, Pts, C); ProcessR6Chain(Pts, C); }

                    auto BuildLegChain = [&](const sdk::instance& LegInst, sdk::vector3* Out, int& Count) {
                        Out[Count++] = HipCenter;
                        if (LegInst.Address) {
                            sdk::part Leg(LegInst.Address);
                            const auto& P = Leg.primitive();
                            const sdk::vector3 LP = P.position();
                            const sdk::vector3 LS = P.size();
                            const auto         LR = P.rotation();
                            Out[Count++] = LP + LR * sdk::vector3{ 0, LS.y * 0.5f,  0 };
                            Out[Count++] = LP - LR * sdk::vector3{ 0, LS.y * 0.5f,  0 };
                        }
                        };

                    { sdk::vector3 Pts[3]; int C = 0; BuildLegChain(player.LeftLeg, Pts, C); ProcessR6Chain(Pts, C); }
                    { sdk::vector3 Pts[3]; int C = 0; BuildLegChain(player.RightLeg, Pts, C); ProcessR6Chain(Pts, C); }
                }
            }
        }

        style::VisibilityTint = false;

    }
}
