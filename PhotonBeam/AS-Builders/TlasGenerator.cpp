
#include "TlasGenerator.hpp"
#include <stdexcept>

#ifndef ROUND_UP
#define ROUND_UP(v, powerOf2Alignment) (((v) + (powerOf2Alignment)-1) & ~((powerOf2Alignment)-1))
#endif

namespace ASBuilder
{
	TlasGenerator::TlasGenerator(
		ID3D12Device5* pDevice,
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags
	): m_pDevice(pDevice), 
		m_scratchSizeInBytes(0), 
		m_resultSizeInBytes(0), 
		m_instanceDescsSizeInBytes(0),
		m_flags(flags)
	{
		m_instances = std::vector<Instance>();
	}

    void TlasGenerator::AddInstance(
        ID3D12Resource* bottomLevelAS,
        const DirectX::XMFLOAT4X4& transform,
        UINT instanceID,
        UINT hitGroupIndex
    )
    {
        m_instances.emplace_back(Instance(bottomLevelAS, transform, instanceID, hitGroupIndex));
    }

    void TlasGenerator::ComputeASBufferSizes(
        bool allowUpdate,
        UINT64* scratchSizeInBytes,
        UINT64* resultSizeInBytes,
        UINT64* descriptorsSizeInBytes
    )
    {
        m_flags = allowUpdate ? 
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE 
            : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

     
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildDesc{};
        prebuildDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        prebuildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        prebuildDesc.NumDescs = static_cast<UINT>(m_instances.size());
        prebuildDesc.Flags = m_flags;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
        m_pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildDesc, &info);

        // Buffer sizes need to be 256-byte-aligned
        info.ResultDataMaxSizeInBytes =
            ROUND_UP(info.ResultDataMaxSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        info.ScratchDataSizeInBytes =
            ROUND_UP(info.ScratchDataSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        m_resultSizeInBytes = info.ResultDataMaxSizeInBytes;
        m_scratchSizeInBytes = info.ScratchDataSizeInBytes;
        m_instanceDescsSizeInBytes =
            ROUND_UP(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * static_cast<UINT64>(m_instances.size()),
                D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        *scratchSizeInBytes = m_scratchSizeInBytes;
        *resultSizeInBytes = m_resultSizeInBytes;
        *descriptorsSizeInBytes = m_instanceDescsSizeInBytes;
    }

    void TlasGenerator::ComputeASBufferSizes(
        bool allowUpdate,
        UINT64* scratchSizeInBytes,
        UINT64* resultSizeInBytes,
        UINT64* descriptorsSizeInBytes,
        UINT numInstances
    )
    {

        m_flags = allowUpdate ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE
            : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildDesc{};
        prebuildDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        prebuildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        prebuildDesc.NumDescs = numInstances;
        prebuildDesc.Flags = m_flags;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};

        m_pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildDesc, &info);

        info.ResultDataMaxSizeInBytes =
            ROUND_UP(info.ResultDataMaxSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        info.ScratchDataSizeInBytes =
            ROUND_UP(info.ScratchDataSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        m_resultSizeInBytes = info.ResultDataMaxSizeInBytes;
        m_scratchSizeInBytes = info.ScratchDataSizeInBytes;
        m_instanceDescsSizeInBytes =
            ROUND_UP(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * static_cast<UINT64>(numInstances),
                D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        *scratchSizeInBytes = m_scratchSizeInBytes;
        *resultSizeInBytes = m_resultSizeInBytes;
        *descriptorsSizeInBytes = m_instanceDescsSizeInBytes;
    }

    void TlasGenerator::Generate(
        ID3D12GraphicsCommandList4* commandList,
        ID3D12Resource* scratchBuffer,
        ID3D12Resource* resultBuffer,
        ID3D12Resource* descriptorsBuffer,
        bool updateOnly,
        ID3D12Resource* previousResult
    )
    {
        // Copy the descriptors in the target descriptor buffer
        D3D12_RAYTRACING_INSTANCE_DESC* instanceDescs = nullptr;
        descriptorsBuffer->Map(0, nullptr, reinterpret_cast<void**>(&instanceDescs));
        if (!instanceDescs)
        {
            throw std::logic_error("Cannot map the instance descriptor buffer - is it "
                "in the upload heap?");
        }

        auto instanceCount = static_cast<UINT>(m_instances.size());

        if (!updateOnly)
        {
            ZeroMemory(instanceDescs, m_instanceDescsSizeInBytes);
        }

        for (uint32_t i = 0; i < instanceCount; i++)
        {
            instanceDescs[i].InstanceID = m_instances[i].instanceID;
            instanceDescs[i].InstanceContributionToHitGroupIndex = m_instances[i].hitGroupIndex;
            instanceDescs[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
            memcpy(instanceDescs[i].Transform, &m_instances[i].transform, sizeof(instanceDescs[i].Transform));
            instanceDescs[i].AccelerationStructure = m_instances[i].bottomLevelAS->GetGPUVirtualAddress();
            instanceDescs[i].InstanceMask = 0xFF;
        }

        descriptorsBuffer->Unmap(0, nullptr);

        D3D12_GPU_VIRTUAL_ADDRESS pSourceAS = updateOnly ? previousResult->GetGPUVirtualAddress() : 0;
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags = m_flags;

        if (flags == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE && updateOnly)
        {
            flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
        }

        if (m_flags != D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE && updateOnly)
        {
            throw std::logic_error("Cannot update a top-level AS not originally built for updates");
        }

        if (updateOnly && previousResult == nullptr)
        {
            throw std::logic_error("Top-level hierarchy update requires the previous hierarchy");
        }

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
        buildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        buildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        buildDesc.Inputs.InstanceDescs = descriptorsBuffer->GetGPUVirtualAddress();
        buildDesc.Inputs.NumDescs = instanceCount;
        buildDesc.DestAccelerationStructureData = { resultBuffer->GetGPUVirtualAddress() };
        buildDesc.ScratchAccelerationStructureData = { scratchBuffer->GetGPUVirtualAddress() };
        buildDesc.SourceAccelerationStructureData = pSourceAS;
        buildDesc.Inputs.Flags = flags;

        commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

        D3D12_RESOURCE_BARRIER uavBarrier{};
        uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uavBarrier.UAV.pResource = resultBuffer;
        uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        commandList->ResourceBarrier(1, &uavBarrier);
    }

    TlasGenerator::Instance::Instance(ID3D12Resource* blAS, const DirectX::XMFLOAT4X4& tr, UINT iID,
        UINT hgId)
        : bottomLevelAS(blAS), transform(tr), instanceID(iID), hitGroupIndex(hgId)
    {
    }
}
