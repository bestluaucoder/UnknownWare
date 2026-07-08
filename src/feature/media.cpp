#include "feature/media.h"
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

// ── Unicode ───────────────────────────────────────────────────────────────────
static std::string wide_to_utf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8,0,w.c_str(),-1,nullptr,0,nullptr,nullptr);
    std::string s(n,0);
    WideCharToMultiByte(CP_UTF8,0,w.c_str(),-1,s.data(),n,nullptr,nullptr);
    if (!s.empty() && s.back()==0) s.pop_back();
    return s;
}

// ── HTTP GET ──────────────────────────────────────────────────────────────────
static std::vector<BYTE> http_get(const std::wstring& host,
                                   const std::wstring& path, bool https=true)
{
    std::vector<BYTE> out;
    HINTERNET ses=WinHttpOpen(L"UW/1.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!ses) return out;
    HINTERNET con=WinHttpConnect(ses,host.c_str(),
        https?INTERNET_DEFAULT_HTTPS_PORT:INTERNET_DEFAULT_HTTP_PORT,0);
    if(!con){WinHttpCloseHandle(ses);return out;}
    HINTERNET req=WinHttpOpenRequest(con,L"GET",path.c_str(),nullptr,
        WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,https?WINHTTP_FLAG_SECURE:0);
    if(!req){WinHttpCloseHandle(con);WinHttpCloseHandle(ses);return out;}
    DWORD opts=SECURITY_FLAG_IGNORE_CERT_CN_INVALID|
               SECURITY_FLAG_IGNORE_CERT_DATE_INVALID|
               SECURITY_FLAG_IGNORE_UNKNOWN_CA;
    WinHttpSetOption(req,WINHTTP_OPTION_SECURITY_FLAGS,&opts,sizeof(opts));
    if(WinHttpSendRequest(req,WINHTTP_NO_ADDITIONAL_HEADERS,0,
            WINHTTP_NO_REQUEST_DATA,0,0,0)&&WinHttpReceiveResponse(req,nullptr)){
        DWORD avail=0;
        while(WinHttpQueryDataAvailable(req,&avail)&&avail>0){
            size_t off=out.size(); out.resize(off+avail);
            DWORD read=0;
            WinHttpReadData(req,out.data()+off,avail,&read);
            out.resize(off+read);
        }
    }
    WinHttpCloseHandle(req);WinHttpCloseHandle(con);WinHttpCloseHandle(ses);
    return out;
}

static std::wstring url_encode(const std::string& s){
    std::wostringstream o;
    for(unsigned char c:s){
        if(isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~') o<<(wchar_t)c;
        else{wchar_t b[8];swprintf_s(b,L"%%%02X",c);o<<b;}
    }
    return o.str();
}

// ── Source detection ──────────────────────────────────────────────────────────
// Use Windows System Media Transport Controls (SMTC) via WinRT COM.
// This gives us real title, artist, app source, playback position + duration
// for ANY media app — Spotify, Chrome/YouTube, Edge/YTMusic, etc.
// We use the raw WinRT ABI via RoGetActivationFactory to avoid needing C++/WinRT headers.

#include <roapi.h>
#include <windows.media.control.h>
#pragma comment(lib, "runtimeobject.lib")

enum class MediaSource { None, Spotify, YouTube, YouTubeMusic, Generic };

struct MediaInfo {
    MediaSource source   = MediaSource::None;
    std::string title, artist;
    bool        playing  = false;
    float       position = 0.f;  // seconds
    float       duration = 0.f;  // seconds (0 = unknown / live)
};

// Convert a Windows::Foundation::IReference<TimeSpan> to seconds
static float timespan_to_sec(INT64 ticks) {
    // TimeSpan is in 100-nanosecond intervals
    return (float)(ticks / 10000000LL);
}

// Minimal WinRT string helpers
static HSTRING make_hstring(const wchar_t* s) {
    HSTRING h = nullptr;
    WindowsCreateString(s, (UINT32)wcslen(s), &h);
    return h;
}

static MediaInfo detect_media() {
    MediaInfo info;

    // Init WinRT
    HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
    bool uninit = SUCCEEDED(hr) || hr == S_FALSE;

    // Get SMTC session manager
    HSTRING clsid = make_hstring(
        L"Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager");

    IInspectable* mgr_factory = nullptr;
    hr = RoGetActivationFactory(clsid,
        __uuidof(ABI::Windows::Media::Control::IGlobalSystemMediaTransportControlsSessionManagerStatics),
        reinterpret_cast<void**>(&mgr_factory));
    WindowsDeleteString(clsid);

    if (FAILED(hr) || !mgr_factory) {
        if (uninit) RoUninitialize();
        // Fallback to window title scan
        goto fallback;
    }

    {
        using namespace ABI::Windows::Media::Control;
        using namespace ABI::Windows::Foundation;
        using namespace ABI::Windows::Foundation::Collections;

        auto* statics = static_cast<IGlobalSystemMediaTransportControlsSessionManagerStatics*>(mgr_factory);

        // RequestAsync — we call it synchronously via IAsyncOperation
        IAsyncOperation<GlobalSystemMediaTransportControlsSessionManager*>* async_op = nullptr;
        hr = statics->RequestAsync(&async_op);
        statics->Release();

        if (FAILED(hr) || !async_op) {
            if (uninit) RoUninitialize();
            goto fallback;
        }

        // Spin-wait for async (short timeout) — use IAsyncInfo for status polling
        IGlobalSystemMediaTransportControlsSessionManager* session_mgr = nullptr;
        {
            ABI::Windows::Foundation::IAsyncInfo* info_iface = nullptr;
            async_op->QueryInterface(IID_PPV_ARGS(&info_iface));
            for (int i = 0; i < 100 && info_iface; i++) {
                AsyncStatus status{}; info_iface->get_Status(&status);
                if (status == AsyncStatus::Completed) { async_op->GetResults(&session_mgr); break; }
                if (status == AsyncStatus::Error || status == AsyncStatus::Canceled) break;
                Sleep(5);
            }
            if (info_iface) info_iface->Release();
        }
        async_op->Release();

        if (!session_mgr) { if (uninit) RoUninitialize(); goto fallback; }

        // Get current session
        IGlobalSystemMediaTransportControlsSession* session = nullptr;
        session_mgr->GetCurrentSession(&session);
        session_mgr->Release();

        if (!session) { if (uninit) RoUninitialize(); goto fallback; }

        // Get playback info
        IGlobalSystemMediaTransportControlsSessionPlaybackInfo* pb = nullptr;
        session->GetPlaybackInfo(&pb);
        if (pb) {
            GlobalSystemMediaTransportControlsSessionPlaybackStatus status{};
            pb->get_PlaybackStatus(&status);
            info.playing = (status == GlobalSystemMediaTransportControlsSessionPlaybackStatus_Playing);
            pb->Release();
        }

        if (!info.playing) { session->Release(); if (uninit) RoUninitialize(); goto fallback; }

        // Get timeline (position + duration)
        IGlobalSystemMediaTransportControlsSessionTimelineProperties* tl = nullptr;
        session->GetTimelineProperties(&tl);
        if (tl) {
            ABI::Windows::Foundation::TimeSpan pos{}, dur{};
            tl->get_Position(&pos);
            tl->get_MaxSeekTime(&dur);
            info.position = timespan_to_sec(pos.Duration);
            info.duration = timespan_to_sec(dur.Duration);
            tl->Release();
        }

        // Get media properties (title + artist) async
        IAsyncOperation<GlobalSystemMediaTransportControlsSessionMediaProperties*>* props_op = nullptr;
        session->TryGetMediaPropertiesAsync(&props_op);
        if (props_op) {
            IGlobalSystemMediaTransportControlsSessionMediaProperties* props = nullptr;
            {
                ABI::Windows::Foundation::IAsyncInfo* info_iface = nullptr;
                props_op->QueryInterface(IID_PPV_ARGS(&info_iface));
                for (int i = 0; i < 100 && info_iface; i++) {
                    AsyncStatus st{}; info_iface->get_Status(&st);
                    if (st == AsyncStatus::Completed) { props_op->GetResults(&props); break; }
                    if (st == AsyncStatus::Error || st == AsyncStatus::Canceled) break;
                    Sleep(5);
                }
                if (info_iface) info_iface->Release();
            }
            props_op->Release();
            if (props) {
                HSTRING htitle = nullptr, hartist = nullptr;
                props->get_Title(&htitle);
                props->get_Artist(&hartist);
                if (htitle)  info.title  = wide_to_utf8(WindowsGetStringRawBuffer(htitle,  nullptr));
                if (hartist) info.artist = wide_to_utf8(WindowsGetStringRawBuffer(hartist, nullptr));
                WindowsDeleteString(htitle);
                WindowsDeleteString(hartist);
                props->Release();
            }
        }

        // Get source app ID to determine MediaSource
        HSTRING src_app = nullptr;
        session->get_SourceAppUserModelId(&src_app);
        if (src_app) {
            std::wstring app_id = WindowsGetStringRawBuffer(src_app, nullptr);
            std::transform(app_id.begin(), app_id.end(), app_id.begin(), ::towlower);
            if (app_id.find(L"spotify") != std::wstring::npos)
                info.source = MediaSource::Spotify;
            else if (app_id.find(L"chrome")  != std::wstring::npos ||
                     app_id.find(L"msedge")  != std::wstring::npos ||
                     app_id.find(L"firefox") != std::wstring::npos ||
                     app_id.find(L"brave")   != std::wstring::npos) {
                // Check artist/title for YT Music vs YouTube
                std::string low_title = info.title;
                std::transform(low_title.begin(), low_title.end(), low_title.begin(), ::tolower);
                // YouTube Music sets both artist and title; plain YouTube usually has empty artist
                info.source = (!info.artist.empty())
                    ? MediaSource::YouTubeMusic : MediaSource::YouTube;
            } else {
                info.source = MediaSource::Generic;
            }
            WindowsDeleteString(src_app);
        }
        session->Release();
        if (uninit) RoUninitialize();
        return info;
    }

fallback:
    // Legacy window-title fallback if WinRT is unavailable
    info = MediaInfo{};
    // Check Spotify
    {
        struct Ctx{HWND h;};
        Ctx ctx{};
        EnumWindows([](HWND hw, LPARAM lp)->BOOL{
            auto*c=reinterpret_cast<Ctx*>(lp);
            if(!IsWindowVisible(hw)) return TRUE;
            wchar_t buf[512]{}; if(!GetWindowTextW(hw,buf,512)||!buf[0]) return TRUE;
            DWORD pid=0; GetWindowThreadProcessId(hw,&pid);
            HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid); if(!h) return TRUE;
            wchar_t exe[512]{}; DWORD sz=512;
            if(QueryFullProcessImageNameW(h,0,exe,&sz)){
                std::wstring p(exe); std::transform(p.begin(),p.end(),p.begin(),::towlower);
                if(p.find(L"spotify.exe")!=std::wstring::npos){c->h=hw;}
            }
            CloseHandle(h); return c->h?FALSE:TRUE;
        },(LPARAM)&ctx);
        if(ctx.h){
            wchar_t buf[512]{}; GetWindowTextW(ctx.h,buf,512);
            std::string t=wide_to_utf8(buf);
            if(!t.empty()&&t!="Spotify"&&t!="Spotify Premium"&&t!="Advertisement"){
                info.source=MediaSource::Spotify; info.playing=true;
                size_t sep=t.find(" - ");
                if(sep!=std::string::npos){info.artist=t.substr(0,sep);info.title=t.substr(sep+3);}
                else info.title=t;
                return info;
            }
        }
    }
    return info;
}

// ── Image fetching ────────────────────────────────────────────────────────────
static std::vector<BYTE> fetch_itunes_art(const std::string& artist,const std::string& title){
    std::string q=artist+" "+title;
    auto jb=http_get(L"itunes.apple.com",
        L"/search?term="+url_encode(q)+L"&media=music&limit=1&entity=song",true);
    if(jb.empty()) return {};
    std::string json(jb.begin(),jb.end());
    const std::string key="\"artworkUrl100\":\"";
    size_t pos=json.find(key); if(pos==std::string::npos) return {};
    pos+=key.size(); size_t end=json.find('"',pos); if(end==std::string::npos) return {};
    std::string url=json.substr(pos,end-pos);
    size_t sz=url.find("100x100"); if(sz!=std::string::npos) url.replace(sz,7,"300x300");
    size_t sc=url.find("://"); if(sc==std::string::npos) return {};
    std::string rest=url.substr(sc+3);
    size_t sl=rest.find('/'); if(sl==std::string::npos) return {};
    std::string host=rest.substr(0,sl),pth=rest.substr(sl);
    return http_get(std::wstring(host.begin(),host.end()),
                    std::wstring(pth.begin(),pth.end()),true);
}

static std::vector<BYTE> fetch_yt_thumbnail(const std::string& title, std::string& out_channel){
    // Search YouTube, extract first videoId from JSON embedded in page
    auto html=http_get(L"www.youtube.com",
        L"/results?search_query="+url_encode(title),true);
    if(html.empty()) return {};
    std::string page(html.begin(),html.end());
    // videoId
    std::string vid_key="\"videoId\":\"";
    size_t vp=page.find(vid_key); std::string vid;
    if(vp!=std::string::npos){
        vp+=vid_key.size(); size_t ve=page.find('"',vp);
        if(ve!=std::string::npos&&ve-vp==11) vid=page.substr(vp,ve-vp);
    }
    // channel name
    std::string ch_key="\"ownerText\":{\"runs\":[{\"text\":\"";
    size_t cp2=page.find(ch_key);
    if(cp2!=std::string::npos){
        cp2+=ch_key.size(); size_t ce=page.find('"',cp2);
        if(ce!=std::string::npos) out_channel=page.substr(cp2,ce-cp2);
    }
    if(vid.empty()) return {};
    return http_get(L"i.ytimg.com",
        L"/vi/"+std::wstring(vid.begin(),vid.end())+L"/hqdefault.jpg",true);
}

static ID3D11ShaderResourceView* upload_gpu(ID3D11Device* dev,const std::vector<BYTE>& data){
    if(!dev||data.empty()) return nullptr;
    CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    IWICImagingFactory*fac=nullptr;IWICStream*str=nullptr;
    IWICBitmapDecoder*dec=nullptr;IWICBitmapFrameDecode*frm=nullptr;
    IWICFormatConverter*cvt=nullptr;ID3D11Texture2D*tex=nullptr;
    ID3D11ShaderResourceView*srv=nullptr;
    #define RC(p) if(p){(p)->Release();(p)=nullptr;}
    HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&fac));
    if(SUCCEEDED(hr))hr=fac->CreateStream(&str);
    if(SUCCEEDED(hr))hr=str->InitializeFromMemory(const_cast<BYTE*>(data.data()),(DWORD)data.size());
    if(SUCCEEDED(hr))hr=fac->CreateDecoderFromStream(str,nullptr,WICDecodeMetadataCacheOnLoad,&dec);
    if(SUCCEEDED(hr))hr=dec->GetFrame(0,&frm);
    if(SUCCEEDED(hr))hr=fac->CreateFormatConverter(&cvt);
    if(SUCCEEDED(hr))hr=cvt->Initialize(frm,GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,nullptr,0.f,WICBitmapPaletteTypeCustom);
    UINT w=0,h=0; std::vector<BYTE>px;
    if(SUCCEEDED(hr))hr=cvt->GetSize(&w,&h);
    if(SUCCEEDED(hr)&&w&&h){px.resize((size_t)w*h*4);hr=cvt->CopyPixels(nullptr,w*4,(UINT)px.size(),px.data());}
    if(SUCCEEDED(hr)){
        D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=1;d.ArraySize=1;
        d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;
        d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA sub{px.data(),w*4,0};
        hr=dev->CreateTexture2D(&d,&sub,&tex);
    }
    if(SUCCEEDED(hr))hr=dev->CreateShaderResourceView(tex,nullptr,&srv);
    RC(tex)RC(cvt)RC(frm)RC(dec)RC(str)RC(fac)
    CoUninitialize();
    #undef RC
    return SUCCEEDED(hr)?srv:nullptr;
}

