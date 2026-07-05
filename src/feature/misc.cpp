#include <Windows.h>
#include <algorithm>
#include <thread>
#include <chrono>
#include <mutex>
#pragma comment(lib, "Winmm.lib")

#include "engine/engine.h"
#include "global.h"
#include "feature/cache.h"
#include "engine/offset.h"

namespace {
    sdk::vector3 lookvec(const sdk::matrix3& rot)
    {
        return { -rot.data[2], -rot.data[5], -rot.data[8] };
    }

    sdk::vector3 rightvec(const sdk::matrix3& rot)
    {
        return { rot.data[0], rot.data[3], rot.data[6] };
    }

    int misckey(ImGuiKey key)
    {
        if (key >= ImGuiKey_A && key <= ImGuiKey_Z)
            return 'A' + (key - ImGuiKey_A);
        if (key >= ImGuiKey_0 && key <= ImGuiKey_9)
            return '0' + (key - ImGuiKey_0);
        if (key >= ImGuiKey_F1 && key <= ImGuiKey_F12)
            return VK_F1 + (key - ImGuiKey_F1);

        switch (key)
        {
        case ImGuiKey_Tab:          return VK_TAB;
        case ImGuiKey_Space:        return VK_SPACE;
        case ImGuiKey_Enter:        return VK_RETURN;
        case ImGuiKey_Escape:       return VK_ESCAPE;
        case ImGuiKey_Backspace:    return VK_BACK;
        case ImGuiKey_Insert:       return VK_INSERT;
        case ImGuiKey_Delete:       return VK_DELETE;
        case ImGuiKey_LeftArrow:    return VK_LEFT;
        case ImGuiKey_RightArrow:   return VK_RIGHT;
        case ImGuiKey_UpArrow:      return VK_UP;
        case ImGuiKey_DownArrow:    return VK_DOWN;
        case ImGuiKey_PageUp:       return VK_PRIOR;
        case ImGuiKey_PageDown:     return VK_NEXT;
        case ImGuiKey_Home:         return VK_HOME;
        case ImGuiKey_End:          return VK_END;
        case ImGuiKey_LeftCtrl:     return VK_LCONTROL;
        case ImGuiKey_RightCtrl:    return VK_RCONTROL;
        case ImGuiKey_LeftShift:    return VK_LSHIFT;
        case ImGuiKey_RightShift:   return VK_RSHIFT;
        case ImGuiKey_LeftAlt:      return VK_LMENU;
        case ImGuiKey_RightAlt:     return VK_RMENU;
        case ImGuiKey_MouseLeft:    return VK_LBUTTON;
        case ImGuiKey_MouseRight:   return VK_RBUTTON;
        case ImGuiKey_MouseMiddle:  return VK_MBUTTON;
        default:                    return 0;
        }
    }
    uintptr_t worldptr()
    {
        if (!global::model.Address)
            return 0;

        uintptr_t WorkspaceAddr = drive->read<uintptr_t>(global::model.Address + offset::datamodel::workspace);
        if (!WorkspaceAddr)
            return 0;

        return drive->read<uintptr_t>(WorkspaceAddr + offset::workspace::world);
    }

    float readgravity(uintptr_t world)
    {
        return drive->read<float>(world + offset::world::Gravity);
    }

    void writegravity(uintptr_t world, float value)
    {
        drive->write<float>(world + offset::world::Gravity, value);
    }

    void platform(uintptr_t humanoidAddress, bool value)
    {
        drive->write<bool>(humanoidAddress + offset::humanoid::PlatformStand, value);
    }
}

namespace misc {

