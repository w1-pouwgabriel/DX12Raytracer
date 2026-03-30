#include "App/Win32App.h"
#include "DX12Renderer/Renderer.h"

int main() {
    Win32App app("DX12 Raytracer", 1280, 720);
    Renderer renderer;

    if (!app.Create()) return -1;
    if (!renderer.Initialize(app.GetHWND(), 1280, 720, true)) return -1;

    app.OnKeyDown = [&](WPARAM key) {
        if (key == 'R') renderer.GetPipeline().Reload("screenShader");
     };

    while (app.ProcessMessages()) {

        renderer.Render();
    }

    renderer.WaitForGPU();
    return 0;
}