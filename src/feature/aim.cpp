#define NOMINMAX
#include <Windows.h>
#include <thread>
#include <cmath>
#include <cfloat>
#include <string>
#include <algorithm>
#include <utility>
#include <vector>

#include "feature/aim.h"
#include "feature/cache.h"
#include "engine/engine.h"
#include "global.h"
#include "engine/offset.h"
#include "feature/wallcheck.h"

namespace aim {

    static bool knocked(sdk::player& player)
    {
        if (player.character.Address == 0)
            return false;

        sdk::instance BodyEffects = player.character.child("BodyEffects");
        if (BodyEffects.Address == 0)
            return false;

        sdk::instance Ko = BodyEffects.child("K.O");
        if (Ko.Address == 0)
            return false;

        return drive->read<bool>(Ko.Address + offset::misc::Value);
    }

    static bool visible(const sdk::camera& Cam, const sdk::vector3& TargetPos, const sdk::vector2& ScreenPos, std::uintptr_t skip_char = 0)
    {
        HWND Window = FindWindowA(0, "Roblox");
        RECT ClientRect{};
        if (!Window || !GetClientRect(Window, &ClientRect))
            return false;

        if (!(ScreenPos.x >= 0.f && ScreenPos.y >= 0.f &&
            ScreenPos.x <= (float)ClientRect.right &&
            ScreenPos.y <= (float)ClientRect.bottom))
            return false;

        return wallcheck::can_see(Cam.position(), TargetPos, skip_char);
    }

    static float auto_head_radius(sdk::player& player)
    {
        if (!player.Head.Address)
            return 0.f;
        sdk::part head(player.Head.Address);
        sdk::vector3 pos = head.partposition();
        if (std::isnan(pos.x) || std::isnan(pos.y) || std::isnan(pos.z))
            return 0.f;

        sdk::part prim = head.primitive();
        if (!prim.Address)
            return 0.f;

        sdk::vector3 sz = prim.size();
        sdk::matrix3 rot = prim.rotation();
        sdk::vector2 center = global::render.screen(pos);
        if (center.x <= -0.5f || center.y <= -0.5f)
            return 0.f;

        float hx = std::abs(sz.x) * 0.5f;
        float hy = std::abs(sz.y) * 0.5f;
        float hz = std::abs(sz.z) * 0.5f;

        sdk::vector3 corners[8] = {
            pos + sdk::vector3{ rot.data[0]*hx + rot.data[3]*hy + rot.data[6]*hz,
                                rot.data[1]*hx + rot.data[4]*hy + rot.data[7]*hz,
                                rot.data[2]*hx + rot.data[5]*hy + rot.data[8]*hz },
            pos + sdk::vector3{-rot.data[0]*hx + rot.data[3]*hy + rot.data[6]*hz,
                               -rot.data[1]*hx + rot.data[4]*hy + rot.data[7]*hz,
                               -rot.data[2]*hx + rot.data[5]*hy + rot.data[8]*hz },
            pos + sdk::vector3{ rot.data[0]*hx - rot.data[3]*hy + rot.data[6]*hz,
                                rot.data[1]*hx - rot.data[4]*hy + rot.data[7]*hz,
                                rot.data[2]*hx - rot.data[5]*hy + rot.data[8]*hz },
            pos + sdk::vector3{-rot.data[0]*hx - rot.data[3]*hy + rot.data[6]*hz,
                               -rot.data[1]*hx - rot.data[4]*hy + rot.data[7]*hz,
                               -rot.data[2]*hx - rot.data[5]*hy + rot.data[8]*hz },
            pos + sdk::vector3{ rot.data[0]*hx + rot.data[3]*hy - rot.data[6]*hz,
                                rot.data[1]*hx + rot.data[4]*hy - rot.data[7]*hz,
                                rot.data[2]*hx + rot.data[5]*hy - rot.data[8]*hz },
            pos + sdk::vector3{-rot.data[0]*hx + rot.data[3]*hy - rot.data[6]*hz,
                               -rot.data[1]*hx + rot.data[4]*hy - rot.data[7]*hz,
                               -rot.data[2]*hx + rot.data[5]*hy - rot.data[8]*hz },
            pos + sdk::vector3{ rot.data[0]*hx - rot.data[3]*hy - rot.data[6]*hz,
                                rot.data[1]*hx - rot.data[4]*hy - rot.data[7]*hz,
                                rot.data[2]*hx - rot.data[5]*hy - rot.data[8]*hz },
            pos + sdk::vector3{-rot.data[0]*hx - rot.data[3]*hy - rot.data[6]*hz,
                               -rot.data[1]*hx - rot.data[4]*hy - rot.data[7]*hz,
                               -rot.data[2]*hx - rot.data[5]*hy - rot.data[8]*hz }
        };

        float maxDistSq = 0.f;
        for (int i = 0; i < 8; i++) {
            if (std::isnan(corners[i].x) || std::isnan(corners[i].y) || std::isnan(corners[i].z))
                continue;
            sdk::vector2 sc = global::render.screen(corners[i]);
            float dx = sc.x - center.x;
            float dy = sc.y - center.y;
            float d = dx * dx + dy * dy;
            if (d > maxDistSq) maxDistSq = d;
        }

        float r = std::sqrt(maxDistSq) * global::aim::AutoHeadScale;
        if (global::aim::FovCheck)
            r = (std::min)(r, global::aim::TriggerRadius);
        return r;
    }

