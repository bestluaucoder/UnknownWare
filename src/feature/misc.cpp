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
#include <imgui.h>

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

} // end first namespace misc block
namespace misc {
    // ── Desync ────────────────────────────────────────────────────────────────
    // Scans the task scheduler job list, finds PhysicsSenderJob by name,
    // and writes MaxBandwidthBps=0 while the keybind is held/toggled.
    // Restores the original value for 3 seconds after disabling.
    // Approach matches the pastebin.com/PiupZbgq reference implementation.
    void desync()
    {
        static int32_t  original_bw    = -1;
        static bool     was_enabled    = false;
        static bool     is_restoring   = false;
        static ULONGLONG restore_start = 0;

        // Helper: read a null-terminated string from Roblox memory (max 63 chars)
        auto read_str = [&](uintptr_t ptr) -> std::string {
            if (!ptr) return {};
            char buf[64]{};
            ReadProcessMemory(drive->processhandle(), (LPCVOID)ptr, buf, 63, nullptr);
            return std::string(buf);
        };

        // Helper: scan task scheduler and return PhysicsSenderJob address
        auto find_sender_job = [&]() -> uintptr_t {
            // Task scheduler is at a known static pointer
            uintptr_t sched = drive->read<uintptr_t>(
                drive->modulebase() + offset::task::Pointer);
            if (!sched) return 0;

            // The job list is stored as a contiguous array of pointers
            // [start, end) at known offsets within the scheduler struct
            uintptr_t start = drive->read<uintptr_t>(sched + 0xc8);
            uintptr_t end   = drive->read<uintptr_t>(sched + 0xd0);

            if (!start || !end || end <= start) return 0;
            size_t count = (end - start) / 8;
            if (count > 512) count = 512;

            for (size_t i = 0; i < count; i++) {
                uintptr_t job = drive->read<uintptr_t>(start + i * 8);
                if (!job) continue;

                // Job name is a Roblox string at offset 0x18
                uintptr_t name_addr = drive->read<uintptr_t>(job + 0x18);
                if (!name_addr) continue;

                std::string name = read_str(name_addr);
                if (name.find("PhysicsSender") != std::string::npos) {
                    return job;
                }
            }
            return 0;
        };

        while (true) {
            Sleep(1);

            if (!global::model.Address) {
                if (original_bw != -1) original_bw = -1;
                Sleep(50);
                continue;
            }

            // Key state
            bool key_active = false;
            {
                HWND rbx = FindWindowA(nullptr, "Roblox");
                if (rbx && GetForegroundWindow() == rbx &&
                    global::misc::Desync_Key != ImGuiKey_None) {
                    int vk = misckey(global::misc::Desync_Key);
                    if (vk) key_active = (GetAsyncKeyState(vk) & 0x8000) != 0;
                }
            }

            bool enabled = global::misc::Desync && key_active;

            if (enabled && !was_enabled) {
                // Enable — save original and zero it
                uintptr_t job = find_sender_job();
                if (job) {
                    original_bw = drive->read<int32_t>(
                        job + offset::physics_sender::MaxBandwidthBps);
                    drive->write<int32_t>(
                        job + offset::physics_sender::MaxBandwidthBps, 0);
                }
                is_restoring = false;
            }
            else if (!enabled && was_enabled) {
                // Disable — start 3-second restoration
                restore_start = GetTickCount64();
                is_restoring  = true;
            }
            else if (enabled) {
                // Keep writing 0 every tick while active
                uintptr_t job = find_sender_job();
                if (job) drive->write<int32_t>(
                    job + offset::physics_sender::MaxBandwidthBps, 0);
            }

            if (is_restoring && original_bw != -1) {
                ULONGLONG ms = GetTickCount64() - restore_start;
                if (ms < 3000) {
                    uintptr_t job = find_sender_job();
                    if (job) drive->write<int32_t>(
                        job + offset::physics_sender::MaxBandwidthBps,
                        original_bw);
                } else {
                    is_restoring = false;
                    original_bw  = -1;
                }
            }

            was_enabled = enabled;
        }
    }
}

