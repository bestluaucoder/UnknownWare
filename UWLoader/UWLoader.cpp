/*
 * UWLoader - Launcher for rbx-external
 * made by @n1kvz  |  credits to je.rk for motivation
 *
 * Embeds rbx-external.exe as a resource, extracts + launches it.
 */

// Force ANSI APIs so process enumeration works without wide casts
#undef  UNICODE
#undef  _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <TlHelp32.h>
#include <ShlObj.h>
#include <commdlg.h>   // OPENFILENAMEA / GetOpenFileNameA
#include <shellapi.h>
#include <d3d11.h>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <thread>
#include <atomic>
#include <sstream>
#include <cstring>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "advapi32.lib")

#include "../src/Dependencies/ImGui/imgui.h"
#include "../src/Dependencies/ImGui/imgui_internal.h"
#include "../src/Dependencies/ImGui/backends/imgui_impl_win32.h"
#include "../src/Dependencies/ImGui/backends/imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// ── Embedded exe resource ID ──────────────────────────────────────────────────
// The .rc file defines: IDR_MAINEXE RCDATA "rbx-external.exe"
#define IDR_MAINEXE 101

// ── Helpers ───────────────────────────────────────────────────────────────────
static std::string appdata_dir() {
    char buf[MAX_PATH]{};
    SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, buf);
    std::string p = std::string(buf) + "\\UWLoader";
    CreateDirectoryA(p.c_str(), nullptr);
    return p;
}
static std::string cfg_path()     { return appdata_dir() + "\\settings.ini"; }
static std::string extract_path() { return appdata_dir() + "\\rbx-external.exe"; }

static std::string ini_read(const std::string& p, const std::string& k, const std::string& def) {
    char buf[512]{}; GetPrivateProfileStringA("UWLoader", k.c_str(), def.c_str(), buf, sizeof(buf), p.c_str()); return buf;
}
static void ini_write(const std::string& p, const std::string& k, const std::string& v) {
    WritePrivateProfileStringA("UWLoader", k.c_str(), v.c_str(), p.c_str());
}

static bool proc_running(const char* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W e{}; e.dwSize = sizeof(e);
    // Convert name to wide for comparison
    wchar_t wname[MAX_PATH]{};
    MultiByteToWideChar(CP_ACP, 0, name, -1, wname, MAX_PATH);
    bool found = false;
    if (Process32FirstW(snap, &e)) do {
        if (_wcsicmp(e.szExeFile, wname) == 0) { found = true; break; }
    } while (Process32NextW(snap, &e));
    CloseHandle(snap);
    return found;
}

// Extract embedded exe from resource → disk, returns false on failure
static bool extract_exe(HINSTANCE hInst) {
    HRSRC   hRes  = FindResourceA(hInst, MAKEINTRESOURCEA(IDR_MAINEXE), "RCDATA");
    if (!hRes) return false;  // resource not linked yet — skip silently
    HGLOBAL hGlob = LoadResource(hInst, hRes);
    if (!hGlob) return false;
    DWORD   sz    = SizeofResource(hInst, hRes);
    void*   data  = LockResource(hGlob);
    if (!data || !sz) return false;
    std::ofstream f(extract_path(), std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write((char*)data, sz);
    return f.good();
}

// ── Account store ─────────────────────────────────────────────────────────────
struct Account { char name[64]{}; char cookie[512]{}; };
static std::vector<Account> g_accounts;
static int g_sel_account = -1;

static std::string accounts_path() { return appdata_dir() + "\\accounts.dat"; }
static void save_accounts() {
    std::ofstream f(accounts_path(), std::ios::binary | std::ios::trunc);
    if (!f) return;
    int n = (int)g_accounts.size(); f.write((char*)&n, sizeof(n));
    for (auto& a : g_accounts) f.write((char*)&a, sizeof(a));
}
static void load_accounts() {
    std::ifstream f(accounts_path(), std::ios::binary);
    if (!f) return;
    int n = 0; f.read((char*)&n, sizeof(n));
    g_accounts.resize(n);
    for (auto& a : g_accounts) f.read((char*)&a, sizeof(a));
}

// ── D3D11 state ───────────────────────────────────────────────────────────────
static HWND                     g_hwnd  = nullptr;
static ID3D11Device*            g_dev   = nullptr;
static ID3D11DeviceContext*     g_ctx   = nullptr;
static IDXGISwapChain*          g_swap  = nullptr;
static ID3D11RenderTargetView*  g_rtv   = nullptr;
static bool                     g_resize= false;
static HINSTANCE                g_inst  = nullptr;

static void create_rtv() {
    ID3D11Texture2D* bb = nullptr;
    g_swap->GetBuffer(0, IID_PPV_ARGS(&bb));
    g_dev->CreateRenderTargetView(bb, nullptr, &g_rtv);
    bb->Release();
}
static void drop_rtv() { if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; } }

