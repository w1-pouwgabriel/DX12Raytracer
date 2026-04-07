#include "Win32App.h"
#include "Renderer/RaytracingPipeline.h"
#include <iostream>

Win32App::Win32App(const std::string& title, uint32_t width, uint32_t height)
    : m_hwnd(nullptr)
    , m_hInstance(GetModuleHandle(nullptr))
    , m_title(title)
    , m_width(width)
    , m_height(height)
    , m_running(false)
{
}

Win32App::~Win32App() 
{
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
}

bool Win32App::Create() 
{
    // Register window class
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = LPCTSTR("DX12RaytracerWindowClass");
    wc.hIconSm = HICON();

    if (!RegisterClassEx(&wc)) {
        std::cerr << "Failed to register window class" << std::endl;
        return false;
    }

    // Calculate window size including borders
    RECT rect = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    int windowWidth = rect.right - rect.left;
    int windowHeight = rect.bottom - rect.top;

    // Create window
    m_hwnd = CreateWindowEx(
        0,
        LPCTSTR("DX12RaytracerWindowClass"),
        LPCTSTR(m_title.c_str()),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        windowWidth, windowHeight,
        nullptr,
        nullptr,
        m_hInstance,
        this  // Pass 'this' pointer to WM_CREATE
    );

    if (!m_hwnd) {
        std::cerr << "Failed to create window" << std::endl;
        return false;
    }

    // Show window
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    m_running = true;

    return true;
}

int Win32App::Run() 
{
    if (!Create()) return -1;

    m_renderer = std::make_unique<Renderer>();

    if (!m_renderer->Initialize(m_hwnd, m_width, m_height, true)) return -1;

    while (ProcessMessages()) {
        m_renderer->Render();
    }

    m_renderer->WaitForGPU();
    return 0;
}

bool Win32App::ProcessMessages() 
{
    MSG msg = {};

    // Process all pending messages
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            m_running = false;
            return false;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return m_running;
}

LRESULT CALLBACK Win32App::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) 
{
    Win32App* app = nullptr;

    if (msg == WM_CREATE) {
        // Get the Win32App pointer from CreateWindowEx
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lparam);
        app = reinterpret_cast<Win32App*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    } else {
        // Retrieve the Win32App pointer
        app = reinterpret_cast<Win32App*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }


    if (app) {
        return app->HandleMessage(msg, wparam, lparam);
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

LRESULT Win32App::HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
        // Triggers when: User resizes window, maximize/minimize
        case WM_SIZE: {
            m_width = LOWORD(lparam);
            m_height = HIWORD(lparam);

            if (m_renderer && m_width > 0 && m_height > 0) {
                m_renderer->Resize(m_width, m_height);
            }
            
            return 0;
        }

        // Triggers when: User presses a key
        case WM_KEYDOWN: {
            // ESC key closes window
            if (wparam == VK_ESCAPE) {
                PostQuitMessage(0);
                return 0;
            }
            if (wparam == VK_ESCAPE) { PostQuitMessage(0); return 0; }
            break;

            break;
        }

        // Triggers when: User clicks X, Alt+F4, window destroyed
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }

        // Triggers when: Window exposed, minimized/restored
        case WM_PAINT: {
            ValidateRect(m_hwnd, nullptr);
            return 0;
        }
    }

    return DefWindowProc(m_hwnd, msg, wparam, lparam);
}