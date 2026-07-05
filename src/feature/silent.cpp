#define NOMINMAX
#include <Windows.h>
#include <thread>
#include <vector>
#include <algorithm>
#include <immintrin.h>
#include <cmath>
#include <limits>
#include <iostream>
#include "silent.h"
#include "global.h"
#include "engine/engine.h"
#include "engine/math.h"
#include "feature/wallcheck.h"
#include "imgui/addons/imgui_addons.h"

std::uint64_t helper::CachedInputObject = 0;

static float effectivefov()
{
    if (!global::silent::GunBasedFov)
        return global::silent::fov;

    std::string ToolName = global::LocalPlayer.Tool_Name;

    if (ToolName.empty())
        return global::silent::fov;

    std::transform(ToolName.begin(), ToolName.end(), ToolName.begin(), ::tolower);

    if (ToolName.find("double-barrel") != std::string::npos ||
        ToolName.find("double barrel") != std::string::npos ||
        ToolName.find("doublebarrel") != std::string::npos)
    {
        return global::silent::FovDoubleBarrel;
    }
    else if (ToolName.find("tacticalshotgun") != std::string::npos ||
        ToolName.find("tactical shotgun") != std::string::npos)
    {
        return global::silent::FovTacticalShotgun;
    }
    else if (ToolName.find("revolver") != std::string::npos)
    {
        return global::silent::FovRevolver;
    }

    return global::silent::fov;
}

static sdk::instance targetpart(sdk::player& player, int AimPart)
{
    sdk::instance TargetPart{};

    if (AimPart == 0)
    {
        TargetPart = player.Head;
    }
    else if (AimPart == 1)
    {
        if (player.UpperTorso.Address != 0)
            TargetPart = player.UpperTorso;
        else
            TargetPart = player.Torso;
    }
    else if (AimPart == 2)
    {
        if (player.LowerTorso.Address != 0)
            TargetPart = player.LowerTorso;
        else if (player.HumanoidRootPart.Address != 0)
            TargetPart = player.HumanoidRootPart;
        else
            TargetPart = player.Torso;
    }

    if (!TargetPart.Address)
    {
        if (player.Head.Address) TargetPart = player.Head;
        else if (player.HumanoidRootPart.Address) TargetPart = player.HumanoidRootPart;
        else if (player.UpperTorso.Address) TargetPart = player.UpperTorso;
        else if (player.Torso.Address) TargetPart = player.Torso;
        else if (player.LowerTorso.Address) TargetPart = player.LowerTorso;
    }

    return TargetPart;
}

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

    bool KoValue = drive->read<bool>(Ko.Address + offset::misc::Value);

    return KoValue;
}

static bool alive(sdk::player& player)
{
    if (!player.character.Address)
        return false;

    if (player.humanoid.Address)
    {
        sdk::humanoid humanoid(player.humanoid.Address);
        if (humanoid.health() <= 0.f)
            return false;
    }

    sdk::instance BodyEffects = player.character.child("BodyEffects");
    if (BodyEffects.Address)
    {
        sdk::instance Dead = BodyEffects.child("Dead");
        if (Dead.Address && drive->read<bool>(Dead.Address + offset::misc::Value))
            return false;
    }

    if (player.Head.Address)
    {
        sdk::part head(player.Head.Address);
        if (head.partposition().y < -50.f)
            return false;
    }

    return true;
}

static bool validvector(const sdk::vector3& Value)
{
    return !std::isnan(Value.x) && !std::isnan(Value.y) && !std::isnan(Value.z) &&
        !(Value.x == 0.f && Value.y == 0.f && Value.z == 0.f);
}

static sdk::vector3 cross(const sdk::vector3& A, const sdk::vector3& B)
{
    return {
        A.y * B.z - A.z * B.y,
        A.z * B.x - A.x * B.z,
        A.x * B.y - A.y * B.x
    };
}

static bool normalize(sdk::vector3& Value)
{
    const float Length = Value.magnitude();
    if (Length < 0.0001f || std::isnan(Length))
        return false;

    Value = Value / Length;
    return true;
}

static sdk::matrix3 phantomlookat(const sdk::vector3& From, const sdk::vector3& To)
{
    sdk::vector3 Forward = To - From;
    if (!normalize(Forward))
        return {};

    sdk::vector3 WorldUp{ 0.f, 1.f, 0.f };
    sdk::vector3 Right = cross(WorldUp, Forward);
    if (!normalize(Right))
        return {};

    sdk::vector3 Up = cross(Forward, Right);

    sdk::matrix3 Result{};
    Result.data[0] = -Right.x; Result.data[1] = Up.x; Result.data[2] = -Forward.x;
    Result.data[3] = Right.y;  Result.data[4] = Up.y; Result.data[5] = -Forward.y;
    Result.data[6] = -Right.z; Result.data[7] = Up.z; Result.data[8] = -Forward.z;
    return Result;
}