struct ArtState {
    std::mutex mtx;
    std::string pending_key, pending_channel, loaded_key;
    std::vector<BYTE> pending_bytes;
    std::atomic<bool> bytes_ready{false}, loading{false};
    ID3D11ShaderResourceView* srv=nullptr;
};
static ArtState g_art;

static void start_fetch(MediaInfo info, std::string key){
    g_art.loading=true;
    std::thread([info,key](){
        std::vector<BYTE> bytes; std::string channel;
        if(info.source==MediaSource::Spotify||info.source==MediaSource::YouTubeMusic)
            bytes=fetch_itunes_art(info.artist,info.title);
        else if(info.source==MediaSource::YouTube)
            bytes=fetch_yt_thumbnail(info.title,channel);
        std::lock_guard<std::mutex>lk(g_art.mtx);
        if(g_art.pending_key!=key){g_art.loading=false;return;}
        g_art.pending_bytes=std::move(bytes);
        g_art.pending_channel=channel;
        g_art.bytes_ready=true; g_art.loading=false;
    }).detach();
}

// ── Render helpers ────────────────────────────────────────────────────────────
static float mt_lerp(float a,float b,float t){return a+(b-a)*t;}
static float mt_smooth(float cur,float tgt,float spd){
    return mt_lerp(cur,tgt,1.f-expf(-spd*ImGui::GetIO().DeltaTime));
}
static void mt_text(ImDrawList*dl,ImVec2 pos,ImU32 col,const char*txt,ImFont*f=nullptr){
    if(!txt||!*txt)return;
    ImFont*fnt=f?(f):(UiFont?UiFont:ImGui::GetFont());
    dl->AddText(fnt,fnt->LegacySize,pos,col,txt);
}
static bool mt_btn(ImDrawList*dl,const char*id,ImVec2 c,float r,ImU32 cn,ImU32 ch,
                   void(*draw)(ImDrawList*,ImVec2,float,ImU32)){
    ImGui::SetCursorScreenPos(c-ImVec2(r,r));
    ImGui::InvisibleButton(id,ImVec2(r*2.f,r*2.f));
    bool clicked=ImGui::IsItemClicked(ImGuiMouseButton_Left);
    draw(dl,c,r,ImGui::IsItemHovered()?ch:cn); return clicked;
}
static void drw_prev(ImDrawList*d,ImVec2 c,float r,ImU32 col){
    float h=r*.75f,w=r*.6f;
    d->AddLine(c+ImVec2(-r*.55f,-h),c+ImVec2(-r*.55f,h),col,2.f);
    ImVec2 a[3]={{c.x-r*.55f+1.f,c.y-h},{c.x+w*.5f,c.y},{c.x-r*.55f+1.f,c.y+h}};
    d->AddTriangleFilled(a[0],a[1],a[2],col);
    ImVec2 b[3]={{c.x+w*.2f,c.y-h},{c.x+w*1.2f,c.y},{c.x+w*.2f,c.y+h}};
    d->AddTriangleFilled(b[0],b[1],b[2],col);
}
static void drw_next(ImDrawList*d,ImVec2 c,float r,ImU32 col){
    float h=r*.75f,w=r*.6f;
    d->AddLine(c+ImVec2(r*.55f,-h),c+ImVec2(r*.55f,h),col,2.f);
    ImVec2 a[3]={{c.x+r*.55f-1.f,c.y-h},{c.x-w*.5f,c.y},{c.x+r*.55f-1.f,c.y+h}};
    d->AddTriangleFilled(a[0],a[1],a[2],col);
    ImVec2 b[3]={{c.x-w*.2f,c.y-h},{c.x-w*1.2f,c.y},{c.x-w*.2f,c.y+h}};
    d->AddTriangleFilled(b[0],b[1],b[2],col);
}
static void drw_pause(ImDrawList*d,ImVec2 c,float r,ImU32 col){
    d->AddCircle(c,r,col,24,1.5f);
    float bh=r*.42f,bw=r*.16f,gap=r*.14f;
    d->AddRectFilled(c+ImVec2(-gap-bw,-bh),c+ImVec2(-gap,bh),col,2.f);
    d->AddRectFilled(c+ImVec2(gap,-bh),c+ImVec2(gap+bw,bh),col,2.f);
}
static void drw_play(ImDrawList*d,ImVec2 c,float r,ImU32 col){
    d->AddCircle(c,r,col,24,1.5f);
    float th=r*.45f;
    ImVec2 t[3]={{c.x-th*.55f,c.y-th},{c.x+th,c.y},{c.x-th*.55f,c.y+th}};
    d->AddTriangleFilled(t[0],t[1],t[2],col);
}
static ImU32 src_accent(MediaSource s,float a=1.f){
    int ai=(int)(255*a);
    switch(s){
        case MediaSource::Spotify:      return IM_COL32(30, 215,96, ai);
        case MediaSource::YouTube:      return IM_COL32(255,48, 48, ai);
        case MediaSource::YouTubeMusic: return IM_COL32(255,90, 90, ai);
        default:                        return IM_COL32(160,160,170,ai);
    }
}
static const char* src_label(MediaSource s){
    switch(s){
        case MediaSource::Spotify:      return "SPOTIFY";
        case MediaSource::YouTube:      return "YOUTUBE";
        case MediaSource::YouTubeMusic: return "YT MUSIC";
        default:                        return "MEDIA";
    }
}
static void media_key(WORD vk){
    INPUT in[2]{};
    in[0].type=INPUT_KEYBOARD;in[0].ki.wVk=vk;
    in[1].type=INPUT_KEYBOARD;in[1].ki.wVk=vk;in[1].ki.dwFlags=KEYEVENTF_KEYUP;
    SendInput(2,in,sizeof(INPUT));
}

