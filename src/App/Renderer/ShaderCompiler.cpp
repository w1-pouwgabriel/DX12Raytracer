#include "ShaderCompiler.h"
#include <iostream>
#include <stdexcept>
#include <vector>

bool ShaderCompiler::Initialize() {
    if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_utils)))) {
        std::cerr << "[ShaderCompiler] Failed to create DxcUtils.\n"
            << "  Is dxcompiler.dll in your build directory?\n";
        return false;
    }
    if (FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_compiler)))) {
        std::cerr << "[ShaderCompiler] Failed to create DxcCompiler.\n";
        return false;
    }

    // Default include handler resolves #include relative to cwd
    m_utils->CreateDefaultIncludeHandler(&m_includeHandler);

    std::cout << "[ShaderCompiler] DXC initialized.\n";
    return true;
}

ComPtr<IDxcBlob> ShaderCompiler::Compile(
    const std::string& hlslSource,
    const std::wstring& target,
    const std::wstring& entry)
{
    // ?? 1. Wrap source string in a DXC blob ??????????????????????????????
    ComPtr<IDxcBlobEncoding> srcBlob;
    if (FAILED(m_utils->CreateBlob(
        hlslSource.c_str(),
        static_cast<UINT32>(hlslSource.size()),
        CP_UTF8,
        &srcBlob)))
    {
        throw std::runtime_error("[ShaderCompiler] Failed to create source blob");
    }

    // ?? 2. Build argument list ????????????????????????????????????????????
    // Note: pointers into these wstrings must stay alive for the Compile call.
    // They do target and entry are const refs that outlive this scope.
    std::vector<LPCWSTR> args = {
        L"-T", target.c_str(),   // Target profile  (lib_6_3, vs_6_0, etc.)
        L"-HV", L"2021",         // HLSL language version
        L"-Zi",                  // Embed debug info   ? remove for release builds
        L"-Od",                  // Disable optimizations ? remove for release builds
    };

    if (!entry.empty()) {
        args.push_back(L"-E");
        args.push_back(entry.c_str());
    }

    // Compile data together
    DxcBuffer srcBuffer{
        srcBlob->GetBufferPointer(),
        srcBlob->GetBufferSize(),
        DXC_CP_UTF8
    };

    ComPtr<IDxcResult> result;
    m_compiler->Compile(
        &srcBuffer,
        args.data(), static_cast<UINT32>(args.size()),
        m_includeHandler.Get(),
        IID_PPV_ARGS(&result)
    );

    // Print warnings / errors 
    ComPtr<IDxcBlobUtf8> errors;
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
    if (errors && errors->GetStringLength() > 0) {
        std::cerr << "[ShaderCompiler]\n" << errors->GetStringPointer() << "\n";
    }

    // Bail on failure
    HRESULT status;
    result->GetStatus(&status);
    if (FAILED(status)) {
        throw std::runtime_error("[ShaderCompiler] Compilation failed see errors above");
    }

    // 6. Extract compiled DXIL blob ????????????????????????????????????
    ComPtr<IDxcBlob> shaderBlob;
    result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    return shaderBlob;
}