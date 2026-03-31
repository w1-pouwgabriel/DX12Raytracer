#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <algorithm>     // For std::size, typed std::max, etc.
#include <DirectXMath.h> // For XMMATRIX
#include <Windows.h>     // To make a window, of course
#include <d3d12.h>       // The star of our show :)
#include <dxgi1_4.h>     // Needed to make the former two talk to each other

#pragma comment(lib, "user32") // For DefWindowProcW, etc.
#pragma comment(lib, "d3d12")  // You'll never guess this one
#pragma comment(lib, "dxgi")   // Another enigma