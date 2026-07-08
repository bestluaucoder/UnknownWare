#include <dwmapi.h>
#include <cstdio>
#include <chrono>
#include <thread>
#include <d3d11.h>
#include <wincodec.h>
#include <vector>
#include <string>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <algorithm>
#include <utility>
#include <cmath>
#include <cfloat>
#include <cstring>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")

#include "ui/graphic.h"
#include "ui/notify.h"
#include "feature/cache.h"
#include "feature/game.h"
#include "global.h"

#include <misc/imgui_freetype.h>
#include <imgui_internal.h>

#include "ui/font/regular.h"
#include "ui/font/bold.h"
#include "ui/idalovesme/ida_assets.h"
#include "feature/esp.h"
#include "feature/misc.h"
#include "feature/silent.h"
#include "feature/media.h"
#include <addons/Colors/Colors.h>
#include "config.h"

#pragma comment(lib, "windowscodecs.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);

static ID3D11ShaderResourceView* texturefrompng(ID3D11Device* device, const BYTE* data, unsigned int size);

LRESULT CALLBACK wndproc(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
    if (ImGui_ImplWin32_WndProcHandler(Hwnd, Msg, WParam, LParam))
        return true;

    switch (Msg)
    {
    case WM_SYSCOMMAND:
        if ((WParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_SYSKEYDOWN:
        if (WParam == VK_F4) { DestroyWindow(Hwnd); return 0; }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_CLOSE:
        return 0;
    }
    return DefWindowProcA(Hwnd, Msg, WParam, LParam);
}


graphic::graphic() { Detail = std::make_unique<detail>(); }
graphic::~graphic() { dropimgui(); dropwindow(); dropdevice(); }

bool graphic::window()
{
    Detail->WindowClass.cbSize = sizeof(Detail->WindowClass);
    Detail->WindowClass.style = CS_CLASSDC;
    Detail->WindowClass.lpszClassName = "gs-remake by autopsy.lol";
    Detail->WindowClass.hInstance = GetModuleHandleA(0);
    Detail->WindowClass.lpfnWndProc = wndproc;
    RegisterClassExA(&Detail->WindowClass);

    Detail->Window = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        Detail->WindowClass.lpszClassName, "gs-remake by autopsy.lol", WS_POPUP,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        0, 0, Detail->WindowClass.hInstance, 0);

    if (!Detail->Window) return false;

    SetLayeredWindowAttributes(Detail->Window, RGB(0, 0, 0), BYTE(255), LWA_ALPHA);

    RECT ClientArea{}, WindowArea{};
    GetClientRect(Detail->Window, &ClientArea);
    GetWindowRect(Detail->Window, &WindowArea);
    POINT Diff{};
    ClientToScreen(Detail->Window, &Diff);

    MARGINS Margins{
        WindowArea.left + (Diff.x - WindowArea.left),
        WindowArea.top + (Diff.y - WindowArea.top),
        WindowArea.right, WindowArea.bottom
    };
    DwmExtendFrameIntoClientArea(Detail->Window, &Margins);
    ShowWindow(Detail->Window, SW_SHOW);
    UpdateWindow(Detail->Window);
    return true;
}

bool graphic::device()
{
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.RefreshRate.Numerator = 0;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.OutputWindow = Detail->Window;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.Windowed = 1;
    sd.Flags = 0;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

    D3D_FEATURE_LEVEL fl;
    const D3D_FEATURE_LEVEL fll[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        fll, 2, D3D11_SDK_VERSION, &sd, &Detail->SwapChain, &Detail->Device, &fl, &Detail->DeviceContext);

    if (hr == DXGI_ERROR_UNSUPPORTED)
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
            fll, 2, D3D11_SDK_VERSION, &sd, &Detail->SwapChain, &Detail->Device, &fl, &Detail->DeviceContext);

    if (FAILED(hr) || !Detail->SwapChain || !Detail->Device || !Detail->DeviceContext)
    {
        MessageBoxA(nullptr, "This software can not run on your computer.", "Critical Problem", MB_ICONERROR | MB_OK);
        return false;
    }

    ID3D11Texture2D* bb = nullptr;
    if (FAILED(Detail->SwapChain->GetBuffer(0, IID_PPV_ARGS(&bb))) || !bb)
        return false;

    if (bb) {
        HRESULT rtv = Detail->Device->CreateRenderTargetView(bb, nullptr, &Detail->GraphicsTargetView);
        bb->Release();
        return SUCCEEDED(rtv) && Detail->GraphicsTargetView;
    }
    return false;
}

bool graphic::imgui()
{
    ImGui::CreateContext();

    float dpiScale = ImGui_ImplWin32_GetDpiScaleForMonitor(
        ::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    ImGuiIO& IO = ImGui::GetIO();
    IO.IniFilename = nullptr;

    const unsigned int ftFlags =
        ImGuiFreeTypeLoaderFlags_LoadColor |
        ImGuiFreeTypeLoaderFlags_LightHinting;
    IO.Fonts->SetFontLoader(ImGuiFreeType::GetFontLoader());
    IO.Fonts->FontLoaderFlags = ftFlags;

    ImFontConfig cfg{};
    cfg.PixelSnapH = false;
    cfg.OversampleH = 4;
    cfg.OversampleV = 4;
    cfg.RasterizerMultiply = 1.03f;
    cfg.FontLoaderFlags = 0;
    cfg.FontDataOwnedByAtlas = false;

    // Glyph ranges: Basic Latin + Latin Extended + Cyrillic (for Russian track names)
    static const ImWchar glyph_ranges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin Supplement
        0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
        0x2000, 0x206F, // General Punctuation
        0,
    };

    const float sz = 12.0f * dpiScale;
    const char* fontPath = "C:\\Windows\\Fonts\\verdana.ttf";
    const char* fontSemi = "C:\\Windows\\Fonts\\verdanab.ttf";
    const char* fontBold = "C:\\Windows\\Fonts\\tahomabd.ttf";
    const char* fontBlack = "C:\\Windows\\Fonts\\tahomabd.ttf";

    // Tahoma does not have Cyrillic — use Arial or Segoe UI as Cyrillic fallback
    const char* cyrFont = "C:\\Windows\\Fonts\\arial.ttf";

    if (GetFileAttributesA(fontPath) != INVALID_FILE_ATTRIBUTES)
    {
        cfg.GlyphRanges = glyph_ranges;
        UiFont = IO.Fonts->AddFontFromFileTTF(fontPath, sz, &cfg);
        Tahoma_BoldXP = IO.Fonts->AddFontFromFileTTF(
            GetFileAttributesA(fontBold) != INVALID_FILE_ATTRIBUTES ? fontBold : fontPath,
            12.0f * dpiScale, &cfg);
        TitleFont = IO.Fonts->AddFontFromFileTTF(
            GetFileAttributesA(fontBold) != INVALID_FILE_ATTRIBUTES ? fontBold : fontPath,
            12.0f * dpiScale, &cfg);

        // Merge Cyrillic from Arial into UiFont and Tahoma_BoldXP for full coverage
        if (GetFileAttributesA(cyrFont) != INVALID_FILE_ATTRIBUTES) {
            ImFontConfig mergeCfg = cfg;
            mergeCfg.MergeMode  = true;
            mergeCfg.GlyphRanges = glyph_ranges;
            IO.Fonts->AddFontFromFileTTF(cyrFont, sz,            &mergeCfg);
            IO.Fonts->AddFontFromFileTTF(cyrFont, 12.0f*dpiScale, &mergeCfg);
        }
    }
    else
    {
        UiFont = IO.Fonts->AddFontFromMemoryTTF(
            font_regular, (int)font_regular_len, sz, &cfg);
        Tahoma_BoldXP = IO.Fonts->AddFontFromMemoryTTF(
            font_bold, (int)font_bold_len, 12.0f * dpiScale, &cfg);
        TitleFont = IO.Fonts->AddFontFromMemoryTTF(
            font_bold, (int)font_bold_len, 12.0f * dpiScale, &cfg);
    }

    ImFontConfig logoCfg = cfg;
    logoCfg.RasterizerMultiply = 1.05f;
    const char* logoPath = GetFileAttributesA(fontBlack) != INVALID_FILE_ATTRIBUTES ? fontBlack : fontBold;
    const char* logoFallback = GetFileAttributesA(fontSemi) != INVALID_FILE_ATTRIBUTES ? fontSemi : fontPath;
    if (GetFileAttributesA(logoPath) != INVALID_FILE_ATTRIBUTES)
        LogoFont = IO.Fonts->AddFontFromFileTTF(logoPath, 21.0f * dpiScale, &logoCfg);
    else if (GetFileAttributesA(logoFallback) != INVALID_FILE_ATTRIBUTES)
        LogoFont = IO.Fonts->AddFontFromFileTTF(logoFallback, 20.0f * dpiScale, &logoCfg);
    else
        LogoFont = Tahoma_BoldXP;

    ImFontConfig idaTabCfg = cfg;
    idaTabCfg.PixelSnapH = true;
    idaTabCfg.OversampleH = 1;
    idaTabCfg.OversampleV = 1;
    idaTabCfg.RasterizerMultiply = 1.0f;
    idaTabCfg.FontDataOwnedByAtlas = false;
    IdaTabFont = IO.Fonts->AddFontFromMemoryTTF(
        FontsData::TabIcons, FontsData::TabIconsSize, 47.0f * dpiScale, &idaTabCfg);

    ImFontConfig idaLegitCfg = idaTabCfg;
    IdaLegitTabFont = IO.Fonts->AddFontFromMemoryTTF(
        FontsData::LegitTabIcons, FontsData::LegitTabIconsSize, 32.0f * dpiScale, &idaLegitCfg);

    Detail->IdaBgTexture = texturefrompng(
        Detail->Device, TexturesData::BgTexture, TexturesData::BgTextureSize);

    ImGui::StyleColorsDark();
    ImGuiStyle& S = ImGui::GetStyle();
    S.WindowRounding = 0.f;
    S.ChildRounding = 0.f;
    S.FrameRounding = 0.f;
    S.PopupRounding = 0.f;
    S.ScrollbarRounding = 0.f;
    S.GrabRounding = 0.f;
    S.ItemSpacing = { 0.f, 6.f };
    S.FramePadding = { 5.f, 3.f };
    S.WindowPadding = { 0.f, 0.f };
    S.PopupBorderSize = 1.f;
    S.FrameBorderSize = 1.f;
    S.ScrollbarSize = 6.f;

    if (!ImGui_ImplWin32_Init(Detail->Window))                return false;
    if (!Detail->Device || !Detail->DeviceContext)            return false;
    if (!ImGui_ImplDX11_Init(Detail->Device, Detail->DeviceContext)) return false;
    return true;
}

void graphic::dropdevice()
{
    if (Detail->IdaBgTexture) Detail->IdaBgTexture->Release();
    if (Detail->GraphicsTargetView) Detail->GraphicsTargetView->Release();
    if (Detail->SwapChain)          Detail->SwapChain->Release();
    if (Detail->DeviceContext)      Detail->DeviceContext->Release();
    if (Detail->Device)             Detail->Device->Release();
    Detail->IdaBgTexture = nullptr;
    Detail->GraphicsTargetView = nullptr;
    Detail->SwapChain = nullptr;
    Detail->DeviceContext = nullptr;
    Detail->Device = nullptr;
}
void graphic::dropwindow()
{
    if (Detail->Window)
    {
        DestroyWindow(Detail->Window);
        Detail->Window = nullptr;
    }
    if (Detail->WindowClass.lpszClassName)
        UnregisterClassA(Detail->WindowClass.lpszClassName, Detail->WindowClass.hInstance);
}
void graphic::dropimgui()
{
    media::release();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

static bool welcome_done();
static int menukey(ImGuiKey key)
{
    if (key >= ImGuiKey_0 && key <= ImGuiKey_9)
        return '0' + (key - ImGuiKey_0);
    if (key >= ImGuiKey_A && key <= ImGuiKey_Z)
        return 'A' + (key - ImGuiKey_A);
    if (key >= ImGuiKey_F1 && key <= ImGuiKey_F24)
        return VK_F1 + (key - ImGuiKey_F1);
    if (key >= ImGuiKey_Keypad0 && key <= ImGuiKey_Keypad9)
        return VK_NUMPAD0 + (key - ImGuiKey_Keypad0);

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
    case ImGuiKey_Menu: return VK_APPS;
    case ImGuiKey_Apostrophe: return VK_OEM_7;
    case ImGuiKey_Comma: return VK_OEM_COMMA;
    case ImGuiKey_Minus: return VK_OEM_MINUS;
    case ImGuiKey_Period: return VK_OEM_PERIOD;
    case ImGuiKey_Slash: return VK_OEM_2;
    case ImGuiKey_Semicolon: return VK_OEM_1;
    case ImGuiKey_Equal: return VK_OEM_PLUS;
    case ImGuiKey_LeftBracket: return VK_OEM_4;
    case ImGuiKey_Backslash: return VK_OEM_5;
    case ImGuiKey_RightBracket: return VK_OEM_6;
    case ImGuiKey_GraveAccent: return VK_OEM_3;
    case ImGuiKey_CapsLock: return VK_CAPITAL;
    case ImGuiKey_ScrollLock: return VK_SCROLL;
    case ImGuiKey_NumLock: return VK_NUMLOCK;
    case ImGuiKey_PrintScreen: return VK_SNAPSHOT;
    case ImGuiKey_Pause: return VK_PAUSE;
    case ImGuiKey_KeypadDecimal: return VK_DECIMAL;
    case ImGuiKey_KeypadDivide: return VK_DIVIDE;
    case ImGuiKey_KeypadMultiply: return VK_MULTIPLY;
    case ImGuiKey_KeypadSubtract: return VK_SUBTRACT;
    case ImGuiKey_KeypadAdd: return VK_ADD;
    case ImGuiKey_KeypadEnter: return VK_RETURN;
    case ImGuiKey_MouseLeft: return VK_LBUTTON;
    case ImGuiKey_MouseRight: return VK_RBUTTON;
    case ImGuiKey_MouseMiddle: return VK_MBUTTON;
    case ImGuiKey_MouseX1: return VK_XBUTTON1;
    case ImGuiKey_MouseX2: return VK_XBUTTON2;
    default: return 0;
    }
}

void graphic::begin()
{
    MSG Msg;
    while (PeekMessage(&Msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (Msg.message == WM_QUIT)
            ExitProcess(0);
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }

    static bool lastStreamproof = !global::setting::Streamproof;
    if (lastStreamproof != global::setting::Streamproof)
    {
        SetWindowDisplayAffinity(Detail->Window,
            global::setting::Streamproof ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
        lastStreamproof = global::setting::Streamproof;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    {
        HWND _roblox = FindWindowA(0, "Roblox");
        HWND _fg = GetForegroundWindow();
        static bool welcomed = false;
        if (!welcome_done()) {
            welcomed = false;
        } else if (!welcomed) {
            welcomed = true;
            Running = true;
        }

        LONG exStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED;
        if (!Running) exStyle |= WS_EX_TRANSPARENT;
        SetWindowLong(Detail->Window, GWL_EXSTYLE, exStyle);

        static double lastToggle = -1.0;
        const double now = ImGui::GetTime();
        const int menuVk = menukey(global::setting::Menu_Key);
        if (welcome_done() && menuVk != 0 && (GetAsyncKeyState(menuVk) & 1) && (_fg == _roblox || _fg == Detail->Window) && now - lastToggle >= .18)
        {
            lastToggle = now;
            Running = !Running;
        }
    }
}

void graphic::end()
{
    if (!Detail->DeviceContext || !Detail->GraphicsTargetView || !Detail->SwapChain)
        return;

    ImGui::Render();
    const float cc[4]{ 0.f, 0.f, 0.f, 0.f };
    Detail->DeviceContext->OMSetRenderTargets(1, &Detail->GraphicsTargetView, nullptr);
    Detail->DeviceContext->ClearRenderTargetView(Detail->GraphicsTargetView, cc);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    const int mode = ImClamp(global::setting::Performance_Mode, 0, 2);
    static auto lastFrame = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    const auto targetFrame = mode == 0
        ? std::chrono::microseconds(16667)
        : (mode == 2 ? std::chrono::microseconds(6944) : std::chrono::microseconds(0));
    if (targetFrame.count() > 0)
    {
        const auto elapsed = now - lastFrame;
        if (elapsed < targetFrame)
            std::this_thread::sleep_for(targetFrame - elapsed);
        lastFrame = std::chrono::steady_clock::now();
    }
    else
    {
        lastFrame = now;
    }

    const UINT syncInterval = mode == 2 ? 0u : 1u;
    const HRESULT hr = Detail->SwapChain->Present(syncInterval, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        ExitProcess(0);
}

static void cursor()
{
}

static const char* pcuser()
{
    static std::string name = []()
        {
            char buffer[257]{};
            DWORD len = GetEnvironmentVariableA("USERNAME", buffer, (DWORD)sizeof(buffer));
            if (!len || len >= sizeof(buffer))
                return std::string("Windows");
            return std::string(buffer, buffer + len);
        }();
    return name.c_str();
}

static bool copytext(const std::string& text)
{
    if (text.empty() || !OpenClipboard(nullptr))
        return false;

    EmptyClipboard();

    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (!memory)
    {
        CloseClipboard();
        return false;
    }

    void* data = GlobalLock(memory);
    if (!data)
    {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    std::memcpy(data, text.c_str(), text.size() + 1);
    GlobalUnlock(memory);

    if (!SetClipboardData(CF_TEXT, memory))
    {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}

template <typename T>
static void releasecom(T*& p)
{
    if (p)
    {
        p->Release();
        p = nullptr;
    }
}

static ID3D11ShaderResourceView* texturefrompng(ID3D11Device* device, const BYTE* data, unsigned int size)
{
    if (!device || !data || !size)
        return nullptr;

    const HRESULT coinit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    IWICImagingFactory* factory = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    ID3D11Texture2D* texture = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;

    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));

    if (SUCCEEDED(hr))
        hr = factory->CreateStream(&stream);
    if (SUCCEEDED(hr))
        hr = stream->InitializeFromMemory(const_cast<BYTE*>(data), size);
    if (SUCCEEDED(hr))
        hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(hr))
        hr = decoder->GetFrame(0, &frame);
    if (SUCCEEDED(hr))
        hr = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr))
        hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone, nullptr, 0.f, WICBitmapPaletteTypeCustom);

    UINT w = 0, h = 0;
    if (SUCCEEDED(hr))
        hr = converter->GetSize(&w, &h);

    std::vector<BYTE> pixels;
    if (SUCCEEDED(hr) && w > 0 && h > 0)
    {
        pixels.resize((size_t)w * (size_t)h * 4u);
        hr = converter->CopyPixels(nullptr, w * 4u, (UINT)pixels.size(), pixels.data());
    }

    if (SUCCEEDED(hr))
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = w;
        desc.Height = h;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA sub{};
        sub.pSysMem = pixels.data();
        sub.SysMemPitch = w * 4u;

        hr = device->CreateTexture2D(&desc, &sub, &texture);
    }

    if (SUCCEEDED(hr))
        hr = device->CreateShaderResourceView(texture, nullptr, &srv);

    releasecom(texture);
    releasecom(converter);
    releasecom(frame);
    releasecom(decoder);
    releasecom(stream);
    releasecom(factory);

    if (SUCCEEDED(coinit))
        CoUninitialize();

    return SUCCEEDED(hr) ? srv : nullptr;
}




