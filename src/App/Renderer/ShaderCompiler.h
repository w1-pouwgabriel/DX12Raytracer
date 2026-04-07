#pragma once
#include <wrl/client.h>
#include <dxcapi.h>
#include <string>
#include <vector>

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

class ShaderCompiler {
public:
    bool Initialize();

    // Compile HLSL source to a DXIL blob
    ComPtr<IDxcBlob> Compile(
        const std::string& hlslSource,
        const std::wstring& target,
        const std::wstring& entry = L""
    );

private:
    ComPtr<IDxcUtils>          m_utils;
    ComPtr<IDxcCompiler3>      m_compiler;
    ComPtr<IDxcIncludeHandler> m_includeHandler;
};