namespace misc {
// ── Tickrate ──────────────────────────────────────────────────────────────────
// Writes to RunService HeartbeatTask MaxFPS. omc0eg multiplies by 4 internally.
void tickrate()
{
    while (true) {
        Sleep(8);
        if (!global::misc::Tickrate || !global::model.Address) { Sleep(50); continue; }

        HWND rbx = FindWindowA(nullptr, "Roblox");
        bool focused = rbx && GetForegroundWindow() == rbx;
        bool active = false;
        if (global::misc::Tickrate_Key != ImGuiKey_None && focused) {
            int vk = misckey(global::misc::Tickrate_Key);
            if (vk) active = (GetAsyncKeyState(vk) & 0x8000) != 0;
        } else {
            active = global::misc::Tickrate;
        }

        // Locate RunService task via task scheduler
        uintptr_t sched = drive->read<uintptr_t>(
            drive->modulebase() + offset::task::Pointer);
        if (!sched) continue;

        uintptr_t start = drive->read<uintptr_t>(sched + 0xc8);
        uintptr_t end   = drive->read<uintptr_t>(sched + 0xd0);
        if (!start || !end || end <= start) continue;

        size_t count = (end - start) / 8;
        if (count > 512) count = 512;

        for (size_t i = 0; i < count; i++) {
            uintptr_t job = drive->read<uintptr_t>(start + i * 8);
            if (!job) continue;
            uintptr_t name_ptr = drive->read<uintptr_t>(job + 0x18);
            if (!name_ptr) continue;
            char buf[64]{};
            ReadProcessMemory(drive->processhandle(), (LPCVOID)name_ptr, buf, 48, nullptr);
            // HeartbeatTask is the physics/game update job
            if (strstr(buf, "Heartbeat") || strstr(buf, "heartbeat")) {
                float target_fps = active
                    ? std::clamp(global::misc::Tickrate_Value, 1.f, 1000.f)
                    : 60.f;
                // omc0eg: tickrate * 4 is the internal unit
                drive->write<float>(job + offset::run::HeartbeatFPS,
                    target_fps * 4.f);
                break;
            }
        }
    }
}

// ── Animation IDs (from omc0eg globals.h) ────────────────────────────────────
struct AnimPack {
    const char* name;
    uint64_t run, walk, jump, idle1, idle2, fall, swim, swim_idle, climb;
};

static const AnimPack k_anim_packs[] = {
    {"Default",    507767714,507777826,507765000,507766666,507766951,507767968,507784897,507785072,507765644},
    {"Astronaut",  891636393,891636393,891627522,891621366,891633237,891617961,891639666,891663592,891609353},
    {"Bubbly",     910025107,910034870,910016857,910004836,910009958,910001910,910028158,910030921,909997997},
    {"Cartoony",   742638842,742640026,742637942,742637544,742638445,742637151,742639220,742639812,742636889},
    {"Elder",      845386501,845403856,845398858,845397899,845400520,845396048,845401742,845403127,845392038},
    {"Knight",     657564596,657552124,658409194,657595757,657568135,657600338,657560551,657557095,658360781},
    {"Levitation", 616010382,616013216,616008936,616006778,616008087,616005863,616011509,616012453,616003713},
    {"Mage",       707861613,707897309,707853694,707742142,707855907,707829716,707876443,707894699,707826056},
    {"Ninja",      656118852,656121766,656117878,656117400,656118341,656115606,656119721,656121397,656114359},
    {"Pirate",     750783738,750785693,750782230,750781874,750782770,750780242,750784579,750785176,750779899},
    {"Robot",      616091570,616095330,616090535,616088211,616089559,616087089,616092998,616094091,616086039},
    {"Rthro",     2510198475,2510202577,2510197830,2510197257,2510196951,2510195892,2510199791,2510201162,2510192778},
    {"Stylish",    616140816,616146177,616139451,616136790,616138447,616134815,616143378,616144772,616133594},
    {"Superhero",  616117076,616122287,616115533,616111295,616113536,616108001,616119360,616120861,616104706},
    {"Toy",        782842708,782843345,782847020,782841498,782845736,782846423,782844582,782845186,782843869},
    {"Vampire",   1083462077,1083473930,1083455352,1083445855,1083450166,1083443587,1083464683,1083467779,1083438238},
    {"Werewolf",  1083216690,1083178339,1083218792,1083195517,1083214717,1083189019,1083222527,1083225406,1083182000},
    {"Zombie",     616163682,616168032,616161997,616158929,616160636,616157476,616165109,616166655,616156119},
};
static constexpr int k_anim_pack_count = (int)(sizeof(k_anim_packs)/sizeof(k_anim_packs[0]));

const char** anim_pack_names(int* count_out) {
    static const char* names[32]{};
    static bool built = false;
    if (!built) {
        for (int i=0;i<k_anim_pack_count;i++) names[i]=k_anim_packs[i].name;
        built=true;
    }
    if (count_out) *count_out = k_anim_pack_count;
    return names;
}

// Apply an animation pack to the local player's Animator
static void apply_anim_pack(int pack_idx)
{
    if (pack_idx < 0 || pack_idx >= k_anim_pack_count) return;
    if (!global::LocalPlayer.character.Address) return;

    const AnimPack& p = k_anim_packs[pack_idx];

    // Find Humanoid → Animator
    sdk::instance humanoid = global::LocalPlayer.humanoid;
    if (!humanoid.Address) return;

    sdk::instance animator = humanoid.child("Animator");
    if (!animator.Address) return;

    // The animation IDs are stored in AnimationTrack instances under Animator.
    // We use the AnimationId property offset to overwrite each playing animation.
    // Map of animation slot name → new ID
    struct Slot { const char* name; uint64_t id; };
    Slot slots[] = {
        {"run",       p.run},   {"walk",      p.walk},
        {"jump",      p.jump},  {"idle",      p.idle1},
        {"fall",      p.fall},  {"swim",      p.swim},
        {"climb",     p.climb},
    };

    auto children = animator.children();
    for (auto& child : children) {
        if (!child.Address) continue;
        std::string name = child.name();
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);

        for (auto& slot : slots) {
            if (name.find(slot.name) != std::string::npos) {
                // AnimationId offset holds a uint64 asset ID
                drive->write<uint64_t>(child.Address + offset::misc::AnimationId,
                    slot.id);
                break;
            }
        }
    }
}

