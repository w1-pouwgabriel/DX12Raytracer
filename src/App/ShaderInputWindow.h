#pragma once
#include <Windows.h>
#include <string>

// Small tool window with a text box for typing a shader filename.
// Call GetShaderPath() when R is pressed to get the current value.
class ShaderInputWindow {
public:
    ShaderInputWindow() = default;
    ~ShaderInputWindow();

    // Creates and shows the tool window next to the main window
    bool Create(HWND parent, const std::string& defaultPath);

    // Returns the current text in the edit box
    std::string GetShaderPath() const;

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam);

    HWND m_hwnd = nullptr;
    HWND m_label = nullptr;
    HWND m_editBox = nullptr;
};