static LRESULT CALLBACK wnd_proc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hw, msg, wp, lp)) return TRUE;
    if (msg == WM_SIZE && wp != SIZE_MINIMIZED) g_resize = true;
    if (msg == WM_SYSCOMMAND && (wp & 0xfff0) == SC_KEYMENU) return 0;
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcA(hw, msg, wp, lp);
}

// ── App state ─────────────────────────────────────────────────────────────────
static char  g_exe_path[MAX_PATH]  = {};
static char  g_status[256]         = "Ready.";
static bool  g_roblox_running      = false;
static std::atomic<bool> g_launching{false};
static bool  g_wait_roblox         = true;
static bool  g_close_after_launch  = false;
static int   g_theme               = 0;   // 0=green  1=purple
static char  g_new_acc_name[64]    = {};
static char  g_new_acc_cookie[512] = {};

static void settings_load() {
    auto p = cfg_path();
    strncpy_s(g_exe_path, ini_read(p,"ExePath","").c_str(), MAX_PATH-1);
    g_wait_roblox        = ini_read(p,"WaitRoblox","1")   == "1";
    g_close_after_launch = ini_read(p,"CloseAfter","0")   == "0" ? false : true;
    g_theme              = std::stoi(ini_read(p,"Theme","0"));
    load_accounts();
}
static void settings_save() {
    auto p = cfg_path();
    ini_write(p,"ExePath",    g_exe_path);
    ini_write(p,"WaitRoblox", g_wait_roblox        ? "1":"0");
    ini_write(p,"CloseAfter", g_close_after_launch ? "1":"0");
    ini_write(p,"Theme",      std::to_string(g_theme));
    save_accounts();
}

// ── Theme ─────────────────────────────────────────────────────────────────────
static ImVec4 acc_col() {
    return g_theme==1 ? ImVec4(0.51f,0.35f,1.f,1.f) : ImVec4(0.64f,0.83f,0.12f,1.f);
}
static ImU32  acc_u32() {
    auto c=acc_col();
    return IM_COL32((int)(c.x*255),(int)(c.y*255),(int)(c.z*255),255);
}