// ── Main render ───────────────────────────────────────────────────────────────
void media::render()
{
    if(!global::overlay::Spotify) return;

    static MediaInfo   current;
    static ULONGLONG   last_poll=0, track_start=GetTickCount64();
    static std::string last_key;
    static float       hover_t=0.f, prog_anim=0.f;

    const float alpha=global::overlay::Spotify;
    ULONGLONG   now=GetTickCount64();

    if(now-last_poll>900){
        last_poll=now;
        MediaInfo info=detect_media();
        std::string key=std::to_string((int)info.source)+"|"+info.artist+"|"+info.title;
        if(key!=last_key){
            last_key=key; track_start=now; current=info;
            std::lock_guard<std::mutex>lk(g_art.mtx);
            if(info.playing&&!info.title.empty()&&!g_art.loading){
                g_art.pending_key=key; g_art.bytes_ready=false; g_art.pending_bytes.clear();
                if(g_art.srv&&g_art.loaded_key!=key){g_art.srv->Release();g_art.srv=nullptr;g_art.loaded_key.clear();}
                start_fetch(info,key);
            }
        } else if(current.playing&&!g_art.loading&&!g_art.bytes_ready.load()){
            std::lock_guard<std::mutex>lk(g_art.mtx);
            static ULONGLONG last_retry=0;
            if(!g_art.srv&&g_art.loaded_key!=last_key&&!current.title.empty()&&now-last_retry>15000){
                last_retry=now; g_art.pending_key=key; g_art.bytes_ready=false;
                g_art.pending_bytes.clear(); start_fetch(current,key);
            }
        }
    }

    if(g_art.bytes_ready.load()){
        std::lock_guard<std::mutex>lk(g_art.mtx);
        if(g_art.bytes_ready.load()){
            if(g_art.srv){g_art.srv->Release();g_art.srv=nullptr;}
            ID3D11Device*dev=screen?screen->Detail->Device:nullptr;
            if(dev&&!g_art.pending_bytes.empty()) g_art.srv=upload_gpu(dev,g_art.pending_bytes);
            if(!g_art.pending_channel.empty()&&current.artist.empty())
                current.artist=g_art.pending_channel;
            g_art.loaded_key=g_art.pending_key; g_art.bytes_ready=false;
        }
    }

    float elapsed=(now-track_start)/1000.f;
    ImU32 acc=src_accent(current.source,alpha);

    const float W=284.f, H_IDLE=74.f, H_HOVER=102.f;
    float cur_h=mt_lerp(H_IDLE,H_HOVER,hover_t);

    ImGui::SetNextWindowSize(ImVec2(W,cur_h),ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x-W-16.f,16.f),ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,10.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0.f,0.f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0,0,0,0));

    bool open=true;
    if(!ImGui::Begin("##media_tracker",&open,
        ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoScrollbar|
        ImGuiWindowFlags_NoScrollWithMouse|ImGuiWindowFlags_NoFocusOnAppearing|
        ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize)){
        ImGui::End();ImGui::PopStyleColor();ImGui::PopStyleVar(3);return;
    }

    ImVec2 wp=ImGui::GetWindowPos(), ws=ImGui::GetWindowSize();
    ImDrawList*dl=ImGui::GetWindowDrawList();
    bool hov=ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows|
                                     ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    hover_t=mt_smooth(hover_t,hov?1.f:0.f,10.f);

    dl->AddRectFilled(wp,wp+ws,IM_COL32(13,13,17,(int)(225*alpha)),10.f);
    dl->AddRect(wp,wp+ws,IM_COL32(255,255,255,(int)(20*alpha)),10.f,0,1.f);
    dl->AddRectFilled(wp+ImVec2(12.f,0.f),wp+ImVec2(W-12.f,2.f),acc,1.f);

    // Source label
    {
        ImFont*rf=UiFont?UiFont:ImGui::GetFont();
        const char*slbl=src_label(current.source);
        float ssz=rf->LegacySize*0.85f;
        ImVec2 sz=rf->CalcTextSizeA(ssz,FLT_MAX,0.f,slbl);
        dl->AddText(rf,ssz,wp+ImVec2(W-sz.x-8.f,5.f),src_accent(current.source,alpha*0.6f),slbl);
    }

    // Art
    const float ART=54.f,PAD=9.f;
    ImVec2 atl=wp+ImVec2(PAD,(H_IDLE-ART)*0.5f), abr=atl+ImVec2(ART,ART);
    {
        std::lock_guard<std::mutex>lk(g_art.mtx);
        if(g_art.srv&&g_art.loaded_key==last_key&&current.playing){
            if(current.source==MediaSource::YouTube){
                float tw=ART*(16.f/9.f), ox=(tw>ART)?-(tw-ART)*0.5f:0.f;
                dl->PushClipRect(atl,abr,true);
                dl->AddImageRounded((ImTextureID)g_art.srv,atl+ImVec2(ox,0.f),
                    atl+ImVec2(ox+tw,ART),ImVec2(0,0),ImVec2(1,1),
                    IM_COL32(255,255,255,(int)(255*alpha)),6.f);
                dl->PopClipRect();
            } else {
                dl->AddImageRounded((ImTextureID)g_art.srv,atl,abr,
                    ImVec2(0,0),ImVec2(1,1),IM_COL32(255,255,255,(int)(255*alpha)),6.f);
            }
            dl->AddRect(atl,abr,IM_COL32(255,255,255,(int)(22*alpha)),6.f,0,1.f);
        } else {
            dl->AddRectFilled(atl,abr,IM_COL32(28,28,34,(int)(200*alpha)),6.f);
            dl->AddRect(atl,abr,IM_COL32(60,60,70,(int)(100*alpha)),6.f,0,1.f);
            if(current.playing){
                ImVec2 c=atl+ImVec2(ART*0.5f,ART*0.5f);
                float t2=(float)ImGui::GetTime()*1.9f;
                dl->PathArcTo(c,ART*0.28f,t2,t2+IM_PI*1.3f,16);
                dl->PathStroke(acc,false,2.5f);
            }
        }
    }

    // Text
    ImFont*bf=Tahoma_BoldXP?Tahoma_BoldXP:ImGui::GetFont();
    ImFont*rf=UiFont?UiFont:ImGui::GetFont();
    float tx=abr.x+10.f, trw=wp.x+W-tx-PAD, ty=wp.y+13.f;

    auto trunc=[&](const std::string&s,ImFont*f,float mw)->std::string{
        if(s.empty())return s;
        float sc=f->LegacySize/ImGui::GetFontSize();
        if(ImGui::CalcTextSize(s.c_str()).x*sc<=mw)return s;
        std::string t=s;
        while(!t.empty()&&ImGui::CalcTextSize((t+"...").c_str()).x*sc>mw)t.pop_back();
        return t+"...";
    };

    mt_text(dl,ImVec2(tx,ty),IM_COL32(232,232,240,(int)(240*alpha)),
        trunc(current.playing?current.title:"Nothing Playing",bf,trw).c_str(),bf);
    mt_text(dl,ImVec2(tx,ty+bf->LegacySize+3.f),IM_COL32(155,155,165,(int)(180*alpha)),
        trunc(current.playing?(current.artist.empty()?"Unknown":current.artist):"",rf,trw).c_str(),rf);

    // Progress bar — real position/duration from SMTC
    const float BY=wp.y+H_IDLE-13.f,BX0=tx,BX1=wp.x+W-PAD,BW=BX1-BX0,BH=3.f;
    if(current.playing){
        // Use real duration if available, otherwise don't show bar
        float dur  = current.duration > 0.f ? current.duration : 0.f;
        float pos  = current.position;
        float tgt  = (dur > 0.f) ? (pos / dur) : 0.f;
        prog_anim  = mt_smooth(prog_anim, tgt, 4.f);

        if(dur > 0.f) {
            ImGui::SetCursorScreenPos(ImVec2(BX0,BY-4.f));
            ImGui::InvisibleButton("##mprog",ImVec2(BW,BH+8.f));
            // Progress bar is read-only for now (seeking requires SMTC seek call)
            bool ph=ImGui::IsItemHovered();
            dl->AddRectFilled(ImVec2(BX0,BY),ImVec2(BX1,BY+BH),IM_COL32(55,55,60,(int)(150*alpha)),2.f);
            float fw=BW*prog_anim;
            if(fw>0.f) dl->AddRectFilled(ImVec2(BX0,BY),ImVec2(BX0+fw,BY+BH),acc,2.f);
            if(hover_t>0.01f||ph){
                float dr=mt_lerp(0.f,5.f,hover_t); if(dr<1.f)dr=1.f;
                dl->AddCircleFilled(ImVec2(BX0+fw,BY+BH*.5f),dr+1.f,IM_COL32(0,0,0,(int)(80*alpha)),12);
                dl->AddCircleFilled(ImVec2(BX0+fw,BY+BH*.5f),dr,IM_COL32(255,255,255,(int)(240*alpha)),12);
            }
            // Time stamps
            ImFont* rf2 = UiFont ? UiFont : ImGui::GetFont();
            char t_pos[16], t_dur[16];
            snprintf(t_pos,sizeof(t_pos),"%d:%02d",(int)(pos/60),(int)fmodf(pos,60.f));
            snprintf(t_dur,sizeof(t_dur),"%d:%02d",(int)(dur/60),(int)fmodf(dur,60.f));
            float tsz = rf2->LegacySize * 0.8f;
            dl->AddText(rf2,tsz,ImVec2(BX0,BY-tsz-1.f),
                IM_COL32(120,120,130,(int)(150*alpha)),t_pos);
            ImVec2 dursz=rf2->CalcTextSizeA(tsz,FLT_MAX,0.f,t_dur);
            dl->AddText(rf2,tsz,ImVec2(BX1-dursz.x,BY-tsz-1.f),
                IM_COL32(120,120,130,(int)(150*alpha)),t_dur);
        }
    }

    // Controls
    if(hover_t>0.02f){
        float cy=wp.y+H_IDLE+mt_lerp(18.f,6.f,hover_t), ccx=wp.x+W*0.5f, ia=alpha*hover_t;
        ImU32 cn=IM_COL32(210,210,215,(int)(200*ia)), ch=IM_COL32(255,255,255,(int)(255*ia));
        ImU32 an=src_accent(current.source,0.82f*ia), ah=src_accent(current.source,ia);
        if(mt_btn(dl,"##mprev",ImVec2(ccx-52.f,cy),9.f,cn,ch,drw_prev)) media_key(VK_MEDIA_PREV_TRACK);
        if(current.playing){if(mt_btn(dl,"##mpp",ImVec2(ccx,cy),13.f,an,ah,drw_pause)) media_key(VK_MEDIA_PLAY_PAUSE);}
        else               {if(mt_btn(dl,"##mpp",ImVec2(ccx,cy),13.f,an,ah,drw_play))  media_key(VK_MEDIA_PLAY_PAUSE);}
        if(mt_btn(dl,"##mnext",ImVec2(ccx+52.f,cy),9.f,cn,ch,drw_next)) media_key(VK_MEDIA_NEXT_TRACK);
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void media::release(){
    std::lock_guard<std::mutex>lk(g_art.mtx);
    if(g_art.srv){g_art.srv->Release();g_art.srv=nullptr;}
}
