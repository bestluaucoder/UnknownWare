#pragma once
#define IMGUI_DEFINE_MATH_OPERATORS
#include <memory>
#include <d3d11.h>
#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>

#include <string>
#include <vector> 

inline ImFont* Tahoma_BoldXP = nullptr;
inline ImFont* UiFont = nullptr;
inline ImFont* TitleFont = nullptr;
inline ImFont* LogoFont = nullptr;
inline ImFont* IdaTabFont = nullptr;
inline ImFont* IdaLegitTabFont = nullptr;

struct detail {
	HWND Window = nullptr;
	WNDCLASSEX WindowClass = {};
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	ID3D11RenderTargetView* GraphicsTargetView = nullptr;
	ID3D11ShaderResourceView* IdaBgTexture = nullptr;
	IDXGISwapChain* SwapChain = nullptr;
};

class graphic {
public:
    graphic();
    ~graphic();

    bool Running = false;

    void begin();
    void menu();
    void visual();
    void end();

    bool device();
    bool window();
    bool imgui();

    std::unique_ptr<detail> Detail = std::make_unique<detail>();

private:
    void dropdevice();
    void dropwindow();
    void dropimgui();
};

inline std::unique_ptr<graphic> screen = std::make_unique<graphic>();
