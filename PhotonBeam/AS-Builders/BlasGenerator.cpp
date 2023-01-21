
#include "BlasGenerator.hpp"

// Helper to compute aligned buffer sizes
#ifndef ROUND_UP
#define ROUND_UP(v, powerOf2Alignment)                                         \
  (((v) + (powerOf2Alignment)-1) & ~((powerOf2Alignment)-1))
#endif

namespace ASBuilder {

    BlasGenerator::BlasGenerator(
        ID3D12Device5* pDevice
    ) :m_pDevice(pDevice), 
        m_scratchSizeInBytes(0), 
        m_resultSizeInBytes(0)
    {
        m_geometryDescs = std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>();
    }

    void BlasGenerator::AddAabbBuffer(
        ID3D12Resource* aabbBuffer,
        UINT64 aabbOffsetInBytes,
        uint32_t aabbCount,
        bool isOpaque,
        bool allowDuplicateAnyHit
    )
    {
        D3D12_RAYTRACING_GEOMETRY_DESC descriptor = {};
        descriptor.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
        descriptor.AABBs.AABBs.StartAddress = aabbBuffer->GetGPUVirtualAddress() + aabbOffsetInBytes;
        descriptor.AABBs.AABBs.StrideInBytes = sizeof(D3D12_RAYTRACING_AABB);
        descriptor.AABBs.AABBCount = aabbCount;

        descriptor.Flags = isOpaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
        if (!allowDuplicateAnyHit)
            descriptor.Flags |= D3D12_RAYTRACING_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION;

        m_geometryDescs.push_back(descriptor);
    }

    void BlasGenerator::AddVertexBuffer(
        ID3D12Resource *vertexBuffer, 
        UINT64 vertexOffsetInBytes,
        uint32_t vertexCount, 
        UINT vertexSizeInBytes, 
        ID3D12Resource *transformBuffer, 
        UINT64 transformOffsetInBytes,   
        bool isOpaque 
    )
    {
      AddVertexBuffer(
          vertexBuffer, 
          vertexOffsetInBytes, 
          vertexCount,
          vertexSizeInBytes, 
          nullptr, 
          0, 
          0, 
          transformBuffer,
          transformOffsetInBytes,
          isOpaque
      );
    }

    void BlasGenerator::AddVertexBuffer(
        ID3D12Resource *vertexBuffer,
        UINT64 vertexOffsetInBytes,
        uint32_t vertexCount,
        UINT vertexSizeInBytes,
        ID3D12Resource *indexBuffer,
        UINT64 indexOffsetInBytes,
        uint32_t indexCount,
        ID3D12Resource *transformBuffer,
        UINT64 transformOffsetInBytes,
        bool isOpaque
    ) 
    {
        D3D12_RAYTRACING_GEOMETRY_DESC descriptor = {};
        descriptor.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        descriptor.Triangles.VertexBuffer.StartAddress = vertexBuffer->GetGPUVirtualAddress() + vertexOffsetInBytes;
        descriptor.Triangles.VertexBuffer.StrideInBytes = vertexSizeInBytes;
        descriptor.Triangles.VertexCount = vertexCount;
        descriptor.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        descriptor.Triangles.IndexBuffer = indexBuffer ? (indexBuffer->GetGPUVirtualAddress() + indexOffsetInBytes) : 0;
        descriptor.Triangles.IndexFormat = indexBuffer ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_UNKNOWN;
        descriptor.Triangles.IndexCount = indexCount;
        descriptor.Triangles.Transform3x4 = transformBuffer ? (transformBuffer->GetGPUVirtualAddress() + transformOffsetInBytes) : 0;
        descriptor.Flags = isOpaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
        m_geometryDescs.push_back(descriptor);
    }

    void BlasGenerator::ComputeASBufferSizes(
        bool allowUpdate, 
        UINT64 *scratchSizeInBytes,
        UINT64 *resultSizeInBytes   
    ) 
    {

        m_flags = allowUpdate ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE 
            : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
  
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildDesc{};
        prebuildDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        prebuildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        prebuildDesc.NumDescs = static_cast<UINT>(m_geometryDescs.size());
        prebuildDesc.pGeometryDescs = m_geometryDescs.data();
        prebuildDesc.Flags = m_flags;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};

        m_pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildDesc, &info);

        // Buffer sizes need to be 256-byte-aligned
        *scratchSizeInBytes =
            ROUND_UP(info.ScratchDataSizeInBytes,
                    D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        *resultSizeInBytes = ROUND_UP(info.ResultDataMaxSizeInBytes,
                                    D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        // Store the memory requirements for use during build
        m_scratchSizeInBytes = *scratchSizeInBytes;
        m_resultSizeInBytes = *resultSizeInBytes;
    }

    void BlasGenerator::Generate(
        ID3D12GraphicsCommandList4 *commandList,
        ID3D12Resource *scratchBuffer,
        ID3D12Resource *resultBuffer, 
        bool updateOnly,
        ID3D12Resource *previousResult 
    )
    {
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags = m_flags;

        if (flags == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE && updateOnly)
            flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
        

        // Sanity checks
        if (m_flags != D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE && updateOnly)
            throw std::logic_error( "Cannot update a bottom-level AS not originally built for updates");
     
        if (updateOnly && previousResult == nullptr)
            throw std::logic_error("Bottom-level hierarchy update requires the previous hierarchy");
    

        if (m_resultSizeInBytes == 0 || m_scratchSizeInBytes == 0)
            throw std::logic_error(
                "Invalid scratch and result buffer sizes - ComputeASBufferSizes needs "
                "to be called before Build"
            );
        
        // Create a descriptor of the requested builder work, to generate a
        // bottom-level AS from the input parameters
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc{};
        buildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        buildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        buildDesc.Inputs.NumDescs = static_cast<UINT>(m_geometryDescs.size());
        buildDesc.Inputs.pGeometryDescs = m_geometryDescs.data();
        buildDesc.DestAccelerationStructureData = {resultBuffer->GetGPUVirtualAddress()};
        buildDesc.ScratchAccelerationStructureData = {scratchBuffer->GetGPUVirtualAddress()};
        buildDesc.SourceAccelerationStructureData = previousResult ? previousResult->GetGPUVirtualAddress() : 0;
        buildDesc.Inputs.Flags = flags;

        // Build the AS
        commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

        D3D12_RESOURCE_BARRIER uavBarrier{};
        uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uavBarrier.UAV.pResource = resultBuffer;
        uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        commandList->ResourceBarrier(1, &uavBarrier);
    }
} 
