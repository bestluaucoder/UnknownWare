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
        if (BodyEffects.Address)
        {
            sdk::instance Ko = BodyEffects.child("K.O");
            if (Ko.Address)
                return drive->read<bool>(Ko.Address + offset::misc::Value);
        }

        if (player.humanoid.Address)
        {
            sdk::humanoid hum(player.humanoid.Address);
            return hum.health() <= 0.f;
        }

        return false;
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
                !Plr.character.Address || (!Plr.Head.Address && !Plr.HumanoidRootPart.Address))
                continue;

            if (!bounds_check(Plr, Cursor.x, Cursor.y, Padding))
                continue;

            if (global::aim::DeadCheck && knocked(Plr))
                continue;

            if (global::aim::VisibleCheck)
            {
                std::uintptr_t target_part = Plr.Head.Address ? Plr.Head.Address : Plr.HumanoidRootPart.Address;
                sdk::part HitPart(target_part);
                sdk::vector3 TargetPos = HitPart.partposition();
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

    static bool aim_valid(sdk::player& plr)
    {
        if (plr.Local_Player) return false;
        if (global::LocalPlayer.character.Address != 0 &&
            plr.character.Address == global::LocalPlayer.character.Address) return false;
        if (!plr.character.Address || (!plr.Head.Address && !plr.HumanoidRootPart.Address)) return false;
        if (global::aimbot::DeadCheck && knocked(plr)) return false;
        if (global::aimbot::TeamCheck)
        {
            uint64_t localT = global::actor.local().teamid();
            if (localT != 0)
            {
                sdk::actor e(plr.player.Address);
                if (e.teamid() == localT) return false;
            }
        }
        return true;
    }

    static sdk::vector2 aim_target_pos(sdk::player& plr)
    {
        sdk::instance part;
        switch (global::aimbot::Hitbox) {
        case 0: part = plr.Head; break;
        case 1: part = plr.Torso; break;
        case 2: part = plr.UpperTorso; break;
        case 3: part = plr.LowerTorso; break;
        case 4: part = plr.HumanoidRootPart; break;
        case 5:
        {
            int r = rand() % 5;
            switch (r) {
            case 0: part = plr.Head; break;
            case 1: part = plr.Torso; break;
            case 2: part = plr.UpperTorso; break;
            case 3: part = plr.LowerTorso; break;
            case 4: part = plr.HumanoidRootPart; break;
            }
            break;
        }
        default: part = plr.Head; break;
        }
        if (!part.Address && plr.Head.Address) part = plr.Head;
        if (!part.Address && plr.HumanoidRootPart.Address) part = plr.HumanoidRootPart;
        if (!part.Address) return { -1.f, -1.f };
        sdk::part p(part.Address);
        sdk::vector3 wp = p.partposition();
        if (std::isnan(wp.x) || std::isnan(wp.y) || std::isnan(wp.z))
            return { -1.f, -1.f };

        if (global::aimbot::Prediction && plr.HumanoidRootPart.Address)
        {
            sdk::part root(plr.HumanoidRootPart.Address);
            sdk::vector3 vel = root.velocity();
            if (!std::isnan(vel.x) && !std::isnan(vel.y) && !std::isnan(vel.z))
                wp = wp + vel * 0.2f;
        }

        return global::render.screen(wp);
    }

    static void aim_move(int dx, int dy)
    {
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dx = dx;
        input.mi.dy = dy;
        input.mi.dwFlags = MOUSEEVENTF_MOVE;
        SendInput(1, &input, sizeof(INPUT));
    }

    void aimbot()
    {
        if (!global::aimbot::Enabled || !global::render.Address)
            return;

        int Vk = ImGuiKeyToVK(global::aimbot::Key);
        if (!Vk) return;

        HWND RobloxHwnd = FindWindowA(0, "Roblox");
        bool RobloxFocused = RobloxHwnd && GetForegroundWindow() == RobloxHwnd;
        bool Pressed = RobloxFocused && (GetAsyncKeyState(Vk) & 0x8000) != 0;

        static bool AimbotToggled = false;
        static bool AimbotLastPressed = false;
        bool active = false;
        if (global::aimbot::Mode == ImKeyBindMode_Toggle) {
            if (Pressed && !AimbotLastPressed) AimbotToggled = !AimbotToggled;
            active = AimbotToggled;
        } else {
            active = Pressed;
        }
        AimbotLastPressed = Pressed;

        if (!active) return;

        POINT CursorPos;
        if (!RobloxHwnd || !GetCursorPos(&CursorPos) || !ScreenToClient(RobloxHwnd, &CursorPos))
            return;

        sdk::vector2 center{ (float)CursorPos.x, (float)CursorPos.y };

        auto players = cache::snapshot();

        static uint64_t sticky_addr = 0;

        int best_idx = -1;
        float best_score = FLT_MAX;
        sdk::vector2 best_screen{};

        for (int i = 0; i < (int)players.size(); i++)
        {
            auto& plr = players[i];
            if (!aim_valid(plr)) continue;

            sdk::vector2 sp = aim_target_pos(plr);
            if (sp.x <= -0.5f || sp.y <= -0.5f) continue;

            float dx = sp.x - center.x;
            float dy = sp.y - center.y;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (global::aimbot::VisibleCheck)
            {
                sdk::part hp(plr.Head.Address);
                if (!visible(global::camera, hp.partposition(), sp, plr.character.Address))
                    continue;
            }

            float fov = global::aimbot::FOV;
            if (global::aimbot::Legit && global::aimbot::RandomFOV > 0.f)
                fov = (std::max)(0.f, fov - ((float)rand() / RAND_MAX) * global::aimbot::RandomFOV);

            if (dist > fov) continue;

            float score;
            switch (global::aimbot::TargetPriority) {
            case 0: score = dist; break;
            case 1: score = plr.Distance; break;
            case 2: score = plr.Health; break;
            default: score = dist; break;
            }

            if (global::aimbot::Sticky && sticky_addr != 0 &&
                plr.character.Address == sticky_addr)
            {
                score = -FLT_MAX;
            }

            if (score < best_score) {
                best_score = score;
                best_idx = i;
                best_screen = sp;
            }
        }

        if (best_idx < 0)
        {
            if (global::aimbot::Sticky) sticky_addr = 0;
            return;
        }

        if (global::aimbot::Sticky)
            sticky_addr = players[best_idx].character.Address;

        static ULONGLONG aimStartTime = 0;
        static sdk::vector2 aim_current = { 0.f, 0.f };
        static bool aim_first = true;
        static uint64_t last_target_addr = 0;

        // Reset interpolation state when target switches
        uint64_t cur_target_addr = players[best_idx].character.Address;
        if (cur_target_addr != last_target_addr) {
            last_target_addr = cur_target_addr;
            aim_first   = true;
            aimStartTime = 0;
        }

        if (aimStartTime == 0) {
            aimStartTime = GetTickCount64();
            return;
        }

        ULONGLONG reaction = global::aimbot::ReactionMs;
        if (global::aimbot::Legit)
            reaction += (ULONGLONG)((float)rand() / RAND_MAX * 100.f);

        if (GetTickCount64() - aimStartTime < reaction)
            return;

        if (aim_first) {
            aim_current = center;
            aim_first = false;
        }

        float smooth = (std::max)(1.f, global::aimbot::Smoothing);
        float speed  = (std::max)(0.1f, global::aimbot::Speed);

        if (global::aimbot::AutoSens) {
            float win_sens = 10.f;
            HKEY hk;
            if (!RegOpenKeyExA(HKEY_CURRENT_USER, "Control Panel\\Mouse", 0, KEY_READ, &hk)) {
                DWORD sz = sizeof(DWORD), val = 10;
                RegQueryValueExA(hk, "MouseSensitivity", nullptr, nullptr, (LPBYTE)&val, &sz);
                RegCloseKey(hk);
                win_sens = (float)val;
            }
            float sens_scale = 0.5f + (win_sens - 1.f) / 19.f;
            speed  = 3.f / sens_scale;
            smooth = 4.f + sens_scale * 6.f;
        }

        if (global::aimbot::Legit) {
            smooth *= 0.8f + ((float)rand() / RAND_MAX) * 0.4f;
            speed  *= 0.85f + ((float)rand() / RAND_MAX) * 0.3f;
        }

        // t = normalised progress toward target [0,1]
        const float dt   = 0.001f;
        const float base = 1.f - expf(-(speed / smooth) * dt * 60.f);

        // Apply easing to approach factor
        // SmoothEaseIn controls how fast we accelerate toward target
        // SmoothEaseOut controls how fast we decelerate near target
        auto apply_ease = [](float t, int mode) -> float {
            t = std::clamp(t, 0.f, 1.f);
            switch (mode) {
                case 1: { float inv=1.f-t; return 1.f-inv*inv*inv; }  // ease-out cubic
                case 2: { float inv=1.f-t; return 1.f-inv*inv*inv*inv*inv; } // ease-out quint
                default: return t; // linear
            }
        };

        float dx_f = best_screen.x - aim_current.x;
        float dy_f = best_screen.y - aim_current.y;
        float dist_to_target = sqrtf(dx_f*dx_f + dy_f*dy_f);

        // Use ease-in when far from target, ease-out when close
        float t_progress = (dist_to_target > 0.f) ? (1.f - std::clamp(dist_to_target / 200.f, 0.f, 1.f)) : 1.f;
        float k_in  = apply_ease(base, global::aimbot::SmoothEaseIn);
        float k_out = apply_ease(base, global::aimbot::SmoothEaseOut);
        const float k = k_in * (1.f - t_progress) + k_out * t_progress;

        if (dist_to_target >= 0.5f)
        {
            aim_current.x += dx_f * k;
            aim_current.y += dy_f * k;

            if (global::aimbot::Legit && global::aimbot::Jitter > 0.f)
            {
                float jscale = std::clamp(1.f - 1.f / smooth, 0.f, 1.f) * global::aimbot::Jitter;
                aim_current.x += ((float)rand() / RAND_MAX - 0.5f) * 2.f * jscale;
                aim_current.y += ((float)rand() / RAND_MAX - 0.5f) * 2.f * jscale;
            }

            // Sub-pixel accumulation — prevents int-truncation jitter
            static float acc_x = 0.f, acc_y = 0.f;
            float raw_x = aim_current.x - center.x + acc_x;
            float raw_y = aim_current.y - center.y + acc_y;
            int move_x = (int)raw_x;
            int move_y = (int)raw_y;
            acc_x = raw_x - (float)move_x;
            acc_y = raw_y - (float)move_y;

            if (move_x != 0 || move_y != 0)
                aim_move(move_x, move_y);
        }
        else
        {
            aim_current = best_screen;
        }

        float aim_dist = std::sqrt((best_screen.x - aim_current.x) * (best_screen.x - aim_current.x) +
                                   (best_screen.y - aim_current.y) * (best_screen.y - aim_current.y));

        if (global::aimbot::AutoShoot && aim_dist < (global::aim::FovCheck ? global::aim::TriggerRadius : 5.f))
        {
            static ULONGLONG lastAuto = 0;
            ULONGLONG now = GetTickCount64();
            if (now - lastAuto > (ULONGLONG)std::max(0, global::aim::TriggerDelayMs))
            {
                INPUT shot[2]{};
                shot[0].type = INPUT_MOUSE;
                shot[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                shot[1].type = INPUT_MOUSE;
                shot[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
                SendInput(2, shot, sizeof(INPUT));
                lastAuto = now;
            }
        }
    }

    void run() {
        std::thread([]() {
            while (true) {
                triggerbot();
                aimbot();
                Sleep(global::aim::TriggerBot || global::aimbot::Enabled ? 1 : 10);
            }
            }).detach();
    }

}