static void apply_theme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = s.FrameRounding = s.PopupRounding = s.ChildRounding = 8.f;
    s.ScrollbarRounding = s.GrabRounding = 6.f;
    s.WindowBorderSize = s.ChildBorderSize = 1.f;
    s.FramePadding  = ImVec2(8.f,5.f);
    s.ItemSpacing   = ImVec2(8.f,6.f);
    s.ScrollbarSize = 8.f;

    ImVec4 bg  = ImVec4(0.055f,0.055f,0.07f,1.f);
    ImVec4 bg2 = ImVec4(0.08f, 0.08f, 0.10f,1.f);
    ImVec4 bg3 = ImVec4(0.11f, 0.11f, 0.14f,1.f);
    ImVec4 brd = ImVec4(1.f,1.f,1.f,0.10f);
    ImVec4 acc = acc_col();

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]         = bg;
    c[ImGuiCol_ChildBg]          = bg2;
    c[ImGuiCol_PopupBg]          = bg2;
    c[ImGuiCol_Border]           = brd;
    c[ImGuiCol_FrameBg]          = bg3;
    c[ImGuiCol_FrameBgHovered]   = ImVec4(bg3.x+.04f,bg3.y+.04f,bg3.z+.04f,1.f);
    c[ImGuiCol_FrameBgActive]    = ImVec4(acc.x*.3f,acc.y*.3f,acc.z*.3f,1.f);
    c[ImGuiCol_TitleBg]          = bg;
    c[ImGuiCol_TitleBgActive]    = bg2;
    c[ImGuiCol_ScrollbarBg]      = ImVec4(0,0,0,0);
    c[ImGuiCol_ScrollbarGrab]    = ImVec4(acc.x*.4f,acc.y*.4f,acc.z*.4f,1.f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(acc.x*.6f,acc.y*.6f,acc.z*.6f,1.f);
    c[ImGuiCol_ScrollbarGrabActive]  = acc;
    c[ImGuiCol_CheckMark]        = acc;
    c[ImGuiCol_SliderGrab]       = acc;
    c[ImGuiCol_SliderGrabActive] = ImVec4(acc.x+.1f,acc.y+.1f,acc.z+.1f,1.f);
    c[ImGuiCol_Button]           = ImVec4(acc.x*.18f,acc.y*.18f,acc.z*.18f,1.f);
    c[ImGuiCol_ButtonHovered]    = ImVec4(acc.x*.32f,acc.y*.32f,acc.z*.32f,1.f);
    c[ImGuiCol_ButtonActive]     = ImVec4(acc.x*.52f,acc.y*.52f,acc.z*.52f,1.f);
    c[ImGuiCol_Header]           = ImVec4(acc.x*.22f,acc.y*.22f,acc.z*.22f,1.f);
    c[ImGuiCol_HeaderHovered]    = ImVec4(acc.x*.35f,acc.y*.35f,acc.z*.35f,1.f);
    c[ImGuiCol_HeaderActive]     = ImVec4(acc.x*.50f,acc.y*.50f,acc.z*.50f,1.f);
    c[ImGuiCol_Tab]              = bg3;
    c[ImGuiCol_TabHovered]       = ImVec4(acc.x*.35f,acc.y*.35f,acc.z*.35f,1.f);
    c[ImGuiCol_TabActive]        = ImVec4(acc.x*.28f,acc.y*.28f,acc.z*.28f,1.f);
    c[ImGuiCol_Separator]        = brd;
    c[ImGuiCol_SeparatorHovered] = ImVec4(acc.x*.5f,acc.y*.5f,acc.z*.5f,1.f);
    c[ImGuiCol_SeparatorActive]  = acc;
    c[ImGuiCol_Text]             = ImVec4(0.92f,0.92f,0.95f,1.f);
    c[ImGuiCol_TextDisabled]     = ImVec4(0.45f,0.45f,0.50f,1.f);
    c[ImGuiCol_ResizeGrip]       = ImVec4(0,0,0,0);
}

// ── Launch logic ──────────────────────────────────────────────────────────────
static void do_launch() {
    g_launching = true;

    // First try to extract embedded exe if present
    std::string target = extract_path();
    if (extract_exe(g_inst)) {
        snprintf(g_status, sizeof(g_status), "Extracted rbx-external.exe");
    } else if (g_exe_path[0] != '\0') {
        // Fall back to manually specified path
        target = g_exe_path;
    } else {
        snprintf(g_status, sizeof(g_status), "No exe found. Set path in Settings.");
        g_launching = false; return;
    }

    if (!std::filesystem::exists(target)) {
        snprintf(g_status, sizeof(g_status), "Exe not found: %s", target.c_str());
        g_launching = false; return;
    }

    if (g_wait_roblox) {
        snprintf(g_status, sizeof(g_status), "Waiting for Roblox...");
        int waited = 0;
        while (!proc_running("RobloxPlayerBeta.exe") && waited < 30) {
            Sleep(1000); waited++;
        }
        if (!proc_running("RobloxPlayerBeta.exe")) {
            snprintf(g_status, sizeof(g_status), "Roblox not detected after 30s");
            g_launching = false; return;
        }
        Sleep(3000);
    }

    snprintf(g_status, sizeof(g_status), "Launching...");
    std::string dir = std::filesystem::path(target).parent_path().string();
    std::string cmd = "\"" + target + "\"";
    STARTUPINFOA si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    bool ok = CreateProcessA(nullptr, (LPSTR)cmd.c_str(), nullptr, nullptr,
        FALSE, 0, nullptr, dir.c_str(), &si, &pi) != FALSE;
    if (ok) {
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        snprintf(g_status, sizeof(g_status), "Launched successfully!");
        if (g_close_after_launch) PostQuitMessage(0);
    } else {
        snprintf(g_status, sizeof(g_status), "Launch failed (error %lu)", GetLastError());
    }
    g_launching = false;
}