namespace color
{
    // ── Ghost Sigil inspired palette ──────────────────────────────────────────
    static ImU32 Win         = IM_COL32(14, 14, 18, 245);
    static ImU32 Surface     = IM_COL32(10, 10, 14, 255);
    static ImU32 SurfaceLift = IM_COL32(28, 28, 36, 255);
    static ImU32 card        = IM_COL32(20, 20, 28, 255);
    static ImU32 CardHov     = IM_COL32(30, 30, 42, 255);
    static ImU32 Border      = IM_COL32(255, 255, 255, 28);
    static ImU32 BorderDim   = IM_COL32(255, 255, 255, 14);
    static ImU32 Divider     = IM_COL32(255, 255, 255, 18);
    static ImU32 Accent      = IM_COL32(163, 212, 31, 255);
    static ImU32 AccentDim   = IM_COL32(72,  89,  24, 160);
    static ImU32 AccentHov   = IM_COL32(190, 231, 72, 255);
    static ImU32 AccentFg    = IM_COL32(235, 245, 204, 255);
    static ImU32 Purple      = IM_COL32(130, 90, 255, 255);
    static ImU32 PurpleDim   = IM_COL32(60,  40, 120, 160);
    static ImU32 Glow        = IM_COL32(163, 212, 31, 38);
    static ImU32 GlowPurple  = IM_COL32(130, 90, 255, 28);
    static ImU32 StripeCyan  = IM_COL32(55, 177, 218, 255);
    static ImU32 StripeLime  = IM_COL32(163, 212, 31, 255);
    static ImU32 text        = IM_COL32(220, 220, 228, 255);
    static ImU32 TextMid     = IM_COL32(150, 150, 162, 255);
    static ImU32 TextDim     = IM_COL32(80,  80,  92, 255);
    static ImU32 White       = IM_COL32(255, 255, 255, 255);
    static ImU32 Danger      = IM_COL32(255, 80, 104, 255);
    static ImU32 DangerDim   = IM_COL32(63,  18,  28, 255);
    static ImU32 DangerBrd   = IM_COL32(135, 35,  55, 255);
    static ImU32 Track       = IM_COL32(40,  40,  52, 255);
    static ImU32 Transp      = IM_COL32(0,   0,   0,  0);

    static ImU32 lerp(ImU32 a, ImU32 b, float t);
    static ImU32 alpha(ImU32 c, float a);

    static ImU32 fromfloat(const float col[4], float alphaMul = 1.f)
    {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], col[3] * alphaMul));
    }

    static void refresh()
    {
        // Base window — deep dark with slight blue-purple tint
        const ImU32 userAccent  = fromfloat(global::setting::color::Accent);
        const ImU32 userAccent2 = fromfloat(global::setting::color::Accent2);
        const ImU32 userWindow  = fromfloat(global::setting::color::Window);
        const ImU32 userCard    = fromfloat(global::setting::color::card);
        const ImU32 userText    = fromfloat(global::setting::color::text);

        Win         = userWindow;
        Surface     = lerp(Win, IM_COL32(0, 0, 0, 255), .72f);
        card        = userCard;
        SurfaceLift = lerp(card, White, .06f);
        CardHov     = lerp(card, White, .12f);

        Accent      = userAccent;
        Purple      = userAccent2;
        AccentDim   = alpha(Accent, .40f);
        AccentHov   = lerp(Accent, White, .22f);
        AccentFg    = lerp(Accent, White, .68f);
        PurpleDim   = alpha(Purple, .38f);
        Glow        = alpha(Accent, .18f);
        GlowPurple  = alpha(Purple, .15f);

        text        = userText;
        TextMid     = lerp(text, Win, .38f);
        TextDim     = lerp(text, Win, .62f);

        Border      = IM_COL32(255, 255, 255, 28);
        BorderDim   = IM_COL32(255, 255, 255, 14);
        Divider     = IM_COL32(255, 255, 255, 18);
        Track       = IM_COL32(255, 255, 255, 30);
    }

    static ImU32 lerp(ImU32 a, ImU32 b, float t)
    {
        auto ch = [](ImU32 c, int s) { return (int)((c >> s) & 0xFF); };
        auto mix = [&](int s) { return (int)(ch(a, s) * (1.f - t) + ch(b, s) * t); };
        return IM_COL32(mix(0), mix(8), mix(16), mix(24));
    }

    static ImU32 alpha(ImU32 c, float a)
    {
        const int alpha = (int)(((c >> 24) & 0xFF) * ImClamp(a, 0.f, 1.f));
        return (c & 0x00FFFFFF) | ((ImU32)alpha << 24);
    }
}

namespace notify
{
    struct toast
    {
        kind Type = kind::info;
        std::string Title;
        std::string Body;
        std::chrono::steady_clock::time_point Created;
        float Lifetime = 3.2f;
    };

    static std::mutex s_mutex;
    static std::deque<toast> s_toasts;

    void push(kind type, std::string title, std::string body, float lifetime)
    {
        if (!global::overlay::Notifications) return;
        if (title.empty() && body.empty())
            return;

        std::lock_guard<std::mutex> lock(s_mutex);
        while (s_toasts.size() >= 5)
            s_toasts.pop_front();

        s_toasts.push_back({
            type,
            std::move(title),
            std::move(body),
            std::chrono::steady_clock::now(),
            ImClamp(lifetime, 1.2f, 8.0f)
            });
    }

    static ImU32 accent(kind type, float alpha)
    {
        alpha = ImClamp(alpha, 0.f, 1.f);
        switch (type)
        {
        case kind::success: return IM_COL32(88, 244, 178, (int)(255.f * alpha));
        case kind::warning: return IM_COL32(255, 208, 106, (int)(255.f * alpha));
        case kind::damage: return IM_COL32(255, 90, 116, (int)(255.f * alpha));
        default: return color::alpha(color::Accent, alpha);
        }
    }

    static float easeoutcubic(float value)
    {
        value = 1.f - ImClamp(value, 0.f, 1.f);
        return 1.f - value * value * value;
    }

    void render()
    {
        if (!global::overlay::Notifications) return;

        std::vector<toast> items;
        const auto now = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            s_toasts.erase(
                std::remove_if(s_toasts.begin(), s_toasts.end(),
                    [&](const toast& item) {
                        return std::chrono::duration<float>(now - item.Created).count() > item.Lifetime;
                    }),
                s_toasts.end());
            items.assign(s_toasts.begin(), s_toasts.end());
        }
        if (items.empty()) return;

        ImDrawList* draw = ImGui::GetForegroundDrawList();
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const float  W   = 280.f;   // toast width
        const float  R   = 10.f;    // corner radius
        const float  PAD = 16.f;    // screen edge padding
        const float  GAP = 8.f;     // gap between toasts

        // Damage toasts stack from top-left; normal toasts stack from bottom-right
        float damageY = PAD;
        float normalY = display.y - PAD;

        ImFont* boldFont = Tahoma_BoldXP ? Tahoma_BoldXP : ImGui::GetFont();
        ImFont* regFont  = UiFont        ? UiFont        : ImGui::GetFont();

        for (int i = 0; i < (int)items.size(); i++)
        {
            const toast& item = items[i];
            const float age = std::chrono::duration<float>(now - item.Created).count();

            // Slide in from right (or left for damage), fade out at end
            const float in_t  = ImClamp(age / 0.20f, 0.f, 1.f);
            const float in_e  = 1.f - (1.f - in_t) * (1.f - in_t) * (1.f - in_t); // ease out cubic
            const float out_t = ImClamp((item.Lifetime - age) / 0.30f, 0.f, 1.f);
            const float alpha = in_e * out_t;
            if (alpha <= 0.01f) continue;

            const bool  isDamage = (item.Type == kind::damage);
            const bool  hasBody  = !item.Body.empty();
            const float H        = hasBody ? 62.f : 46.f;

            // Slide offset
            const float slide = (1.f - in_e) * 22.f;

            float tx, ty;
            if (isDamage) {
                tx = PAD - slide;
                ty = damageY;
                damageY += H + GAP;
            } else {
                tx = display.x - W - PAD + slide;
                ty = normalY - H;
                normalY -= H + GAP;
            }

            const ImVec2 tMin(tx, ty);
            const ImVec2 tMax(tx + W, ty + H);

            // Accent color per type
            ImU32 accentCol;
            switch (item.Type) {
                case kind::success: accentCol = IM_COL32(50, 220, 120, 255); break;
                case kind::warning: accentCol = IM_COL32(255, 195, 60,  255); break;
                case kind::damage:  accentCol = IM_COL32(255, 75,  100, 255); break;
                default:            accentCol = color::Accent;               break;
            }

            // Shadow
            for (int s = 3; s >= 1; s--) {
                const float sp = (float)s * 3.f;
                draw->AddRectFilled(
                    tMin + ImVec2(-sp*.2f, sp*.4f),
                    tMax + ImVec2(sp*.2f, sp*.4f),
                    IM_COL32(0, 0, 0, (int)((18 - s * 4) * alpha)), R + sp);
            }

            // Background — dark glass
            draw->AddRectFilled(tMin, tMax, IM_COL32(12, 12, 17, (int)(230 * alpha)), R);

            // Inner highlight (top edge specular)
            draw->AddRectFilled(tMin + ImVec2(R, 1.f),
                tMin + ImVec2(W - R, 2.5f),
                IM_COL32(255, 255, 255, (int)(16 * alpha)), 1.f);

            // Border
            draw->AddRect(tMin, tMax,
                IM_COL32(255, 255, 255, (int)(20 * alpha)), R, 0, 1.f);

            // Left accent bar
            draw->AddRectFilled(tMin + ImVec2(0.f, R),
                tMin + ImVec2(3.f, H - R),
                color::alpha(accentCol, alpha * 0.9f));
            // Round the accent bar top/bottom with the corner
            draw->AddRectFilled(tMin + ImVec2(0.f, R * 0.5f),
                tMin + ImVec2(3.f, R),
                color::alpha(accentCol, alpha * 0.9f));
            draw->AddRectFilled(tMin + ImVec2(0.f, H - R),
                tMin + ImVec2(3.f, H - R * 0.5f),
                color::alpha(accentCol, alpha * 0.9f));

            // Accent dot (icon)
            const float dotX = tMin.x + 16.f;
            const float dotY = tMin.y + (hasBody ? 13.f : H * 0.5f);
            draw->AddCircleFilled({ dotX, dotY }, 4.f,
                color::alpha(accentCol, alpha), 10);
            draw->AddCircleFilled({ dotX, dotY }, 2.5f,
                IM_COL32(255, 255, 255, (int)(200 * alpha)), 10);

            // Title
            const float textX = tMin.x + 28.f;
            draw->AddText(boldFont, boldFont->LegacySize,
                { textX, tMin.y + (hasBody ? 8.f : (H - boldFont->LegacySize) * 0.5f) },
                IM_COL32(235, 238, 245, (int)(245 * alpha)),
                item.Title.c_str());

            // Body
            if (hasBody) {
                draw->AddText(regFont, regFont->LegacySize,
                    { textX, tMin.y + boldFont->LegacySize + 12.f },
                    IM_COL32(148, 158, 170, (int)(220 * alpha)),
                    item.Body.c_str());
            }

            // Progress bar (shrinks over lifetime)
            const float progress = 1.f - ImClamp(age / item.Lifetime, 0.f, 1.f);
            const float barY = tMax.y - 3.f;
            const float barX0 = tMin.x + R;
            const float barX1 = tMax.x - R;
            // Track
            draw->AddRectFilled({ barX0, barY }, { barX1, barY + 2.f },
                IM_COL32(255, 255, 255, (int)(18 * alpha)), 1.f);
            // Fill
            if (progress > 0.f) {
                draw->AddRectFilled({ barX0, barY },
                    { barX0 + (barX1 - barX0) * progress, barY + 2.f },
                    color::alpha(accentCol, alpha * 0.7f), 1.f);
            }
        }
    }
}

namespace fx
{
    static std::unordered_map<ImGuiID, float> s_t;

    static float saturate(float v)
    {
        return ImClamp(v, 0.f, 1.f);
    }

    static float easeoutcubic(float t)
    {
        t = 1.f - saturate(t);
        return 1.f - t * t * t;
    }

    static float easeoutquint(float t)
    {
        t = 1.f - saturate(t);
        return 1.f - t * t * t * t * t;
    }

    static float easeinoutcubic(float t)
    {
        t = saturate(t);
        return t < .5f ? 4.f * t * t * t : 1.f - powf(-2.f * t + 2.f, 3.f) * .5f;
    }

    static float damp(float value, float target, float speed)
    {
        const float dt = ImClamp(ImGui::GetIO().DeltaTime, 0.f, 1.f / 30.f);
        const float k = 1.f - expf(-speed * dt);
        const float v = ImLerp(value, target, k);
        return fabsf(v - target) < .0007f ? target : v;
    }

    static float anim(ImGuiID id, bool on, float speed = 12.f)
    {
        float& v = s_t[id];
        v = damp(v, on ? 1.f : 0.f, speed);
        return v;
    }

    static float togglet(ImGuiID id, bool on)
    {
        return easeinoutcubic(anim(id, on, 9.5f));
    }
}

namespace icon
{
    static void crosshair(ImDrawList* dl, ImVec2 c, float sz, ImU32 col)
    {
        const float r = sz * .38f, gap = sz * .13f, ll = sz * .26f, lw = 1.5f;
        dl->AddCircle(c, r, col, 32, lw);
        dl->AddCircleFilled(c, 2.0f, col);
        dl->AddLine({ c.x,       c.y - r - gap }, { c.x,       c.y - r - gap - ll }, col, lw);
        dl->AddLine({ c.x,       c.y + r + gap }, { c.x,       c.y + r + gap + ll }, col, lw);
        dl->AddLine({ c.x - r - gap, c.y }, { c.x - r - gap - ll, c.y }, col, lw);
        dl->AddLine({ c.x + r + gap, c.y }, { c.x + r + gap + ll, c.y }, col, lw);
    }

    static void eye(ImDrawList* dl, ImVec2 c, float sz, ImU32 col)
    {
        const float rx = sz * .44f, ry = sz * .22f, lw = 1.5f;
        const ImVec2 L = { c.x - rx,c.y }, R = { c.x + rx,c.y };
        const ImVec2 T = { c.x,c.y - ry }, B = { c.x,c.y + ry };
        dl->AddBezierCubic(L, { L.x + rx * .65f,T.y - ry * .3f }, { R.x - rx * .65f,T.y - ry * .3f }, R, col, lw);
        dl->AddBezierCubic(L, { L.x + rx * .65f,B.y + ry * .3f }, { R.x - rx * .65f,B.y + ry * .3f }, R, col, lw);
        dl->AddCircleFilled(c, sz * .13f, col);
    }

    static void globe(ImDrawList* dl, ImVec2 c, float sz, ImU32 col)
    {
        const float r = sz * .40f, cx = sz * .22f, lw = 1.5f;
        dl->AddCircle(c, r, col, 32, lw);
        dl->AddLine({ c.x - r, c.y }, { c.x + r, c.y }, col, lw);
        dl->AddBezierCubic({ c.x,c.y - r }, { c.x + cx,c.y - r * .5f }, { c.x + cx,c.y + r * .5f }, { c.x,c.y + r }, col, lw);
        dl->AddBezierCubic({ c.x,c.y - r }, { c.x - cx,c.y - r * .5f }, { c.x - cx,c.y + r * .5f }, { c.x,c.y + r }, col, lw);
    }

    static void layer(ImDrawList* dl, ImVec2 c, float sz, ImU32 col)
    {
        const float hw = sz * .35f, hh = sz * .07f, gap = sz * .17f;
        for (int i = -1; i <= 1; i++)
            dl->AddRectFilled({ c.x - hw, c.y + i * gap - hh }, { c.x + hw, c.y + i * gap + hh }, col, 1.f);
    }

    static void gear(ImDrawList* dl, ImVec2 c, float sz, ImU32 col)
    {
        const float ri = sz * .28f, ro = sz * .42f;
        dl->AddCircle(c, ri, col, 24, 1.5f);
        for (int i = 0; i < 6; i++)
        {
            const float a = (float)i / 6.f * 6.2832f;
            const float a1 = a - .24f, a2 = a + .24f;
            dl->AddQuadFilled(
                { c.x + cosf(a1) * ri, c.y + sinf(a1) * ri },
                { c.x + cosf(a2) * ri, c.y + sinf(a2) * ri },
                { c.x + cosf(a2) * ro, c.y + sinf(a2) * ro },
                { c.x + cosf(a1) * ro, c.y + sinf(a1) * ro }, col);
        }
    }

    static void diamond(ImDrawList* dl, ImVec2 c, float sz, ImU32 col)
    {
        const float r = sz * .42f;
        dl->AddQuad({ c.x, c.y - r }, { c.x + r, c.y }, { c.x, c.y + r }, { c.x - r, c.y }, col, 1.6f);
        dl->AddLine({ c.x - r * .45f, c.y }, { c.x + r * .45f, c.y }, col, 1.3f);
        dl->AddCircleFilled(c, sz * .08f, col, 12);
    }
}

namespace side
{
    static constexpr float kW = 82.f;
    static constexpr float kCompactW = 82.f;
    static constexpr float kTabH = 67.f;
    static constexpr float kLogoH = 19.f;

    using IconFn = void(*)(ImDrawList*, ImVec2, float, ImU32);

    static bool tab(ImDrawList* dl, ImVec2 wp, float startY, float railW, bool compact,
        int idx, int cur, int total, IconFn icon, const char* label)
    {
        const bool   active = (idx == cur);
        const float tabH = 66.f;

        const float tabW = 74.f;
        const float tabX = wp.x + 6.f;
        const float tabStep = 66.f;
        const float tabY = wp.y + startY + tabStep * idx;

        const ImVec2 tMin = { tabX, tabY };
        const ImVec2 tMax = tMin + ImVec2(tabW, tabH);
        const ImVec2 center = ImVec2(tMin.x + tabW * .5f, tMin.y + tabH * .5f);

        ImGui::SetCursorScreenPos(tMin);
        ImGui::PushID(idx);
        ImGui::InvisibleButton("##tab", { tabW, tabH });

        const bool clicked = ImGui::IsItemClicked();
        const bool hov = ImGui::IsItemHovered();
        const ImGuiID id = ImGui::GetItemID();
        ImGui::PopID();

        const float t = fx::easeoutcubic(fx::anim(id, active || hov, 12.f));
        const float at = fx::easeoutcubic(fx::anim(id ^ 0x8A13C4u, active, 12.f));
        if (!active)
        {
            dl->AddRectFilled(tMin, tMax, IM_COL32(12, 12, 12, 255));
            dl->AddLine({ tMax.x, tMin.y - 1.f }, { tMax.x, tMax.y }, color::Border);
            dl->AddLine({ tMax.x - 1.f, tMin.y }, { tMax.x - 1.f, tMax.y }, IM_COL32(0, 0, 0, 255));
        }
        else
        {
            dl->AddLine(tMin, { tMax.x + 1.f, tMin.y }, color::Border);
            dl->AddLine(tMin - ImVec2(0.f, 1.f), { tMax.x, tMin.y - 1.f }, IM_COL32(0, 0, 0, 255));
            dl->AddLine({ tMin.x, tMax.y - 2.f }, { tMax.x + 1.f, tMax.y - 2.f }, color::Border);
            dl->AddLine({ tMin.x, tMax.y - 1.f }, { tMax.x, tMax.y - 1.f }, IM_COL32(0, 0, 0, 255));
        }

        if (idx == total - 1)
        {
            const float y_start = tabY + tabH;
            const float y_end = wp.y + 546.f;
            if (y_end > y_start)
            {
                dl->AddRectFilled({ tabX, y_start }, { tabX + tabW, y_end }, IM_COL32(12, 12, 12, 255));
                dl->AddLine({ tabX + tabW, y_start - 1.f }, { tabX + tabW, y_end }, color::Border);
                dl->AddLine({ tabX + tabW - 1.f, y_start }, { tabX + tabW - 1.f, y_end }, IM_COL32(0, 0, 0, 255));
            }
        }

        if (hov && !active)
            dl->AddRectFilled(tMin + ImVec2(1.f, 1.f), tMax - ImVec2(1.f, 1.f),
                IM_COL32(255, 255, 255, (int)(t * 8.f)));

        static constexpr const char* glyphs[] = { "A", "", "", "G", "", "", "", "H" };
        const char* glyph = idx >= 0 && idx < IM_ARRAYSIZE(glyphs) ? glyphs[idx] : label;
        if (glyph && glyph[0] != '\0')
        {
            ImFont* font = IdaTabFont ? IdaTabFont : (LogoFont ? LogoFont : (Tahoma_BoldXP ? Tahoma_BoldXP : ImGui::GetFont()));
            const float size = IdaTabFont ? 47.f : 34.f + at * 2.f;
            const ImVec2 textSize = font->CalcTextSizeA(size, FLT_MAX, 0.f, glyph);
            const ImU32 baseCol = hov ? IM_COL32(140, 140, 140, 255) : IM_COL32(90, 90, 90, 255);
            const ImU32 iconCol = color::lerp(baseCol, IM_COL32(210, 210, 210, 255), at);
            dl->AddText(font, size, center - textSize * .5f + ImVec2(1.f, 1.f), IM_COL32(0, 0, 0, 155), glyph);
            dl->AddText(font, size, center - textSize * .5f, iconCol, glyph);
        }
        else if (icon)
        {
            const float iconSz = 36.f + at * 2.f;
            const ImU32 baseCol = hov ? IM_COL32(140, 140, 140, 255) : IM_COL32(90, 90, 90, 255);
            const ImU32 iconCol = color::lerp(baseCol, IM_COL32(210, 210, 210, 255), at);
            icon(dl, center, iconSz, iconCol);
        }

        return clicked;
    }
}

