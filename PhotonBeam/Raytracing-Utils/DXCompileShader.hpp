#pragma once


#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>

#pragma comment(lib,"dxcompiler.lib")


Microsoft::WRL::ComPtr<IDxcBlob> DxCompileShaderLibrary(LPCWSTR fileName, LPCWSTR targetProfile, LPCWSTR entryPoint = L"");


