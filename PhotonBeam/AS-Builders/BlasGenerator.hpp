
#pragma once

#include "d3d12.h"

#include <stdexcept>
#include <vector>

namespace ASBuilder
{

    /// Helper class to generate bottom-level acceleration structures for raytracing
    class BlasGenerator
    {
    public:
        BlasGenerator(
            ID3D12Device5* pDevice
        );
        void AddAabbBuffer(
            ID3D12Resource* aabbBuffer,
            UINT64 aabbOffsetInBytes,
            uint32_t aabbCount,
            bool isOpaque = false,
            bool allowDuplicateAnyHit = false
        );

        void AddVertexBuffer(
            ID3D12Resource* vertexBuffer,
            UINT64 vertexOffsetInBytes,
            uint32_t vertexCount,
            UINT vertexSizeInBytes,
            ID3D12Resource* transformBuffer,
            UINT64 transformOffsetInBytes,
            bool isOpaque = true
        );

        void AddVertexBuffer(
            ID3D12Resource* vertexBuffer,
            UINT64 vertexOffsetInBytes,
            uint32_t vertexCount,
            UINT vertexSizeInBytes,
            ID3D12Resource* indexBuffer,
            UINT64 indexOffsetInBytes,
            uint32_t indexCount,          /// Number of indices to consider in the buffer
            ID3D12Resource* transformBuffer, /// Buffer containing a 4x4 transform
            UINT64 transformOffsetInBytes,
            bool isOpaque = true
        );

        void ComputeASBufferSizes(
          bool allowUpdate,
          UINT64* scratchSizeInBytes, 
          UINT64* resultSizeInBytes                          
        );


        void Generate(
            ID3D12GraphicsCommandList4* commandList,
            ID3D12Resource* scratchBuffer,                 
            ID3D12Resource* resultBuffer,
            bool updateOnly = false,
            ID3D12Resource* previousResult = nullptr /// Optional previous acceleration structure, used
        );

    private:
        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> m_geometryDescs;

        UINT64 m_scratchSizeInBytes;
        UINT64 m_resultSizeInBytes;
        ID3D12Device5* m_pDevice;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS m_flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    };

}