namespace ui
{
    static constexpr float kTW = 34.f;
    static constexpr float kTH = 18.f;
    static constexpr float kTrkH = 4.f;
    static constexpr float kColW = 15.f;
    static constexpr float kColH = 14.f;
    static constexpr float kCardPadX = 22.f;
    static constexpr float kCardPadY = 26.f;
    static constexpr float kCardPadR = 22.f;
    static constexpr float kControlIndent = 20.f;

    static void labelsection(const char* text)
    {
        ImDrawList* dl    = ImGui::GetWindowDrawList();
        const ImVec2 p    = ImGui::GetCursorScreenPos();
        const float  avail = ImGui::GetContentRegionAvail().x;
        const float  fh   = ImGui::GetFontSize();

        // Dim uppercase label
        dl->AddText(p, color::TextDim, text);

        // Line after text
        const float textW = ImGui::CalcTextSize(text).x;
        const float lineY = p.y + fh * .5f;
        dl->AddLine({ p.x + textW + 8.f, lineY },
                    { p.x + avail,        lineY },
                    color::Divider, 1.f);

        ImGui::Dummy({ 0.f, fh + 6.f });
    }

    static void gap(float px = 4.f) { ImGui::Dummy({ 0.f, px }); }

    static float controlwidth()
    {
        return ImClamp(ImGui::GetWindowSize().x - kCardPadX - kCardPadR - kControlIndent, 63.f, 202.f);
    }

    static float rightslot(float width, float pad = 0.f)
    {
        return ImMax(kCardPadX, ImGui::GetWindowSize().x - kCardPadR - width - pad);
    }

    static bool toggle(const char* label, bool* v)
    {
        ImGui::PushID(label);
        const ImVec2 p   = ImGui::GetCursorScreenPos();
        ImDrawList*  dl  = ImGui::GetWindowDrawList();

        const ImVec2 textSize = ImGui::CalcTextSize(label);
        const float  rowH     = ImMax(20.f, textSize.y);
        const float  TW = 36.f, TH = 18.f, TR = TH * 0.5f;

        // Hit region: only cover the pill + label width, NOT full row width.
        // This prevents the toggle eating clicks intended for bind/color widgets
        // placed via SameLine() to the right.
        const float hitW = TW + 10.f + textSize.x;
        ImGui::InvisibleButton("##t", { hitW, rowH });
        const bool clicked = ImGui::IsItemClicked();
        const bool hov     = ImGui::IsItemHovered();
        if (clicked) *v = !*v;

        // Smooth knob animation
        const float t = fx::easeinoutcubic(fx::anim(ImGui::GetItemID(), *v, 9.5f));

        // Pill track
        const ImVec2 tMin = p + ImVec2(0.f, floorf((rowH - TH) * .5f));
        const ImVec2 tMax = tMin + ImVec2(TW, TH);
        const ImU32  trackOff = color::alpha(color::Border, 0.6f);
        const ImU32  trackOn  = color::alpha(color::Accent, 0.85f);
        const ImU32  track    = color::lerp(trackOff, trackOn, t);
        dl->AddRectFilled(tMin, tMax, track, TR);
        dl->AddRect(tMin, tMax, color::alpha(color::Border, 0.4f), TR, 0, 1.f);

        // Knob
        const float kx = tMin.x + TR + t * (TW - TH);
        const float ky = tMin.y + TR;
        dl->AddCircleFilled({ kx + 0.5f, ky + 0.5f }, TR - 3.f, IM_COL32(0, 0, 0, 55));
        dl->AddCircleFilled({ kx, ky }, TR - 3.f, color::White);

        // Label
        const float lx = tMin.x + TW + 10.f;
        const float ly = p.y + (rowH - ImGui::GetFontSize()) * .5f;
        dl->AddText({ lx, ly },
            *v ? color::text : color::TextMid, label);

        ImGui::PopID();
        return clicked;
    }

    static bool togglecolor(const char* tLabel, bool* v,
        const char* cLabel, float col[4])
    {
        const bool changed = toggle(tLabel, v);
        ImGui::SameLine(rightslot(kColW));
        ImGui::PushID(cLabel);
        {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImU32  sw = IM_COL32((int)(col[0] * 255), (int)(col[1] * 255),
                (int)(col[2] * 255), (int)(col[3] * 255));
            dl->AddRectFilled(p, p + ImVec2(kColW, kColH), sw);
            dl->AddRect(p, p + ImVec2(kColW, kColH), color::BorderDim);
            dl->AddRect(p + ImVec2(1.f, 1.f), p + ImVec2(kColW - 1.f, kColH - 1.f), *v ? color::Accent : color::Border);
            ImGui::InvisibleButton("##s", { kColW, kColH });
            if (ImGui::IsItemClicked()) ImGui::OpenPopup("##cp");
            ImGui::SetNextWindowPos({ p.x + kColW + 4.f, p.y - 4.f });
            ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::ColorConvertU32ToFloat4(color::card));
            ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(color::Border));
            if (ImGui::BeginPopup("##cp")) {
                ImGui::ColorPicker4("##pk", col,
                    ImGuiColorEditFlags_NoSidePreview |
                    ImGuiColorEditFlags_AlphaBar |
                    ImGuiColorEditFlags_PickerHueBar);
                ImGui::EndPopup();
            }
            ImGui::PopStyleColor(2);
        }
        ImGui::PopID();
        return changed;
    }

    static void color4(const char* label, float col[4])
    {
        ImGui::PushID(label);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImU32  sw = IM_COL32((int)(col[0] * 255), (int)(col[1] * 255),
            (int)(col[2] * 255), (int)(col[3] * 255));
        dl->AddRectFilled(p, p + ImVec2(kColW, kColH), sw);
        dl->AddRect(p, p + ImVec2(kColW, kColH), color::BorderDim);
        dl->AddRect(p + ImVec2(1.f, 1.f), p + ImVec2(kColW - 1.f, kColH - 1.f), color::Border);
        ImGui::InvisibleButton("##sw", { kColW, kColH });
        if (ImGui::IsItemClicked()) ImGui::OpenPopup("##cpe");
        ImGui::SetNextWindowPos({ p.x + kColW + 4.f, p.y - 4.f });
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::ColorConvertU32ToFloat4(color::card));
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(color::Border));
        if (ImGui::BeginPopup("##cpe")) {
            ImGui::ColorPicker4("##pk", col,
                ImGuiColorEditFlags_NoSidePreview |
                ImGuiColorEditFlags_AlphaBar |
                ImGuiColorEditFlags_PickerHueBar);
            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopID();
    }

    static void dualcolor(const char* la, float* ca, const char* lb, float* cb)
    {
        ImGui::SameLine(rightslot(kColW * 2.f + 4.f));
        color4(la, ca);
        ImGui::SameLine(0.f, 4.f);
        color4(lb, cb);
    }

    static void tricolor(const char* la, float* ca, const char* lb, float* cb, const char* lc, float* cc)
    {
        ImGui::SameLine(rightslot(kColW * 3.f + 8.f));
        color4(la, ca); ImGui::SameLine(0.f, 4.f);
        color4(lb, cb); ImGui::SameLine(0.f, 4.f);
        color4(lc, cc);
    }

    static bool sliderfloat(const char* label, float* v, float mn, float mx, const char* fmt = "%.2f")
    {
        ImGui::PushID(label);
        ImDrawList* dl     = ImGui::GetWindowDrawList();
        const ImVec2 orig  = ImGui::GetCursorScreenPos();
        const float  avail = ImGui::GetContentRegionAvail().x;
        const float  lH    = ImGui::GetFontSize();
        const float  w     = ImMin(controlwidth(), ImMax(63.f, avail - 20.f));
        const float  KR    = 6.f;
        const float  BH    = 4.f;
        const float  totalH = lH + 14.f;
        const ImVec2 p = orig + ImVec2(20.f, 0.f);

        ImGui::InvisibleButton("##sl", { ImMin(avail, 20.f + w), totalH });
        const bool active = ImGui::IsItemActive();
        const bool hov    = ImGui::IsItemHovered();

        const ImVec2 barMin = { p.x, p.y + lH + 5.f };
        const ImVec2 barMax = { p.x + w, barMin.y + BH };

        if (active) {
            float rel = (ImGui::GetIO().MousePos.x - barMin.x) / w;
            *v = mn + ImClamp(rel, 0.f, 1.f) * (mx - mn);
        }

        const float t = mx == mn ? 0.f : ImClamp((*v - mn) / (mx - mn), 0.f, 1.f);

        // Track
        dl->AddRectFilled(barMin, barMax, color::Track, BH * .5f);

        // Fill
        const float fillW = ImClamp(w * t, 1.f, w);
        if (fillW > 0.f) {
            dl->AddRectFilled(barMin, { barMin.x + fillW, barMax.y },
                active ? color::AccentHov : color::Accent, BH * .5f);
        }

        // Knob — grows slightly on hover
        const float kr = active ? KR + 1.f : (hov ? KR + 0.5f : KR);
        const ImVec2 kc = { barMin.x + fillW, barMin.y + BH * .5f };
        dl->AddCircleFilled({ kc.x + 0.5f, kc.y + 0.5f }, kr, IM_COL32(0, 0, 0, 80));
        dl->AddCircleFilled(kc, kr, active ? color::AccentHov : color::Accent);

        // Label + value
        char buf[32]{};
        snprintf(buf, sizeof(buf), fmt ? fmt : "%.2f", *v);
        dl->AddText(p + ImVec2(0.f, -1.f), color::TextMid, label);
        const ImVec2 valSz = ImGui::CalcTextSize(buf);
        dl->AddText({ barMax.x - valSz.x, p.y - 1.f }, color::text, buf);

        ImGui::PopID();
        return ImGui::IsItemDeactivatedAfterEdit();
    }

    static bool sliderint(const char* label, int* v, int mn, int mx)
    {
        float fv = (float)*v;
        bool  r = sliderfloat(label, &fv, (float)mn, (float)mx, "%.0f");
        *v = (int)roundf(fv);
        return r;
    }

    static bool combo(const char* label, int* idx, const std::vector<const char*>& items)
    {
        ImGui::PushID(label);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const float avail = ImGui::GetContentRegionAvail().x;
        const float labelH = ImGui::GetFontSize();
        const float w = ImMin(controlwidth(), ImMax(63.f, avail - 20.f));
        const float h = 20.f;
        const float totalH = labelH + 4.f + h;
        const ImVec2 p = origin + ImVec2(20.f, 0.f);
        const ImVec2 frameMin = p + ImVec2(0.f, labelH + 4.f);
        const ImVec2 frameMax = frameMin + ImVec2(w, h);

        ImGui::InvisibleButton("##cb", { ImMin(avail, 20.f + w), totalH });
        const bool hov = ImGui::IsItemHovered()
            && ImGui::GetIO().MousePos.x >= frameMin.x && ImGui::GetIO().MousePos.x <= frameMax.x
            && ImGui::GetIO().MousePos.y >= frameMin.y && ImGui::GetIO().MousePos.y <= frameMax.y;
        const bool clicked = ImGui::IsItemClicked();

        const ImU32 top = hov ? IM_COL32(41, 41, 41, 255) : IM_COL32(31, 31, 31, 255);
        const ImU32 bot = hov ? IM_COL32(46, 46, 46, 255) : IM_COL32(36, 36, 36, 255);
        dl->AddText(p + ImVec2(0.f, -1.f), color::text, label);
        dl->AddRectFilledMultiColor(frameMin + ImVec2(1.f, 1.f), frameMax - ImVec2(1.f, 1.f),
            top, top, bot, bot);
        dl->AddRect(frameMin, frameMax, color::BorderDim);

        const char* cur = (*idx >= 0 && *idx < (int)items.size()) ? items[*idx] : "---";
        dl->AddText({ frameMin.x + 7.f, frameMin.y + (h - ImGui::GetFontSize()) * .5f }, color::text, cur);

        const float cx = frameMin.x + w - 9.f, cy = frameMin.y + h * .5f;
        dl->AddLine({ cx - 1.f, cy - 3.f }, { cx + 5.f, cy - 3.f }, color::BorderDim);
        dl->AddTriangleFilled({ cx - 1.f, cy - 1.f }, { cx + 2.f, cy + 3.f }, { cx + 5.f, cy - 1.f }, color::TextMid);

        bool changed = false;
        if (clicked) ImGui::OpenPopup("##cop");
        ImGui::SetNextWindowPos({ frameMin.x, frameMax.y + 2.f });
        ImGui::SetNextWindowSize({ w, 0.f });
        ImGui::SetNextWindowSizeConstraints({ w,0.f }, { w,160.f });
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::ColorConvertU32ToFloat4(color::card));
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(color::Border));
        if (ImGui::BeginPopup("##cop"))
        {
            for (int i = 0; i < (int)items.size(); i++)
            {
                const bool sel = (i == *idx);
                ImGui::PushStyleColor(ImGuiCol_Header,
                    ImGui::ColorConvertU32ToFloat4(sel ? color::AccentDim : color::Transp));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                    ImGui::ColorConvertU32ToFloat4(color::AccentDim));
                ImGui::PushStyleColor(ImGuiCol_Text,
                    ImGui::ColorConvertU32ToFloat4(sel ? color::AccentFg : color::text));
                if (ImGui::Selectable(items[i], sel))
                {
                    *idx = i;
                    changed = true;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopStyleColor(3);
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopID();
        return changed;
    }

    static float bindwidth(ImGuiKey key, ImKeyBindMode mode)
    {
        const char* kn = (key == ImGuiKey_None) ? "[-]" : ImGui::GetKeyName(key);
        if (!kn || !*kn) kn = "[-]";
        const char* mn = (mode == ImKeyBindMode_Hold) ? "HOLD"
                       : (mode == ImKeyBindMode_Always) ? "ALWY" : "TOGG";
        const float kw = ImMax(ImGui::CalcTextSize(kn).x + 14.f, 38.f);
        const float mw = ImGui::CalcTextSize(mn).x + 10.f;
        return kw + 3.f + mw;
    }
    static void bind(const char* id, ImGuiKey* key, ImKeyBindMode* mode)
    {
        static bool    s_listen = false;
        static ImGuiID s_ownerId = 0;
        ImGui::PushID(id);
        const ImGuiID self = ImGui::GetID("##kb");

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float h = 18.f;
        const char* kn = ImGui::GetKeyName(*key);
        if (!kn || !*kn || *key == ImGuiKey_None) kn = "[-]";
        const char* mn = (*mode == ImKeyBindMode_Hold) ? "HOLD"
                       : (*mode == ImKeyBindMode_Always) ? "ALWY" : "TOGG";
        const float kw = ImMax(ImGui::CalcTextSize(kn).x + 14.f, 38.f);
        const float mw = ImGui::CalcTextSize(mn).x + 10.f;
        const bool  listening = s_listen && (s_ownerId == self);

        // Allow overlapping with previous item (toggle placed via SameLine)
        ImGui::SetNextItemAllowOverlap();
        const ImVec2 kp = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##k", { kw, h });
        // Left-click = start listening, Right-click = clear to None
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))  { s_listen = true; s_ownerId = self; }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) { *key = ImGuiKey_None; s_listen = false; }
        if (listening) {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                s_listen = false;
            } else {
                for (int k = ImGuiKey_Tab; k < ImGuiKey_COUNT; k++) {
                    if (k == ImGuiKey_Escape) continue;
                    if (ImGui::IsKeyPressed((ImGuiKey)k, false)) {
                        *key = (ImGuiKey)k;
                        s_listen = false;
                        break;
                    }
                }
            }
        }
        // Re-evaluate display string after possible clear
        const char* kn_display = (*key == ImGuiKey_None) ? "[-]"
                                 : (listening ? "..." : kn);
        dl->AddRectFilled(kp, kp + ImVec2(kw, h), listening ? color::AccentDim : color::SurfaceLift);
        dl->AddRect(kp, kp + ImVec2(kw, h), listening ? color::Accent : color::BorderDim);
        dl->AddText({ kp.x + (kw - ImGui::CalcTextSize(kn_display).x) * .5f,
                     kp.y + (h - ImGui::GetFontSize()) * .5f },
            listening ? color::AccentFg
            : (*key == ImGuiKey_None ? color::TextDim : color::text),
            kn_display);

        ImGui::SameLine(0.f, 3.f);

        ImGui::SetNextItemAllowOverlap();
        const ImVec2 mp = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##m", { mw, h });
        const bool mhov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked())
            *mode = (*mode == ImKeyBindMode_Hold) ? ImKeyBindMode_Always
                  : (*mode == ImKeyBindMode_Always) ? ImKeyBindMode_Toggle
                  : ImKeyBindMode_Hold;
        dl->AddRectFilled(mp, mp + ImVec2(mw, h), mhov ? color::CardHov : color::SurfaceLift);
        dl->AddRect(mp, mp + ImVec2(mw, h), color::BorderDim);
        dl->AddText({ mp.x + (mw - ImGui::CalcTextSize(mn).x) * .5f,
                     mp.y + (h - ImGui::GetFontSize()) * .5f }, color::TextMid, mn);

        ImGui::PopID();
    }

    static float keywidth(ImGuiKey key)
    {
        const char* kn = ImGui::GetKeyName(key);
        if (!kn || !*kn) kn = "NONE";
        return ImMax(ImGui::CalcTextSize(kn).x + 18.f, 48.f);
    }

    static void keyselect(const char* id, ImGuiKey* key)
    {
        static bool    s_listen = false;
        static ImGuiID s_ownerId = 0;
        ImGui::PushID(id);
        const ImGuiID self = ImGui::GetID("##ks");

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float h = 18.f;
        const char* kn = ImGui::GetKeyName(*key);
        if (!kn || !*kn || *key == ImGuiKey_None) kn = "[-]";
        const float w = keywidth(*key);
        const bool listening = s_listen && (s_ownerId == self);

        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##key", { w, h });
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))  { s_listen = true; s_ownerId = self; }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) { *key = ImGuiKey_None; s_listen = false; }
        if (listening) {
            for (int k = ImGuiKey_Tab; k < ImGuiKey_COUNT; k++)
                if (ImGui::IsKeyPressed((ImGuiKey)k, false))
                {
                    *key = (ImGuiKey)k;
                    s_listen = false;
                    break;
                }
        }

        const char* text = listening ? "..." : (*key == ImGuiKey_None ? "[-]" : kn);
        const float tw = ImGui::CalcTextSize(text).x;
        dl->AddRectFilled(p, p + ImVec2(w, h), listening ? color::AccentDim : color::SurfaceLift);
        dl->AddRect(p, p + ImVec2(w, h), listening ? color::Accent : color::BorderDim);
        dl->AddText({ p.x + (w - tw) * .5f, p.y + (h - ImGui::GetFontSize()) * .5f },
            listening ? color::AccentFg
            : (*key == ImGuiKey_None ? color::TextDim : color::text),
            text);

        ImGui::PopID();
    }

    static bool button(const char* label, ImU32 bg, ImU32 brd, ImU32 txt,
        float w = -1.f, float h = 22.f)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const bool defaultSize = w < 0.f;
        const float avail = ImGui::GetContentRegionAvail().x;
        const float W = defaultSize ? controlwidth() : w;
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 p = origin + (defaultSize ? ImVec2(20.f, 0.f) : ImVec2(0.f, 0.f));
        ImGui::InvisibleButton(label, { defaultSize ? ImMin(avail, 20.f + W) : W, h });
        const bool hov = ImGui::IsItemHovered();
        const bool act = ImGui::IsItemActive();
        const ImU32 fill = act ? color::lerp(bg, color::White, .06f)
            : (hov ? color::lerp(bg, color::White, .10f) : bg);
        dl->AddRectFilledMultiColor(p + ImVec2(1.f, 1.f), p + ImVec2(W - 1.f, h - 1.f),
            fill, fill, color::lerp(fill, IM_COL32(0, 0, 0, 255), .18f), color::lerp(fill, IM_COL32(0, 0, 0, 255), .18f));
        dl->AddRect(p, p + ImVec2(W, h), color::BorderDim);
        dl->AddRect(p + ImVec2(1.f, 1.f), p + ImVec2(W - 1.f, h - 1.f), hov ? color::lerp(brd, color::Accent, .35f) : brd);
        const float tw = ImGui::CalcTextSize(label).x;
        dl->AddText({ p.x + (W - tw) * .5f, p.y + (h - ImGui::GetFontSize()) * .5f }, txt, label);
        return ImGui::IsItemClicked();
    }

    struct card {
        static bool begin(const char* id, ImVec2 size, const char* title = nullptr)
        {
            const ImVec2 p  = ImGui::GetCursorScreenPos();
            ImDrawList*  dl = ImGui::GetWindowDrawList();

            // Ghost Sigil–style frosted glass card
            auto drawCard = [&](ImDrawList* out) {
                if (size.x <= 0.f || size.y <= 0.f) return;
                const float R = 10.f;

                // Shadow
                for (int i = 4; i >= 1; i--) {
                    const float sp = (float)i * 4.f;
                    out->AddRectFilled(
                        p + ImVec2(-sp*.3f, sp*.2f),
                        p + size + ImVec2(sp*.3f, sp*.5f),
                        IM_COL32(0, 0, 0, (int)(20 - i * 3)), R + sp);
                }

                // Card fill
                out->AddRectFilled(p, p + size, color::card, R);

                // Subtle inner highlight (top edge)
                out->AddRectFilled(p + ImVec2(1.f, 1.f),
                    p + ImVec2(size.x - 1.f, 3.f),
                    IM_COL32(255, 255, 255, 12), R);

                // Border
                out->AddRect(p, p + size, color::Border, R, 0, 1.f);

                // Title label
                if (title && *title) {
                    ImFont* tf      = Tahoma_BoldXP ? Tahoma_BoldXP : ImGui::GetFont();
                    const float tsz = ImGui::GetFontSize();
                    const ImVec2 tp = p + ImVec2(kCardPadX, 8.f);
                    out->AddText(tf, tsz, tp, color::TextDim, title);
                    // Divider under title
                    const float dY = tp.y + tsz + 5.f;
                    out->AddLine({ p.x + 1.f, dY }, { p.x + size.x - 1.f, dY },
                        color::Divider, 1.f);
                }
            };

            drawCard(dl);

            const float titleOffset = (title && *title) ? (ImGui::GetFontSize() + 16.f) : 0.f;
            ImGui::PushStyleColor(ImGuiCol_ChildBg,  ImGui::ColorConvertU32ToFloat4(color::Transp));
            ImGui::PushStyleColor(ImGuiCol_Border,   ImGui::ColorConvertU32ToFloat4(color::Transp));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,  0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                ImVec2(kCardPadX, titleOffset > 0.f ? 4.f : kCardPadY));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 6.f));
            const bool open = ImGui::BeginChild(id, size, false,
                ImGuiWindowFlags_AlwaysUseWindowPadding);
            if (open) {
                drawCard(ImGui::GetWindowDrawList());
                ImGui::SetCursorPos(ImVec2(kCardPadX,
                    (titleOffset > 0.f ? titleOffset : kCardPadY)));
            }
            return open;
        }
        static void end()
        {
            ImGui::EndChild();
            ImGui::PopStyleVar(4);
            ImGui::PopStyleColor(2);
        }
    };

}



