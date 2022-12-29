#pragma once


#include <d3d12.h>
#include <dxcapi.h>
#include "../Common/d3dUtil.h"

#pragma comment(lib,"dxcompiler.lib")


#ifndef ThrowIfFalse
#define ThrowIfFalse(x)                                              \
{                                                                     \
    HRESULT hr__ = x ? S_OK : E_FAIL;                                    \
    std::wstring wfn = AnsiToWString(__FILE__);                       \
    if(!(x)) { throw DxException(hr__, L"ThrowIfFalse", wfn, __LINE__); } \
}
#endif

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

    // Assign a name to the object to aid with debugging.
#if defined(_DEBUG) || defined(DBG)
    inline void SetName(ID3D12Object * pObject, LPCWSTR name)
    {
        pObject->SetName(name);
    }
    inline void SetNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index)
    {
        WCHAR fullName[50];
        if (swprintf_s(fullName, L"%s[%u]", name, index) > 0)
        {
            pObject->SetName(fullName);
        }
    }
#else
    inline void SetName(ID3D12Object*, LPCWSTR)
    {
    }
    inline void SetNameIndexed(ID3D12Object*, LPCWSTR, UINT)
    {
    }
#endif
    
    Microsoft::WRL::ComPtr<IDxcBlob> CompileShaderLibrary(LPCWSTR fileName, LPCWSTR targetProfile, LPCWSTR entryPoint= L"");


    inline UINT Align(UINT size, UINT alignment)
    {
        return (size + (alignment - 1)) & ~(alignment - 1);
    }


    class GpuUploadBuffer
    {
    public:
        Microsoft::WRL::ComPtr<ID3D12Resource> GetResource() { return m_resource; }
        virtual void Release() { m_resource.Reset(); }
    protected:
        Microsoft::WRL::ComPtr<ID3D12Resource> m_resource;

        GpuUploadBuffer() {}
        ~GpuUploadBuffer()
        {
            if (m_resource.Get())
            {
                m_resource->Unmap(0, nullptr);
            }
        }

        void Allocate(ID3D12Device* device, UINT bufferSize, LPCWSTR resourceName = nullptr)
        {
            auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

            auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
            ThrowIfFailed(device->CreateCommittedResource(
                &uploadHeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&m_resource)));
            m_resource->SetName(resourceName);
        }

        uint8_t* MapCpuWriteOnly()
        {
            uint8_t* mappedData;
            // We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
            CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
            ThrowIfFailed(m_resource->Map(0, &readRange, reinterpret_cast<void**>(&mappedData)));
            return mappedData;
        }
    };

    struct D3DBuffer
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle;
        D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandle;
    };

    // Helper class to create and update a constant buffer with proper constant buffer alignments.
    // Usage: 
    //    ConstantBuffer<...> cb;
    //    cb.Create(...);
    //    cb.staging.var = ... ; | cb->var = ... ; 
    //    cb.CopyStagingToGPU(...);
    //    Set...View(..., cb.GputVirtualAddress());
    template <class T>
    class ConstantBuffer : public GpuUploadBuffer
    {
        uint8_t* m_mappedConstantData;
        UINT m_alignedInstanceSize;
        UINT m_numInstances;

    public:
        ConstantBuffer() : m_alignedInstanceSize(0), m_numInstances(0), m_mappedConstantData(nullptr) {}

        void Create(ID3D12Device* device, UINT numInstances = 1, LPCWSTR resourceName = nullptr)
        {
            m_numInstances = numInstances;
            m_alignedInstanceSize = Align(sizeof(T), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
            UINT bufferSize = numInstances * m_alignedInstanceSize;
            Allocate(device, bufferSize, resourceName);
            m_mappedConstantData = MapCpuWriteOnly();
        }

        void CopyStagingToGpu(UINT instanceIndex = 0)
        {
            memcpy(m_mappedConstantData + instanceIndex * m_alignedInstanceSize, &staging, sizeof(T));
        }

        // Accessors
        T staging;
        T* operator->() { return &staging; }
        UINT NumInstances() { return m_numInstances; }
        D3D12_GPU_VIRTUAL_ADDRESS GpuVirtualAddress(UINT instanceIndex = 0)
        {
            return m_resource->GetGPUVirtualAddress() + instanceIndex * m_alignedInstanceSize;
        }
    };


    // Helper class to create and update a structured buffer.
    // Usage: 
    //    StructuredBuffer<...> sb;
    //    sb.Create(...);
    //    sb[index].var = ... ; 
    //    sb.CopyStagingToGPU(...);
    //    Set...View(..., sb.GputVirtualAddress());
    template <class T>
    class StructuredBuffer : public GpuUploadBuffer
    {
        T* m_mappedBuffers;
        std::vector<T> m_staging;
        UINT m_numInstances;

    public:
        // Performance tip: Align structures on sizeof(float4) boundary.
        // Ref: https://developer.nvidia.com/content/understanding-structured-buffer-performance
        static_assert(sizeof(T) % 16 == 0, L"Align structure buffers on 16 byte boundary for performance reasons.");

        StructuredBuffer() : m_mappedBuffers(nullptr), m_numInstances(0) {}

        void Create(ID3D12Device* device, UINT numElements, UINT numInstances = 1, LPCWSTR resourceName = nullptr)
        {
            m_staging.resize(numElements);
            UINT bufferSize = numInstances * numElements * sizeof(T);
            Allocate(device, bufferSize, resourceName);
            m_mappedBuffers = reinterpret_cast<T*>(MapCpuWriteOnly());
        }

        void CopyStagingToGpu(UINT instanceIndex = 0)
        {
            memcpy(m_mappedBuffers + instanceIndex * NumElementsPerInstance(), &m_staging[0], InstanceSize());
        }

        // Accessors
        T& operator[](UINT elementIndex) { return m_staging[elementIndex]; }
        size_t NumElementsPerInstance() { return m_staging.size(); }
        UINT NumInstances() { return m_staging.size(); }
        size_t InstanceSize() { return NumElementsPerInstance() * sizeof(T); }
        D3D12_GPU_VIRTUAL_ADDRESS GpuVirtualAddress(UINT instanceIndex = 0)
        {
            return m_resource->GetGPUVirtualAddress() + instanceIndex * InstanceSize();
        }
    };

    // Shader record = {{Shader ID}, {RootArguments}}
    class ShaderRecord
    {
    public:
        ShaderRecord(void* pShaderIdentifier, UINT shaderIdentifierSize) :
            shaderIdentifier(pShaderIdentifier, shaderIdentifierSize)
        {
        }

        ShaderRecord(void* pShaderIdentifier, UINT shaderIdentifierSize, void* pLocalRootArguments, UINT localRootArgumentsSize) :
            shaderIdentifier(pShaderIdentifier, shaderIdentifierSize),
            localRootArguments(pLocalRootArguments, localRootArgumentsSize)
        {
        }

        void CopyTo(void* dest) const
        {
            uint8_t* byteDest = static_cast<uint8_t*>(dest);
            memcpy(byteDest, shaderIdentifier.ptr, shaderIdentifier.size);
            if (localRootArguments.ptr)
            {
                memcpy(byteDest + shaderIdentifier.size, localRootArguments.ptr, localRootArguments.size);
            }
        }

        struct PointerWithSize {
            void* ptr;
            UINT size;

            PointerWithSize() : ptr(nullptr), size(0) {}
            PointerWithSize(void* _ptr, UINT _size) : ptr(_ptr), size(_size) {};
        };
        PointerWithSize shaderIdentifier;
        PointerWithSize localRootArguments;
    };

    // Shader table = {{ ShaderRecord 1}, {ShaderRecord 2}, ...}
    class ShaderTable : public GpuUploadBuffer
    {
        uint8_t* m_mappedShaderRecords;
        UINT m_shaderRecordSize;

        // Debug support
        std::wstring m_name;
        std::vector<ShaderRecord> m_shaderRecords;

        ShaderTable() {}
    public:
        ShaderTable(ID3D12Device* device, UINT numShaderRecords, UINT shaderRecordSize, LPCWSTR resourceName = nullptr)
            : m_name(resourceName)
        {
            m_shaderRecordSize = Align(shaderRecordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
            m_shaderRecords.reserve(numShaderRecords);
            UINT bufferSize = numShaderRecords * m_shaderRecordSize;
            Allocate(device, bufferSize, resourceName);
            m_mappedShaderRecords = MapCpuWriteOnly();
        }

        void push_back(const ShaderRecord& shaderRecord)
        {
            ThrowIfFalse(m_shaderRecords.size() < m_shaderRecords.capacity());
            m_shaderRecords.push_back(shaderRecord);
            shaderRecord.CopyTo(m_mappedShaderRecords);
            m_mappedShaderRecords += m_shaderRecordSize;
        }

        UINT GetShaderRecordSize() { return m_shaderRecordSize; }

        // Pretty-print the shader records.
        void DebugPrint(std::unordered_map<void*, std::wstring> shaderIdToStringMap)
        {
            std::wstringstream wstr;
            wstr << L"|--------------------------------------------------------------------\n";
            wstr << L"|Shader table - " << m_name.c_str() << L": "
                << m_shaderRecordSize << L" | "
                << m_shaderRecords.size() * m_shaderRecordSize << L" bytes\n";

            for (UINT i = 0; i < m_shaderRecords.size(); i++)
            {
                wstr << L"| [" << i << L"]: ";
                wstr << shaderIdToStringMap[m_shaderRecords[i].shaderIdentifier.ptr] << L", ";
                wstr << m_shaderRecords[i].shaderIdentifier.size << L" + " << m_shaderRecords[i].localRootArguments.size << L" bytes \n";
            }
            wstr << L"|--------------------------------------------------------------------\n";
            wstr << L"\n";
            OutputDebugStringW(wstr.str().c_str());
        }

    };

}

#define NAME_D3D12_OBJECT(x) raytrace_helper::SetName((x).Get(), L#x)
#define NAME_D3D12_OBJECT_INDEXED(x, n) raytrace_helper::SetNameIndexed((x)[n].Get(), L#x, n)