    void fly()
    {
        timeBeginPeriod(1);

        bool prevKeyState = false;
        bool wasFlying = false;
        float savedGravity = 196.2f;

        while (true)
        {
            Sleep(1);

            HWND robloxWnd = FindWindowA(0, "Roblox");
            bool robloxFocused = robloxWnd && GetForegroundWindow() == robloxWnd;

            int  vk = misckey(global::misc::Fly_Key);
            bool keyDown = robloxFocused && vk && (GetAsyncKeyState(vk) & 0x8000);

            if (global::misc::Fly_Mode == ImKeyBindMode_Toggle)
            {
                if (keyDown && !prevKeyState)
                    global::misc::fly = !global::misc::fly;
            }
            else
            {
                global::misc::fly = keyDown;
            }

            prevKeyState = keyDown;

            auto& lp = global::LocalPlayer;
            if (!lp.HumanoidRootPart.Address || !lp.humanoid.Address || !global::camera.Address)
            {
                wasFlying = false;
                Sleep(8);
                continue;
            }

            const uintptr_t world = worldptr();
            if (!world)
            {
                Sleep(8);
                continue;
            }

            if (!global::misc::fly)
            {
                if (wasFlying)
                {
                    writegravity(world, savedGravity);
                    platform(lp.humanoid.Address, false);
                    sdk::part hrp(lp.HumanoidRootPart.Address);
                    hrp.velocity({ 0.f, 0.f, 0.f });
                    wasFlying = false;
                }
                Sleep(8);
                continue;
            }

            if (!wasFlying)
            {
                savedGravity = readgravity(world);
                wasFlying = true;
            }

            writegravity(world, 0.f);
            platform(lp.humanoid.Address, true);

            const bool W = GetAsyncKeyState('W') & 0x8000;
            const bool S = GetAsyncKeyState('S') & 0x8000;
            const bool A = GetAsyncKeyState('A') & 0x8000;
            const bool D = GetAsyncKeyState('D') & 0x8000;
            const bool Up = GetAsyncKeyState(VK_SPACE) & 0x8000;
            const bool Down = GetAsyncKeyState(VK_LCONTROL) & 0x8000;

            sdk::camera cam(global::camera.Address);
            const sdk::matrix3 camRot = cam.rotation();
            const sdk::vector3 look = lookvec(camRot);
            const sdk::vector3 right = rightvec(camRot);

            sdk::vector3 dir(0.f, 0.f, 0.f);

            if (W)  dir = dir + look;
            if (S)  dir = dir - look;
            if (A)  dir = dir - right;
            if (D)  dir = dir + right;
            if (Up)   dir.y += 1.f;
            if (Down) dir.y -= 1.f;

            if (dir.magnitude() > 0.f)
                dir = dir.normalize();

            sdk::part hrp(lp.HumanoidRootPart.Address);
            hrp.velocity({
                dir.x * global::misc::Fly_Speed,
                dir.y * global::misc::Fly_Speed,
                dir.z * global::misc::Fly_Speed
                });
        }
    }

    void walkspeed()
    {
        float originalSpeed = 16.f;
        bool originalSet = false;

        while (true)
        {
            Sleep(5);

            const auto& lp = global::LocalPlayer;
            if (!lp.humanoid.Address)
            {
                originalSet = false;
                Sleep(25);
                continue;
            }

            if (!global::misc::walkspeed)
            {
                if (originalSet)
                {
                    drive->write<float>(lp.humanoid.Address + offset::humanoid::walkspeed, originalSpeed);
                    drive->write<float>(lp.humanoid.Address + offset::humanoid::WalkspeedCheck, originalSpeed);
                    originalSet = false;
                }
                Sleep(25);
                continue;
            }

            if (!originalSet)
            {
                originalSpeed = drive->read<float>(lp.humanoid.Address + offset::humanoid::walkspeed);
                if (originalSpeed <= 0.f || originalSpeed > 500.f)
                    originalSpeed = 16.f;
                originalSet = true;
            }

            const float speed = std::clamp(global::misc::Walkspeed_Speed, 1.f, 500.f);
            drive->write<float>(lp.humanoid.Address + offset::humanoid::walkspeed, speed);
            drive->write<float>(lp.humanoid.Address + offset::humanoid::WalkspeedCheck, speed);
        }
    }

    static void primitivesize(const sdk::instance& part, const sdk::vector3& size)
    {
        if (!part.Address)
            return;

        sdk::part primitive = sdk::part(part.Address).primitive();
        if (!primitive.Address)
            return;

        drive->write<sdk::vector3>(primitive.Address + offset::primitive::Size, size);
        drive->write<bool>(primitive.Address + offset::primitiveflag::CanCollide, false);
    }

    void hitbox()
    {
        constexpr sdk::vector3 kNormalSize{ 2.f, 2.f, 1.f };
        bool restored = true;

        while (true)
        {
            Sleep(10);

            if (!global::misc::hitbox)
            {
                if (!restored)
                {
                    const auto players = cache::snapshot();
                    for (const sdk::player& player : players)
                    {
                        if (player.Local_Player)
                            continue;
                        primitivesize(player.HumanoidRootPart, kNormalSize);
                    }
                    restored = true;
                }
                Sleep(35);
                continue;
            }

            restored = false;
            const sdk::vector3 hitboxSize{
                std::clamp(global::misc::Hitbox_Size_X, 1.f, 75.f),
                std::clamp(global::misc::Hitbox_Size_Y, 1.f, 75.f),
                std::clamp(global::misc::Hitbox_Size_Z, 1.f, 75.f)
            };

            const auto players = cache::snapshot();
            for (const sdk::player& player : players)
            {
                if (player.Local_Player)
                    continue;
                if (global::LocalPlayer.character.Address &&
                    player.character.Address == global::LocalPlayer.character.Address)
                    continue;
                primitivesize(player.HumanoidRootPart, hitboxSize);
            }
        }
    }

    void run()
    {
        std::thread(fly).detach();
        std::thread(walkspeed).detach();
        std::thread(hitbox).detach();
    }
}