static sdk::matrix3 lerpmatrix(const sdk::matrix3& From, const sdk::matrix3& To, float T)
{
    sdk::matrix3 Result{};
    for (int i = 0; i < 9; ++i)
        Result.data[i] = From.data[i] + (To.data[i] - From.data[i]) * T;
    return Result;
}

static float clampf(float Value, float Min, float Max)
{
    return std::max(Min, std::min(Value, Max));
}

static sdk::vector3 linearvelocity(const sdk::instance& PartInst)
{
    if (!PartInst.Address)
        return {};

    sdk::part part(PartInst.Address);
    sdk::part primitive = part.primitive();
    if (!primitive.Address)
        return {};

    return drive->read<sdk::vector3>(primitive.Address + offset::primitive::AssemblyLinearVelocity);
}

static sdk::vector3 silentprediction(const sdk::instance& PartInst, const sdk::vector3& Position)
{
    if (!global::silent::Prediction)
        return Position;

    const sdk::vector3 Velocity = linearvelocity(PartInst);
    if (std::isnan(Velocity.x) || std::isnan(Velocity.y) || std::isnan(Velocity.z))
        return Position;

    if (global::silent::AutoPrediction)
    {
        float Distance = 0.f;
        if (global::camera.Address)
            Distance = global::camera.position().distance(Position);

        const float SpeedTerm = clampf(Velocity.magnitude() / 2200.f, 0.f, .045f);
        const float DistanceTerm = clampf(Distance / 9500.f, .018f, .185f);
        const float Factor = DistanceTerm + SpeedTerm;
        return Position + Velocity * Factor;
    }

    return {
        Position.x + Velocity.x * global::silent::PredictionX,
        Position.y + Velocity.y * global::silent::PredictionY,
        Position.z + Velocity.z * global::silent::PredictionZ
    };
}

static bool targetpos(sdk::player& player, sdk::vector3& Out)
{
    sdk::instance TargetPart = targetpart(player, global::silent::AimPart);
    if (!TargetPart.Address)
        return false;

    sdk::part part(TargetPart.Address);
    sdk::part primitive = part.primitive();
    if (!primitive.Address)
        return false;

    Out = silentprediction(TargetPart, primitive.position());
    return validvector(Out);
}

static bool camerasilent(sdk::player& Target)
{
    if (!global::camera.Address)
        return false;

    sdk::instance CameraInst(global::camera.Address);
    sdk::instance CameraPartInst = CameraInst.child("Part");
    if (!CameraPartInst.Address)
        return false;

    sdk::part CameraPart(CameraPartInst.Address);
    sdk::part CameraPrimitive = CameraPart.primitive();
    if (!CameraPrimitive.Address)
        return false;

    const sdk::vector3 CameraPos = global::camera.position();
    if (!validvector(CameraPos))
        return false;

    sdk::vector3 TargetPos{};
    if (!targetpos(Target, TargetPos))
        return false;

    sdk::matrix3 TargetMatrix = phantomlookat(CameraPos, TargetPos);
    TargetMatrix.data[4] = 0.01f;

    sdk::matrix3 CurrentMatrix = global::camera.rotation();
    sdk::matrix3 FinalMatrix = lerpmatrix(CurrentMatrix, TargetMatrix, 1.f);
    CameraPrimitive.rotation(FinalMatrix);
    return true;
}

static bool targetfov(sdk::player& player)
{
    if (player.character.Address == 0)
        return false;

    POINT CursorPoint;
    HWND Window = FindWindowA(nullptr, "Roblox");
    if (!Window || !GetCursorPos(&CursorPoint) || !ScreenToClient(Window, &CursorPoint))
        return false;

    sdk::vector2 Cursor = { static_cast<float>(CursorPoint.x), static_cast<float>(CursorPoint.y) };

    sdk::vector3 partposition{};
    if (!targetpos(player, partposition))
        return false;
    sdk::vector2 PartScreen = global::render.screen(partposition);

    if (PartScreen.x <= -0.5f || PartScreen.y <= -0.5f)
        return false;

    float DistanceFromCursor = PartScreen.distance(Cursor);

    return DistanceFromCursor <= effectivefov();
}