// ── UI ────────────────────────────────────────────────────────────────────────
static void render_ui() {
    apply_theme();
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f,0.f));
    ImGui::Begin("##root", nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoResize|
        ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 ws = ImGui::GetWindowSize();
    const float TH = 40.f;

    // Title bar background
    dl->AddRectFilled(wp, wp+ImVec2(ws.x,TH), IM_COL32(10,10,14,255));
    // Accent bottom line on header
    dl->AddRectFilled(wp+ImVec2(0.f,TH-2.f), wp+ImVec2(ws.x,TH), acc_u32());

    // Title
    float fsz = ImGui::GetFontSize();
    dl->AddText(wp+ImVec2(14.f,(TH-fsz)*.5f), IM_COL32(230,230,235,255), "UW External");

    // Credits (right-aligned)
    const char* cred = "made by @n1kvz  |  credits to je.rk";
    float cw = ImGui::CalcTextSize(cred).x;
    dl->AddText(wp+ImVec2(ws.x-cw-14.f,(TH-fsz)*.5f), IM_COL32(80,80,95,255), cred);

    // Drag title bar
    ImGui::SetCursorScreenPos(wp);
    ImGui::InvisibleButton("##drag", ImVec2(ws.x-48.f, TH));
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        RECT wr; GetWindowRect(g_hwnd, &wr);
        SetWindowPos(g_hwnd, nullptr,
            wr.left+(int)io.MouseDelta.x, wr.top+(int)io.MouseDelta.y,
            0,0, SWP_NOSIZE|SWP_NOZORDER);
    }

    // Close button
    ImGui::SetCursorScreenPos(wp+ImVec2(ws.x-46.f,4.f));
    ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(55,15,15,220));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(200,35,35,255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(240,55,55,255));
    if (ImGui::Button("X##close", ImVec2(36.f,32.f))) PostQuitMessage(0);
    ImGui::PopStyleColor(3);

    // Body area
    ImGui::SetCursorScreenPos(wp+ImVec2(0.f,TH+4.f));
    ImGui::BeginChild("##body", ImVec2(ws.x, ws.y-TH-4.f), false);

    // Status pill
    ImGui::SetCursorPosX(10.f); ImGui::SetCursorPosY(8.f);
    g_roblox_running = proc_running("RobloxPlayerBeta.exe");
    ImVec2 sp = ImGui::GetCursorScreenPos();
    float sw = ImGui::CalcTextSize(g_status).x + 22.f;
    ImU32 pill_bg  = g_roblox_running ? IM_COL32(20,70,20,200) : IM_COL32(55,25,8,200);
    ImU32 pill_brd = g_roblox_running ? IM_COL32(50,190,50,180) : IM_COL32(190,110,35,180);
    dl->AddRectFilled(sp, sp+ImVec2(sw,22.f), pill_bg, 11.f);
    dl->AddRect(sp, sp+ImVec2(sw,22.f), pill_brd, 11.f, 0, 1.f);
    dl->AddText(sp+ImVec2(11.f,4.f), IM_COL32(215,215,215,255), g_status);
    ImGui::Dummy(ImVec2(sw,22.f));
    ImGui::Spacing();

    if (ImGui::BeginTabBar("##tabs")) {

        // ── LAUNCH ───────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Launch")) {
            ImGui::Spacing();
            bool has_embedded = FindResourceA(g_inst, MAKEINTRESOURCEA(IDR_MAINEXE), "RCDATA") != nullptr;
            if (has_embedded) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f,1.f,0.5f,1.f));
                ImGui::Text("rbx-external.exe embedded in loader");
                ImGui::PopStyleColor();
            } else {
                ImGui::TextDisabled("No embedded exe — set path manually:");
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80.f);
                ImGui::InputTextWithHint("##exe", "Path to rbx-external.exe", g_exe_path, MAX_PATH);
                ImGui::SameLine();
                if (ImGui::Button("Browse", ImVec2(74.f,0.f))) {
                    char path[MAX_PATH]{};
                    OPENFILENAMEA ofn{};
                    ofn.lStructSize = sizeof(ofn);
                    ofn.lpstrFilter = "Exe Files (*.exe)\0*.exe\0\0";
                    ofn.lpstrFile   = path;
                    ofn.nMaxFile    = MAX_PATH;
                    ofn.Flags       = OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;
                    if (GetOpenFileNameA(&ofn))
                        strncpy_s(g_exe_path, path, MAX_PATH-1);
                }
            }
            ImGui::Spacing();
            ImGui::Checkbox("Wait for Roblox to open first", &g_wait_roblox);
            ImGui::Checkbox("Close loader after launch",     &g_close_after_launch);
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

            bool busy = g_launching.load();
            if (busy) ImGui::BeginDisabled();
            ImVec4 a = acc_col();
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(a.x*.22f,a.y*.22f,a.z*.22f,1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(a.x*.40f,a.y*.40f,a.z*.40f,1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(a.x*.60f,a.y*.60f,a.z*.60f,1.f));
            if (ImGui::Button(busy?"Launching...":"LAUNCH", ImVec2(-1.f,44.f)))
                std::thread(do_launch).detach();
            ImGui::PopStyleColor(3);
            if (busy) ImGui::EndDisabled();
            ImGui::EndTabItem();
        }

        // ── ACCOUNTS ─────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Accounts")) {
            ImGui::Spacing();
            ImGui::TextDisabled("Store .ROBLOSECURITY cookies for quick account switching.");
            ImGui::Spacing();
            ImGui::BeginChild("##acclist", ImVec2(-1.f,150.f), true);
            for (int i = 0; i < (int)g_accounts.size(); i++) {
                char lbl[96];
                snprintf(lbl, sizeof(lbl), "%s##a%d",
                    g_accounts[i].name[0] ? g_accounts[i].name : "(unnamed)", i);
                if (ImGui::Selectable(lbl, g_sel_account==i))
                    g_sel_account = i;
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Delete")) {
                        g_accounts.erase(g_accounts.begin()+i);
                        if (g_sel_account >= (int)g_accounts.size()) g_sel_account = -1;
                        save_accounts();
                    }
                    ImGui::EndPopup();
                }
            }
            ImGui::EndChild();
            if (g_sel_account>=0 && g_sel_account<(int)g_accounts.size()) {
                ImGui::Spacing();
                if (ImGui::Button("Copy Cookie to Clipboard", ImVec2(-1.f,0.f))) {
                    const char* ck = g_accounts[g_sel_account].cookie;
                    if (OpenClipboard(g_hwnd)) {
                        EmptyClipboard();
                        HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, strlen(ck)+1);
                        if (hg) { memcpy(GlobalLock(hg),ck,strlen(ck)+1); GlobalUnlock(hg); SetClipboardData(CF_TEXT,hg); }
                        CloseClipboard();
                        snprintf(g_status, sizeof(g_status), "Cookie copied to clipboard.");
                    }
                }
                ImGui::TextDisabled("Paste into Roblox browser cookie store");
            }
            ImGui::Separator(); ImGui::Spacing(); ImGui::Text("Add account:");
            ImGui::SetNextItemWidth(150.f);
            ImGui::InputTextWithHint("##an","Display name",g_new_acc_name,64);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-80.f);
            ImGui::InputTextWithHint("##ac",".ROBLOSECURITY",g_new_acc_cookie,512,
                ImGuiInputTextFlags_Password);
            ImGui::SameLine();
            if (ImGui::Button("Add",ImVec2(-1.f,0.f)) && g_new_acc_cookie[0]) {
                Account a{}; strncpy_s(a.name,g_new_acc_name,63); strncpy_s(a.cookie,g_new_acc_cookie,511);
                g_accounts.push_back(a); g_new_acc_name[0]=g_new_acc_cookie[0]='\0'; save_accounts();
            }
            ImGui::EndTabItem();
        }

        // ── SETTINGS ─────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Settings")) {
            ImGui::Spacing();
            ImGui::Text("Theme:");
            ImGui::SameLine(90.f);
            if (ImGui::RadioButton("Green",  g_theme==0)) { g_theme=0; settings_save(); }
            ImGui::SameLine();
            if (ImGui::RadioButton("Purple", g_theme==1)) { g_theme=1; settings_save(); }
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            ImGui::Text("Override exe path (if not embedded):");
            ImGui::SetNextItemWidth(-1.f);
            if (ImGui::InputText("##exeset",g_exe_path,MAX_PATH)) settings_save();
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            if (ImGui::Checkbox("Wait for Roblox on launch",  &g_wait_roblox))        settings_save();
            if (ImGui::Checkbox("Close loader after launch",  &g_close_after_launch)) settings_save();
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            if (ImGui::Button("Save Settings",ImVec2(-1.f,0.f))) settings_save();
            ImGui::EndTabItem();
        }

        // ── ABOUT ─────────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("About")) {
            ImGui::Spacing();
            ImVec4 a = acc_col();
            ImGui::TextColored(a, "UW External Loader");
            ImGui::Spacing();
            ImGui::TextDisabled("made by @n1kvz");
            ImGui::TextDisabled("credits to je.rk for motivation");
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            ImGui::TextDisabled("Build: " __DATE__ "  " __TIME__);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::EndChild();
    ImGui::End();
}

