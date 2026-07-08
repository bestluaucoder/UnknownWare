#include "feature/spotify.h"
#include "global.h"
#include "ui/graphic.h"
#include <Windows.h>
#include <wincodec.h>
#include <winhttp.h>
#include <d3d11.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <cmath>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "windowscodecs.lib")

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

// ── Unicode ──────────────────────────────────────────────────────────────────
static std::string wide_to_utf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    if (!s.empty() && s.back() == 0) s.pop_back();
    return s;
}

// ── Spotify window ────────────────────────────────────────────────────────────
static HWND find_spotify_window() {
    HWND hwnd = GetTopWindow(nullptr);
    while (hwnd) {
        if (IsWindowVisible(hwnd)) {
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid) {
                HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                if (h) {
                    wchar_t exe[512]{};
                    DWORD sz = 512;
                    if (QueryFullProcessImageNameW(h, 0, exe, &sz)) {
                        std::wstring path(exe);
                        std::transform(path.begin(), path.end(), path.begin(), ::towlower);
                        if (path.find(L"spotify.exe") != std::wstring::npos) {
                            CloseHandle(h);
                            return hwnd;
                        }
                    }
                    CloseHandle(h);
                }
            }
        }
        hwnd = GetNextWindow(hwnd, GW_HWNDNEXT);
    }
    return nullptr;
}

struct TrackInfo { std::string title, artist; bool playing = false; };

static TrackInfo get_current_track() {
    TrackInfo info;
    HWND hwnd = find_spotify_window();
    if (!hwnd) return info;
    wchar_t buf[512]{};
    GetWindowTextW(hwnd, buf, 512);
    std::string text = wide_to_utf8(buf);
    if (text.empty() || text == "Spotify" || text == "Spotify Premium") return info;
    info.playing = true;
    size_t sep = text.find(" - ");
    if (sep != std::string::npos && sep > 0 && sep + 3 < text.size()) {
        info.artist = text.substr(0, sep);
        info.title  = text.substr(sep + 3);
        size_t pipe = info.title.find(" | ");
        if (pipe != std::string::npos) info.title = info.title.substr(0, pipe);
    } else {
        info.title = text;
        size_t pipe = info.title.find(" | ");
        if (pipe != std::string::npos) info.title = info.title.substr(0, pipe);
    }
    while (!info.title.empty()  && info.title.back()  == ' ') info.title.pop_back();
    while (!info.artist.empty() && info.artist.back() == ' ') info.artist.pop_back();
    return info;
}

// ── Media keys ────────────────────────────────────────────────────────────────
static void media_key(WORD vk) {
    INPUT in[2]{};
    in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = vk;
    in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = vk; in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, in, sizeof(INPUT));
}
static void spotify_play_pause() { media_key(VK_MEDIA_PLAY_PAUSE); }
static void spotify_next()       { media_key(VK_MEDIA_NEXT_TRACK); }
static void spotify_prev()       { media_key(VK_MEDIA_PREV_TRACK); }

// ── Album art ─────────────────────────────────────────────────────────────────
// We keep the raw JPEG bytes on the background thread, then upload to GPU on
// the main thread to avoid D3D11 multi-thread issues.
struct ArtState {
    std::mutex              mtx;
    std::string             pending_key;   // key being loaded
    std::vector<BYTE>       pending_bytes; // bytes ready to upload
    std::atomic<bool>       bytes_ready{false};
    std::string             loaded_key;    // key whose SRV is current
    ID3D11ShaderResourceView* srv = nullptr;
    std::atomic<bool>       loading{false};
};
static ArtState g_art;

static std::vector<BYTE> http_get(const std::wstring& host, const std::wstring& path, bool https=true) {
    std::vector<BYTE> out;
    HINTERNET ses = WinHttpOpen(L"UW/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) return out;
    HINTERNET con = WinHttpConnect(ses, host.c_str(),
        https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!con) { WinHttpCloseHandle(ses); return out; }
    HINTERNET req = WinHttpOpenRequest(con, L"GET", path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, https ? WINHTTP_FLAG_SECURE : 0);
    if (!req) { WinHttpCloseHandle(con); WinHttpCloseHandle(ses); return out; }
    if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(req, nullptr)) {
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(req, &avail) && avail > 0) {
            size_t off = out.size(); out.resize(off + avail);
            DWORD read = 0;
            WinHttpReadData(req, out.data() + off, avail, &read);
            out.resize(off + read);
        }
    }
    WinHttpCloseHandle(req); WinHttpCloseHandle(con); WinHttpCloseHandle(ses);
    return out;
}