static bool visiblecheck(const sdk::vector3& target, const sdk::vector2& screen)
{
    HWND window = FindWindowA(nullptr, "Roblox");
    RECT rect{};
    if (!window || !GetClientRect(window, &rect))
        return false;

    if (!(screen.x >= 0.f && screen.y >= 0.f &&
        screen.x <= (float)rect.right &&
        screen.y <= (float)rect.bottom))
        return false;

    if (!global::camera.Address)
        return false;

    return wallcheck::can_see(global::camera.position(), target);
}

static sdk::player closestplayer()
{
    POINT CursorPoint;
    HWND Window = FindWindowA(nullptr, "Roblox");
    if (!Window || !GetCursorPos(&CursorPoint) || !ScreenToClient(Window, &CursorPoint))
        return {};

    sdk::vector2 Cursor = { static_cast<float>(CursorPoint.x), static_cast<float>(CursorPoint.y) };

    std::vector<sdk::player> PlayersSnapshot;
    {
        PlayersSnapshot = cache::snapshot();
    }

    if (PlayersSnapshot.empty())
        return {};

    sdk::player ClosestPlayer{};
    float ShortestDistance = std::numeric_limits<float>::max();

    for (sdk::player& player : PlayersSnapshot)
    {
        if (player.character.Address == 0)
            continue;

        if (global::LocalPlayer.character.Address != 0 &&
            player.character.Address == global::LocalPlayer.character.Address)
            continue;
        if (!alive(player))
            continue;

        sdk::vector3 partposition{};
        if (!targetpos(player, partposition))
            continue;
        sdk::vector2 PartScreen = global::render.screen(partposition);

        if (PartScreen.x <= -0.5f || PartScreen.y <= -0.5f)
            continue;
        if (global::silent::VisibleCheck && !visiblecheck(partposition, PartScreen))
            continue;

        float DistanceFromCursor = PartScreen.distance(Cursor);
        float WorldDistance = 0.f;
        if (global::camera.Address)
            WorldDistance = global::camera.position().distance(partposition);

        if (global::silent::UseFov && DistanceFromCursor > effectivefov())
            continue;

        if (global::silent::KnockedCheck && knocked(player))
            continue;

        const float Score = global::silent::TargetPriority == 1 ? WorldDistance : DistanceFromCursor;
        if (Score < ShortestDistance)
        {
            ShortestDistance = Score;
            ClosestPlayer = player;
        }
    }

    return ClosestPlayer;
}

static bool validtarget(sdk::player& player)
{
    if (!player.character.Address || !player.Head.Address)
        return false;

    if (global::LocalPlayer.character.Address != 0 &&
        player.character.Address == global::LocalPlayer.character.Address)
        return false;

    if (!alive(player))
        return false;

    if (global::silent::KnockedCheck && knocked(player))
        return false;

    sdk::vector3 pos{};
    if (!targetpos(player, pos))
        return false;

    sdk::vector2 screen = global::render.screen(pos);
    if (screen.x <= -0.5f || screen.y <= -0.5f)
        return false;

    if (global::silent::UseFov && !targetfov(player))
        return false;

    if (global::silent::VisibleCheck && !visiblecheck(pos, screen))
        return false;

    return true;
}

static bool validinputobject(std::uint64_t Object)
{
    if (!Object || Object == 0xFFFFFFFFFFFFFFFF)
        return false;

    sdk::vector2 Pos = drive->read<sdk::vector2>(Object + offset::mouseservice::MousePosition);
    return std::isfinite(Pos.x) && std::isfinite(Pos.y) &&
        std::abs(Pos.x) < 100000.0f && std::abs(Pos.y) < 100000.0f;
}

static std::uint64_t inputobject(std::uint64_t BaseAddress)
{
    if (!BaseAddress)
        return 0;

    constexpr std::uintptr_t LainInputObject = 0x110;
    const std::uintptr_t Candidates[] = {
        LainInputObject,
        offset::mouseservice::InputObject,
        offset::mouseservice::InputObject2,
        offset::mouseservice::InputObject + 0x8,
        offset::mouseservice::InputObject2 - 0x8
    };

    for (std::uintptr_t Candidate : Candidates)
    {
        std::uint64_t Object = drive->read<std::uint64_t>(BaseAddress + Candidate);
        if (validinputobject(Object))
            return Object;
    }

    return 0;
}

void helper::x(std::uint64_t Position)
{
    drive->write<std::uint64_t>(Address + offset::silent::FramePositionOffsetX, Position);
}

void helper::y(std::uint64_t Position)
{
    drive->write<std::uint64_t>(Address + offset::silent::FramePositionOffsetY, Position);
}

