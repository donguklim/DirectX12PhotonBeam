#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount)
{
    ThrowIfFailed(device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

    PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
    ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);

    PcRay = std::make_unique<UploadBuffer<PushConstantRay>>(device, 1, true);
    PcBeam = std::make_unique<UploadBuffer<PushConstantBeam>>(device, 1, true);
}

FrameResource::~FrameResource()
{

}