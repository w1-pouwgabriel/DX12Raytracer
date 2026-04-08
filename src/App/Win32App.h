#pragma once

#include <Windows.h>
#include <string>
#include <functional>
#include <cstdint>
#include "Renderer/Renderer.h"

class Win32App {
public:
    Win32App(const std::string& title, uint32_t width, uint32_t height);
    ~Win32App();

    // Disable copy
    Win32App(const Win32App&) = delete;
    Win32App& operator=(const Win32App&) = delete;

    // Create and show window
    bool Create();
    int  Run();
    bool ProcessMessages();
    void ResizeWindow(uint32_t width, uint32_t height);

    // Getters
    HWND GetHWND() const { return m_hwnd; }
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    bool IsRunning() const { return m_running; }

private:
    // Window message handler
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam);

    std::unique_ptr<Renderer>         m_renderer;

    HWND m_hwnd;
    HINSTANCE m_hInstance;
    std::string m_title;
    uint32_t m_width;
    uint32_t m_height;

    // Flag to control the main loop
    bool m_running;

    HMENU m_menu;
    // Menu item IDs
    static const int ID_RESOLUTION_720P = 1001;
    static const int ID_RESOLUTION_1080P = 1002;
    static const int ID_RESOLUTION_1440P = 1003;
    static const int ID_RESOLUTION_4K = 1004;
};