void anim_changer()
{
    static int last_applied = -1;
    while (true) {
        Sleep(100);
        if (!global::misc::AnimChanger || !global::model.Address) {
            if (last_applied != 0) { apply_anim_pack(0); last_applied = 0; }
            Sleep(200);
            continue;
        }
        HWND rbx = FindWindowA(nullptr, "Roblox");
        bool focused = rbx && GetForegroundWindow() == rbx;
        bool active = false;
        if (global::misc::AnimKey != ImGuiKey_None && focused) {
            int vk = misckey(global::misc::AnimKey);
            if (vk) active = (GetAsyncKeyState(vk) & 0x8000) != 0;
        } else {
            active = global::misc::AnimChanger;
        }

        int target_pack = active ? global::misc::AnimPack : 0;
        if (target_pack != last_applied) {
            apply_anim_pack(target_pack);
            last_applied = target_pack;
        }
    }
}

// ── Lighting / Clock Time ─────────────────────────────────────────────────────
// Ported from omc0eg lighting_exp.cpp — physically-based sun/moon position math
// writes directly to the Lighting instance via driver.

static const float k_pi     = 3.1415927f;
static const float k_day    = 86400.f;
static const float k_earth  = 0.4101523742186675f; // earth tilt rad
static const float k_moon_t = 0.089803443f;        // moon tilt rad
static const float k_solar_year = 31558152.96f;
static const float k_half_solar = 182.6282f;
static const float k_lunar_mo   = 2551442.8f;

