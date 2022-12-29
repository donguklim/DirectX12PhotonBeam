
#include "raytraceHelper.hpp"


Microsoft::WRL::ComPtr<IDxcBlob> raytrace_helper::CompileShaderLibrary(LPCWSTR fileName, LPCWSTR targetProfile, LPCWSTR entryPoint)
{
    static  Microsoft::WRL::ComPtr<IDxcCompiler> pCompiler = nullptr;
    static  Microsoft::WRL::ComPtr<IDxcLibrary> pLibrary = nullptr;
    static  Microsoft::WRL::ComPtr<IDxcIncludeHandler> dxcIncludeHandler = nullptr;

    HRESULT hr;

    // Initialize the DXC compiler and compiler helper
    if (!pCompiler)
    {
        ThrowIfFailed(
            DxcCreateInstance(
                CLSID_DxcCompiler, 
                IID_PPV_ARGS(pCompiler.GetAddressOf())
            )
        );
        ThrowIfFailed(
            DxcCreateInstance(
                CLSID_DxcLibrary, 
                IID_PPV_ARGS(pLibrary.GetAddressOf())
            )
        );
        ThrowIfFailed(
            pLibrary->CreateIncludeHandler(
               dxcIncludeHandler.GetAddressOf()
            )
        );
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
    Microsoft::WRL::ComPtr<IDxcBlobEncoding> pTextBlob;
    ThrowIfFailed(
        pLibrary->CreateBlobWithEncodingFromPinned(
            (LPBYTE)sShader.c_str(),
            (uint32_t)sShader.size(),
            0,
            pTextBlob.GetAddressOf()
        )
    );

    // Compile
    Microsoft::WRL::ComPtr<IDxcOperationResult> pResult;
    ThrowIfFailed(
        pCompiler->Compile(
            pTextBlob.Get(),
            fileName, 
            entryPoint,
            targetProfile,
            nullptr, 
            0, 
            nullptr, 
            0,
            dxcIncludeHandler.Get(),
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