static std::wstring url_encode(const std::string& s) {
    std::wostringstream out;
    for (unsigned char c : s) {
        if (isalnum(c) || c=='-'||c=='_'||c=='.'||c=='~') out << (wchar_t)c;
        else { wchar_t b[8]; swprintf_s(b,L"%%%02X",c); out<<b; }
    }
    return out.str();
}

static std::vector<BYTE> fetch_album_art(const std::string& artist, const std::string& title) {
    std::string q = artist + " " + title;
    std::wstring path = L"/search?term=" + url_encode(q) + L"&media=music&limit=1&entity=song";
    auto jb = http_get(L"itunes.apple.com", path, true);
    if (jb.empty()) return {};
    std::string json(jb.begin(), jb.end());
    const std::string key = "\"artworkUrl100\":\"";
    size_t pos = json.find(key); if (pos==std::string::npos) return {};
    pos += key.size();
    size_t end = json.find('"', pos); if (end==std::string::npos) return {};
    std::string url = json.substr(pos, end-pos);
    size_t sz = url.find("100x100");
    if (sz!=std::string::npos) url.replace(sz,7,"300x300");
    size_t sc = url.find("://"); if (sc==std::string::npos) return {};
    std::string rest = url.substr(sc+3);
    size_t sl = rest.find('/'); if (sl==std::string::npos) return {};
    std::string host = rest.substr(0,sl), pth = rest.substr(sl);
    return http_get(std::wstring(host.begin(),host.end()),
                    std::wstring(pth.begin(),pth.end()), true);
}

// Upload bytes → D3D11 SRV. Must be called on the render thread.
static ID3D11ShaderResourceView* upload_to_gpu(ID3D11Device* dev, const std::vector<BYTE>& data) {
    if (!dev || data.empty()) return nullptr;
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IWICImagingFactory* fac=nullptr; IWICStream* str=nullptr;
    IWICBitmapDecoder* dec=nullptr; IWICBitmapFrameDecode* frm=nullptr;
    IWICFormatConverter* cvt=nullptr; ID3D11Texture2D* tex=nullptr;
    ID3D11ShaderResourceView* srv=nullptr;
    #define RC(p) if(p){(p)->Release();(p)=nullptr;}
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&fac));
    if(SUCCEEDED(hr)) hr=fac->CreateStream(&str);
    if(SUCCEEDED(hr)) hr=str->InitializeFromMemory(const_cast<BYTE*>(data.data()),(DWORD)data.size());
    if(SUCCEEDED(hr)) hr=fac->CreateDecoderFromStream(str,nullptr,WICDecodeMetadataCacheOnLoad,&dec);
    if(SUCCEEDED(hr)) hr=dec->GetFrame(0,&frm);
    if(SUCCEEDED(hr)) hr=fac->CreateFormatConverter(&cvt);
    if(SUCCEEDED(hr)) hr=cvt->Initialize(frm,GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,nullptr,0.f,WICBitmapPaletteTypeCustom);
    UINT w=0,h=0; std::vector<BYTE> px;
    if(SUCCEEDED(hr)) hr=cvt->GetSize(&w,&h);
    if(SUCCEEDED(hr)&&w&&h){ px.resize((size_t)w*h*4); hr=cvt->CopyPixels(nullptr,w*4,(UINT)px.size(),px.data()); }
    if(SUCCEEDED(hr)){
        D3D11_TEXTURE2D_DESC d{}; d.Width=w;d.Height=h;d.MipLevels=1;d.ArraySize=1;
        d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;
        d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA sub{px.data(),w*4,0};
        hr=dev->CreateTexture2D(&d,&sub,&tex);
    }
    if(SUCCEEDED(hr)) hr=dev->CreateShaderResourceView(tex,nullptr,&srv);
    RC(tex) RC(cvt) RC(frm) RC(dec) RC(str) RC(fac)
    CoUninitialize();
    #undef RC
    return SUCCEEDED(hr)?srv:nullptr;
}

static void start_art_fetch(const std::string& artist, const std::string& title, const std::string& key) {
    g_art.loading = true;
    std::thread([artist,title,key](){
        auto bytes = fetch_album_art(artist, title);
        std::lock_guard<std::mutex> lk(g_art.mtx);
        if (g_art.pending_key != key) { g_art.loading = false; return; }
        g_art.pending_bytes = std::move(bytes);
        g_art.bytes_ready   = true;
        g_art.loading       = false;
    }).detach();
}

// ── Helpers ───────────────────────────────────────────────────────────────────
static float sp_lerp(float a, float b, float t){ return a+(b-a)*t; }
static float sp_smooth(float cur, float tgt, float spd){
    float dt=ImGui::GetIO().DeltaTime;
    return sp_lerp(cur,tgt,1.f-expf(-spd*dt));
}

