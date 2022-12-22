
#include "raytraceHelper.hpp"


Microsoft::WRL::ComPtr<IDxcBlob> raytrace_helper::CompileShaderLibrary(LPCWSTR fileName)
{
    static IDxcCompiler* pCompiler = nullptr;
    static IDxcLibrary* pLibrary = nullptr;
    static IDxcIncludeHandler* dxcIncludeHandler;

    HRESULT hr;

    // Initialize the DXC compiler and compiler helper
    if (!pCompiler)
    {
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, __uuidof(IDxcCompiler), (void**)&pCompiler));
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcLibrary, __uuidof(IDxcLibrary), (void**)&pLibrary));
        ThrowIfFailed(pLibrary->CreateIncludeHandler(&dxcIncludeHandler));
    }

    // Open and read the file
    std::ifstream shaderFile(fileName);
    if (shaderFile.good() == false)
    {
        throw std::logic_error("Cannot find shader file");
    }
    std::stringstream strStream;
    strStream << shaderFile.rdbuf();
    std::string sShader = strStream.str();

    // Create blob from the string
    IDxcBlobEncoding* pTextBlob;
    ThrowIfFailed(
        pLibrary->CreateBlobWithEncodingFromPinned(
        (LPBYTE)sShader.c_str(), 
            (uint32_t)sShader.size(), 
            0, 
            &pTextBlob
        )
    );

    // Compile
    Microsoft::WRL::ComPtr<IDxcOperationResult> pResult;
    ThrowIfFailed(
        pCompiler->Compile(
            pTextBlob, 
            fileName, 
            L"", 
            L"lib_6_3", 
            nullptr, 
            0, 
            nullptr, 
            0,
            dxcIncludeHandler, 
            pResult.GetAddressOf()
        )
    );

    // Verify the result
    HRESULT resultCode;
    ThrowIfFailed(pResult->GetStatus(&resultCode));
    if (FAILED(resultCode))
    {
        Microsoft::WRL::ComPtr<IDxcBlobEncoding> pError;
        hr = pResult->GetErrorBuffer(pError.GetAddressOf());
        if (FAILED(hr))
        {
            throw std::logic_error("Failed to get shader compiler error");
        }

        // Convert error blob to a string
        std::vector<char> infoLog(pError->GetBufferSize() + 1);
        memcpy(infoLog.data(), pError->GetBufferPointer(), pError->GetBufferSize());
        infoLog[pError->GetBufferSize()] = 0;

        std::string errorMsg = "Shader Compiler Error:\n";
        errorMsg.append(infoLog.data());

        MessageBoxA(nullptr, errorMsg.c_str(), "Error!", MB_OK);
        throw std::logic_error("Failed compile shader");
    }

    Microsoft::WRL::ComPtr<IDxcBlob> pBlob = nullptr;

    ThrowIfFailed(pResult->GetResult(pBlob.GetAddressOf()));
    return pBlob;
}