namespace hud
{
    static ImU32 accent(float alpha = 1.f) { return color::fromfloat(global::overlay::color::Accent, alpha); }
    static ImU32 accent2(float alpha = 1.f) { return color::fromfloat(global::overlay::color::Accent2, alpha); }
    static ImU32 panelcolor(float alpha = 1.f) { return color::fromfloat(global::overlay::color::panel, alpha); }
    static ImU32 textcolor(float alpha = 1.f) { return color::fromfloat(global::overlay::color::text, alpha); }

    static void clamppanel(ImVec2& pos, ImVec2 size)
    {
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const float pad = 8.f;
        pos.x = ImClamp(pos.x, pad, ImMax(pad, display.x - size.x - pad));
        pos.y = ImClamp(pos.y, pad, ImMax(pad, display.y - size.y - pad));
    }

    static bool samepos(ImVec2 a, ImVec2 b)
    {
        return fabsf(a.x - b.x) < .5f && fabsf(a.y - b.y) < .5f;
    }

    static void solusdefaultplacement()
    {
        static bool placed = false;
        if (placed)
            return;

        placed = true;
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        if (samepos(global::overlay::Watermark_Pos, ImVec2(18.f, 18.f)))
            global::overlay::Watermark_Pos = ImVec2(ImMax(8.f, display.x - 208.f), 8.f);
        if (samepos(global::overlay::Hotkeys_Pos, ImVec2(18.f, 72.f)))
            global::overlay::Hotkeys_Pos = ImVec2(ImMax(8.f, display.x - 275.f), 40.f);
    }

    static const char* keyname(ImGuiKey key)
    {
        const char* name = ImGui::GetKeyName(key);
        return name && *name ? name : "NONE";
    }

    static const char* modename(ImKeyBindMode mode)
    {
        return mode == ImKeyBindMode_Hold ? "hold" : "toggle";
    }

    static const char* solusmode(ImKeyBindMode mode)
    {
        return mode == ImKeyBindMode_Hold ? "[holding]" : "[toggled]";
    }

    static ImU32 solusaccent(float alpha = 1.f)
    {
        return IM_COL32(142, 165, 229, (int)(255.f * ImClamp(alpha, 0.f, 1.f)));
    }

    static ImU32 soluswhite(float alpha = 1.f)
    {
        return IM_COL32(245, 245, 255, (int)(255.f * ImClamp(alpha, 0.f, 1.f)));
    }

    static ImFont* solusfont()
    {
        return Tahoma_BoldXP ? Tahoma_BoldXP : ImGui::GetFont();
    }

    static void solusbox(ImDrawList* dl, ImVec2 p, ImVec2 s, bool hovered, bool active)
    {
        const float t = hovered || active ? 1.f : 0.f;
        const ImVec2 mx = p + s;
        dl->AddRectFilled(p + ImVec2(1.f, 2.f), mx + ImVec2(1.f, 2.f), IM_COL32(0, 0, 0, 58));
        dl->AddRectFilled(p, mx, IM_COL32(8, 8, 10, 168 + (int)(t * 18.f)));
        dl->AddRect(p, mx, IM_COL32(0, 0, 0, 220), 0.f, 0, 1.f);

        const ImU32 accentCol = color::lerp(solusaccent(.88f), solusaccent(1.f), t);
        const float corner = ImMin(26.f, s.x * .35f);
        dl->AddLine(p + ImVec2(2.f, 0.f), p + ImVec2(corner, 0.f), accentCol, 1.f);
        dl->AddLine(p + ImVec2(0.f, 2.f), p + ImVec2(0.f, ImMin(12.f, s.y - 2.f)), accentCol, 1.f);
        dl->AddLine(mx - ImVec2(corner, s.y), mx - ImVec2(2.f, s.y), accentCol, 1.f);
        dl->AddLine(mx - ImVec2(0.f, s.y - 2.f), mx - ImVec2(0.f, s.y - ImMin(12.f, s.y - 2.f)), accentCol, 1.f);
    }