static void sp_text(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* txt, ImFont* f=nullptr){
    if(!txt||!*txt) return;
    ImFont* fnt = f ? f : (UiFont ? UiFont : ImGui::GetFont());
    dl->AddText(fnt, fnt->LegacySize, pos, col, txt);
}

// Simple icon button — fully self-contained, no std::function overhead
// Returns true on click. Uses ImGui item system so input works correctly.
static bool sp_icon_btn(ImDrawList* dl, const char* id, ImVec2 center, float r,
                         ImU32 col_normal, ImU32 col_hover,
                         void(*draw)(ImDrawList*,ImVec2,float,ImU32))
{
    ImGui::SetCursorScreenPos(center - ImVec2(r,r));
    ImGui::InvisibleButton(id, ImVec2(r*2.f, r*2.f));
    bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    ImU32 col = ImGui::IsItemHovered() ? col_hover : col_normal;
    draw(dl, center, r, col);
    return clicked;
}

static void draw_prev(ImDrawList* d, ImVec2 c, float r, ImU32 col){
    float h=r*.75f, w=r*.6f;
    d->AddLine(c+ImVec2(-r*.55f,-h),c+ImVec2(-r*.55f,h),col,2.f);
    ImVec2 a[3]={{c.x-r*.55f+1.f,c.y-h},{c.x+w*.5f,c.y},{c.x-r*.55f+1.f,c.y+h}};
    d->AddTriangleFilled(a[0],a[1],a[2],col);
    ImVec2 b[3]={{c.x+w*.2f,c.y-h},{c.x+w*1.2f,c.y},{c.x+w*.2f,c.y+h}};
    d->AddTriangleFilled(b[0],b[1],b[2],col);
}
static void draw_next(ImDrawList* d, ImVec2 c, float r, ImU32 col){
    float h=r*.75f, w=r*.6f;
    d->AddLine(c+ImVec2(r*.55f,-h),c+ImVec2(r*.55f,h),col,2.f);
    ImVec2 a[3]={{c.x+r*.55f-1.f,c.y-h},{c.x-w*.5f,c.y},{c.x+r*.55f-1.f,c.y+h}};
    d->AddTriangleFilled(a[0],a[1],a[2],col);
    ImVec2 b[3]={{c.x-w*.2f,c.y-h},{c.x-w*1.2f,c.y},{c.x-w*.2f,c.y+h}};
    d->AddTriangleFilled(b[0],b[1],b[2],col);
}
static void draw_pause(ImDrawList* d, ImVec2 c, float r, ImU32 col){
    d->AddCircle(c,r,col,24,1.5f);
    float bh=r*.42f,bw=r*.16f,gap=r*.14f;
    d->AddRectFilled(c+ImVec2(-gap-bw,-bh),c+ImVec2(-gap,bh),col,2.f);
    d->AddRectFilled(c+ImVec2(gap,-bh),c+ImVec2(gap+bw,bh),col,2.f);
}
static void draw_play(ImDrawList* d, ImVec2 c, float r, ImU32 col){
    d->AddCircle(c,r,col,24,1.5f);
    float th=r*.45f;
    ImVec2 t[3]={{c.x-th*.55f,c.y-th},{c.x+th,c.y},{c.x-th*.55f,c.y+th}};
    d->AddTriangleFilled(t[0],t[1],t[2],col);
}