// ── Entry point ───────────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    g_inst = hInst;
    settings_load();

    WNDCLASSEXA wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = wnd_proc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "UWLoader";
    RegisterClassExA(&wc);

    const int W=520, H=420;
    g_hwnd = CreateWindowExA(WS_EX_APPWINDOW, "UWLoader", "UW Loader",
        WS_POPUP|WS_VISIBLE,
        (GetSystemMetrics(SM_CXSCREEN)-W)/2,
        (GetSystemMetrics(SM_CYSCREEN)-H)/2,
        W, H, nullptr, nullptr, hInst, nullptr);

    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount       = 2;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.Width  = W; scd.BufferDesc.Height = H;
    scd.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow      = g_hwnd;
    scd.SampleDesc.Count  = 1;
    scd.Windowed          = TRUE;
    scd.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL fl;
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &scd, &g_swap, &g_dev, &fl, &g_ctx);
    create_rtv();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_dev, g_ctx);

    MSG msg{};
    while (msg.message != WM_QUIT) {
        if (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessageA(&msg); continue;
        }
        if (g_resize) { drop_rtv(); g_swap->ResizeBuffers(0,0,0,DXGI_FORMAT_UNKNOWN,0); create_rtv(); g_resize=false; }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        render_ui();
        ImGui::Render();

        float cc[4]{0.05f,0.05f,0.07f,1.f};
        g_ctx->OMSetRenderTargets(1,&g_rtv,nullptr);
        g_ctx->ClearRenderTargetView(g_rtv,cc);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swap->Present(1,0);
    }

    settings_save();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    drop_rtv();
    if (g_swap) g_swap->Release();
    if (g_ctx)  g_ctx->Release();
    if (g_dev)  g_dev->Release();
    DestroyWindow(g_hwnd);
    UnregisterClassA("UWLoader", hInst);
    return 0;
}