    static int virtualkey(ImGuiKey key)
    {
        if (key >= ImGuiKey_0 && key <= ImGuiKey_9)
            return '0' + (key - ImGuiKey_0);
        if (key >= ImGuiKey_A && key <= ImGuiKey_Z)
            return 'A' + (key - ImGuiKey_A);
        if (key >= ImGuiKey_F1 && key <= ImGuiKey_F24)
            return VK_F1 + (key - ImGuiKey_F1);
        if (key >= ImGuiKey_Keypad0 && key <= ImGuiKey_Keypad9)
            return VK_NUMPAD0 + (key - ImGuiKey_Keypad0);

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
        case ImGuiKey_Menu: return VK_APPS;
        case ImGuiKey_Apostrophe: return VK_OEM_7;
        case ImGuiKey_Comma: return VK_OEM_COMMA;
        case ImGuiKey_Minus: return VK_OEM_MINUS;
        case ImGuiKey_Period: return VK_OEM_PERIOD;
        case ImGuiKey_Slash: return VK_OEM_2;
        case ImGuiKey_Semicolon: return VK_OEM_1;
        case ImGuiKey_Equal: return VK_OEM_PLUS;
        case ImGuiKey_LeftBracket: return VK_OEM_4;
        case ImGuiKey_Backslash: return VK_OEM_5;
        case ImGuiKey_RightBracket: return VK_OEM_6;
        case ImGuiKey_GraveAccent: return VK_OEM_3;
        case ImGuiKey_CapsLock: return VK_CAPITAL;
        case ImGuiKey_ScrollLock: return VK_SCROLL;
        case ImGuiKey_NumLock: return VK_NUMLOCK;
        case ImGuiKey_PrintScreen: return VK_SNAPSHOT;
        case ImGuiKey_Pause: return VK_PAUSE;
        case ImGuiKey_KeypadDecimal: return VK_DECIMAL;
        case ImGuiKey_KeypadDivide: return VK_DIVIDE;
        case ImGuiKey_KeypadMultiply: return VK_MULTIPLY;
        case ImGuiKey_KeypadSubtract: return VK_SUBTRACT;
        case ImGuiKey_KeypadAdd: return VK_ADD;
        case ImGuiKey_KeypadEnter: return VK_RETURN;
        case ImGuiKey_AppBack: return VK_BROWSER_BACK;
        case ImGuiKey_AppForward: return VK_BROWSER_FORWARD;
        case ImGuiKey_Oem102: return VK_OEM_102;
        case ImGuiKey_MouseLeft: return VK_LBUTTON;
        case ImGuiKey_MouseRight: return VK_RBUTTON;
        case ImGuiKey_MouseMiddle: return VK_MBUTTON;
        case ImGuiKey_MouseX1: return VK_XBUTTON1;
        case ImGuiKey_MouseX2: return VK_XBUTTON2;
        default: return 0;
        }
    }

    static bool asynckey(ImGuiKey key)
    {
        const int vk = virtualkey(key);
        return vk != 0 && (GetAsyncKeyState(vk) & 0x8000);
    }

    static bool keylive(bool enabled, ImGuiKey key, ImKeyBindMode mode)
    {
        if (!enabled) return false;
        if (mode == ImKeyBindMode_Always) return true;
        if (mode == ImKeyBindMode_Hold) return asynckey(key);
        return true; // Toggle — always on when enabled
    }

    static int hotkeycount()
    {
        return 0;
    }

    static void panelbase(ImDrawList* dl, ImVec2 p, ImVec2 s, bool hovered, bool active)
    {
        const float r = 11.f;
        const float t = hovered || active ? 1.f : 0.f;
        for (int i = 0; i < 5; i++)
        {
            const float spread = 9.f + i * 7.f;
            const int a = (int)((30.f - i * 4.8f) + t * 8.f);
            dl->AddRectFilled(p - ImVec2(spread * .45f, spread * .25f) + ImVec2(0.f, 9.f + i * 2.f),
                p + s + ImVec2(spread * .45f, spread * .62f),
                IM_COL32(0, 0, 0, a), r + spread);
        }

        dl->AddRectFilledMultiColorRounded(p - ImVec2(15.f, 10.f),
            p + s + ImVec2(15.f, 18.f),
            accent((8.f + t * 5.f) / 255.f),
            accent2((8.f + t * 5.f) / 255.f),
            IM_COL32(0, 0, 0, 0),
            IM_COL32(0, 0, 0, 0), r + 15.f);

        dl->AddRectFilled(p, p + s, panelcolor(), r);
        dl->AddRectFilledMultiColorRounded(p + ImVec2(1.f, 1.f), p + s - ImVec2(1.f, 1.f),
            IM_COL32(255, 255, 255, 10), accent(10.f / 255.f),
            IM_COL32(0, 0, 0, 70), accent2(10.f / 255.f), r - 1.f);
        dl->AddRect(p, p + s,
            color::lerp(accent(.42f), accent(.82f), t),
            r, 0, 1.f);
    }

    template <typename DrawFn>
    static void panel(const char* id, ImVec2& pos, ImVec2 size, bool movable, DrawFn draw)
    {
        clamppanel(pos, size);

        constexpr float drawPad = 30.f;
        ImGui::SetNextWindowPos(pos - ImVec2(drawPad, drawPad), ImGuiCond_Always);
        ImGui::SetNextWindowSize(size + ImVec2(drawPad * 2.f, drawPad * 2.f), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, 0.f));

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        if (!movable)
            flags |= ImGuiWindowFlags_NoInputs;

        if (ImGui::Begin(id, nullptr, flags))
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 p = ImGui::GetWindowPos() + ImVec2(drawPad, drawPad);
            ImGui::SetCursorScreenPos(p);
            ImGui::InvisibleButton("##drag", size);
            const bool hovered = movable && ImGui::IsItemHovered();
            const bool active = movable && ImGui::IsItemActive();

            if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.f))
            {
                pos += ImGui::GetIO().MouseDelta;
                clamppanel(pos, size);
            }

            draw(dl, p, size, hovered, active);
        }

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

    static void watermark(ImDrawList* dl, ImVec2 p, ImVec2 s, bool hovered, bool active)
    {
        // Ghost Sigil style — dark glass pill with accent line
        const float R = 8.f;
        // Shadow
        dl->AddRectFilled(p + ImVec2(0.f, 3.f), p + s + ImVec2(0.f, 3.f),
            IM_COL32(0, 0, 0, 55), R);
        // Background
        dl->AddRectFilled(p, p + s, IM_COL32(12, 12, 17, 210), R);
        // Top specular
        dl->AddRectFilled(p + ImVec2(R, 1.f), p + ImVec2(s.x - R, 2.5f),
            IM_COL32(255, 255, 255, 18), 1.f);
        // Border
        dl->AddRect(p, p + s, IM_COL32(255, 255, 255, 22), R, 0, 1.f);
        // Accent left bar
        dl->AddRectFilled(p + ImVec2(0.f, R), p + ImVec2(2.5f, s.y - R),
            accent(.75f));

        ImFont* font = Tahoma_BoldXP ? Tahoma_BoldXP : ImGui::GetFont();
        const float fs = font->LegacySize;
        ImFont* reg = UiFont ? UiFont : font;

        // "unknownware" brand — accent colored
        const char* brand = "unknownware";
        const ImVec2 bsz = font->CalcTextSizeA(fs, FLT_MAX, 0.f, brand);
        dl->AddText(font, fs, p + ImVec2(12.f, (s.y - fs) * .5f),
            accent(1.f), brand);

        // Separator dot
        float cx = p.x + 12.f + bsz.x + 8.f;
        dl->AddCircleFilled({ cx, p.y + s.y * .5f }, 2.f,
            IM_COL32(255, 255, 255, 55), 6);
        cx += 10.f;

        // FPS
        const float fps = ImMax(1.f, ImGui::GetIO().Framerate);
        char fps_buf[32]; std::snprintf(fps_buf, sizeof(fps_buf), "%.0f fps", fps);
        const ImVec2 fpssz = reg->CalcTextSizeA(reg->LegacySize, FLT_MAX, 0.f, fps_buf);
        dl->AddText(reg, reg->LegacySize, { cx, p.y + (s.y - reg->LegacySize) * .5f },
            IM_COL32(160, 165, 178, 200), fps_buf);
        cx += fpssz.x + 8.f;

        dl->AddCircleFilled({ cx, p.y + s.y * .5f }, 2.f,
            IM_COL32(255, 255, 255, 55), 6);
        cx += 10.f;

        // Time
        SYSTEMTIME st{}; GetLocalTime(&st);
        char time_buf[16]; std::snprintf(time_buf, sizeof(time_buf), "%02u:%02u", st.wHour, st.wMinute);
        dl->AddText(reg, reg->LegacySize, { cx, p.y + (s.y - reg->LegacySize) * .5f },
            IM_COL32(130, 135, 148, 180), time_buf);
    }

    static void hotkeyrow(ImDrawList* dl, ImVec2 p, float w, const char* label, ImGuiKey key, ImKeyBindMode mode, bool live)
    {
        ImFont* font = UiFont ? UiFont : ImGui::GetFont();
        const float fs = font->LegacySize;
        const char* kn = ImGui::GetKeyName(key);
        if (!kn || !*kn) kn = "---";

        // Purple glow row background when active
        if (live) {
            dl->AddRectFilled(p - ImVec2(6.f, 1.f), p + ImVec2(w + 6.f, fs + 3.f),
                IM_COL32(130, 90, 255, 28), 4.f);
        }

        // Label
        ImU32 labelCol = live
            ? IM_COL32(210, 190, 255, 255)
            : IM_COL32(180, 180, 192, 180);
        dl->AddText(font, fs, p, labelCol, label);

        // Key chip (right-aligned)
        const float chipPad = 5.f;
        ImVec2 knSz = font->CalcTextSizeA(fs, FLT_MAX, 0.f, kn);
        float chipW = knSz.x + chipPad * 2.f;
        float chipX = p.x + w - chipW;
        ImVec2 chipMin(chipX, p.y - 1.f);
        ImVec2 chipMax(chipX + chipW, p.y + fs + 1.f);

        ImU32 chipBg  = live ? IM_COL32(100, 60, 200, 160) : IM_COL32(30, 30, 40, 160);
        ImU32 chipBrd = live ? IM_COL32(160, 100, 255, 200) : IM_COL32(80, 80, 100, 120);
        dl->AddRectFilled(chipMin, chipMax, chipBg, 3.f);
        dl->AddRect(chipMin, chipMax, chipBrd, 3.f, 0, 1.f);
        dl->AddText(font, fs, { chipX + chipPad, p.y },
            live ? IM_COL32(230, 210, 255, 255) : IM_COL32(180, 180, 200, 200), kn);

        // Mode dot
        if (mode == ImKeyBindMode_Toggle) {
            ImU32 dotCol = live ? IM_COL32(160, 100, 255, 255) : IM_COL32(80, 80, 100, 160);
            dl->AddCircleFilled({ chipX - 6.f, p.y + fs * .5f }, 2.5f, dotCol, 8);
        }
    }

    static void hotkey(ImDrawList* dl, ImVec2 p, ImVec2 s, bool hovered, bool active)
    {
        // Ghost Sigil style panel
        const float R = 8.f;
        dl->AddRectFilled(p + ImVec2(0.f, 3.f), p + s + ImVec2(0.f, 3.f), IM_COL32(0, 0, 0, 55), R);
        dl->AddRectFilled(p, p + s, IM_COL32(12, 12, 17, 210), R);
        dl->AddRectFilled(p + ImVec2(R, 1.f), p + ImVec2(s.x - R, 2.5f), IM_COL32(255, 255, 255, 14), 1.f);
        dl->AddRect(p, p + s, IM_COL32(255, 255, 255, 22), R, 0, 1.f);
        // Purple accent top bar
        dl->AddRectFilled(p + ImVec2(R + 2.f, 0.f), p + ImVec2(s.x - R - 2.f, 2.f),
            IM_COL32(130, 90, 255, 180), 1.f);

        ImFont* titleFont = Tahoma_BoldXP ? Tahoma_BoldXP : ImGui::GetFont();
        ImFont* rowFont   = UiFont ? UiFont : ImGui::GetFont();
        const float tfs   = titleFont->LegacySize;
        const float rfs   = rowFont->LegacySize;

        // Title
        const char* titleText = "keybinds";
        ImVec2 tsz = titleFont->CalcTextSizeA(tfs, FLT_MAX, 0.f, titleText);
        dl->AddText(titleFont, tfs, { p.x + (s.x - tsz.x) * .5f, p.y + 7.f },
            IM_COL32(210, 215, 230, 220), titleText);

        // Separator
        float sepY = p.y + 7.f + tfs + 5.f;
        dl->AddLine({ p.x + 8.f, sepY }, { p.x + s.x - 8.f, sepY },
            IM_COL32(255, 255, 255, 18), 1.f);

        // Build bind list — every configured keybind
        struct BindEntry { const char* label; ImGuiKey key; ImKeyBindMode mode; bool live; };
        std::vector<BindEntry> entries;

        auto check_live = [](ImGuiKey k, ImKeyBindMode m) -> bool {
            if (k == ImGuiKey_None) return false;
            if (m == ImKeyBindMode_Always) return true;
            const int vk = virtualkey(k);
            if (!vk) return false;
            return (GetAsyncKeyState(vk) & 0x8000) != 0;
        };

        // Show every feature that has a key bound AND is enabled
        if (global::aim::TriggerBot_Key != ImGuiKey_None && global::aim::TriggerBot)
            entries.push_back({ "Triggerbot", global::aim::TriggerBot_Key, global::aim::TriggerBot_Mode,
                check_live(global::aim::TriggerBot_Key, global::aim::TriggerBot_Mode) });
        if (global::aimbot::Key != ImGuiKey_None && global::aimbot::Enabled)
            entries.push_back({ "Aimbot", global::aimbot::Key, global::aimbot::Mode,
                check_live(global::aimbot::Key, global::aimbot::Mode) });
        if (global::silent::Enabled && global::silent::Silent_Key != ImGuiKey_None)
            entries.push_back({ "Silent Aim", global::silent::Silent_Key, global::silent::Silent_Mode,
                check_live(global::silent::Silent_Key, global::silent::Silent_Mode) });
        if (global::esp::Enabled)
            entries.push_back({ "ESP", global::setting::Menu_Key, ImKeyBindMode_Toggle, global::esp::Enabled });
        if (global::misc::fly && global::misc::Fly_Key != ImGuiKey_None)
            entries.push_back({ "Fly", global::misc::Fly_Key, global::misc::Fly_Mode,
                check_live(global::misc::Fly_Key, global::misc::Fly_Mode) });
        if (global::misc::Desync && global::misc::Desync_Key != ImGuiKey_None)
            entries.push_back({ "Desync", global::misc::Desync_Key, global::misc::Desync_Mode,
                check_live(global::misc::Desync_Key, global::misc::Desync_Mode) });
        if (global::misc::Tickrate && global::misc::Tickrate_Key != ImGuiKey_None)
            entries.push_back({ "Tickrate", global::misc::Tickrate_Key, global::misc::Tickrate_Mode,
                check_live(global::misc::Tickrate_Key, global::misc::Tickrate_Mode) });
        if (global::misc::AnimChanger && global::misc::AnimKey != ImGuiKey_None)
            entries.push_back({ "Animation", global::misc::AnimKey, global::misc::AnimMode,
                check_live(global::misc::AnimKey, global::misc::AnimMode) });
        if (global::misc::ClockTime && global::misc::ClockKey != ImGuiKey_None)
            entries.push_back({ "Lighting", global::misc::ClockKey, global::misc::ClockMode,
                check_live(global::misc::ClockKey, global::misc::ClockMode) });
        if (global::crosshair::Enabled)
            entries.push_back({ "Crosshair", ImGuiKey_None, ImKeyBindMode_Always, true });
        if (global::tracer::Enabled)
            entries.push_back({ "Tracers", ImGuiKey_None, ImKeyBindMode_Always, true });

        if (entries.empty()) {
            dl->AddText(rowFont, rfs, p + ImVec2(10.f, sepY + 8.f),
                IM_COL32(100, 100, 115, 160), "No binds set");
            return;
        }

        float rowY = sepY + 8.f;
        const float rowH = rfs + 6.f;
        const float rowW = s.x - 18.f;

        for (const auto& e : entries) {
            if (e.key == ImGuiKey_None) continue;
            hotkeyrow(dl, { p.x + 10.f, rowY }, rowW, e.label, e.key, e.mode, e.live);
            rowY += rowH;
            if (rowY + rowH > p.y + s.y - 6.f) break;
        }
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

    static bool partpose(const sdk::instance& inst, sdk::vector3& out, sdk::matrix3& rot)
    {
        if (!inst.Address)
            return false;

        sdk::part part(inst.Address);
        sdk::part primitive = part.primitive();
        if (!primitive.Address)
            return false;

        out = primitive.position();
        rot = primitive.rotation();
        return !(std::isnan(out.x) || std::isnan(out.y) || std::isnan(out.z));
    }

    static bool partposition(const sdk::instance& inst, sdk::vector3& out)
    {
        sdk::matrix3 ignored{};
        return partpose(inst, out, ignored);
    }

    static bool playerposition(const sdk::player& player, sdk::vector3& out)
    {
        if (partposition(player.HumanoidRootPart, out))
            return true;
        if (partposition(player.LowerTorso, out))
            return true;
        if (partposition(player.Torso, out))
            return true;
        return partposition(player.Head, out);
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

    static bool aiminglocal(const sdk::player& player, const sdk::vector3& localPos)
    {
        sdk::vector3 origin{};
        sdk::vector3 direction{};
        if (!aimray(player, origin, direction))
            return false;

        const sdk::vector3 toLocal = localPos - origin;
        const float along = dot(toLocal, direction);
        const float maxLength = ImClamp(global::overlay::AimView_MaxLength, 50.f, 1000.f);
        if (along < 0.f || along > maxLength)
            return false;

        const sdk::vector3 closest = origin + direction * along;
        const float miss = closest.distance(localPos);
        const float tolerance = ImClamp(toLocal.magnitude() * .018f, 4.0f, 10.0f);
        return miss <= tolerance;
    }

    struct aimthreat
    {
        int Count = 0;
        std::string name;
    };

    static aimthreat getthreat()
    {
        aimthreat threat{};

        sdk::vector3 localPos{};
        if (!playerposition(global::LocalPlayer, localPos))
            return threat;

        const auto players = cache::snapshot();
        for (const auto& player : players)
        {
            if (!player.character.Address)
                continue;
            if (global::LocalPlayer.character.Address &&
                player.character.Address == global::LocalPlayer.character.Address)
                continue;

            if (!aiminglocal(player, localPos))
                continue;

            ++threat.Count;
            if (threat.name.empty())
                threat.name = !player.Display_Name.empty() ? player.Display_Name : player.name;
        }

        return threat;
    }

    static void aimwarning(ImDrawList* dl)
    {
        const aimthreat threat = getthreat();
        if (threat.Count <= 0)
            return;

        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const float pulse = (sinf((float)ImGui::GetTime() * 6.0f) + 1.f) * .5f;
        const char* title = "AIM WARNING";
        char detail[96]{};
        if (threat.Count == 1)
            std::snprintf(detail, sizeof(detail), "%s is aiming at you", threat.name.c_str());
        else
            std::snprintf(detail, sizeof(detail), "%dx players aiming at you", threat.Count);

        ImFont* font = Tahoma_BoldXP ? Tahoma_BoldXP : ImGui::GetFont();
        const float fontSize = font->LegacySize;
        const ImVec2 titleSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, title);
        const ImVec2 detailSize = ImGui::CalcTextSize(detail);
        const float width = ImClamp(ImMax(titleSize.x, detailSize.x) + 74.f, 258.f, 420.f);
        const ImVec2 warnMin(display.x * .5f - width * .5f, 72.f);
        const ImVec2 warnMax(warnMin.x + width, warnMin.y + 58.f);

        for (int i = 0; i < 4; ++i)
        {
            const float spread = 5.f + i * 5.f;
            dl->AddRectFilled(warnMin - ImVec2(spread, spread * .45f) + ImVec2(0.f, 5.f + i),
                warnMax + ImVec2(spread, spread * .7f),
                IM_COL32(0, 0, 0, 42 - i * 7), 10.f + spread);
        }

        dl->AddRectFilled(warnMin, warnMax, IM_COL32(23, 6, 12, 222), 9.f);
        dl->AddRectFilledMultiColorRounded(warnMin + ImVec2(1.f, 1.f), warnMax - ImVec2(1.f, 1.f),
            IM_COL32(255, 80, 104, 40 + (int)(pulse * 34.f)),
            IM_COL32(100, 117, 255, 24),
            IM_COL32(0, 0, 0, 82),
            IM_COL32(255, 80, 104, 18), 9.f);
        dl->AddRect(warnMin, warnMax, IM_COL32(255, 80, 104, 170 + (int)(pulse * 60.f)), 9.f, 0, 1.35f);

        const ImVec2 icon(warnMin.x + 22.f, warnMin.y + 29.f);
        dl->AddTriangleFilled(icon + ImVec2(0.f, -12.f), icon + ImVec2(12.f, 10.f), icon + ImVec2(-12.f, 10.f),
            IM_COL32(255, 80, 104, 235));
        dl->AddText(font, fontSize, ImVec2(icon.x - 3.f, icon.y - 8.f), IM_COL32(23, 6, 12, 255), "!");

        dl->AddText(font, fontSize, ImVec2(warnMin.x + 48.f, warnMin.y + 9.f), IM_COL32(255, 238, 241, 255), title);
        dl->AddText(ImVec2(warnMin.x + 48.f, warnMin.y + 33.f), IM_COL32(255, 178, 190, 255), detail);
    }

    static void render(bool movable)
    {
        solusdefaultplacement();

        if (global::overlay::watermark)
            panel("##unknownware_watermark", global::overlay::Watermark_Pos,
                ImVec2(280.f, 30.f), movable, watermark);

        if (global::overlay::hotkey && global::overlay::ShowKeybinds)
        {
            // Count actual entries to size panel correctly
            int bindRows = 0;
            if (global::aim::TriggerBot_Key != ImGuiKey_None && global::aim::TriggerBot) bindRows++;
            if (global::aimbot::Key != ImGuiKey_None && global::aimbot::Enabled)         bindRows++;
            if (global::silent::Enabled && global::silent::Silent_Key != ImGuiKey_None)  bindRows++;
            if (global::esp::Enabled)                                                    bindRows++;
            if (global::misc::fly && global::misc::Fly_Key != ImGuiKey_None)             bindRows++;
            if (global::misc::Desync && global::misc::Desync_Key != ImGuiKey_None)       bindRows++;
            if (global::misc::Tickrate && global::misc::Tickrate_Key != ImGuiKey_None)   bindRows++;
            if (global::misc::AnimChanger && global::misc::AnimKey != ImGuiKey_None)     bindRows++;
            if (global::misc::ClockTime && global::misc::ClockKey != ImGuiKey_None)      bindRows++;
            if (global::crosshair::Enabled)                                              bindRows++;
            if (global::tracer::Enabled)                                                 bindRows++;
            bindRows = ImMax(1, bindRows);

            const float rowH = (UiFont ? UiFont : ImGui::GetFont())->LegacySize + 6.f;
            const float panelH = 36.f + bindRows * rowH;
            panel("##unknownware_hotkeys", global::overlay::Hotkeys_Pos,
                ImVec2(190.f, panelH), movable, hotkey);
        }

        // ── Notifications panel (damage log) — only visible when menu is open ──
        if (movable && global::overlay::Notifications && global::overlay::DamageLogs)
        {
            ImVec2& notif_pos = global::overlay::Notif_Pos;

            // Clamp
            const ImVec2 disp = ImGui::GetIO().DisplaySize;
            const ImVec2 notif_size(220.f, 28.f);
            notif_pos.x = ImClamp(notif_pos.x, 0.f, disp.x - notif_size.x);
            notif_pos.y = ImClamp(notif_pos.y, 0.f, disp.y - notif_size.y);

            constexpr float drawPad = 20.f;
            ImGui::SetNextWindowPos(notif_pos - ImVec2(drawPad, drawPad), ImGuiCond_Always);
            ImGui::SetNextWindowSize(notif_size + ImVec2(drawPad * 2.f, drawPad * 2.f), ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, 0.f));

            if (ImGui::Begin("##notif_anchor", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBackground |
                ImGuiWindowFlags_NoBringToFrontOnFocus))
            {
                ImVec2 p = ImGui::GetWindowPos() + ImVec2(drawPad, drawPad);
                ImDrawList* ndl = ImGui::GetWindowDrawList();

                ImGui::SetCursorScreenPos(p);
                ImGui::InvisibleButton("##notif_drag", notif_size);
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.f)) {
                    notif_pos += ImGui::GetIO().MouseDelta;
                }

                // Header bar
                const float R = 6.f;
                ndl->AddRectFilled(p, p + notif_size, IM_COL32(12, 12, 17, 195), R);
                ndl->AddRect(p, p + notif_size, IM_COL32(255, 255, 255, 20), R, 0, 1.f);
                // Purple left bar (damage color)
                ndl->AddRectFilled(p + ImVec2(0.f, R), p + ImVec2(2.5f, notif_size.y - R),
                    IM_COL32(255, 75, 100, 180));
                // Label
                ImFont* lf = UiFont ? UiFont : ImGui::GetFont();
                const char* lbl = "notifications";
                ImVec2 lsz = lf->CalcTextSizeA(lf->LegacySize, FLT_MAX, 0.f, lbl);
                ndl->AddText(lf, lf->LegacySize,
                    p + ImVec2((notif_size.x - lsz.x) * .5f, (notif_size.y - lf->LegacySize) * .5f),
                    IM_COL32(180, 185, 200, 200), lbl);
            }
            ImGui::End();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
        }
    }
}


static void dissolve(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float t, float time)
{
    const float a = sinf(fx::saturate(t) * 3.14159265f);
    if (a <= .01f)
        return;

    const float w = mx.x - mn.x;
    const float h = mx.y - mn.y;
    for (int i = 0; i < 30; i++)
    {
        const float seed = (float)i * 12.9898f;
        const float u = fmodf(fabsf(sinf(seed) * 43758.5453f), 1.f);
        const float v = fmodf(fabsf(sinf(seed + 41.371f) * 24634.6345f), 1.f);
        const float drift = sinf(time * 2.1f + seed) * 5.f * (1.f - fabsf(t - .5f) * 1.55f);
        const float size = 1.0f + v * 1.8f;
        ImVec2 p;

        if ((i & 3) == 0)
            p = { mn.x + u * w, mn.y - 11.f + drift };
        else if ((i & 3) == 1)
            p = { mn.x + u * w, mx.y + 8.f + drift };
        else if ((i & 3) == 2)
            p = { mn.x - 10.f + drift, mn.y + u * h };
        else
            p = { mx.x + 8.f + drift, mn.y + u * h };

        const int alpha = (int)(a * (14.f + v * 28.f));
        const ImU32 col = (i % 5) == 0 ? IM_COL32(100, 117, 255, alpha) : IM_COL32(0, 174, 255, alpha);
        dl->AddRectFilled(p, p + ImVec2(size, size), col, 1.5f);
    }
}

static bool focusroblox()
{
    HWND roblox = FindWindowA(nullptr, "Roblox");
    if (!roblox || !IsWindow(roblox))
        return false;

    if (GetForegroundWindow() == roblox)
        return true;

    if (IsIconic(roblox))
        ShowWindow(roblox, SW_RESTORE);

    const DWORD currentThread = GetCurrentThreadId();
    const DWORD targetThread = GetWindowThreadProcessId(roblox, nullptr);
    HWND foreground = GetForegroundWindow();
    const DWORD foregroundThread = foreground ? GetWindowThreadProcessId(foreground, nullptr) : 0;

    const bool attachForeground = foregroundThread != 0 && foregroundThread != currentThread;
    const bool attachTarget = targetThread != 0 && targetThread != currentThread && targetThread != foregroundThread;

    if (attachForeground)
        AttachThreadInput(currentThread, foregroundThread, TRUE);
    if (attachTarget)
        AttachThreadInput(currentThread, targetThread, TRUE);

    BringWindowToTop(roblox);
    const bool focused = SetForegroundWindow(roblox) != FALSE;

    if (attachTarget)
        AttachThreadInput(currentThread, targetThread, FALSE);
    if (attachForeground)
        AttachThreadInput(currentThread, foregroundThread, FALSE);

    return focused || GetForegroundWindow() == roblox;
}

static void keepfocus(float elapsed)
{
    static double lastFocusAttempt = -1.0;
    const double now = ImGui::GetTime();
    if (elapsed > 4.0f || (lastFocusAttempt >= 0.0 && now - lastFocusAttempt < .35))
        return;

    lastFocusAttempt = now;
    focusroblox();
}

static double welcome_start = -1.0;

static bool welcome_done()
{
    return welcome_start < 0.0 || (float)(ImGui::GetTime() - welcome_start) > 3.5f;
}

static void welcome(bool menuOpen)
{
    if (welcome_start < 0.0)
        welcome_start = ImGui::GetTime();

    const float elapsed = (float)(ImGui::GetTime() - welcome_start);
    if (elapsed > 3.5f)
        return;

    if (!menuOpen)
        keepfocus(elapsed);

    const float inT = fx::easeoutcubic(ImClamp(elapsed / .45f, 0.f, 1.f));
    const float outT = elapsed < 2.8f ? 1.f : 1.f - fx::easeoutcubic(ImClamp((elapsed - 2.8f) / .7f, 0.f, 1.f));
    const float alpha = inT * outT;
    if (alpha <= .01f)
        return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const ImVec2 center = display * .5f;

    dl->AddRectFilled({ 0.f, 0.f }, display, IM_COL32(2, 5, 9, (int)(232.f * alpha)));

    float spin_angle = elapsed * 2.8f;
    float orbit_r = 48.f;
    ImVec2 orbit_center = center + ImVec2(0.f, -10.f);
    int segments = 3;
    for (int i = 0; i < segments; i++) {
        float a = spin_angle + (6.2832f * i / segments);
        float radius = 3.5f + sinf(elapsed * 4.f + i * 2.1f) * 1.2f;
        ImVec2 p = orbit_center + ImVec2(cosf(a) * orbit_r, sinf(a) * orbit_r);
        dl->AddCircleFilled(p, radius, IM_COL32(0, 174, 255, (int)(245.f * alpha)), 12);
    }

    ImFont* logo = LogoFont ? LogoFont : (TitleFont ? TitleFont : ImGui::GetFont());
    const float titleSize = logo->CalcTextSizeA(logo->LegacySize * 1.3f, FLT_MAX, 0.f, "UNKNOWNWARE").x;
    const ImVec2 titlePos(center.x - titleSize * .5f, center.y + 20.f);
    dl->AddText(logo, logo->LegacySize * 1.3f, titlePos + ImVec2(0.f, 2.f), IM_COL32(0, 0, 0, (int)(190.f * alpha)), "UNKNOWNWARE");
    dl->AddText(logo, logo->LegacySize * 1.3f, titlePos, IM_COL32(242, 250, 255, (int)(255.f * alpha)), "UNKNOWNWARE");

    float prog = ImClamp(elapsed / 2.8f, 0.f, 1.f);
    float barW = 160.f;
    ImVec2 barMin(center.x - barW * .5f, center.y + 50.f);
    ImVec2 barMax = barMin + ImVec2(barW * prog, 2.5f);
    dl->AddRectFilled(ImVec2(center.x - barW * .5f, barMin.y), ImVec2(center.x + barW * .5f, barMax.y), IM_COL32(20, 35, 48, (int)(200.f * alpha)), 1.5f);
    dl->AddRectFilled(barMin, barMax, IM_COL32(0, 174, 255, (int)(255.f * alpha)), 1.5f);
}