// ── Main render ───────────────────────────────────────────────────────────────
void spotify::render()
{
    if (!global::overlay::Spotify) return;

    static TrackInfo   current;
    static ULONGLONG   last_poll   = 0;
    static ULONGLONG   track_start = GetTickCount64();
    static std::string last_key;
    static float       hover_t     = 0.f;
    static float       prog_anim   = 0.f;

    const float alpha = global::overlay::Spotify;
    ULONGLONG   now   = GetTickCount64();

    // ── Poll ──────────────────────────────────────────────────────────────────
    if (now - last_poll > 900) {
        last_poll = now;
        TrackInfo info = get_current_track();
        std::string key = info.artist + "|" + info.title;
        if (key != last_key) {
            last_key    = key;
            track_start = now;
            current     = info;
            // New track — always kick off a fresh fetch
            std::lock_guard<std::mutex> lk(g_art.mtx);
            if (info.playing && !info.title.empty() && !g_art.loading) {
                g_art.pending_key   = key;
                g_art.bytes_ready   = false;
                g_art.pending_bytes.clear();
                // Don't require key != loaded_key: always re-fetch on track change
                if (g_art.srv && g_art.loaded_key != key) {
                    g_art.srv->Release();
                    g_art.srv = nullptr;
                    g_art.loaded_key.clear();
                }
                start_art_fetch(info.artist, info.title, key);
            }
        } else if (current.playing &&
                   !g_art.loading &&
                   !g_art.bytes_ready.load()) {
            // Same track but no image yet (failed or never fetched) — retry every 15s
            std::lock_guard<std::mutex> lk(g_art.mtx);
            bool needs_retry = g_art.srv == nullptr &&
                               g_art.loaded_key != last_key &&
                               !g_art.loading &&
                               !current.title.empty();
            static ULONGLONG last_retry = 0;
            if (needs_retry && now - last_retry > 15000) {
                last_retry = now;
                g_art.pending_key   = key;
                g_art.bytes_ready   = false;
                g_art.pending_bytes.clear();
                start_art_fetch(current.artist, current.title, key);
            }
        }
    }

    // ── Upload art on render thread (D3D11 thread-safe) ───────────────────────
    if (g_art.bytes_ready.load()) {
        std::lock_guard<std::mutex> lk(g_art.mtx);
        if (g_art.bytes_ready.load()) {
            if (g_art.srv) { g_art.srv->Release(); g_art.srv = nullptr; }
            ID3D11Device* dev = screen ? screen->Detail->Device : nullptr;
            if (dev && !g_art.pending_bytes.empty())
                g_art.srv = upload_to_gpu(dev, g_art.pending_bytes);
            g_art.loaded_key  = g_art.pending_key;
            g_art.bytes_ready = false;
        }
    }

    float elapsed = (now - track_start) / 1000.f;

    // ── Window ────────────────────────────────────────────────────────────────
    const float W        = 284.f;
    const float H_IDLE   = 74.f;
    const float H_HOVER  = 102.f;
    float cur_h = sp_lerp(H_IDLE, H_HOVER, hover_t);

    ImGui::SetNextWindowSize(ImVec2(W, cur_h), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - W - 16.f, 16.f),
        ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   10.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.f,0.f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0,0,0,0));

    bool open = true;
    bool began = ImGui::Begin("##sp", &open,
        ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoScrollbar|
        ImGuiWindowFlags_NoScrollWithMouse|ImGuiWindowFlags_NoFocusOnAppearing|
        ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize);

    if (!began) {
        ImGui::End(); ImGui::PopStyleColor(); ImGui::PopStyleVar(3); return;
    }

    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 ws = ImGui::GetWindowSize();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    bool hov = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows|
                                       ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    hover_t = sp_smooth(hover_t, hov ? 1.f : 0.f, 10.f);

    // Background
    dl->AddRectFilled(wp, wp+ws, IM_COL32(13,13,17,(int)(225*alpha)), 10.f);
    dl->AddRect(wp, wp+ws, IM_COL32(255,255,255,(int)(20*alpha)), 10.f, 0, 1.f);
    // Green top bar
    dl->AddRectFilled(wp+ImVec2(12.f,0.f), wp+ImVec2(W-12.f,2.f),
        IM_COL32(30,215,96,(int)(200*alpha)), 1.f);

    // Art
    const float ART=54.f, PAD=9.f;
    ImVec2 atl = wp + ImVec2(PAD,(H_IDLE-ART)*.5f);
    ImVec2 abr = atl + ImVec2(ART,ART);
    {
        std::lock_guard<std::mutex> lk(g_art.mtx);
        if (g_art.srv && g_art.loaded_key == last_key && current.playing) {
            dl->AddImageRounded((ImTextureID)g_art.srv, atl, abr,
                ImVec2(0,0),ImVec2(1,1), IM_COL32(255,255,255,(int)(255*alpha)), 6.f);
            dl->AddRect(atl,abr,IM_COL32(255,255,255,(int)(25*alpha)),6.f,0,1.f);
        } else {
            dl->AddRectFilled(atl,abr,IM_COL32(28,28,34,(int)(200*alpha)),6.f);
            dl->AddRect(atl,abr,IM_COL32(60,60,70,(int)(100*alpha)),6.f,0,1.f);
            ImVec2 c=atl+ImVec2(ART*.5f,ART*.5f);
            if (current.playing) {
                // Always spin if loading or if we have no art yet (waiting for retry)
                float t2=(float)ImGui::GetTime()*1.9f;
                dl->PathArcTo(c,ART*.28f,t2,t2+IM_PI*1.3f,16);
                dl->PathStroke(IM_COL32(30,215,96,(int)(200*alpha)),false,2.5f);
            }
        }
    }

    // Text
    float tx = abr.x + 10.f, trw = wp.x + W - tx - PAD;
    ImFont* bf = Tahoma_BoldXP ? Tahoma_BoldXP : ImGui::GetFont();
    ImFont* rf = UiFont ? UiFont : ImGui::GetFont();
    float ty = wp.y + 13.f;

    auto trunc = [&](const std::string& s, ImFont* f, float mw) -> std::string {
        float fw = ImGui::CalcTextSize(s.c_str()).x * (f->LegacySize / ImGui::GetFontSize());
        if (fw <= mw) return s;
        std::string t = s;
        while (!t.empty() && ImGui::CalcTextSize((t+"...").c_str()).x*(f->LegacySize/ImGui::GetFontSize()) > mw)
            t.pop_back();
        return t + "...";
    };

    sp_text(dl, ImVec2(tx,ty), IM_COL32(232,232,240,(int)(240*alpha)),
        trunc(current.playing ? current.title : "Not Playing", bf, trw).c_str(), bf);
    sp_text(dl, ImVec2(tx, ty+bf->LegacySize+3.f),
        IM_COL32(155,155,165,(int)(180*alpha)),
        trunc(current.playing ? (current.artist.empty()?"Unknown Artist":current.artist) : "", rf, trw).c_str(), rf);

    // Progress bar — clickable to seek (updates prog_anim target)
    const float BY=wp.y+H_IDLE-13.f, BX0=tx, BX1=wp.x+W-PAD, BW=BX1-BX0, BH=3.f;
    if (current.playing) {
        const float dur=210.f;
        float tgt = fmodf(elapsed,dur)/dur;
        prog_anim = sp_smooth(prog_anim, tgt, 2.5f);

        // Clickable track area
        ImGui::SetCursorScreenPos(ImVec2(BX0, BY-4.f));
        ImGui::InvisibleButton("##prog", ImVec2(BW, BH+8.f));
        if (ImGui::IsItemActive()) {
            float rel = (ImGui::GetIO().MousePos.x - BX0) / BW;
            rel = rel < 0.f ? 0.f : rel > 1.f ? 1.f : rel;
            prog_anim = rel;
            track_start = now - (ULONGLONG)(rel * dur * 1000.f);
        }
        bool prog_hov = ImGui::IsItemHovered();

        dl->AddRectFilled(ImVec2(BX0,BY),ImVec2(BX1,BY+BH),IM_COL32(55,55,60,(int)(150*alpha)),2.f);
        float fw=BW*prog_anim;
        if (fw>0.f) dl->AddRectFilled(ImVec2(BX0,BY),ImVec2(BX0+fw,BY+BH),IM_COL32(30,215,96,(int)(230*alpha)),2.f);
        if (hover_t>0.01f || prog_hov) {
            float dr=sp_lerp(0.f,5.f,hover_t); if(dr<1.f)dr=1.f;
            dl->AddCircleFilled(ImVec2(BX0+fw,BY+BH*.5f),dr+1.f,IM_COL32(0,0,0,(int)(80*alpha)),12);
            dl->AddCircleFilled(ImVec2(BX0+fw,BY+BH*.5f),dr,IM_COL32(255,255,255,(int)(240*alpha)),12);
        }
    }

    // Controls
    if (hover_t > 0.02f) {
        float cy  = wp.y + H_IDLE + sp_lerp(18.f, 6.f, hover_t);
        float ccx = wp.x + W * .5f;
        float ia  = alpha * hover_t;
        ImU32 cn  = IM_COL32(210,210,215,(int)(200*ia));
        ImU32 ch2 = IM_COL32(255,255,255,(int)(255*ia));
        ImU32 gn  = IM_COL32(30,215,96,(int)(210*ia));
        ImU32 gh  = IM_COL32(80,235,120,(int)(255*ia));

        if (sp_icon_btn(dl,"##prev",ImVec2(ccx-52.f,cy),9.f,cn,ch2,draw_prev)) spotify_prev();
        if (current.playing) {
            if (sp_icon_btn(dl,"##pp",ImVec2(ccx,cy),13.f,gn,gh,draw_pause)) spotify_play_pause();
        } else {
            if (sp_icon_btn(dl,"##pp",ImVec2(ccx,cy),13.f,gn,gh,draw_play))  spotify_play_pause();
        }
        if (sp_icon_btn(dl,"##next",ImVec2(ccx+52.f,cy),9.f,cn,ch2,draw_next)) spotify_next();
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void spotify::release() {
    std::lock_guard<std::mutex> lk(g_art.mtx);
    if (g_art.srv) { g_art.srv->Release(); g_art.srv = nullptr; }
}
