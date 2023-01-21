
#pragma once

#include "d3d12.h"

#include <DirectXMath.h>

#include <vector>

namespace ASBuilder
{
    class TlasGenerator
    {
    public:
        TlasGenerator(
            ID3D12Device5* device,
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE
        );

        void
            AddInstance(ID3D12Resource* bottomLevelAS, 
                const DirectX::XMFLOAT4X4& transform, 
                UINT instanceID,
                UINT hitGroupIndex
            );

        void ComputeASBufferSizes(
            bool allowUpdate,
            UINT64* scratchSizeInBytes,
            UINT64* resultSizeInBytes,
            UINT64* descriptorsSizeInBytes
        );

        void ComputeASBufferSizes(
            bool allowUpdate,
            UINT64* scratchSizeInBytes,
            UINT64* resultSizeInBytes,
            UINT64* descriptorsSizeInBytes,
            UINT numInstances
        );

        void Generate(
            ID3D12GraphicsCommandList4* commandList,
            ID3D12Resource* scratchBuffer,
            ID3D12Resource* resultBuffer,
            ID3D12Resource* descriptorsBuffer,
            bool updateOnly = false,
            ID3D12Resource* previousResult = nullptr
        );

    private:
        struct Instance
        {
            Instance(ID3D12Resource* blAS, const DirectX::XMFLOAT4X4& tr, UINT iID, UINT hgId);
            ID3D12Resource* bottomLevelAS;
            const DirectX::XMFLOAT4X4 transform;
            UINT instanceID;
            UINT hitGroupIndex;
        };

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS m_flags;
        std::vector<Instance> m_instances;

        UINT64 m_scratchSizeInBytes;
        UINT64 m_instanceDescsSizeInBytes;
        UINT64 m_resultSizeInBytes;
        ID3D12Device5* m_pDevice;
    };
}