struct C3 { float r,g,b; };
static C3 lerp3(C3 a, C3 b, float t){
    return {a.r+(b.r-a.r)*t, a.g+(b.g-a.g)*t, a.b+(b.b-a.b)*t};
}
static C3 spline3(float t, const float* times, const C3* cols, int n) {
    if (t <= times[0])   return cols[0];
    if (t >= times[n-1]) return cols[n-1];
    for (int i=0;i<n-1;i++) {
        if (t>=times[i] && t<=times[i+1]) {
            float a = (t-times[i])/(times[i+1]-times[i]);
            return lerp3(cols[i], cols[i+1], a);
        }
    }
    return cols[0];
}
static C3 sky_ambient(float t) {
    const float H=3600.f,SR=6*H,SS=18*H,ST=H;
    static const float ts[]={0,SR-2*H,SR-H,SR-H/2,SR,SR+ST,SS-ST,SS,SS+H/3,k_day};
    static const C3   cs[]={{0,0,0},{0,0,0},{.07f,.07f,.1f},{.2f,.15f,.01f},
        {.2f,.15f,.01f},{1,1,1},{1,1,1},{.4f,.2f,.05f},{0,0,0},{0,0,0}};
    return spline3(t,ts,cs,10);
}
static C3 sky_ambient2(float t) {
    const float H=3600.f,SR=6*H,SS=18*H,ST=H;
    static const float ts[]={0,SR-3*H,SR-2*H,SR-H/2,SR,SR+ST,SS-ST,SS,SS+H/3,SS+2*H,SS+3*H,k_day};
    static const C3   cs[]={{0,0,0},{0,0,0},{.21f,.21f,.28f},{.4f,.3f,.3f},{.3f,.2f,.3f},
        {1,1,1},{1,1,1},{.4f,.3f,.2f},{.3f,.2f,.3f},{.3f,.2f,.3f},{0,0,0},{0,0,0}};
    return spline3(t,ts,cs,12);
}
static C3 light_color(float t) {
    const float H=3600.f,SR=6*H,SS=18*H,ST=H;
    static const float ts[]={0,SR-H,SR,SR+ST/4,SR+ST,SS-ST,SS-ST/2,SS,SS+H/2,k_day};
    static const C3   cs[]={{.2f,.2f,.2f},{.1f,.1f,.1f},{0,0,0},{.6f,.6f,0},
        {.75f,.75f,.75f},{.75f,.75f,.75f},{.1f,.1f,.075f},{.1f,.05f,.05f},{.1f,.1f,.1f},{.2f,.2f,.2f}};
    return spline3(t,ts,cs,10);
}