void graphic::menu()
{
    color::refresh();

    static int Section = 0;
    static int LastSection = 0;
    static float SectionAlpha = 1.f;
    static int HeaderPrevSection = 0;
    static float HeaderAlpha = 1.f;
    static float MenuT = 0.f;
    static bool MenuPosReady = false;
    static ImVec2 MenuPos = {};

    ImGuiIO& IO = ImGui::GetIO();

    const bool compact = false;

    const float kWinW = 660.f;
    const float kWinH = 560.f;
    constexpr float kR = 0.f;
    const float sbW = compact ? side::kCompactW : side::kW;
    const float kHeaderH = 29.f;

    MenuT = fx::damp(MenuT, Running ? 1.f : 0.f, Running ? 8.5f : 7.2f);
    if (!Running && MenuT <= .01f)
        return;
    const float MenuEase = fx::easeoutquint(MenuT);
    const float MenuAlpha = fx::easeoutcubic(MenuT);
    const float BackdropT = fx::easeinoutcubic(MenuT);
    const ImVec2 MenuOffset = { 0.f, (1.f - MenuEase) * 18.f };

    if (!MenuPosReady)
    {
        MenuPos = IO.DisplaySize / 2.f;
        MenuPosReady = true;
    }

    const float minX = kWinW * .5f;
    const float maxX = IO.DisplaySize.x - kWinW * .5f;
    const float minY = kWinH * .5f;
    const float maxY = IO.DisplaySize.y - kWinH * .5f;
    MenuPos.x = maxX > minX ? ImClamp(MenuPos.x, minX, maxX) : IO.DisplaySize.x * .5f;
    MenuPos.y = maxY > minY ? ImClamp(MenuPos.y, minY, maxY) : IO.DisplaySize.y * .5f;

    const ImVec2 menuMin = MenuPos + MenuOffset - ImVec2(kWinW, kWinH) * .5f;
    const ImVec2 menuMax = menuMin + ImVec2(kWinW, kWinH);
    ImDrawList* BDL = ImGui::GetBackgroundDrawList();
    BDL->AddRectFilled({ 0.f, 0.f }, IO.DisplaySize, IM_COL32(0, 0, 0, (int)(BackdropT * 24.f)));
    BDL->AddRectFilled(menuMin + ImVec2(4.f, 5.f), menuMax + ImVec2(4.f, 5.f),
        IM_COL32(0, 0, 0, (int)(BackdropT * 90.f)));

    ImGui::SetNextWindowSize({ kWinW, kWinH }, ImGuiCond_Always);
    ImGui::SetNextWindowPos(MenuPos + MenuOffset, ImGuiCond_Always, { 0.5f, 0.5f });

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, MenuAlpha);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::ColorConvertU32ToFloat4(color::Win));

    const bool open = ImGui::Begin("##autopsy.lol", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoBackground);

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
    if (!open) { ImGui::End(); return; }

    ImVec2 WP = ImGui::GetWindowPos();
    const ImVec2 WS = ImGui::GetWindowSize();
    ImDrawList* DL = ImGui::GetWindowDrawList();

    const float time = (float)ImGui::GetTime();
    const float glow = (sinf(time * 2.3f) + 1.f) * .5f;

    auto dragMenu = [&]()
        {
            if (Running && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.f))
            {
                MenuPos += IO.MouseDelta;
                MenuPos.x = maxX > minX ? ImClamp(MenuPos.x, minX, maxX) : IO.DisplaySize.x * .5f;
                MenuPos.y = maxY > minY ? ImClamp(MenuPos.y, minY, maxY) : IO.DisplaySize.y * .5f;
                ImGui::SetWindowPos(MenuPos + MenuOffset - WS * .5f, ImGuiCond_Always);
                WP = ImGui::GetWindowPos();
            }
        };

    ImGui::SetCursorScreenPos(WP);
    ImGui::InvisibleButton("##drag_logo", { sbW, compact ? 8.f : side::kLogoH });
    dragMenu();
    if (compact)
    {
        const float dragTop = 8.f + side::kTabH * 6.f + 10.f;
        if (dragTop < WS.y)
        {
            ImGui::SetCursorScreenPos({ WP.x, WP.y + dragTop });
            ImGui::InvisibleButton("##drag_sidebar_bottom", { sbW, WS.y - dragTop });
            dragMenu();
        }
    }
    ImGui::SetCursorScreenPos({ WP.x + sbW, WP.y });
    ImGui::InvisibleButton("##drag_header", { WS.x - sbW, kHeaderH });
    dragMenu();

    DL->PushClipRect(WP - ImVec2(2.f, 2.f), WP + WS + ImVec2(2.f, 2.f), false);

    // ── Ghost Sigil window chrome ─────────────────────────────────────────────
    const float RAD = 12.f;

    // Ambient glow layers behind the window
    for (int i = 4; i >= 1; i--) {
        const float sp    = (float)i * 8.f;
        const float gAlph = (0.028f - i * 0.005f) * MenuAlpha;
        DL->AddRectFilled(
            WP - ImVec2(sp * .4f, sp * .3f),
            WP + WS + ImVec2(sp * .4f, sp * .6f),
            color::alpha(color::Accent, gAlph), RAD + sp);
    }

    // Main window fill — deep dark with subtle purple tint
    DL->AddRectFilled(WP, WP + WS,
        color::alpha(color::Win, MenuAlpha), RAD);

    // Background texture (if present)
    if (Detail->IdaBgTexture) {
        constexpr float idaTextureDim = 4096.f;
        const ImVec2 uv0 = { WP.x / idaTextureDim,          WP.y / idaTextureDim };
        const ImVec2 uv1 = { (WP.x + WS.x) / idaTextureDim, (WP.y + WS.y) / idaTextureDim };
        DL->AddImageRounded((ImTextureID)(intptr_t)Detail->IdaBgTexture,
            WP, WP + WS, uv0, uv1,
            IM_COL32(255, 255, 255, (int)(MenuAlpha * 18.f)), RAD);
    }

    // Inner top-highlight (specular)
    DL->AddRectFilled(WP + ImVec2(RAD, 1.f),
        WP + ImVec2(WS.x - RAD, 3.f),
        IM_COL32(255, 255, 255, (int)(MenuAlpha * 18.f)), 1.f);

    // Outer border
    DL->AddRect(WP, WP + WS,
        color::alpha(color::Border, MenuAlpha), RAD, 0, 1.f);

    // Accent top-bar (2px)
    DL->AddRectFilled(WP + ImVec2(RAD + 2.f, 0.f),
        WP + ImVec2(WS.x - RAD - 2.f, 2.f),
        color::alpha(color::Accent, MenuAlpha * .85f), 1.f);

    // ── Sidebar background ────────────────────────────────────────────────────
    DL->AddRectFilled(WP + ImVec2(1.f, 1.f),
        WP + ImVec2(sbW - 1.f, WS.y - 1.f),
        color::alpha(IM_COL32(255, 255, 255, 10), MenuAlpha),
        RAD - 1.f, ImDrawFlags_RoundCornersLeft);
    DL->AddLine(WP + ImVec2(sbW, kHeaderH),
        WP + ImVec2(sbW, WS.y),
        color::alpha(color::Divider, MenuAlpha), 1.f);

    // ── Header background ─────────────────────────────────────────────────────
    DL->AddRectFilled(WP + ImVec2(1.f, 1.f),
        WP + ImVec2(WS.x - 1.f, kHeaderH),
        color::alpha(IM_COL32(255, 255, 255, 12), MenuAlpha),
        RAD - 1.f, ImDrawFlags_RoundCornersTop);
    DL->AddLine(WP + ImVec2(sbW, kHeaderH),
        WP + ImVec2(WS.x, kHeaderH),
        color::alpha(color::Divider, MenuAlpha), 1.f);

    // ── Logo ──────────────────────────────────────────────────────────────────
    {
        ImFont* lf   = LogoFont ? LogoFont : (Tahoma_BoldXP ? Tahoma_BoldXP : ImGui::GetFont());
        const float ls = lf->LegacySize + 2.f;
        const ImVec2 lp = WP + ImVec2(14.f, (kHeaderH - ls) * .5f);
        DL->AddText(lf, ls, lp + ImVec2(.5f, .5f),
            color::alpha(IM_COL32(0, 0, 0, 120), MenuAlpha), "rbx");
        DL->AddText(lf, ls, lp,
            color::alpha(color::text, MenuAlpha), "rbx");
        // Accent dot
        DL->AddCircleFilled(lp + ImVec2(ImGui::CalcTextSize("rbx").x + 4.f, ls * .45f),
            2.5f, color::alpha(color::Accent, MenuAlpha), 8);
    }

    DL->PopClipRect();

    using Fn = void(*)(ImDrawList*, ImVec2, float, ImU32);
    constexpr Fn icons[] = { icon::crosshair, icon::diamond, icon::crosshair, icon::eye, icon::layer, icon::diamond, icon::layer, icon::gear };
    constexpr const char* labels[] = { "Triggerbot", "Aimbot", "Silent", "Visuals", "Misc", "Games", "Players", "Settings" };
    constexpr int kTabs = 8;

    const float tabY = 19.f;

    // ── Ghost Sigil sliding pill nav ──────────────────────────────────────────
    {
        const float TAB_H   = 44.f;
        const float TAB_PAD = 8.f;
        const float TPX     = 8.f;
        const float startY  = kHeaderH + 8.f;

        // Animated active pill position
        static float s_pill_y = startY;
        const float  pill_target = startY + Section * (TAB_H + 4.f);
        s_pill_y = fx::damp(s_pill_y, pill_target, 20.f);

        // Draw pill
        const ImVec2 pillMin = WP + ImVec2(TPX, s_pill_y);
        const ImVec2 pillMax = WP + ImVec2(sbW - TPX, s_pill_y + TAB_H);
        DL->AddRectFilled(pillMin, pillMax,
            color::alpha(color::AccentDim, MenuAlpha * .6f), 10.f);
        DL->AddRect(pillMin, pillMax,
            color::alpha(color::Accent, MenuAlpha * .55f), 10.f, 0, 1.f);

        // Draw each tab
        for (int i = 0; i < kTabs; i++) {
            const float ty    = startY + i * (TAB_H + 4.f);
            const ImVec2 tMin = WP + ImVec2(TPX, ty);
            const ImVec2 tMax = WP + ImVec2(sbW - TPX, ty + TAB_H);
            const ImVec2 ctr  = { tMin.x + (sbW - TPX * 2.f) * .5f, tMin.y + TAB_H * .5f };

            const bool hov = ImGui::IsMouseHoveringRect(tMin, tMax);
            if (hov && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                Section = i;

            // Hover tint
            if (hov && i != Section)
                DL->AddRectFilled(tMin, tMax,
                    color::alpha(IM_COL32(255,255,255,8), MenuAlpha), 10.f);

            // Icon
            const bool  active  = (i == Section);
            const float iconSz  = 20.f;
            const float iconAlp = active ? MenuAlpha : MenuAlpha * (hov ? 0.65f : 0.38f);
            const ImU32 iconCol = color::alpha(active ? color::AccentFg : color::text, iconAlp);
            if (icons[i])
                icons[i](DL, ctr - ImVec2(0.f, 6.f), iconSz, iconCol);

            // Label
            const ImVec2 lblSz = ImGui::CalcTextSize(labels[i]);
            DL->AddText({ ctr.x - lblSz.x * .5f, ctr.y + 6.f },
                color::alpha(active ? color::AccentFg : color::TextMid,
                    MenuAlpha * (active ? 1.f : 0.5f)),
                labels[i]);
        }
    }

    // Keep for_loop compat with old code that reads Section via side::tab
    for (int i = 0; i < kTabs; i++) { (void)i; }

    if (LastSection != Section)
    {
        HeaderPrevSection = LastSection;
        LastSection = Section;
        SectionAlpha = 0.f;
        HeaderAlpha = 0.f;
    }
    SectionAlpha = fx::damp(SectionAlpha, 1.f, 8.5f);
    const float SectionEase = fx::easeoutcubic(SectionAlpha);
    HeaderAlpha = fx::damp(HeaderAlpha, 1.f, 9.5f);
    (void)HeaderPrevSection;
    (void)HeaderAlpha;

    const float contentX = WP.x + 100.f;
    const float contentW = WS.x - 123.f;
    const float bodyY = WP.y + kHeaderH + 1.f;
    const float bodyH = WS.y - 53.f;

    constexpr float kPad = 0.f;
    ImGui::SetCursorScreenPos({ contentX + kPad + (1.f - SectionEase) * 18.f, bodyY + kPad });
    ImGui::PushClipRect({ contentX, bodyY - 14.f }, { contentX + contentW, WP.y + WS.y }, true);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, SectionEase);
    ImGui::BeginGroup();

    const float bInW = contentW - kPad * 2.f;
    const float bInH = bodyH - kPad * 2.f;
    const float halfW = (bInW - 8.f) * .5f;
    const float halfH = (bInH - 8.f) * .5f;


    if (Section == 0)
    {
        if (ui::card::begin("##tb", { halfW, bInH }, "TRIGGERBOT"))
        {
            ui::toggle("Enable", &global::aim::TriggerBot);
            ImGui::SameLine(ui::rightslot(ui::bindwidth(global::aim::TriggerBot_Key, global::aim::TriggerBot_Mode), 4.f));
            ui::bind("##tbk", &global::aim::TriggerBot_Key, &global::aim::TriggerBot_Mode);

            ui::gap(4.f);
            if (global::aim::TriggerBot) {
                ui::sliderfloat("Radius", &global::aim::TriggerRadius, 1.f, 25.f);
                ui::toggle("Interval", &global::aim::IntervalToggle);
                if (global::aim::IntervalToggle)
                    ui::sliderint("Delay (ms)", &global::aim::TriggerDelayMs, 0, 250);
            }

            ui::toggle("FOV Check", &global::aim::FovCheck);
            ui::toggle("Automatic", &global::aim::DistanceScale);
            ui::sliderfloat("Head Size", &global::aim::AutoHeadScale, 0.1f, 3.f, "%.1f");
            ui::toggle("Visualise Hitbox", &global::aim::VisualiseHitbox);
            ui::toggle("Visible Check", &global::aim::VisibleCheck);
            ui::toggle("Dead Check", &global::aim::DeadCheck);

            ui::gap(6.f);
            ui::labelsection("HIT PART");
            auto hitpart = [](const char* label, bool* var) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(*var ? color::AccentFg : color::TextMid));
                ImGui::Selectable(label, var, ImGuiSelectableFlags_DontClosePopups);
                ImGui::PopStyleColor();
            };
            ImGui::PushStyleColor(ImGuiCol_Header, ImGui::ColorConvertU32ToFloat4(color::AccentDim));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::ColorConvertU32ToFloat4(color::AccentDim));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::ColorConvertU32ToFloat4(color::Accent));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.f, 4.f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 1.f));
            int hitCount = 0;
            if (global::aim::HitPart_Head) ++hitCount;
            if (global::aim::HitPart_Torso) ++hitCount;
            if (global::aim::HitPart_UpperTorso) ++hitCount;
            if (global::aim::HitPart_LowerTorso) ++hitCount;
            if (global::aim::HitPart_HumanoidRootPart) ++hitCount;
            if (global::aim::HitPart_LeftArm) ++hitCount;
            if (global::aim::HitPart_RightArm) ++hitCount;
            if (global::aim::HitPart_LeftLeg) ++hitCount;
            if (global::aim::HitPart_RightLeg) ++hitCount;
            char preview[64]{};
            std::snprintf(preview, sizeof(preview), "%d / 9 selected", hitCount);
            if (ImGui::BeginCombo("##tbhc", preview))
            {
                hitpart("Head", &global::aim::HitPart_Head);
                hitpart("Torso", &global::aim::HitPart_Torso);
                hitpart("Upper Torso", &global::aim::HitPart_UpperTorso);
                hitpart("Lower Torso", &global::aim::HitPart_LowerTorso);
                hitpart("Humanoid Root", &global::aim::HitPart_HumanoidRootPart);
                hitpart("Left Arm", &global::aim::HitPart_LeftArm);
                hitpart("Right Arm", &global::aim::HitPart_RightArm);
                hitpart("Left Leg", &global::aim::HitPart_LeftLeg);
                hitpart("Right Leg", &global::aim::HitPart_RightLeg);
                ImGui::EndCombo();
            }
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);
        }
        ui::card::end();

        ImGui::SameLine(0.f, 6.f);

        if (ui::card::begin("##tbscales", { halfW, bInH }, "HITBOX SIZE"))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 3.f));
            auto scale_slider = [](const char* label, float* val, bool enabled) {
                if (!enabled) { ImGui::BeginDisabled(); }
                ui::sliderfloat(label, val, 0.1f, 50.f);
                if (!enabled) { ImGui::EndDisabled(); }
            };
            scale_slider("Head", &global::aim::HitPart_Head_Scale, global::aim::HitPart_Head);
            scale_slider("Torso", &global::aim::HitPart_Torso_Scale, global::aim::HitPart_Torso);
            scale_slider("Upper Torso", &global::aim::HitPart_UpperTorso_Scale, global::aim::HitPart_UpperTorso);
            scale_slider("Lower Torso", &global::aim::HitPart_LowerTorso_Scale, global::aim::HitPart_LowerTorso);
            scale_slider("Humanoid Root", &global::aim::HitPart_HumanoidRootPart_Scale, global::aim::HitPart_HumanoidRootPart);
            scale_slider("Left Arm", &global::aim::HitPart_LeftArm_Scale, global::aim::HitPart_LeftArm);
            scale_slider("Right Arm", &global::aim::HitPart_RightArm_Scale, global::aim::HitPart_RightArm);
            scale_slider("Left Leg", &global::aim::HitPart_LeftLeg_Scale, global::aim::HitPart_LeftLeg);
            scale_slider("Right Leg", &global::aim::HitPart_RightLeg_Scale, global::aim::HitPart_RightLeg);
            ImGui::PopStyleVar();
        }
        ui::card::end();
    }


    if (Section == 1)
    {
        if (ui::card::begin("##ab", { halfW, bInH }, "AIMBOT"))
        {
            ui::toggle("Enabled", &global::aimbot::Enabled);
            ImGui::SameLine(ui::rightslot(ui::bindwidth(global::aimbot::Key, global::aimbot::Mode), 4.f));
            ui::bind("##abk", &global::aimbot::Key, &global::aimbot::Mode);

            ui::gap(4.f);
            ui::toggle("Auto Sensitivity", &global::aimbot::AutoSens);
            if (!global::aimbot::AutoSens) {
                ui::sliderfloat("Smoothing",  &global::aimbot::Smoothing, 1.f, 30.f);
                ui::sliderfloat("Speed",      &global::aimbot::Speed,     0.1f, 20.f);
                ui::combo("Ease In",  &global::aimbot::SmoothEaseIn,  { "Linear","Ease-Out Cubic","Ease-Out Quint" });
                ui::combo("Ease Out", &global::aimbot::SmoothEaseOut, { "Linear","Ease-Out Cubic","Ease-Out Quint" });
            }
            ui::sliderfloat("FOV", &global::aimbot::FOV, 1.f, 360.f);
            ui::combo("Priority", &global::aimbot::TargetPriority, { "Crosshair", "Distance", "Health" });
            ui::combo("Hitbox",   &global::aimbot::Hitbox, { "Head","Torso","Upper Torso","Lower Torso","Root","Random" });
            ui::toggle("Prediction",    &global::aimbot::Prediction);
            ui::toggle("Auto Shoot",    &global::aimbot::AutoShoot);
            ui::toggle("Visible Check", &global::aimbot::VisibleCheck);
            ui::toggle("Dead Check",    &global::aimbot::DeadCheck);
            ui::toggle("Team Check",    &global::aimbot::TeamCheck);
            ui::toggle("Sticky Aim",    &global::aimbot::Sticky);
        }
        ui::card::end();

        ImGui::SameLine(0.f, 6.f);

        if (ui::card::begin("##ablegit", { halfW, bInH }, "LEGIT / MISC"))
        {
            ui::gap(2.f);
            ui::labelsection("HUMANISE");
            ui::sliderfloat("Jitter",        &global::aimbot::Jitter,     0.f, 10.f);
            ui::sliderfloat("Random FOV",    &global::aimbot::RandomFOV,  0.f, 90.f);
            ui::sliderint("Reaction (ms)",   &global::aimbot::ReactionMs, 0,   500);
            ui::gap(4.f);
            ui::labelsection("CHECKS");
            ui::toggle("Legit Mode",  &global::aimbot::Legit);
        }
        ui::card::end();
    }
    if (Section == 2)
    {
        if (ui::card::begin("##sa", { halfW, bInH }, "SILENT AIM"))
        {
            ui::toggle("Enable", &global::silent::Enabled);
            ImGui::SameLine(ui::rightslot(ui::bindwidth(global::silent::Silent_Key, global::silent::Silent_Mode), 4.f));
            ui::bind("##sak", &global::silent::Silent_Key, &global::silent::Silent_Mode);

            ui::gap(4.f);
            ui::toggle("Visible Check",  &global::silent::VisibleCheck);
            ui::toggle("Knocked Check",  &global::silent::KnockedCheck);
            ui::toggle("Use FOV",        &global::silent::UseFov);
            if (global::silent::UseFov)
                ui::sliderfloat("FOV", &global::silent::fov, 1.f, 500.f);
            ui::toggle("Gun-Based FOV",  &global::silent::GunBasedFov);
            ui::combo("Aim Part", &global::silent::AimPart, { "Head","Upper Torso","Lower Torso" });
            ui::combo("Priority", &global::silent::TargetPriority, { "Crosshair","Distance" });
            ui::gap(4.f);
            ui::labelsection("PREDICTION");
            ui::toggle("Prediction",      &global::silent::Prediction);
            if (global::silent::Prediction) {
                ui::toggle("Auto Predict", &global::silent::AutoPrediction);
                if (!global::silent::AutoPrediction) {
                    ui::sliderfloat("X", &global::silent::PredictionX, 0.f, 0.5f, "%.3f");
                    ui::sliderfloat("Y", &global::silent::PredictionY, 0.f, 0.5f, "%.3f");
                    ui::sliderfloat("Z", &global::silent::PredictionZ, 0.f, 0.5f, "%.3f");
                }
            }
        }
        ui::card::end();

        ImGui::SameLine(0.f, 6.f);

        if (ui::card::begin("##mb", { halfW, bInH }, "MAGIC BULLET"))
        {
            ui::toggle("Enable", &global::silent::MagicBullet);
            ui::gap(4.f);
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImGui::ColorConvertU32ToFloat4(color::TextMid));
            ImGui::TextWrapped(
                "Redirects Lua GC ray userdatas toward the "
                "closest target each frame. Works independently "
                "of silent aim.");
            ImGui::PopStyleColor();
        }
        ui::card::end();
    }

    if (Section == 3)
    {
        if (ui::card::begin("##esp", { halfW, bInH }, "ESP TOGGLES"))
        {
            ui::toggle("Master Enable", &global::esp::Enabled);
            ui::toggle("Local Player",  &global::esp::Local_Player);
            ui::toggle("Visible Check", &global::esp::VisibleCheck);
            if (global::esp::VisibleCheck) {
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(color::TextMid), "Visible");
                ui::color4("##espvisc", global::esp::color::Visible);
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(color::TextMid), "Not Visible");
                ui::color4("##espnotvisc", global::esp::color::NotVisible);
            }
            ui::toggle("Dead Check", &global::esp::Dead_Check);
            ui::gap(4.f);

            ui::labelsection("BOX");
            ui::togglecolor("Box", &global::esp::Box, "##bxc", global::esp::color::Box);
            if (global::esp::Box) {
                ui::combo("Style", &global::esp::Box_Type, { "Bounding","Corner" });
                ui::gap(2.f);
                ui::toggle("Box Fill", &global::esp::Box_Fill);
                if (global::esp::Box_Fill) {
                    ui::dualcolor("##bft", global::esp::color::BoxFill_Top,
                        "##bfb", global::esp::color::BoxFill_Bottom);
                    ui::toggle("Gradient", &global::esp::Box_Fill_Gradient);
                    if (global::esp::Box_Fill_Gradient) {
                        ui::toggle("Rotation", &global::esp::Box_Fill_Gradient_Rotate);
                        if (global::esp::Box_Fill_Gradient_Rotate) {
                            ui::combo("Rotation Type", &global::esp::Box_Fill_Type,
                                { "Side","Bottom","Spin" });
                            ui::sliderint("Speed", &global::esp::BoxFillSpeed, 1, 5);
                        }
                    }
                }
            }
            ui::gap(4.f);

            ui::labelsection("HEALTH");
            ui::togglecolor("Health Bar", &global::esp::Healthbar,
                "##hbc", global::esp::color::Healthbar);
            if (global::esp::Healthbar) {
                ui::combo("Bar Style", &global::esp::Healthbar_Type, { "Static","Gradient" });
                if (global::esp::Healthbar_Type == 1) {
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(color::TextMid),
                        "Gradient (top \xE2\x80\x94 mid \xE2\x80\x94 bottom)");
                    ui::tricolor("##hbt", global::esp::color::Healthbar_Top,
                        "##hbm", global::esp::color::Healthbar_Middle,
                        "##hbb", global::esp::color::Healthbar_Bottom);
                }
                ui::sliderint("Bar Gap", &global::esp::gap, 1, 5);
                ui::sliderint("Bar Thickness", &global::esp::Thickness, 1, 5);
            }
            ui::togglecolor("Health Text", &global::esp::Health,
                "##htc", global::esp::color::Health);
            ui::gap(4.f);

            ui::labelsection("LABELS");
            ui::togglecolor("Name", &global::esp::name, "##nc", global::esp::color::name);
            if (global::esp::name)
                ui::combo("Name Format", &global::esp::Name_Type,
                    { "Name","Display Name","Name & Display" });
            ui::togglecolor("Distance", &global::esp::Distance, "##dc", global::esp::color::Distance);
            ui::togglecolor("Rig Type", &global::esp::Rig_Type, "##rc", global::esp::color::Rig_Type);
            ui::togglecolor("Tool", &global::esp::tool, "##tc", global::esp::color::tool);
            ui::gap(4.f);

            ui::labelsection("3D");
            ui::togglecolor("Skeleton", &global::esp::Skeleton, "##skc", global::esp::color::Skeleton);
            ui::togglecolor("Trails", &global::esp::Trails, "##trc", global::esp::color::Trails);
            ui::togglecolor("Chinese Hats", &global::esp::Chinese_Hat, "##chhatc", global::esp::color::hat);
            ui::togglecolor("Aim Lines", &global::esp::aimline, "##aimlc", global::esp::color::aimline);
            ui::toggle("Chams", &global::esp::Chams);
            if (global::esp::Chams) {
                ui::dualcolor("##chc", global::esp::color::Chams,
                    "##choc", global::esp::color::ChamsOutline);
                ui::toggle("Fade", &global::esp::ChamsFade);
                if (global::esp::ChamsFade)
                    ui::sliderint("Fade Speed", &global::esp::ChamsFadeSpeed, 1, 5);
            }
        }
        ui::card::end();

        ImGui::SameLine(0.f, 6.f);

        if (ui::card::begin("##vopt", { halfW, bInH }, "OPTIONS"))
        {
            ui::toggle("Exclude Team", &global::setting::Team_Check);
            ui::toggle("Exclude Client", &global::setting::Client_Check);
            ui::toggle("Bot Check", &global::setting::BotCheck);
            ui::gap(4.f);
            ui::labelsection("RENDERING");
            ui::toggle("Limit Render Distance", &global::esp::Limit_Distance);
            if (global::esp::Limit_Distance)
                ui::sliderfloat("Render Distance", &global::esp::Render_Distance, 0.f, 50000.f);
            ui::gap(4.f);
            ui::labelsection("OVERLAY");
            ui::toggle("Watermark", &global::overlay::watermark);
            ui::toggle("Keybinds", &global::overlay::ShowKeybinds);
            ui::toggle("Damage Logs", &global::overlay::DamageLogs);
            ui::toggle("Notifications", &global::overlay::Notifications);
            ui::sliderfloat("Media Tracker", &global::overlay::Spotify, 0.f, 1.f);
            if (global::esp::aimline)
                ui::sliderfloat("AimView Length", &global::overlay::AimView_MaxLength, 50.f, 1000.f);

            ui::gap(4.f);
            ui::labelsection("TRACERS");
            ui::toggle("Tracers", &global::tracer::Enabled);
            if (global::tracer::Enabled) {
                ui::combo("Style",  &global::tracer::Style,  { "Straight","Curved","Dashed" });
                ui::combo("Origin", &global::tracer::Origin, { "Cursor","Center","Top","Bottom","My Head","My HRP" });
                ui::combo("Target", &global::tracer::Dest,   { "Head","Root","Closest","My Head (cursor follow)" });
                ui::sliderfloat("Thickness##tr", &global::tracer::Thickness, 0.5f, 5.f, "%.1f");
                ui::toggle("Outline##tr", &global::tracer::Outline);
                ui::color4("##trc", global::tracer::Color);
            }
        }
        ui::card::end();
    }

    if (Section == 4)
    {
        if (ui::card::begin("##misc", { halfW, bInH }, "MISC"))
        {
            ui::labelsection("MOVEMENT");
            ui::toggle("Fly", &global::misc::fly);
            ImGui::SameLine(ui::rightslot(ui::bindwidth(global::misc::Fly_Key, global::misc::Fly_Mode), 4.f));
            ui::bind("##flyk", &global::misc::Fly_Key, &global::misc::Fly_Mode);
            if (global::misc::fly)
                ui::sliderfloat("Fly Speed", &global::misc::Fly_Speed, 5.f, 200.f);

            ui::gap(4.f);
            ui::toggle("Walkspeed", &global::misc::walkspeed);
            if (global::misc::walkspeed)
                ui::sliderfloat("Speed", &global::misc::Walkspeed_Speed, 1.f, 500.f, "%.0f");

            ui::gap(4.f);
            ui::labelsection("HITBOX");
            ui::toggle("Expand Hitbox", &global::misc::hitbox);
            if (global::misc::hitbox) {
                ui::sliderfloat("Size X", &global::misc::Hitbox_Size_X, 1.f, 75.f, "%.1f");
                ui::sliderfloat("Size Y", &global::misc::Hitbox_Size_Y, 1.f, 75.f, "%.1f");
                ui::sliderfloat("Size Z", &global::misc::Hitbox_Size_Z, 1.f, 75.f, "%.1f");
            }

            ui::gap(4.f);
            ui::labelsection("DESYNC");
            ui::toggle("Desync", &global::misc::Desync);
            ImGui::SameLine(ui::rightslot(ui::bindwidth(global::misc::Desync_Key, global::misc::Desync_Mode), 4.f));
            ui::bind("##dsk", &global::misc::Desync_Key, &global::misc::Desync_Mode);
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImGui::ColorConvertU32ToFloat4(color::TextMid));
            ImGui::TextWrapped("Sets PhysicsSenderJob bandwidth to 0 "
                "while held/toggled. Restores for 3s on release.");
            ImGui::PopStyleColor();

            ui::gap(4.f);
            ui::labelsection("TICKRATE");
            ui::toggle("Override Tickrate", &global::misc::Tickrate);
            if (global::misc::Tickrate) {
                ImGui::SameLine(ui::rightslot(ui::bindwidth(global::misc::Tickrate_Key, global::misc::Tickrate_Mode), 4.f));
                ui::bind("##tck", &global::misc::Tickrate_Key, &global::misc::Tickrate_Mode);
                ui::sliderfloat("Rate", &global::misc::Tickrate_Value, 1.f, 240.f, "%.0f");
            }

            ui::gap(4.f);
            ui::labelsection("ANIMATION");
            ui::toggle("Anim Changer", &global::misc::AnimChanger);
            if (global::misc::AnimChanger) {
                ImGui::SameLine(ui::rightslot(ui::bindwidth(global::misc::AnimKey, global::misc::AnimMode), 4.f));
                ui::bind("##ank", &global::misc::AnimKey, &global::misc::AnimMode);
                // Build combo from pack names
                int pack_count = 0;
                const char** names = misc::anim_pack_names(&pack_count);
                std::vector<const char*> pack_list(names, names+pack_count);
                ui::combo("Pack", &global::misc::AnimPack, pack_list);
            }

            ui::gap(4.f);
            ui::labelsection("LIGHTING");
            ui::toggle("Lighting Override", &global::misc::Lighting);
            if (global::misc::Lighting) {
                ui::toggle("Clock Time", &global::misc::ClockTime);
                if (global::misc::ClockTime) {
                    ImGui::SameLine(ui::rightslot(ui::bindwidth(global::misc::ClockKey, global::misc::ClockMode), 4.f));
                    ui::bind("##clk", &global::misc::ClockKey, &global::misc::ClockMode);
                    // Display as hours 0-24
                    float hours = global::misc::ClockTimeValue / 3600.f;
                    if (ui::sliderfloat("Clock Hour", &hours, 0.f, 24.f, "%.1f"))
                        global::misc::ClockTimeValue = hours * 3600.f;
                }
            }
        }
        ui::card::end();

        ImGui::SameLine(0.f, 6.f);

        if (ui::card::begin("##crosstracer", { halfW, bInH }, "CROSSHAIR"))
        {
            ui::toggle("Enable", &global::crosshair::Enabled);
            if (global::crosshair::Enabled) {
                ui::combo("Type",     &global::crosshair::Type,     { "Static","Animated","Spin" });
                ui::combo("Position", &global::crosshair::Position, { "Follow Cursor","Screen Center" });
                if (global::crosshair::Type == 2)
                    ui::sliderfloat("Spin Speed", &global::crosshair::SpinSpeed, 0.1f, 10.f, "%.1f");
                ui::sliderfloat("Size",      &global::crosshair::Size,      2.f,  40.f, "%.0f");
                ui::sliderfloat("Gap",       &global::crosshair::Gap,       0.f,  20.f, "%.0f");
                ui::sliderfloat("Thickness", &global::crosshair::Thickness, 0.5f,  5.f, "%.1f");
                ui::toggle("Outline", &global::crosshair::Outline);
                ui::color4("##xhc", global::crosshair::Color);
            }
        }
        ui::card::end();
    }
    if (Section == 5)
    {
        // ── Game Selector ──────────────────────────────────────────────────
        struct GameEntry {
            const char* name;
            const char* desc;
            const char* placeId;
            ImU32       accent;
        };
        static const GameEntry games[] = {
            { "Da Hood",       "Street RP / PVP",  "2788229376",  IM_COL32(220, 60,  60,  255) },
            { "DUELIST: PVP",  "1v1 Sword Combat", "17625359955", IM_COL32( 80, 160, 255, 255) },
        };
        static int hovered_game = -1;

        if (ui::card::begin("##gs", { bInW, bInH }, "GAME SELECTOR"))
        {
            ui::labelsection("FEATURED GAMES");
            ui::gap(4.f);

            const float cardW = bInW - ui::kCardPadX * 2.f;
            const float cardH = 64.f;
            const float cardGap = 8.f;

            ImDrawList* cdl = ImGui::GetWindowDrawList();

            for (int i = 0; i < 2; i++)
            {
                const GameEntry& g = games[i];
                ImVec2 cp = ImGui::GetCursorScreenPos();
                ImVec2 cm = { cp.x + cardW, cp.y + cardH };

                ImGui::PushID(i);
                ImGui::InvisibleButton("##gc", { cardW, cardH });
                bool ghov = ImGui::IsItemHovered();
                bool gclk = ImGui::IsItemClicked();
                ImGui::PopID();

                // Smooth hover
                static float ghov_t[2] = {};
                ghov_t[i] = fx::damp(ghov_t[i], ghov ? 1.f : 0.f, 14.f);
                float gt = ghov_t[i];

                // Card bg
                ImU32 cbg = color::lerp(color::card, color::CardHov, gt);
                cdl->AddRectFilled(cp, cm, cbg, 8.f);

                // Left accent bar
                cdl->AddRectFilled(cp + ImVec2(0.f, 6.f),
                    cp + ImVec2(3.f, cardH - 6.f),
                    color::alpha(g.accent, 0.55f + gt * 0.45f), 2.f);

                // Glow on hover
                if (gt > 0.01f)
                    cdl->AddRect(cp, cm,
                        color::alpha(g.accent, gt * 0.35f), 8.f, 0, 1.5f);
                else
                    cdl->AddRect(cp, cm, color::Border, 8.f, 0, 1.f);

                // Game name
                cdl->AddText(cp + ImVec2(14.f, 14.f),
                    color::lerp(color::text, color::White, gt), g.name);

                // Description
                cdl->AddText(cp + ImVec2(14.f, 14.f + ImGui::GetFontSize() + 4.f),
                    color::alpha(color::TextMid, 0.8f), g.desc);

                // Launch button (appears on hover)
                if (gt > 0.05f)
                {
                    const float btnW = 70.f, btnH = 22.f;
                    ImVec2 bp = { cm.x - btnW - 10.f, cp.y + (cardH - btnH) * .5f };
                    ImVec2 bm = bp + ImVec2(btnW, btnH);

                    ImU32 btnBg  = color::alpha(g.accent, 0.20f + gt * 0.25f);
                    ImU32 btnBrd = color::alpha(g.accent, 0.50f + gt * 0.40f);
                    cdl->AddRectFilled(bp, bm, btnBg, 6.f);
                    cdl->AddRect(bp, bm, btnBrd, 6.f, 0, 1.f);

                    const char* btnLbl = "Launch";
                    ImVec2 ls = ImGui::CalcTextSize(btnLbl);
                    cdl->AddText(bp + ImVec2((btnW - ls.x) * .5f, (btnH - ls.y) * .5f),
                        color::alpha(color::White, gt), btnLbl);

                    // Click → open Roblox deeplink
                    if (gclk)
                    {
                        char link[128];
                        std::snprintf(link, sizeof(link),
                            "roblox://placeId=%s", g.placeId);
                        ShellExecuteA(nullptr, "open", link, nullptr, nullptr, SW_SHOWNORMAL);
                    }
                }

                ImGui::SetCursorScreenPos({ cp.x, cm.y + cardGap });
            }

            ui::gap(12.f);
            ui::labelsection("INFO");
            ui::gap(4.f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(color::TextMid));
            ImGui::TextWrapped("Select a game above to launch it directly in Roblox. "
                               "More games will be added in future updates.");
            ImGui::PopStyleColor();
        }
        ui::card::end();
    }
    if (Section == 6)
    {
        static int sel = -1;
        std::vector<sdk::player> players;
        try { players = cache::snapshot(); } catch (...) {}

        if (ui::card::begin("##plist", { halfW, bInH }, "PLAYERS"))
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(color::Win));
            ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(color::Border));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.f);
            ImGui::BeginChild("##plc", ImGui::GetContentRegionAvail(), true);
            for (int i = 0; i < (int)players.size(); i++)
            {
                auto& p = players[i];
                ImGui::PushStyleColor(ImGuiCol_Header, ImGui::ColorConvertU32ToFloat4(color::AccentDim));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::ColorConvertU32ToFloat4(color::AccentDim));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::ColorConvertU32ToFloat4(color::Accent));
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(sel == i ? color::AccentFg : color::TextMid));
                char label[64];
                std::snprintf(label, sizeof(label), "%s [%.0fm]", p.name.c_str(), p.Distance);
                if (ImGui::Selectable(label, sel == i))
                    sel = i;
                ImGui::PopStyleColor(4);
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
        }
        ui::card::end();

        ImGui::SameLine(0.f, 6.f);

        if (ui::card::begin("##pdetail", { halfW, bInH }, "DETAILS"))
        {
            if (sel >= 0 && sel < (int)players.size())
            {
                auto& p = players[sel];
                try {
                    ImGui::Text("Name: %s", p.name.c_str());
                    ImGui::Text("Display: %s", p.Display_Name.c_str());
                    if (p.UserID) ImGui::Text("UserID: %llu", (unsigned long long)p.UserID);
                    ImGui::Text("Health: %.0f / %.0f", p.Health, p.MaxHealth);
                    ImGui::Text("Distance: %.0fm", p.Distance);
                    ImGui::Text("Character: 0x%llX", (unsigned long long)p.character.Address);

                    if (p.character.Address)
                    {
                        ImGui::Separator();
                        ImGui::Text("BODY PARTS:");
                        auto print_part = [&](const char* label, const sdk::instance& inst) {
                            ImGui::Text("  %s: 0x%llX %s", label, (unsigned long long)inst.Address,
                                inst.Address ? "OK" : "MISSING");
                        };
                        print_part("Head", p.Head);
                        print_part("Torso", p.Torso);
                        print_part("UpperTorso", p.UpperTorso);
                        print_part("LowerTorso", p.LowerTorso);
                        print_part("HumanoidRootPart", p.HumanoidRootPart);
                        print_part("Humanoid", p.humanoid);

                        ImGui::Separator();
                        ImGui::Text("CHARACTER CHILDREN:");
                        auto chars = p.character.children();
                        for (auto& c : chars)
                        {
                            if (!c.Address) continue;
                            ImGui::Text("  0x%llX %s (%s)", (unsigned long long)c.Address,
                                c.name().c_str(), c.kind().c_str());
                        }
                    }
                    else
                    {
                        ImGui::TextColored(ImVec4(1,0,0,1), "No character address!");
                    }
                }
                catch (...) { ImGui::TextColored(ImVec4(1,0,0,1), "Error reading player data"); }
            }
            else
            {
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(color::TextMid), "Select a player from the list");
            }
        }
        ui::card::end();
    }

    if (Section == 7)
    {
        static char           cfgName[128] = "";
        static char           cfgImport[131072] = "";
        static std::string    cfgNotice;
        static float          cfgNoticeUntil = 0.f;
        static std::vector<std::string> cfgList;
        static bool           cfgRefreshed = false;
        if (!cfgRefreshed) { config::refresh(cfgList); cfgRefreshed = true; }

        if (ui::card::begin("##cfg", { halfW, bInH }, "CONFIGS"))
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(color::Win));
            ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(color::Border));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.f);
            ImGui::BeginChild("##cl", { ImGui::GetContentRegionAvail().x, 108.f }, true);
            for (auto& cfg : cfgList)
            {
                const bool sel = (strcmp(cfgName, cfg.c_str()) == 0);
                ImGui::PushStyleColor(ImGuiCol_Header,
                    ImGui::ColorConvertU32ToFloat4(color::AccentDim));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                    ImGui::ColorConvertU32ToFloat4(color::AccentDim));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                    ImGui::ColorConvertU32ToFloat4(color::Accent));
                ImGui::PushStyleColor(ImGuiCol_Text,
                    ImGui::ColorConvertU32ToFloat4(sel ? color::AccentFg : color::TextMid));
                if (ImGui::Selectable(cfg.c_str(), sel))
                    strcpy_s(cfgName, sizeof(cfgName), cfg.c_str());
                ImGui::PopStyleColor(4);
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);

            ui::gap(4.f);

            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(color::Win));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4(color::CardHov));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImGui::ColorConvertU32ToFloat4(color::CardHov));
            ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(color::Border));
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(color::text));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.f);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputText("##cn", cfgName, sizeof(cfgName));
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(5);

            ui::gap(4.f);

            const float bw = (ImGui::GetContentRegionAvail().x - 8.f) / 3.f;
            if (ui::button("Load", color::card, color::Border, color::text, bw)) {
                if (cfgName[0]) {
                    const bool exists = std::filesystem::exists(config::path(cfgName));
                    config::load(cfgName);
                    notify::push(exists ? notify::kind::success : notify::kind::warning,
                        exists ? "Config loaded" : "Config missing", cfgName);
                }
            }
            ImGui::SameLine(0.f, 4.f);
            if (ui::button("Save", color::AccentDim, color::Accent, color::AccentFg, bw)) {
                if (cfgName[0]) {
                    config::save(cfgName);
                    config::refresh(cfgList);
                    notify::push(notify::kind::success, "Config saved", cfgName);
                }
            }
            ImGui::SameLine(0.f, 4.f);
            if (ui::button("Delete", color::DangerDim, color::DangerBrd, color::Danger, bw)) {
                if (cfgName[0]) {
                    const std::string deleted = cfgName;
                    config::remove(cfgName);
                    config::refresh(cfgList);
                    cfgName[0] = '\0';
                    notify::push(notify::kind::warning, "Config deleted", deleted);
                }
            }

            ui::gap(8.f);
            ui::labelsection("SHARE");

            const float shareW = (ImGui::GetContentRegionAvail().x - 4.f) / 2.f;
            if (ui::button("Export", color::AccentDim, color::Accent, color::AccentFg, shareW))
            {
                if (cfgName[0])
                {
                    config::save(cfgName);
                    config::refresh(cfgList);

                    std::string encoded;
                    if (config::export64(cfgName, encoded))
                    {
                        strncpy_s(cfgImport, sizeof(cfgImport), encoded.c_str(), _TRUNCATE);
                        cfgNotice = copytext(encoded)
                            ? "Config copied to clipboard"
                            : "Config exported, clipboard failed";
                        notify::push(notify::kind::success, "Config exported", cfgName);
                        cfgNoticeUntil = (float)ImGui::GetTime() + 2.8f;
                    }
                    else
                    {
                        cfgNotice = "Config export failed";
                        notify::push(notify::kind::warning, "Export failed", cfgName);
                        cfgNoticeUntil = (float)ImGui::GetTime() + 2.8f;
                    }
                }
                else
                {
                    cfgNotice = "Enter config name first";
                    notify::push(notify::kind::warning, "Config name required");
                    cfgNoticeUntil = (float)ImGui::GetTime() + 2.8f;
                }
            }
            ImGui::SameLine(0.f, 4.f);
            if (ui::button("Import", color::card, color::Border, color::text, shareW))
            {
                ImGui::OpenPopup("##import_config_popup");
            }

            if (!cfgNotice.empty() && ImGui::GetTime() < cfgNoticeUntil)
            {
                ui::gap(4.f);
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(color::Accent), "%s", cfgNotice.c_str());
            }

            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowSize(ImVec2(460.f, 318.f), ImGuiCond_Appearing);
            ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::ColorConvertU32ToFloat4(color::Surface));
            ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(color::Border));
            ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(2.f / 255.f, 7.f / 255.f, 11.f / 255.f, 0.78f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.f, 14.f));
            if (ImGui::BeginPopupModal("##import_config_popup", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
            {
                ImDrawList* modalDraw = ImGui::GetWindowDrawList();
                ImVec2 modalPos = ImGui::GetWindowPos();
                ImVec2 modalSize = ImGui::GetWindowSize();
                modalDraw->AddRectFilled(modalPos, modalPos + modalSize, color::Win);
                modalDraw->AddRectFilled(modalPos, modalPos + ImVec2(modalSize.x, 44.f), color::card);
                modalDraw->AddRectFilled(modalPos + ImVec2(0.f, 43.f), modalPos + ImVec2(modalSize.x, 44.f), color::Border);
                modalDraw->AddRectFilled(modalPos + ImVec2(14.f, 15.f), modalPos + ImVec2(17.f, 29.f), color::Accent);
                ImGui::SetCursorPos(ImVec2(25.f, 13.f));
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(color::text), "IMPORT CONFIG");
                ImGui::SetCursorPos(ImVec2(14.f, 58.f));
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(color::TextMid), "Paste config base64 here");
                ui::gap(8.f);

                const float popupWidth = ImGui::GetContentRegionAvail().x;
                if (ui::button("Paste", color::card, color::Border, color::text, 96.f))
                {
                    const char* clip = ImGui::GetClipboardText();
                    if (clip)
                        strncpy_s(cfgImport, sizeof(cfgImport), clip, _TRUNCATE);
                }

                ui::gap(8.f);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(color::Win));
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4(color::CardHov));
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImGui::ColorConvertU32ToFloat4(color::CardHov));
                ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(color::Border));
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(color::text));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.f);
                ImGui::InputTextMultiline("##cfgimport", cfgImport, sizeof(cfgImport),
                    ImVec2(popupWidth, 116.f));
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(5);

                ui::gap(10.f);
                const float modalW = (ImGui::GetContentRegionAvail().x - 6.f) / 2.f;
                if (ui::button("Import Base64", color::AccentDim, color::Accent, color::AccentFg, modalW))
                {
                    if (cfgName[0] && cfgImport[0] && config::import64(cfgName, cfgImport))
                    {
                        config::refresh(cfgList);
                        config::load(cfgName);
                        cfgNotice = "Config imported";
                        notify::push(notify::kind::success, "Config imported", cfgName);
                        ImGui::CloseCurrentPopup();
                    }
                    else
                    {
                        cfgNotice = cfgName[0] ? "Config import failed" : "Enter config name first";
                        notify::push(notify::kind::warning,
                            cfgName[0] ? "Import failed" : "Config name required", cfgName);
                    }
                    cfgNoticeUntil = (float)ImGui::GetTime() + 2.8f;
                }
                ImGui::SameLine(0.f, 6.f);
                if (ui::button("Cancel", color::card, color::Border, color::text, modalW))
                    ImGui::CloseCurrentPopup();

                ImGui::EndPopup();
            }
            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor(3);
        }
        ui::card::end();

        ImGui::SameLine(0.f, 6.f);

        if (ui::card::begin("##gen", { halfW, bInH }, "GENERAL"))
        {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(color::TextMid), "Menu Key");
            ImGui::SameLine(ui::rightslot(ui::keywidth(global::setting::Menu_Key), 4.f));
            ui::keyselect("##menukey", &global::setting::Menu_Key);
            ui::gap(4.f);
            ui::toggle("Compact UI", &global::setting::Compact_UI);
            ui::gap(2.f);
            ui::toggle("Streamproof", &global::setting::Streamproof);
            ui::gap(2.f);
            ui::combo("Frame Pace", &global::setting::Performance_Mode, { "Eco 60","Balanced VSync","Fast 144" });
            ui::gap(4.f);
            ui::labelsection("MENU COLORS");
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(color::TextMid), "Accent");
            ImGui::SameLine(ui::rightslot(ui::kColW));
            ui::color4("##thacc", global::setting::color::Accent);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(color::TextMid), "Accent 2");
            ImGui::SameLine(ui::rightslot(ui::kColW));
            ui::color4("##thacc2", global::setting::color::Accent2);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(color::TextMid), "Window");
            ImGui::SameLine(ui::rightslot(ui::kColW));
            ui::color4("##thwin", global::setting::color::Window);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(color::TextMid), "Cards");
            ImGui::SameLine(ui::rightslot(ui::kColW));
            ui::color4("##thcard", global::setting::color::card);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(color::TextMid), "Text");
            ImGui::SameLine(ui::rightslot(ui::kColW));
            ui::color4("##thtext", global::setting::color::text);

            static bool dbg = false;
            ui::gap(4.f);
            if (ImGui::TreeNodeEx("Diagnostics", ImGuiTreeNodeFlags_SpanAvailWidth))
            {
                ImGui::Text("Actors:  0x%llX", (unsigned long long)global::actor.Address);
                ImGui::Text("Camera:  0x%llX", (unsigned long long)global::camera.Address);
                ImGui::Text("Render:  0x%llX", (unsigned long long)global::render.Address);
                ImGui::Text("Local:   0x%llX", (unsigned long long)global::LocalPlayer.character.Address);
                ImGui::Text("GameID:  %llu", (unsigned long long)global::GameID);
                {
                    auto snap = cache::snapshot();
                    ImGui::Text("Players: %zu", snap.size());
                }
                ImGui::Text("Workspc: 0x%llX", (unsigned long long)global::workspace.Address);
                ImGui::Separator();
                auto& gp = game::get();
                if (gp.source == game::Profile::WORKSPACE)
                    ImGui::Text("Cache:   Workspace");
                else if (gp.source == game::Profile::STANDARD)
                    ImGui::Text("Cache:   Standard");
                else
                    ImGui::Text("Cache:   None");
                ImGui::Text("Rig:     %s", gp.is_r15 ? "R15" : gp.is_r6 ? "R6" : gp.has_custom_rig ? "Custom" : "?");
                ImGui::Text("Dead:    %s", gp.dead_uses_bodyeffects ? "BodyEffects>K.O" : "Humanoid.Health");
                ImGui::Text("Teams:   %s", gp.has_teams ? "Yes" : "No");
                ImGui::Text("Hitbox Parts: %d", gp.hitbox_part_count);
                ImGui::TreePop();
            }

            const float remaining = ImGui::GetContentRegionAvail().y - 34.f;
            if (remaining > 0.f) ImGui::Dummy({ 0.f, remaining });

            if (ui::button("  UNLOAD  ", color::DangerDim, color::DangerBrd, color::Danger,
                ImGui::GetContentRegionAvail().x, 26.f))
                ExitProcess(0);
        }
        ui::card::end();
    }

    ImGui::EndGroup();
    ImGui::PopStyleVar();
    ImGui::PopClipRect();
    ImGui::End();
}

