#pragma once

#include <Windows.h>
#include <string>
#include <functional>
#include <cstdint>

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

    // Getters
    HWND GetHWND() const { return m_hwnd; }
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    bool IsRunning() const { return m_running; }

    // Callbacks - set these to handle events
    std::function<void()> OnInit;          // Called after window creation
    std::function<void()> OnUpdate;        // Called each frame
    std::function<void()> OnRender;        // Called each frame after update
    std::function<void()> OnDestroy;       // Called before window destruction
    std::function<void(uint32_t, uint32_t)> OnResize;  // Called when window resizes
    std::function<void(WPARAM)>             OnKeyDown;

private:
    // Window message handler
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam);

    HWND m_hwnd;
    HINSTANCE m_hInstance;
    std::string m_title;
    uint32_t m_width;
    uint32_t m_height;
    bool m_running;
};