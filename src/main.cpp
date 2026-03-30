#include "App/Win32App.h"
#include "App/ShaderInputWindow.h"
#include "DX12Renderer/Renderer.h"

int main() {
    Win32App app("DX12 Raytracer", 1280, 720);
    Renderer renderer;

    if (!app.Create()) return -1;
    if (!renderer.Initialize(app.GetHWND(), 1280, 720, true)) return -1;

    ShaderInputWindow shaderInput;

    // Open the tool window next to the main window,
    // pre-filled with the default shader path
    shaderInput.Create(app.GetHWND(), "assets/shaders/screenShader.hlsl");

    // R key: reload using whatever filename is in the text box
    app.OnKeyDown = [&](WPARAM key) {
        if (key == 'R') {
            std::string path = shaderInput.GetShaderPath();
            renderer.GetPipeline().Reload(path);
        }
    };


    while (app.ProcessMessages()) {

        renderer.Render();
    }

    renderer.WaitForGPU();
    return 0;
}