void graphic::visual()
{
    color::refresh();
    cursor();
    esp::run();

    if (global::aim::VisualiseHitbox && global::aim::TriggerBot)
    {
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        auto Players = cache::snapshot();
        for (auto& Plr : Players) {
            if (Plr.Local_Player ||
                (global::LocalPlayer.character.Address != 0 && Plr.character.Address == global::LocalPlayer.character.Address) ||
                !Plr.character.Address || !Plr.Head.Address)
                continue;

            if (global::aim::DistanceScale && Plr.Head.Address)
            {
                sdk::part head(Plr.Head.Address);
                sdk::vector3 pos = head.partposition();
                if (!std::isnan(pos.x) && !std::isnan(pos.y) && !std::isnan(pos.z))
                    {
                        sdk::vector2 screen = global::render.screen(pos);
                        if (screen.x > -0.5f && screen.y > -0.5f)
                        {
                            sdk::part prim = head.primitive();
                            float r = 10.f;
                            if (prim.Address) {
                                sdk::vector3 sz = prim.size();
                                sdk::matrix3 rot = prim.rotation();
                                float hx = std::abs(sz.x) * 0.5f;
                                float hy = std::abs(sz.y) * 0.5f;
                                float hz = std::abs(sz.z) * 0.5f;
                                float mx = 0.f;
                                for (int xi = -1; xi <= 1; xi += 2)
                                    for (int yi = -1; yi <= 1; yi += 2)
                                        for (int zi = -1; zi <= 1; zi += 2) {
                                            sdk::vector3 wp = pos + sdk::vector3{
                                                rot.data[0]*hx*xi + rot.data[3]*hy*yi + rot.data[6]*hz*zi,
                                                rot.data[1]*hx*xi + rot.data[4]*hy*yi + rot.data[7]*hz*zi,
                                                rot.data[2]*hx*xi + rot.data[5]*hy*yi + rot.data[8]*hz*zi };
                                            sdk::vector2 sc = global::render.screen(wp);
                                            float d = (sc.x-screen.x)*(sc.x-screen.x)+(sc.y-screen.y)*(sc.y-screen.y);
                                            if (d > mx) mx = d;
                                        }
                                r = std::sqrt(mx) * global::aim::AutoHeadScale;
                            }
                            if (global::aim::FovCheck)
                                r = (std::min)(r, global::aim::TriggerRadius);
                            dl->AddCircle(ImVec2(screen.x, screen.y), r, IM_COL32(0, 174, 255, 100), 24, 1.8f);
                            dl->AddCircleFilled(ImVec2(screen.x, screen.y), r, IM_COL32(0, 174, 255, 20));
                        }
                    }
            }
            else
            {
                struct VisEntry { sdk::instance inst; float radius; };
                std::vector<VisEntry> ents;
                float base = global::aim::FovCheck ? global::aim::TriggerRadius : 1.f;
                if (global::aim::HitPart_Head && Plr.Head.Address) ents.push_back({ Plr.Head, base * global::aim::HitPart_Head_Scale });
                if (global::aim::HitPart_Torso && Plr.Torso.Address) ents.push_back({ Plr.Torso, base * global::aim::HitPart_Torso_Scale });
                if (global::aim::HitPart_UpperTorso && Plr.UpperTorso.Address) ents.push_back({ Plr.UpperTorso, base * global::aim::HitPart_UpperTorso_Scale });
                if (global::aim::HitPart_LowerTorso && Plr.LowerTorso.Address) ents.push_back({ Plr.LowerTorso, base * global::aim::HitPart_LowerTorso_Scale });
                if (global::aim::HitPart_HumanoidRootPart && Plr.HumanoidRootPart.Address) ents.push_back({ Plr.HumanoidRootPart, base * global::aim::HitPart_HumanoidRootPart_Scale });
                if (global::aim::HitPart_LeftArm && Plr.LeftArm.Address) ents.push_back({ Plr.LeftArm, base * global::aim::HitPart_LeftArm_Scale });
                if (global::aim::HitPart_RightArm && Plr.RightArm.Address) ents.push_back({ Plr.RightArm, base * global::aim::HitPart_RightArm_Scale });
                if (global::aim::HitPart_LeftLeg && Plr.LeftLeg.Address) ents.push_back({ Plr.LeftLeg, base * global::aim::HitPart_LeftLeg_Scale });
                if (global::aim::HitPart_RightLeg && Plr.RightLeg.Address) ents.push_back({ Plr.RightLeg, base * global::aim::HitPart_RightLeg_Scale });

                for (auto& e : ents) {
                    sdk::part part(e.inst.Address);
                    sdk::vector3 pos = part.partposition();
                    if (std::isnan(pos.x) || std::isnan(pos.y) || std::isnan(pos.z)) continue;
                    sdk::vector2 screen = global::render.screen(pos);
                    if (screen.x <= -0.5f || screen.y <= -0.5f) continue;
                    dl->AddCircle(ImVec2(screen.x, screen.y), e.radius, IM_COL32(255, 255, 255, 80), 24, 1.5f);
                    dl->AddCircleFilled(ImVec2(screen.x, screen.y), e.radius, IM_COL32(255, 255, 255, 18));
                }
            }
        }
    }

    media::render();
    hud::render(Running);
    notify::render();
    welcome(Running);

    // Crosshair + Tracers drawn on foreground
    {
        ImDrawList* fdl = ImGui::GetForegroundDrawList();
        esp::crosshair(fdl);
        {
            std::vector<sdk::player> snap;
            try { snap = cache::snapshot(); } catch(...) {}
            esp::tracers(fdl, snap);
        }
    }
}