void helper::init(std::uint64_t Address)
{
    this->Address = Address;
    CachedInputObject = inputobject(Address);

    if (CachedInputObject && CachedInputObject != 0xFFFFFFFFFFFFFFFF)
    {
        const char* BasePointer = reinterpret_cast<const char*>(CachedInputObject);
        _mm_prefetch(BasePointer + offset::mouseservice::MousePosition, _MM_HINT_T0);
        _mm_prefetch(BasePointer + offset::mouseservice::MousePosition + sizeof(sdk::vector2), _MM_HINT_T0);
    }
}

void helper::writepos(std::uint64_t Address, float X, float Y)
{
    CachedInputObject = inputobject(Address);
    if (CachedInputObject != 0 && CachedInputObject != 0xFFFFFFFFFFFFFFFF)
    {
        sdk::vector2 NewPosition = { X, Y };
        drive->write<sdk::vector2>(CachedInputObject + offset::mouseservice::MousePosition, NewPosition);
    }
}

static bool silentactive()
{
    if (!global::silent::Enabled)
        return false;

    return SilentAimLocked;
}

static void silentkey()
{
    int Vk = ImGuiKeyToVK(global::silent::Silent_Key);
    if (!Vk) return;

    HWND RobloxHwnd = FindWindowA(nullptr, "Roblox");
    bool RobloxFocused = RobloxHwnd && GetForegroundWindow() == RobloxHwnd;
    bool Pressed = RobloxFocused && (GetAsyncKeyState(Vk) & 0x8000) != 0;

    if (global::silent::Silent_Mode == ImKeyBindMode_Toggle)
    {
        if (Pressed && !SilentAimKeyWasPressed)
        {
            SilentAimLocked = !SilentAimLocked;
        }

        if (!SilentAimLocked)
        {
            SilentCachedTarget = {};
            IsSilentReady = false;
        }
    }
    else
    {
        if (Pressed)
        {
            SilentAimLocked = true;
        }
        else
        {
            SilentAimLocked = false;
            SilentCachedTarget = {};
            IsSilentReady = false;
        }
    }

    SilentAimKeyWasPressed = Pressed;
}

