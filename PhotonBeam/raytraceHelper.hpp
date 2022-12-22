#pragma once


#include <d3d12.h>
#include <dxcapi.h>
#include "../Common/d3dUtil.h"

#pragma comment(lib,"dxcompiler.lib")


namespace raytrace_helper
{
    static const D3D12_HEAP_PROPERTIES pmUploadHeapProps = {
    D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0 };

    static const D3D12_HEAP_PROPERTIES pmDefaultHeapProps = {
        D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0 };

    //--------------------------------------------------------------------------------------------------
    //
    //
    inline Microsoft::WRL::ComPtr<ID3D12Resource> CreateBuffer(ID3D12Device* m_device, uint64_t size,
        D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initState,
        const D3D12_HEAP_PROPERTIES& heapProps)
    {
        D3D12_RESOURCE_DESC bufDesc = {};
        bufDesc.Alignment = 0;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufDesc.Flags = flags;
        bufDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufDesc.Height = 1;
        bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufDesc.MipLevels = 1;
        bufDesc.SampleDesc.Count = 1;
        bufDesc.SampleDesc.Quality = 0;
        bufDesc.Width = size;

        Microsoft::WRL::ComPtr<ID3D12Resource> resultBuffer;

        ThrowIfFailed(
            m_device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &bufDesc,
                initState,
                nullptr,
                IID_PPV_ARGS(resultBuffer.GetAddressOf())
            )
        );

        return resultBuffer;
    }

    
    Microsoft::WRL::ComPtr<IDxcBlob> CompileShaderLibrary(LPCWSTR fileName);

}