    static bool bounds_check(sdk::player& player, float cx, float cy, float base_radius)
    {
        if (global::aim::DistanceScale)
        {
            float r = auto_head_radius(player);
            if (r <= 0.f) return false;
            sdk::part h(player.Head.Address);
            sdk::vector2 sc = global::render.screen(h.partposition());
            float dx = sc.x - cx, dy = sc.y - cy;
            return dx * dx + dy * dy <= r * r;
        }

        struct HitEntry { sdk::instance inst; float scale; };
        std::vector<HitEntry> Parts;
        if (global::aim::HitPart_Head && player.Head.Address) Parts.push_back({ player.Head, global::aim::HitPart_Head_Scale });
        if (global::aim::HitPart_Torso && player.Torso.Address) Parts.push_back({ player.Torso, global::aim::HitPart_Torso_Scale });
        if (global::aim::HitPart_UpperTorso && player.UpperTorso.Address) Parts.push_back({ player.UpperTorso, global::aim::HitPart_UpperTorso_Scale });
        if (global::aim::HitPart_LowerTorso && player.LowerTorso.Address) Parts.push_back({ player.LowerTorso, global::aim::HitPart_LowerTorso_Scale });
        if (global::aim::HitPart_HumanoidRootPart && player.HumanoidRootPart.Address) Parts.push_back({ player.HumanoidRootPart, global::aim::HitPart_HumanoidRootPart_Scale });
        if (global::aim::HitPart_LeftArm && player.LeftArm.Address) Parts.push_back({ player.LeftArm, global::aim::HitPart_LeftArm_Scale });
        if (global::aim::HitPart_RightArm && player.RightArm.Address) Parts.push_back({ player.RightArm, global::aim::HitPart_RightArm_Scale });
        if (global::aim::HitPart_LeftLeg && player.LeftLeg.Address) Parts.push_back({ player.LeftLeg, global::aim::HitPart_LeftLeg_Scale });
        if (global::aim::HitPart_RightLeg && player.RightLeg.Address) Parts.push_back({ player.RightLeg, global::aim::HitPart_RightLeg_Scale });

        if (Parts.empty())
            return false;

        for (const auto& entry : Parts)
        {
            sdk::part part(entry.inst.Address);
            sdk::vector3 pos = part.partposition();
            if (std::isnan(pos.x) || std::isnan(pos.y) || std::isnan(pos.z))
                continue;

            sdk::vector2 screen = global::render.screen(pos);
            if (screen.x <= -0.5f || screen.y <= -0.5f)
                continue;

            float radius = global::aim::FovCheck ? (base_radius * entry.scale) : entry.scale;

            const float dx = screen.x - cx;
            const float dy = screen.y - cy;
            if ((dx * dx + dy * dy) <= (radius * radius))
                return true;
        }
        return false;
    }

    void triggerbot()
    {
        if (!global::aim::TriggerBot || !global::render.Address)
            return;

        int Vk = ImGuiKeyToVK(global::aim::TriggerBot_Key);
        if (!Vk) return;

        HWND RobloxHwnd = FindWindowA(0, "Roblox");
        bool RobloxFocused = RobloxHwnd && GetForegroundWindow() == RobloxHwnd;
        bool Pressed = RobloxFocused && (GetAsyncKeyState(Vk) & 0x8000) != 0;

        static bool Toggled = false;
        static bool LastPressed = false;

        bool active = false;
        if (global::aim::TriggerBot_Mode == ImKeyBindMode_Toggle) {
            if (Pressed && !LastPressed)
                Toggled = !Toggled;
            active = Toggled;
        } else {
            active = Pressed;
        }
        LastPressed = Pressed;

        if (!active) return;

        static ULONGLONG LastShot = 0;
        POINT CursorPos;
        if (!RobloxHwnd || !GetCursorPos(&CursorPos) || !ScreenToClient(RobloxHwnd, &CursorPos))
            return;

        const sdk::vector2 Cursor{ (float)CursorPos.x, (float)CursorPos.y };
        const float Padding = std::max(0.f, global::aim::TriggerRadius);

        auto PlayersSnapshot = cache::snapshot();
        for (auto& Plr : PlayersSnapshot) {
            if (Plr.Local_Player ||
                (global::LocalPlayer.character.Address != 0 && Plr.character.Address == global::LocalPlayer.character.Address) ||
                !Plr.character.Address || !Plr.Head.Address)
                continue;

            if (!bounds_check(Plr, Cursor.x, Cursor.y, Padding))
                continue;

            if (global::aim::DeadCheck && knocked(Plr))
                continue;

            if (global::aim::VisibleCheck)
            {
                sdk::vector3 TargetPos;
                sdk::part HeadPart(Plr.Head.Address);
                TargetPos = HeadPart.partposition();
                if (!visible(global::camera, TargetPos, Cursor, Plr.character.Address))
                    continue;
            }

            const ULONGLONG Now = GetTickCount64();
            if (global::aim::IntervalToggle && Now - LastShot < (ULONGLONG)std::max(0, global::aim::TriggerDelayMs))
                return;

            INPUT Input[2]{};
            Input[0].type = INPUT_MOUSE;
            Input[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            Input[1].type = INPUT_MOUSE;
            Input[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(2, Input, sizeof(INPUT));
            LastShot = Now;
            return;
        }
    }

    void run() {
        std::thread([]() {
            while (true) {
                triggerbot();
                Sleep(global::aim::TriggerBot ? 1 : 10);
            }
            }).detach();
    }

}