static void write_clock(float time_seconds)
{
    if (!global::light.Address) return;

    float tod = time_seconds - floorf(time_seconds/k_day)*k_day;
    float angle = (tod*2.f*k_pi)/k_day;

    // Sun/moon position
    sdk::vector3 sun_pos  = { sinf(angle),       -cosf(angle),       0.f };
    sdk::vector3 moon_pos = { sinf(angle+k_pi),  -cosf(angle+k_pi),  0.f };

    // Apply earth tilt to sun
    float lat_rad  = 51.5f * k_pi / 180.f; // London latitude default
    float day_of_y = (time_seconds - floorf(time_seconds/k_solar_year)*time_seconds) / k_day;
    float sun_off  = -k_earth*cosf((day_of_y-k_half_solar)*k_pi/k_half_solar) - lat_rad;

    auto axis_norm = [](sdk::vector3 v) -> sdk::vector3 {
        float len = v.magnitude(); if (len<1e-6f) return {0,0,1}; return v/len; };
    sdk::vector3 sun_axis = axis_norm({
        0.f*sun_pos.z - 1.f*sun_pos.y,
        1.f*sun_pos.x - 0.f*sun_pos.z,
        0.f*sun_pos.y - 1.f*sun_pos.x});
    // Rotate sun around axis by sun_off (simplified: just offset y component)
    float true_sun_y = sun_pos.y * cosf(sun_off) - sun_pos.z * sinf(sun_off);
    sdk::vector3 true_sun = { sun_pos.x, true_sun_y, sun_pos.z };

    // Moon phase
    float moon_ph = floorf(time_seconds/k_lunar_mo)*2.f*k_pi;
    sdk::vector3 moon_ph_pos = { sinf(moon_ph+angle), -cosf(moon_ph+angle), 0.f };
    float moon_off = ((-k_earth+k_moon_t)*sinf(moon_ph/2.f)) - lat_rad;
    float true_moon_y = moon_ph_pos.y*cosf(moon_off) - moon_ph_pos.z*sinf(moon_off);
    sdk::vector3 true_moon = { moon_ph_pos.x, true_moon_y, moon_ph_pos.z };

    // Light direction = sun if above horizon, else moon
    sdk::vector3 light_dir = (true_sun.y > -0.3f) ? true_sun : true_moon;

    C3 lc  = light_color(tod);
    C3 sa1 = sky_ambient(tod);
    C3 sa2 = sky_ambient2(tod);

    // Write to lighting instance
    using namespace offset::light;
    drive->write<sdk::vector3>(global::light.Address + LightDirection, light_dir);
    drive->write<float>(global::light.Address + ClockTime,
        tod / 3600.f);  // ClockTime is in hours
    drive->write<float>(global::light.Address + Brightness,     1.f);
    drive->write<sdk::vector3>(global::light.Address + SunPosition,  true_sun);
    drive->write<sdk::vector3>(global::light.Address + MoonPosition, true_moon);
    // Ambient colors stored as Color3 (3 floats)
    drive->write<float>(global::light.Address + Ambient+0, sa1.r);
    drive->write<float>(global::light.Address + Ambient+4, sa1.g);
    drive->write<float>(global::light.Address + Ambient+8, sa1.b);
    drive->write<float>(global::light.Address + OutdoorAmbient+0, sa2.r);
    drive->write<float>(global::light.Address + OutdoorAmbient+4, sa2.g);
    drive->write<float>(global::light.Address + OutdoorAmbient+8, sa2.b);
    drive->write<float>(global::light.Address + LightColor+0, lc.r);
    drive->write<float>(global::light.Address + LightColor+4, lc.g);
    drive->write<float>(global::light.Address + LightColor+8, lc.b);
}

void lighting()
{
    while (true) {
        Sleep(16);
        if (!global::misc::Lighting || !global::misc::ClockTime || !global::model.Address) {
            Sleep(100); continue;
        }
        HWND rbx = FindWindowA(nullptr, "Roblox");
        bool focused = rbx && GetForegroundWindow() == rbx;
        bool active = false;
        if (global::misc::ClockKey != ImGuiKey_None && focused) {
            int vk = misckey(global::misc::ClockKey);
            if (vk) active = (GetAsyncKeyState(vk) & 0x8000) != 0;
        } else {
            active = global::misc::ClockTime;
        }
        if (active)
            write_clock(global::misc::ClockTimeValue);
    }
}

} // namespace misc

// Real run() — all functions visible here
namespace misc {
    void run()
    {
        std::thread(fly).detach();
        std::thread(walkspeed).detach();
        std::thread(hitbox).detach();
        std::thread(desync).detach();
        std::thread(tickrate).detach();
        std::thread(anim_changer).detach();
        std::thread(lighting).detach();
    }
}