void silent::frame() {

    sdk::player Target{};
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    HWND Window = FindWindowA(0, "Roblox");

    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        if (!global::model.Address || !global::render.Address) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (!Window || !IsWindow(Window))
            Window = FindWindowA(0, "Roblox");

        sdk::instance CurrentMouseService = global::model.childclass("MouseService");
        if (!mouseservice || mouseservice->Address != CurrentMouseService.Address)
            mouseservice = std::make_unique<sdk::instance>(CurrentMouseService);

        silentkey();

        if (SilentAimInstance.Address != 0 && SilentHasOriginalSizes) {
            if (global::silent::Enabled) {
                drive->write<sdk::vector2>(SilentAimInstance.Address + offset::gui::Size, { 0, 0 });

                auto children = SilentAimInstance.children();
                for (auto& Child : children) {
                    if (Child.Address)
                        drive->write<sdk::vector2>(Child.Address + offset::gui::Size, { 0, 0 });
                }
            }
            else {
                drive->write<sdk::vector2>(SilentAimInstance.Address + offset::gui::Size, SilentOriginalSize);
                for (const auto& [ChildAddr, OrigSize] : SilentOriginalChildrenSizes) {
                    drive->write<sdk::vector2>(ChildAddr, OrigSize);
                }
            }
        }

        if (!silentactive()) {
            IsSilentReady = false;
            SilentPrimitiveActive = false;
            SilentCachedTarget = {};
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        static int AimInstanceCheckCounter = 0;
        if (AimInstanceCheckCounter++ % 10 == 0) {
            try {
                sdk::instance LocalPlayer = sdk::instance(drive->read<std::uintptr_t>(global::model.childclass("Players").Address + offset::player::LocalPlayer));
                sdk::instance PlayerGui = LocalPlayer.child("PlayerGui");

                if (PlayerGui.Address != 0) {
                    sdk::instance FoundAimFrame{};
                    auto GuiChildren = PlayerGui.children();

                    for (auto& Child : GuiChildren) {
                        if (!Child.Address) continue;

                        std::string name = Child.name();
                        if (name == "Aim") {
                            FoundAimFrame = Child;
                            break;
                        }

                        std::string kind = Child.kind();
                        if (kind == "Frame" || kind == "ScreenGui" || kind == "GuiObject") {
                            std::string LowerName = name;
                            std::transform(LowerName.begin(), LowerName.end(), LowerName.begin(), ::tolower);

                            if (LowerName.find("main") != std::string::npos) {
                                auto Grandchildren = Child.children();
                                for (auto& GChild : Grandchildren) {
                                    if (GChild.Address && GChild.name() == "Aim") {
                                        FoundAimFrame = GChild;
                                        break;
                                    }
                                }
                            }
                        }
                        if (FoundAimFrame.Address) break;
                    }

                    if (FoundAimFrame.Address != SilentAimInstance.Address) {
                        SilentAimInstance = FoundAimFrame;
                        SilentHasOriginalSizes = false;
                        SilentOriginalChildrenSizes.clear();

                        if (SilentAimInstance.Address != 0) {
                            SilentOriginalSize = drive->read<sdk::vector2>(SilentAimInstance.Address + offset::gui::Size);
                            auto AimChildren = SilentAimInstance.children();
                            for (auto& C : AimChildren) {
                                if (C.Address) {
                                    sdk::vector2 CSize = drive->read<sdk::vector2>(C.Address + offset::gui::Size);
                                    SilentOriginalChildrenSizes.push_back({ C.Address, CSize });
                                }
                            }
                            SilentHasOriginalSizes = true;
                        }
                    }
                }
            }
            catch (...) {}
        }

        if (!global::silent::StickyAim || !validtarget(SilentCachedTarget))
        {
            Target = closestplayer();
            SilentCachedLastTarget = Target;
            SilentCachedTarget = Target;
            SilentFTarget = validtarget(SilentCachedTarget);
        }
        else
        {
            SilentFTarget = true;
        }

        if (SilentFTarget && SilentCachedTarget.character.Address != 0) {
            if (!validtarget(SilentCachedTarget)) {
                SilentFTarget = false;
                SilentCachedTarget = {};
                SilentPrimitiveActive = false;
                IsSilentReady = false;
                continue;
            }

            sdk::instance TargetPart = targetpart(SilentCachedTarget, global::silent::AimPart);
            if (TargetPart.Address != 0) {
                SilentPrimitiveActive = false;

                sdk::vector3 Part3D{};
                if (!targetpos(SilentCachedTarget, Part3D)) {
                    IsSilentReady = false;
                    continue;
                }
                sdk::vector2 PartScreen = global::render.screen(Part3D);

                if (PartScreen.x <= -0.5f || PartScreen.y <= -0.5f) {
                    IsSilentReady = false;
                    continue;
                }
                if (global::silent::VisibleCheck && !visiblecheck(Part3D, PartScreen)) {
                    IsSilentReady = false;
                    continue;
                }

                POINT CursorPos;
                GetCursorPos(&CursorPos);
                if (Window && IsWindow(Window)) ScreenToClient(Window, &CursorPos);

                sdk::vector2 Dims = global::render.size();
                SilentPartPos = PartScreen;
                SilentCachedPositionX = static_cast<std::uint64_t>(CursorPos.x);
                SilentCachedPositionY = static_cast<std::uint64_t>(Dims.y - std::abs(Dims.y - static_cast<float>(CursorPos.y)) - 58);
                IsSilentReady = true;
            }
        }
        else {
            IsSilentReady = false;
            SilentPrimitiveActive = false;
        }
    }
}

void silent::mouse()
{
    helper MouseServiceInstance{};
    bool MouseServiceInitialized = false;
    std::uint64_t LastMouseService = 0;

    for (;;)
    {
        if (!mouseservice || !mouseservice->Address)
        {
            MouseServiceInitialized = false;
            LastMouseService = 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (mouseservice->Address != LastMouseService)
        {
            MouseServiceInitialized = false;
            LastMouseService = mouseservice->Address;
        }

        if (!silentactive())
        {
            SilentPrimitiveActive = false;
            MouseServiceInitialized = false;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (SilentPrimitiveActive)
        {
            MouseServiceInitialized = false;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        if (SilentCachedTarget.character.Address != 0 && IsSilentReady)
        {
            if (SilentPartPos.x <= -0.5f || SilentPartPos.y <= -0.5f ||
                SilentPartPos.x > 15000.0f || SilentPartPos.y > 15000.0f)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            try
            {
                if (!MouseServiceInitialized)
                {
                    MouseServiceInstance.init(mouseservice->Address);
                    MouseServiceInitialized = helper::CachedInputObject != 0;
                }

                if (MouseServiceInitialized)
                {
                    MouseServiceInstance.writepos(mouseservice->Address, SilentPartPos.x, SilentPartPos.y);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                else
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            catch (...)
            {
                MouseServiceInitialized = false;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
}

void silent::run()
{
    std::thread(frame).detach();
    std::thread(mouse).detach();
}
