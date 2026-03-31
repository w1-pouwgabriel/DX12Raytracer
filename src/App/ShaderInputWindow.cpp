#include "ShaderInputWindow.h"

ShaderInputWindow::~ShaderInputWindow() {
    if (m_hwnd) DestroyWindow(m_hwnd);
}

bool ShaderInputWindow::Create(HWND parent, const std::string& defaultPath) {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "ShaderInputWindowClass";
    RegisterClassEx(&wc);

    RECT parentRect = {};
    GetWindowRect(parent, &parentRect);

    m_hwnd = CreateWindowEx(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        "ShaderInputWindowClass",
        "Shader",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VISIBLE, //  WS_THICKFRAME enables resize
        parentRect.right + 8, parentRect.top,
        400, 100,
        nullptr, nullptr, hInst,
        this  //  pass instance so WindowProc can reach it
    );
    if (!m_hwnd) return false;

    m_label = CreateWindow(
        "STATIC", "Shader file (press R in main window to reload):",
        WS_VISIBLE | WS_CHILD,
        8, 6, 320, 16,
        m_hwnd, nullptr, hInst, nullptr
    );

    m_editBox = CreateWindow(
        "EDIT", defaultPath.c_str(),
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        8, 26, 320, 24,
        m_hwnd, nullptr, hInst, nullptr
    );

    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessage(m_label, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessage(m_editBox, WM_SETFONT, (WPARAM)font, TRUE);

    return true;
}

std::string ShaderInputWindow::GetShaderPath() const {
    if (!m_editBox) return {};
    char buf[512] = {};
    GetWindowTextA(m_editBox, buf, sizeof(buf));
    return buf;
}

// Static func just routes the message to the windows instance
LRESULT CALLBACK ShaderInputWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    ShaderInputWindow* self = nullptr;

    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lparam);
        self = reinterpret_cast<ShaderInputWindow*>(cs->lpCreateParams);
         (hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else {
        self = reinterpret_cast<ShaderInputWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    return self ? self->HandleMessage(msg, wparam, lparam)
        : DefWindowProc(hwnd, msg, wparam, lparam);
}

LRESULT ShaderInputWindow::HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_SIZE: {
        int w = LOWORD(lparam);
        int h = HIWORD(lparam);

        // Keep label pinned to top, stretch edit box to fill remaining width
        SetWindowPos(m_label, nullptr, 8, 6, w - 16, 16, SWP_NOZORDER);
        SetWindowPos(m_editBox, nullptr, 8, 26, w - 16, 24, SWP_NOZORDER);
        return 0;
    }
    case WM_CLOSE: {
        // Hide instead of destroy so it can't be accidentally killed
        ShowWindow(m_hwnd, SW_HIDE);
        return 0;
    }
    case WM_PAINT: {
        ValidateRect(m_hwnd, nullptr);
        return 0;
    }
    }
    return DefWindowProc(m_hwnd, msg, wparam, lparam);
}