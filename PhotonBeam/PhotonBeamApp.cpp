
#pragma once

#define NOMINMAX

#include "PhotonBeamApp.hpp"
#include <imgui/imgui.h>
#include <imgui/imgui_impl_win32.h>
#include <imgui/imgui_impl_dx12.h>
#include <imgui_helper.h>
#include "raytraceHelper.hpp"

#include "Shaders/RaytracingHlslCompat.h"
#include "AS-Builders/BlasGenerator.h"
#include <microsoft-directx-graphics-samples/DirectXRaytracingHelper.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

const int gNumFrameResources = 3;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


const wchar_t* PhotonBeamApp::c_beamHitGroupNames[] = {L"HitGroup_Surface"};

const wchar_t* PhotonBeamApp::c_rayHitGroupNames[] = {
    L"HitGroup_Beam", 
    L"HitGroup_Surface"
};

const wchar_t* PhotonBeamApp::c_rayShadersExportNames[to_underlying(ERayTracingShaders::Count)] = {
    L"RayGen",
    L"BeamInt",
    L"BeamAnyHit",
    L"SurfaceInt",
    L"SurfaceAnyHit",
    L"Miss"
};

const wchar_t* PhotonBeamApp::c_beamShadersExportNames[to_underlying(EBeamTracingShaders::Count)] = {
    L"BeamGen",
    L"ClosestHit",
    L"Miss"
};

const CD3DX12_STATIC_SAMPLER_DESC& PhotonBeamApp::GetLinearSampler()
{
    static const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
        0, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP
    );

    return linearWrap;
}


XMFLOAT3 getLightMotion(const float totalTime, const DirectX::XMFLOAT3& lightPosition)
{
    return XMFLOAT3(
        XMScalarSin(totalTime * 1.5f) * 1.2f + lightPosition.x,
        XMScalarCos(totalTime * 1.5f) * 1.2f + lightPosition.y,
        XMScalarSin(totalTime) * 0.8f + lightPosition.z
    );
}


PhotonBeamApp::PhotonBeamApp(HINSTANCE hInstance): 
    D3DApp(hInstance),
    m_offScreenOutputResourceUAVDescriptorHeapIndex(UINT_MAX),
    m_beamTracingDescriptorsAllocated(0),
    m_rayTracingDescriptorsAllocated(0),
    m_maxNumSubBeamInfo(
        ((m_maxNumBeamSamples * 48 + m_maxNumPhotonSamples) / SUB_BEAM_INFO_BUFFER_RESET_COMPUTE_SHADER_GROUP_SIZE)
        * SUB_BEAM_INFO_BUFFER_RESET_COMPUTE_SHADER_GROUP_SIZE
    )
{
    mClientWidth = 1400;
    mClientHeight = 800;
    m_pcRay.seed = 231;
    m_pcBeam.seed = 1017;
    m_pcBeam.nextSeedRatio = 0;
    m_pcRay.nextSeedRatio = 0;
    m_seedTime = 0.0f;
    m_prevUpdateTime = 0.0f;

    mLastMousePos = POINT{};
    m_useRayTracer = true;
    m_isBeamMotionOn = true;
    m_isRandomSeedChanging = true;
    m_airScatterCoff = XMVECTORF32{};
    m_airExtinctCoff = XMVECTORF32{};
    m_sourceLight = XMVECTORF32{};

    for (size_t i = 0; i < to_underlying(RootSignatueEnums::BeamTrace::ERootSignatures::Count); i++)
    {
        m_beamRootSignatures[i] = nullptr;
    }

    for (size_t i = 0; i < to_underlying(RootSignatueEnums::RayTrace::ERootSignatures::Count); i++)
    {
        m_rayRootSignatures[i] = nullptr;
    }

    for (size_t i = 0; i < to_underlying(ERayTracingShaders::Count); i++)
    {
        m_rayShaders[i] = nullptr;
    }

    for (size_t i = 0; i < to_underlying(EBeamTracingShaders::Count); i++)
    {
        m_beamShaders[i] = nullptr;
    }

    SetDefaults();

    m_pcBeam.maxNumBeams = m_maxNumBeamData;
    m_pcBeam.maxNumSubBeams = m_maxNumSubBeamInfo;

}

PhotonBeamApp::~PhotonBeamApp()
{
    if (md3dDevice != nullptr)
        FlushCommandQueue();

    if (ImGui::GetCurrentContext() != nullptr)
    {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    m_gltfScene.destroy();
}

bool PhotonBeamApp::Initialize()
{
    if (!D3DApp::Initialize())
        return false;

    CheckRaytracingSupport();

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    mCamera.SetLens(m_camearaFOV / 180 * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    mCamera.LookAt(XMFLOAT3{ 0.0f, 0.0f, 15.0f }, XMFLOAT3{ 0.0f, 0.0f, 0.0f }, XMFLOAT3{ 0.0f, 1.0f, 0.0f });
    mCamera.UpdateViewMatrix();

    LoadScene();
    CreateTextures();
    BuildRasterizeRootSignature();
    BuildPostRootSignature();
    BuildRayTracingRootSignatures();
    BuildShadersAndInputLayout();
    BuildBeamTracingPSOs();
    BuildRayTracingPSOs();
    BuildRayTracingDescriptorHeaps();

    BuildRenderItems();
    BuildFrameResources();
    BuildDescriptorHeaps();

    BuildPSOs();

    CreateSurfaceBlas();
    CreateSurfaceTlas();
    CreateBeamBlases();

    CreateOffScreenOutputResource();

    ComPtr<ID3D12Resource> resetValuploadBuffer = nullptr;
    CreateBeamBuffers(resetValuploadBuffer);
    BuildBeamTracingShaderTables();
    BuildRayTracingShaderTables();

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();

    mGeometries["cornellBox"].get()->DisposeUploaders();
    m_gltfScene.destroy();

    // release scratch buffers for creating accelerated structures;
    m_beamBlasBuffers.pScratch.Reset();
    m_photonBlasBuffers.pScratch.Reset();
    m_surfaceTlasBuffers.pScratch.Reset();
    for (auto& blasBuffers : m_surfaceBlasBuffers)
    {
        blasBuffers.pScratch.Reset();
    }

    return true;
}

void PhotonBeamApp::CheckRaytracingSupport() {
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    ThrowIfFailed(md3dDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)));
    if (options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_1)
        throw std::runtime_error("Raytracing Tier 1_1 not supported on device");
}

void PhotonBeamApp::InitGui()
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = nullptr;  // Avoiding the INI file
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable Docking
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    //ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    ImGuiH::setStyle();
    ImGuiH::setFonts();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(mhMainWnd);

    ImGui_ImplDX12_Init(
        md3dDevice.Get(),
        gNumFrameResources,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        mGuiDescriptorHeap.Get(),
        mGuiDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        mGuiDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
    );


}

void PhotonBeamApp::SerializeAndCreateRootSignature(
    D3D12_ROOT_SIGNATURE_DESC& desc, 
    ID3D12RootSignature** ppRootSignature
)
{
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(),
        errorBlob.GetAddressOf()
    );

    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(
        md3dDevice->CreateRootSignature(
            0,
            serializedRootSig->GetBufferPointer(),
            serializedRootSig->GetBufferSize(),
            IID_PPV_ARGS(ppRootSignature)
        )
    );
}

LRESULT PhotonBeamApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    return D3DApp::MsgProc(hwnd, msg, wParam, lParam);
}

void PhotonBeamApp::OnResize()
{
    D3DApp::OnResize();

    m_offScreenOutput.Reset();
    if(m_offScreenOutputResourceUAVDescriptorHeapIndex < UINT32_MAX)
        CreateOffScreenOutputResource();

    mCamera.SetLens(m_camearaFOV / 180 * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);

}

void PhotonBeamApp::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);

    // Cycle through the circular frame resource array.
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    // Has the GPU finished processing the commands of the current frame resource?
    // If not, wait until the GPU has completed commands up to this fence point.
    if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));

        if (eventHandle != 0)
        {
            WaitForSingleObject(eventHandle, INFINITE);
            CloseHandle(eventHandle);
        }
    }

    UpdateObjectCBs(gt);
    UpdateMainPassCB(gt);
    UpdateRayTracingPushConstants(gt);
}

void PhotonBeamApp::drawPost()
{
    mCommandList->SetPipelineState(mPSOs["post"].Get());

    auto renderTarget = CurrentBackBuffer();

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTarget, 
        D3D12_RESOURCE_STATE_PRESENT, 
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );

    mCommandList->ResourceBarrier(1, &barrier);

    CD3DX12_CLEAR_VALUE clearValue{ DXGI_FORMAT_R32G32B32_FLOAT, m_clearColor };

    D3D12_RENDER_PASS_BEGINNING_ACCESS renderPassBeginningAccessClear{ D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR, { clearValue } };
    static const D3D12_RENDER_PASS_ENDING_ACCESS renderPassEndingAccessPreserve{ D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE, {} };
    D3D12_RENDER_PASS_RENDER_TARGET_DESC renderPassRenderTargetDesc{
        CurrentBackBufferView(),
        renderPassBeginningAccessClear,
        renderPassEndingAccessPreserve
    };

    mCommandList->BeginRenderPass(
        1,
        &renderPassRenderTargetDesc,
        nullptr,
        D3D12_RENDER_PASS_FLAG_NONE
    );
    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    ID3D12DescriptorHeap* descriptorHeaps[] = { m_postSrvDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    mCommandList->SetGraphicsRootSignature(mPostRootSignature.Get());
    mCommandList->SetGraphicsRootDescriptorTable(0, m_postSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

    mCommandList->IASetVertexBuffers(0, 0, nullptr);
    //mCommandList->IASetIndexBuffer(&indexBufferView);
    mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mCommandList->DrawInstanced(3, 1, 0, 0);
    //mCommandList->DrawIndexedInstanced(3, 0, 0);

}

void PhotonBeamApp::Rasterize()
{
    mCommandList->SetPipelineState(mPSOs["raster"].Get());

    CD3DX12_CLEAR_VALUE clearValue{ DXGI_FORMAT_R32G32B32_FLOAT, m_clearColor };

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(m_offScreenRtvHeap->GetCPUDescriptorHandleForHeapStart());

    D3D12_RENDER_PASS_BEGINNING_ACCESS renderPassBeginningAccessClear{ D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR, { clearValue } };
    static const D3D12_RENDER_PASS_ENDING_ACCESS renderPassEndingAccessPreserve{ D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE, {} };
    D3D12_RENDER_PASS_RENDER_TARGET_DESC renderPassRenderTargetDesc{
        rtvHeapHandle,
        renderPassBeginningAccessClear,
        renderPassEndingAccessPreserve
    };

    static const CD3DX12_CLEAR_VALUE depthStencilClearValue{ DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 1.0f, 0 };
    static const D3D12_RENDER_PASS_ENDING_ACCESS renderPassEndingAccessDiscard{ D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD, {} };

    static const D3D12_RENDER_PASS_BEGINNING_ACCESS renderPassBeginningAccessClearDS{
        D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR, {depthStencilClearValue}
    };

    D3D12_RENDER_PASS_DEPTH_STENCIL_DESC renderPassDepthStencilDesc{
        DepthStencilView(),
        renderPassBeginningAccessClearDS,
        renderPassBeginningAccessClearDS,
        renderPassEndingAccessDiscard,
        renderPassEndingAccessDiscard
    };

    mCommandList->BeginRenderPass(
        1,
        &renderPassRenderTargetDesc,
        &renderPassDepthStencilDesc,
        D3D12_RENDER_PASS_FLAG_NONE
    );
    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    auto passCB = mCurrFrameResource->PassCB->Resource();

    mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

    auto& matBuffer = mGeometries["cornellBox"].get()->MaterialBufferGPU;

    mCommandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());
    mCommandList->SetGraphicsRootDescriptorTable(3, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    DrawRenderItems(mCommandList.Get(), mOpaqueRitems);
    mCommandList->EndRenderPass();

}

void PhotonBeamApp::BeamTrace()
{
    // Reset Sub beam info buffer
    {
        mCommandList->SetPipelineState(mPSOs["bufferReset"].Get());
        mCommandList->SetComputeRootSignature(m_bufferResetRootSignature.Get());
        mCommandList->SetComputeRootUnorderedAccessView(0, m_beamAsInstanceDescData->GetGPUVirtualAddress());

        const auto num_groups = m_maxNumSubBeamInfo / 256;
        mCommandList->Dispatch(num_groups, 1, 1);

        auto resourceBarrierRead = CD3DX12_RESOURCE_BARRIER::Transition(
            m_beamAsInstanceDescData.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_GENERIC_READ
        );

        //mCommandList->ResourceBarrier(1, &resourceBarrierRead);
    }

    auto subBeamBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_beamAsInstanceDescData.Get());
    mCommandList->ResourceBarrier(1, &subBeamBarrier);


    // do beam tracing
    {
        using namespace RootSignatueEnums::BeamTrace;

        auto pcBeam = mCurrFrameResource->PcBeam->Resource();
        auto& globalRootSignature = m_beamRootSignatures[to_underlying(ERootSignatures::Global)];

        mCommandList->SetComputeRootSignature(globalRootSignature.Get());
        mCommandList->SetComputeRootConstantBufferView(to_underlying(EGlobalParams::SceneConstantSlot), pcBeam->GetGPUVirtualAddress());
    
        mCommandList->SetDescriptorHeaps(1, m_beamTracingDescriptorHeap.GetAddressOf());

        // Reset beam counter to zero
        {
            mCommandList->CopyBufferRegion(
                m_beamCounter.Get(),
                0,
                m_beamCounterReset.Get(),
                0,
                sizeof(PhotonBeamCounter)
            );

            auto resourceBarrierRead = CD3DX12_RESOURCE_BARRIER::Transition(
                m_beamCounter.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
            );

            mCommandList->ResourceBarrier(1, &resourceBarrierRead);

        }

        D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};

        dispatchDesc.HitGroupTable.StartAddress = m_beamHitGroupShaderTable->GetGPUVirtualAddress();
        dispatchDesc.HitGroupTable.SizeInBytes = m_beamHitGroupShaderTable->GetDesc().Width;
        dispatchDesc.HitGroupTable.StrideInBytes = m_beamHitGroupShaderTableStrideInBytes;
        dispatchDesc.MissShaderTable.StartAddress = m_beamMissShaderTable->GetGPUVirtualAddress();
        dispatchDesc.MissShaderTable.SizeInBytes = m_beamMissShaderTable->GetDesc().Width;
        dispatchDesc.MissShaderTable.StrideInBytes = m_beamMissShaderTableStrideInBytes;
        dispatchDesc.RayGenerationShaderRecord.StartAddress = m_beamGenShaderTable->GetGPUVirtualAddress();
        dispatchDesc.RayGenerationShaderRecord.SizeInBytes = m_beamGenShaderTable->GetDesc().Width;
        dispatchDesc.Width = 4;
        dispatchDesc.Height = 4;
        dispatchDesc.Depth = (m_numBeamSamples > m_numPhotonSamples ? m_numBeamSamples : m_numPhotonSamples) / 16;

        mCommandList->SetPipelineState1(m_beamStateObject.Get());

        mCommandList->DispatchRays(&dispatchDesc);
    }

    auto resourceBarrierRender = CD3DX12_RESOURCE_BARRIER::Transition(
        m_beamAsInstanceDescData.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE 
    );
    mCommandList->ResourceBarrier(1, &resourceBarrierRender);   
    
    {
        // Create a descriptor of the requested builder work, to generate a top-level
        // AS from the input parameters
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
        buildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        buildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        buildDesc.Inputs.InstanceDescs = m_beamAsInstanceDescData->GetGPUVirtualAddress();
        buildDesc.Inputs.NumDescs = m_maxNumSubBeamInfo;
        buildDesc.DestAccelerationStructureData = { m_beamTlasBuffers.pResult->GetGPUVirtualAddress()
        };
        buildDesc.ScratchAccelerationStructureData = { m_beamTlasBuffers.pScratch->GetGPUVirtualAddress()
        };
        buildDesc.SourceAccelerationStructureData = 0;
        buildDesc.Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

        // Build the top-level AS
        mCommandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
    }
    
    auto tlasBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_beamTlasBuffers.pResult.Get());
    mCommandList->ResourceBarrier(1, &tlasBarrier);
}

void PhotonBeamApp::RayTrace()
{
    using namespace RootSignatueEnums::RayTrace;

    auto pcRay = mCurrFrameResource->PcRay->Resource();
    auto& globalRootSignature = m_rayRootSignatures[to_underlying(ERootSignatures::Global)];

    mCommandList->SetComputeRootSignature(globalRootSignature.Get());
    mCommandList->SetComputeRootConstantBufferView(
        to_underlying(EGlobalParams::SceneConstantSlot), 
        pcRay->GetGPUVirtualAddress()
    );

    mCommandList->SetDescriptorHeaps(1, m_rayTracingDescriptorHeap.GetAddressOf());

    D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};

    dispatchDesc.HitGroupTable.StartAddress = m_rayHitGroupShaderTable->GetGPUVirtualAddress();
    dispatchDesc.HitGroupTable.SizeInBytes = m_rayHitGroupShaderTable->GetDesc().Width;
    dispatchDesc.HitGroupTable.StrideInBytes = m_rayHitGroupShaderTableStrideInBytes;
    dispatchDesc.MissShaderTable.StartAddress = m_rayMissShaderTable->GetGPUVirtualAddress();
    dispatchDesc.MissShaderTable.SizeInBytes = m_rayMissShaderTable->GetDesc().Width;
    dispatchDesc.MissShaderTable.StrideInBytes = m_rayMissShaderTableStrideInBytes;
    dispatchDesc.RayGenerationShaderRecord.StartAddress = m_rayGenShaderTable->GetGPUVirtualAddress();
    dispatchDesc.RayGenerationShaderRecord.SizeInBytes = m_rayGenShaderTable->GetDesc().Width;
    dispatchDesc.Width = mClientWidth;
    dispatchDesc.Height = mClientHeight;
    dispatchDesc.Depth = 1;

    mCommandList->SetPipelineState1(m_rayStateObject.Get());

    mCommandList->DispatchRays(&dispatchDesc);
}

void PhotonBeamApp::Draw(const GameTimer& gt)
{
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), nullptr));

    BeamTrace();

    // Start the Dear ImGui frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    RenderUI();
    ImGui::Render();

    if (m_useRayTracer)
    {
        RayTrace();

    }
    else
    {
        auto resourceBarrierRender = CD3DX12_RESOURCE_BARRIER::Transition(
            m_offScreenOutput.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COMMON
        );
        //mCommandList->ResourceBarrier(1, &resourceBarrierRender);

        Rasterize();
    }

    drawPost();

    ID3D12DescriptorHeap* guiDescriptorHeaps[] = { mGuiDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(guiDescriptorHeaps), guiDescriptorHeaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());
    mCommandList->EndRenderPass();

    // Indicate a state transition on the resource usage.
    auto presentBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
    mCommandList->ResourceBarrier(1, &presentBarrier);

    // Done recording commands.
    ThrowIfFailed(mCommandList->Close());

    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Swap the back and front buffers
    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // Advance the fence value to mark commands up to this fence point.
    mCurrFrameResource->Fence = ++mCurrentFence;

    // Add an instruction to the command queue to set a new fence point. 
    // Because we are on the GPU timeline, the new fence point won't be 
    // set until the GPU finishes processing all the commands prior to this Signal().
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void PhotonBeamApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse)
        return;

    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void PhotonBeamApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void PhotonBeamApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse)
        return;

    if ((btnState & MK_LBUTTON) != 0)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(0.1f * static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.1f * static_cast<float>(y - mLastMousePos.y));

        mCamera.Pitch(dy);
        mCamera.RotateY(-dx);
    }
    else if ((btnState & MK_RBUTTON) != 0)
    {
        // Make each pixel correspond to 0.2 unit in the scene.
        float dx = 0.05f * static_cast<float>(x - mLastMousePos.x);
        float dy = 0.05f * static_cast<float>(y - mLastMousePos.y);

        mCamera.Walk(dx - dy);
    }
    else if ((btnState & MK_MBUTTON) != 0)
    {
        float dx = 0.02f * static_cast<float>(x - mLastMousePos.x);
        float dy = 0.02f * static_cast<float>(y - mLastMousePos.y);

        mCamera.Strafe(dx);
        mCamera.Pedestal(dy);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

void PhotonBeamApp::OnMouseWheel(WPARAM btnState, int delta)
{
    if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse)
        return;

    if (delta != 0)
        mCamera.Walk(0.01f * delta);
}

void PhotonBeamApp::OnKeyboardInput(const GameTimer& gt)
{
    // Bellow will cause error 
    //if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureKeyboard)
        //return;

    const float dt = gt.DeltaTime();

    if (GetAsyncKeyState(VK_LSHIFT) & 0x8000 || GetAsyncKeyState(VK_RSHIFT) & 0x8000)
    {
        if (GetAsyncKeyState('W') & 0x8000)
            m_lightPosition.z -= 10.0f * dt;

        if (GetAsyncKeyState('S') & 0x8000)
            m_lightPosition.z += 10.0f * dt;

        if (GetAsyncKeyState('A') & 0x8000)
            m_lightPosition.x += 10.0f * dt;

        if (GetAsyncKeyState('D') & 0x8000)
            m_lightPosition.x -= 10.0f * dt;

        if (GetAsyncKeyState('Q') & 0x8000)
            m_lightPosition.y -= 10.0f * dt;

        if (GetAsyncKeyState('E') & 0x8000)
            m_lightPosition.y += 10.0f * dt;
    }
    else
    {
        if (GetAsyncKeyState('W') & 0x8000)
            mCamera.Walk(10.0f * dt);

        if (GetAsyncKeyState('S') & 0x8000)
            mCamera.Walk(-10.0f * dt);

        if (GetAsyncKeyState('A') & 0x8000)
            mCamera.Strafe(10.0f * dt);

        if (GetAsyncKeyState('D') & 0x8000)
            mCamera.Strafe(-10.0f * dt);

        if (GetAsyncKeyState('Q') & 0x8000)
            mCamera.Pedestal(-10.0f * dt);

        if (GetAsyncKeyState('E') & 0x8000)
            mCamera.Pedestal(10.0f * dt);
    }

    mCamera.UpdateViewMatrix();
}

void PhotonBeamApp::UpdateObjectCBs(const GameTimer& gt)
{
    auto currObjectCB = mCurrFrameResource->ObjectCB.get();
    for (auto& e : mAllRitems)
    {
        // Only update the cbuffer data if the constants have changed.  
        // This needs to be tracked per frame resource.
        if (e->NumFramesDirty > 0)
        {
            XMMATRIX world = XMLoadFloat4x4(&e->World);

            ObjectConstants objConstants;
            XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
            objConstants.materialIndex = e->MaterialIndex;

            currObjectCB->CopyData(e->ObjCBIndex, objConstants);

            // Next FrameResource need to be updated too.
            e->NumFramesDirty--;
        }
    }
}

void PhotonBeamApp::UpdateMainPassCB(const GameTimer& gt)
{
    XMMATRIX view = mCamera.GetView();
    XMMATRIX proj = mCamera.GetProj();

    XMMATRIX viewProj = XMMatrixMultiply(view, proj);

    auto viewDeterminant = XMMatrixDeterminant(view);
    auto projDeterminant = XMMatrixDeterminant(proj);
    auto viewProjDeterminant = XMMatrixDeterminant(viewProj);

    XMMATRIX invView = XMMatrixInverse(&viewDeterminant, view);
    XMMATRIX invProj = XMMatrixInverse(&projDeterminant, proj);
    XMMATRIX invViewProj = XMMatrixInverse(&viewProjDeterminant, viewProj);

    XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
    mMainPassCB.EyePosW = mCamera.GetPosition3f();

    auto totalTime = gt.TotalTime();
    if (m_isBeamMotionOn)
        mMainPassCB.LightPos = getLightMotion(totalTime, m_lightPosition);
    else
        mMainPassCB.LightPos = m_lightPosition;

    mMainPassCB.lightIntensity = m_lightIntensity;
    mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
    mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
    mMainPassCB.NearZ = mCamera.GetNearZ();
    mMainPassCB.FarZ = mCamera.GetFarZ();
    mMainPassCB.TotalTime = totalTime;
    mMainPassCB.DeltaTime = gt.DeltaTime();

    auto currPassCB = mCurrFrameResource->PassCB.get();
    currPassCB->CopyData(0, mMainPassCB);
}

void PhotonBeamApp::UpdateRayTracingPushConstants(const GameTimer& gt)
{
    XMMATRIX view = mCamera.GetView();
    XMMATRIX proj = mCamera.GetProj();

    XMMATRIX viewProj = XMMatrixMultiply(view, proj);

    auto viewDeterminant = XMMatrixDeterminant(view);
    auto projDeterminant = XMMatrixDeterminant(proj);
    auto viewProjDeterminant = XMMatrixDeterminant(viewProj);

    XMMATRIX invView = XMMatrixInverse(&viewDeterminant, view);
    XMMATRIX invProj = XMMatrixInverse(&projDeterminant, proj);

    XMStoreFloat4x4(&m_pcRay.viewInverse, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&m_pcRay.projInverse, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&m_pcRay.viewProj, XMMatrixTranspose(viewProj));

    m_pcRay.clearColor = XMFLOAT4(m_clearColor[0], m_clearColor[1], m_clearColor[2], 1.0);
    m_pcRay.airScatterCoff = XMFLOAT3(m_airScatterCoff[0], m_airScatterCoff[1], m_airScatterCoff[2]);
    m_pcRay.airExtinctCoff = XMFLOAT3(m_airExtinctCoff[0], m_airExtinctCoff[1], m_airExtinctCoff[2]);
    m_pcRay.airHGAssymFactor = m_hgAssymFactor;
    m_pcRay.photonRadius = m_photonRadius;
    m_pcRay.beamRadius = m_beamRadius;
    m_pcRay.numBeamSources = m_usePhotonBeam ? m_numBeamSamples : 0;
    m_pcRay.numPhotonSources = m_usePhotonMapping ? m_numPhotonSamples : 0;
    m_pcRay.showDirectColor = m_showDirectColor ? 1 : 0;
    
    
    auto totalTime = gt.TotalTime();

    if (m_isRandomSeedChanging)
        m_seedTime += totalTime - m_prevUpdateTime;

    m_prevUpdateTime = totalTime;

    if (m_seedTime < 0)
        m_seedTime = 0;

    if (m_seedTime > m_seedUPdateInterval)
    {
        //m_seedTime = std::fmodf(m_seedTime, m_seedUPdateInterval);
        //m_pcRay.seed++;
        //m_pcBeam.seed++;
    }

    m_pcBeam.nextSeedRatio = 0;
    m_pcRay.nextSeedRatio = 0;

    if(m_isBeamMotionOn)
        m_pcBeam.lightPosition = getLightMotion(totalTime, m_lightPosition);
    else
        m_pcBeam.lightPosition = m_lightPosition;

    m_pcBeam.airExtinctCoff = m_pcRay.airScatterCoff;
    m_pcBeam.airExtinctCoff = m_pcRay.airExtinctCoff;
    m_pcBeam.airHGAssymFactor = m_hgAssymFactor;
    m_pcBeam.beamRadius = m_beamRadius;
    m_pcBeam.photonRadius = m_photonRadius;
    m_pcBeam.sourceLight = XMFLOAT3(m_sourceLight[0], m_sourceLight[1], m_sourceLight[2]);
    m_pcBeam.numBeamSources = m_usePhotonBeam ? m_numBeamSamples : 0;
    m_pcBeam.numPhotonSources = m_usePhotonMapping ? m_numPhotonSamples : 0;
    m_pcBeam.maxNumBeams = m_maxNumBeamData;
    m_pcBeam.maxNumSubBeams = m_maxNumSubBeamInfo;

    

    // Bellow sets scatter and extinct cofficients and source light power, 
  // given the distance from the light source, 
  // the color near the light source
  // the color a unit distance away from the light soruce,
  // and the value of scatter/extinct cofficent
  // the method is based on the following article 
  // A Programmable System for Artistic Volumetric Lighting(2011) Derek Nowrouzezahrai
    const float minimumUnitDistantAlbedo = 0.1f;

    XMVECTOR beamNearColor = m_beamNearColor;
    XMVECTOR beamUnitDistantColor = m_beamUnitDistantColor;
    beamNearColor *= m_beamNearColor[3];
    beamUnitDistantColor *= m_beamUnitDistantColor[3];

    XMVECTOR unitDistantMinColor = beamNearColor * minimumUnitDistantAlbedo;
    beamUnitDistantColor = XMVectorClamp(beamUnitDistantColor, unitDistantMinColor, beamNearColor);

    XMFLOAT3 beamColor;
    XMStoreFloat3(&beamColor, beamUnitDistantColor);

    m_beamUnitDistantColor = XMVECTORF32{beamColor.x, beamColor.y, beamColor.z, 1.0};

    const static XMFLOAT3 oneVal = XMFLOAT3(1, 1, 1);
    const static XMFLOAT3 zeroVal = XMFLOAT3(0, 0, 0);

    XMVECTOR unitDistantAlbedoInverse = beamNearColor / beamUnitDistantColor;

    auto notZero = XMVectorNotEqual(XMLoadFloat3(&zeroVal), beamUnitDistantColor);

    // if there is division by zero, substitute to value one
    unitDistantAlbedoInverse = XMVectorSelect(XMLoadFloat3(&oneVal), unitDistantAlbedoInverse, notZero);

    float beamSourceDist = 15.0f;  //use fixed distance between eye and camera

    auto extinctCoff = XMVectorLogE(unitDistantAlbedoInverse);
    XMStoreFloat3(&m_pcRay.airExtinctCoff, extinctCoff);
    m_pcBeam.airExtinctCoff = m_pcRay.airExtinctCoff;
    m_airExtinctCoff = XMVECTORF32{ m_pcRay.airExtinctCoff.x, m_pcRay.airExtinctCoff.y, m_pcRay.airExtinctCoff.z };

    auto scatterCoff = m_airAlbedo * extinctCoff;
    XMStoreFloat3(&m_pcRay.airScatterCoff, scatterCoff);
    m_pcBeam.airScatterCoff = m_pcRay.airScatterCoff;
    m_airScatterCoff = XMVECTORF32{ m_pcRay.airScatterCoff.x, m_pcRay.airScatterCoff.y, m_pcRay.airScatterCoff.z };

    auto beamSourceDistVec = XMFLOAT3(beamSourceDist, beamSourceDist, beamSourceDist);
    auto lightPower = beamNearColor * XMVectorPow(unitDistantAlbedoInverse, XMLoadFloat3(&beamSourceDistVec)) ;

    const static XMFLOAT3 minVal = XMFLOAT3(0.00001f, 0.00001f, 0.00001f);
    auto greater = XMVectorGreater(extinctCoff, XMLoadFloat3(&minVal));

    lightPower = XMVectorSelect(beamNearColor, lightPower/ scatterCoff, greater) * m_beamIntensity;

    XMStoreFloat3(&m_pcBeam.sourceLight, lightPower);
    m_sourceLight = XMVECTORF32{ m_pcBeam.sourceLight.x, m_pcBeam.sourceLight.y, m_pcBeam.sourceLight.z };


    auto currPcRay = mCurrFrameResource->PcRay.get();
    currPcRay->CopyData(0, m_pcRay);

    auto currPcBeam = mCurrFrameResource->PcBeam.get();
    currPcBeam->CopyData(0, m_pcBeam);

}

void PhotonBeamApp::BuildDescriptorHeaps()
{
    {
        D3D12_DESCRIPTOR_HEAP_DESC guiHeapDesc = {};
        guiHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        guiHeapDesc.NumDescriptors = 1;
        guiHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&guiHeapDesc,
            IID_PPV_ARGS(&mGuiDescriptorHeap)));
    }
    
    {
        D3D12_DESCRIPTOR_HEAP_DESC offScreenRtvHeapDesc{};
        offScreenRtvHeapDesc.NumDescriptors = 1;
        offScreenRtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        offScreenRtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        offScreenRtvHeapDesc.NodeMask = 0;
        ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
            &offScreenRtvHeapDesc, IID_PPV_ARGS(m_offScreenRtvHeap.GetAddressOf())));
    }

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    // descriptor heaps for gltf textures and offscreen output as texture
    srvHeapDesc.NumDescriptors = static_cast<UINT>(m_textures.size()) + 1;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    srvHeapDesc.NumDescriptors = 1;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_postSrvDescriptorHeap)));

    srvHeapDesc.NumDescriptors = static_cast<UINT>(m_textures.size());
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());


    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    for (auto& textureResource : m_textures)
    {
        srvDesc.Format = textureResource->Resource->GetDesc().Format;
        srvDesc.Texture2D.MipLevels = textureResource->Resource->GetDesc().MipLevels;
        md3dDevice->CreateShaderResourceView(textureResource->Resource.Get(), &srvDesc, hDescriptor);
        hDescriptor.Offset(1, mCbvSrvDescriptorSize);
    }
}

void PhotonBeamApp::BuildRayTracingDescriptorHeaps()
{
    // beam tracing
    {
        D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
        // Allocate a heap for 3 + 6 + (number of textures) descriptors:
        // 3 - beam data, sub beam AS instance info, beam counters
        // 6 - indice buffer, vertex buffer, normal buffer, text coordinate buffer, material buffer, mesh buffer
        // number of textures
        descriptorHeapDesc.NumDescriptors =  9 + static_cast<UINT>(m_textures.size());
        descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        descriptorHeapDesc.NodeMask = 0;

        md3dDevice->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_beamTracingDescriptorHeap));
        NAME_D3D12_OBJECT(m_beamTracingDescriptorHeap);

        // set geometry related data
        {
            auto geo = mGeometries["cornellBox"].get();

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Buffer.FirstElement = 0;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            srvDesc.Buffer.NumElements = geo->VertexBufferByteSize / geo->VertexByteStride;
            srvDesc.Buffer.StructureByteStride = geo->VertexByteStride;

            D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle;
            auto vertexHeapIndex = AllocateBeamTracingDescriptor(&descriptorHandle);

            md3dDevice->CreateShaderResourceView(geo->VertexBufferGPU.Get(), &srvDesc, descriptorHandle);

            m_beamTracingVertexDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
                m_beamTracingDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
                vertexHeapIndex,
                mCbvSrvUavDescriptorSize
            );

            uint32_t allocatedIndex{};

            srvDesc.Buffer.NumElements = geo->NormalBufferByteSize / geo->NormalByteStride;
            srvDesc.Buffer.StructureByteStride = geo->NormalByteStride;
            allocatedIndex = AllocateBeamTracingDescriptor(&descriptorHandle);
            ThrowIfFalse(allocatedIndex == vertexHeapIndex + 1);
            md3dDevice->CreateShaderResourceView(geo->NormalBufferGPU.Get(), &srvDesc, descriptorHandle);

            srvDesc.Buffer.NumElements = geo->UvBufferByteSize / geo->UvByteStride;
            srvDesc.Buffer.StructureByteStride = geo->UvByteStride;
            allocatedIndex = AllocateBeamTracingDescriptor(&descriptorHandle);
            ThrowIfFalse(allocatedIndex == vertexHeapIndex + 2);
            md3dDevice->CreateShaderResourceView(geo->UvBufferGPU.Get(), &srvDesc, descriptorHandle);

            srvDesc.Buffer.NumElements = geo->IndexBufferByteSize / sizeof(uint32_t);
            srvDesc.Buffer.StructureByteStride = sizeof(uint32_t);
            allocatedIndex = AllocateBeamTracingDescriptor(&descriptorHandle);
            ThrowIfFalse(allocatedIndex == vertexHeapIndex + 3);
            md3dDevice->CreateShaderResourceView(geo->IndexBufferGPU.Get(), &srvDesc, descriptorHandle);

            srvDesc.Buffer.NumElements = geo->MaterialBufferByteSize / geo->MaterialByteStride;
            srvDesc.Buffer.StructureByteStride = geo->MaterialByteStride;
            allocatedIndex =AllocateBeamTracingDescriptor(&descriptorHandle);
            ThrowIfFalse(allocatedIndex == vertexHeapIndex + 4);
            md3dDevice->CreateShaderResourceView(geo->MaterialBufferGPU.Get(), &srvDesc, descriptorHandle);

            srvDesc.Buffer.NumElements = geo->MeshBufferByteSize / geo->MeshByteStride;
            srvDesc.Buffer.StructureByteStride = geo->MeshByteStride;
            allocatedIndex = AllocateBeamTracingDescriptor(&descriptorHandle);
            ThrowIfFalse(allocatedIndex == vertexHeapIndex + 5);
            md3dDevice->CreateShaderResourceView(geo->MeshBufferGPU.Get(), &srvDesc, descriptorHandle);
        }

        // set texture views to heap
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

            uint32_t textureHeapIndex{};
            // make descriptor handle for the first texure
            {
                auto& textureResource = m_textures[0];
                D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle;
                textureHeapIndex = AllocateBeamTracingDescriptor(&uavDescriptorHandle);

                srvDesc.Format = textureResource->Resource->GetDesc().Format;
                srvDesc.Texture2D.MipLevels = textureResource->Resource->GetDesc().MipLevels;
                md3dDevice->CreateShaderResourceView(textureResource->Resource.Get(), &srvDesc, uavDescriptorHandle);

                m_beamTracingTextureDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
                    m_beamTracingDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
                    textureHeapIndex,
                    mCbvSrvUavDescriptorSize
                );
            }

            // set left textures to the heap
            for (uint32_t i = 1; i < m_textures.size(); i++)
            {
                auto& textureResource = m_textures[i];
                D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle;
                auto allocatedIndex = AllocateBeamTracingDescriptor(&uavDescriptorHandle);
                
                ThrowIfFalse(textureHeapIndex + i == allocatedIndex);

                srvDesc.Format = textureResource->Resource->GetDesc().Format;
                srvDesc.Texture2D.MipLevels = textureResource->Resource->GetDesc().MipLevels;
                md3dDevice->CreateShaderResourceView(textureResource->Resource.Get(), &srvDesc, uavDescriptorHandle);
            }
        }
    }


    // ray tracing
    {
        D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
        // Allocate a heap for  7 + (number of textures) descriptors:
        // 7 - indice buffer, normal buffer, text coordinate buffer, material buffer, mesh buffer, raytracing output bubffer, beam data buffer
        descriptorHeapDesc.NumDescriptors = 7 + static_cast<UINT>(m_textures.size());
        descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        descriptorHeapDesc.NodeMask = 0;

        md3dDevice->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_rayTracingDescriptorHeap));
        NAME_D3D12_OBJECT(m_rayTracingDescriptorHeap);

        // set geometry related data
        {
            auto geo = mGeometries["cornellBox"].get();

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Buffer.FirstElement = 0;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            srvDesc.Buffer.NumElements = geo->NormalBufferByteSize / geo->NormalByteStride;
            srvDesc.Buffer.StructureByteStride = geo->NormalByteStride;

            D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle;
            auto normalHeapIndex = AllocateRayTracingDescriptor(&descriptorHandle);

            md3dDevice->CreateShaderResourceView(geo->NormalBufferGPU.Get(), &srvDesc, descriptorHandle);

            m_rayTracingNormalDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
                m_rayTracingDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
                normalHeapIndex,
                mCbvSrvUavDescriptorSize
            );

            uint32_t allocatedIndex{};

            srvDesc.Buffer.NumElements = geo->UvBufferByteSize / geo->UvByteStride;
            srvDesc.Buffer.StructureByteStride = geo->UvByteStride;
            allocatedIndex = AllocateRayTracingDescriptor(&descriptorHandle);
            ThrowIfFalse(allocatedIndex == normalHeapIndex + 1);
            md3dDevice->CreateShaderResourceView(geo->UvBufferGPU.Get(), &srvDesc, descriptorHandle);

            srvDesc.Buffer.NumElements = geo->IndexBufferByteSize / sizeof(uint32_t);
            srvDesc.Buffer.StructureByteStride = sizeof(uint32_t);
            allocatedIndex = AllocateRayTracingDescriptor(&descriptorHandle);
            ThrowIfFalse(allocatedIndex == normalHeapIndex + 2);
            md3dDevice->CreateShaderResourceView(geo->IndexBufferGPU.Get(), &srvDesc, descriptorHandle);

            srvDesc.Buffer.NumElements = geo->MaterialBufferByteSize / geo->MaterialByteStride;
            srvDesc.Buffer.StructureByteStride = geo->MaterialByteStride;
            allocatedIndex = AllocateRayTracingDescriptor(&descriptorHandle);
            ThrowIfFalse(allocatedIndex == normalHeapIndex + 3);
            md3dDevice->CreateShaderResourceView(geo->MaterialBufferGPU.Get(), &srvDesc, descriptorHandle);

            srvDesc.Buffer.NumElements = geo->MeshBufferByteSize / geo->MeshByteStride;
            srvDesc.Buffer.StructureByteStride = geo->MeshByteStride;
            allocatedIndex = AllocateRayTracingDescriptor(&descriptorHandle);
            ThrowIfFalse(allocatedIndex == normalHeapIndex + 4);
            md3dDevice->CreateShaderResourceView(geo->MeshBufferGPU.Get(), &srvDesc, descriptorHandle);
        }

        // set texture views to heap
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

            uint32_t textureHeapIndex{};

            // make descriptor handle for the first texure
            {
                auto& textureResource = m_textures[0];
                D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle;
                textureHeapIndex = AllocateRayTracingDescriptor(&uavDescriptorHandle);

                srvDesc.Format = textureResource->Resource->GetDesc().Format;
                srvDesc.Texture2D.MipLevels = textureResource->Resource->GetDesc().MipLevels;
                md3dDevice->CreateShaderResourceView(textureResource->Resource.Get(), &srvDesc, uavDescriptorHandle);

                m_rayTracingTextureDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
                    m_rayTracingDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
                    textureHeapIndex,
                    mCbvSrvUavDescriptorSize
                );
            }

            // set left textures to the heap
            for (uint32_t i = 1; i < m_textures.size(); i++)
            {
                auto& textureResource = m_textures[i];
                D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle;
                auto allocatedIndex = AllocateBeamTracingDescriptor(&uavDescriptorHandle);

                ThrowIfFalse(allocatedIndex == textureHeapIndex + i);

                srvDesc.Format = textureResource->Resource->GetDesc().Format;
                srvDesc.Texture2D.MipLevels = textureResource->Resource->GetDesc().MipLevels;
                md3dDevice->CreateShaderResourceView(textureResource->Resource.Get(), &srvDesc, uavDescriptorHandle);
            }
        }

    }

}

void PhotonBeamApp::BuildRasterizeRootSignature()
{    
    CD3DX12_DESCRIPTOR_RANGE texTable{};
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MAX_SHADER_MATERIAL_TEXTURES, 0, 0);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[4] = {};

    slotRootParameter[0].InitAsConstantBufferView(0);
    slotRootParameter[1].InitAsConstantBufferView(1);
    slotRootParameter[2].InitAsShaderResourceView(0, 1);
    slotRootParameter[3].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);

    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
        4, 
        slotRootParameter, 
        1, 
        &GetLinearSampler(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

    SerializeAndCreateRootSignature(rootSigDesc, mRootSignature.GetAddressOf());
}

void PhotonBeamApp::BuildRayTracingRootSignatures()
{
    // sub beam info buffer root signature
    {
        CD3DX12_ROOT_PARAMETER rootParameters[1] = {};
        rootParameters[0].InitAsUnorderedAccessView(0);

        CD3DX12_ROOT_SIGNATURE_DESC desc(1, rootParameters, 1, &GetLinearSampler());
        SerializeAndCreateRootSignature(
            desc,
            m_bufferResetRootSignature.GetAddressOf()
        );
    }

    // Global Root Signature
    // This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
    {
        using namespace RootSignatueEnums::BeamTrace;

        // Beam trace global 
        {
            CD3DX12_ROOT_PARAMETER rootParameters[to_underlying(EGlobalParams::Count)] = {};
            
            rootParameters[to_underlying(EGlobalParams::SceneConstantSlot)].InitAsConstantBufferView(0);
            
            CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(rootParameters), rootParameters);
            SerializeAndCreateRootSignature(
                desc,
                m_beamRootSignatures[to_underlying(ERootSignatures::Global)].GetAddressOf()
            );
        }

        // Beam trace generation
        {
            CD3DX12_ROOT_PARAMETER rootParameters[to_underlying(EGenParams::Count)] = {};
            CD3DX12_DESCRIPTOR_RANGE rwBufferRange{};
            rwBufferRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 3, 0);

            rootParameters[to_underlying(EGenParams::SurfaceASSlot)].InitAsShaderResourceView(0);
            rootParameters[to_underlying(EGenParams::RWBufferSlot)].InitAsDescriptorTable(1, &rwBufferRange);
            
            CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(rootParameters), rootParameters);
            desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
            SerializeAndCreateRootSignature(
                desc,
                m_beamRootSignatures[to_underlying(ERootSignatures::Gen)].GetAddressOf()
            );
        }

        // Beam trace closest hit
        {
            CD3DX12_ROOT_PARAMETER rootParameters[to_underlying(ECloseHitParams::Count)] = {};

            CD3DX12_DESCRIPTOR_RANGE buffersRange{}, textureMapsRange{};
            buffersRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6, 0);
            textureMapsRange.Init(
                D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                static_cast<uint32_t>(m_textures.size()),
                0,
                1
            );

            rootParameters[to_underlying(ECloseHitParams::ReadBuffersSlot)].InitAsDescriptorTable(
                1,
                &buffersRange
            );
            rootParameters[to_underlying(ECloseHitParams::TextureMapsSlot)].InitAsDescriptorTable(
                1, 
                &textureMapsRange
            );

            CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(rootParameters), rootParameters, 1, &GetLinearSampler());
            desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
            SerializeAndCreateRootSignature(
                desc,
                m_beamRootSignatures[to_underlying(ERootSignatures::CloseHit)].GetAddressOf()
            );
        }
        
    }

    {
        using namespace RootSignatueEnums::RayTrace;

        // Ray trace global 
        {
            CD3DX12_ROOT_PARAMETER rootParameters[to_underlying(EGlobalParams::Count)] = {};

            rootParameters[to_underlying(EGlobalParams::SceneConstantSlot)].InitAsConstantBufferView(0);

            CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(rootParameters), rootParameters);
            SerializeAndCreateRootSignature(
                desc,
                m_rayRootSignatures[to_underlying(ERootSignatures::Global)].GetAddressOf()
            );
        }

        // Ray trace generation
        {
            CD3DX12_ROOT_PARAMETER rootParameters[to_underlying(EGenParams::Count)] = {};

            CD3DX12_DESCRIPTOR_RANGE outputImageRange{}, buffersRange{}, textureMapsRange{};

            outputImageRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
            buffersRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 2);
            textureMapsRange.Init(
                D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                static_cast<uint32_t>(m_textures.size()),
                0,
                1
            );

            rootParameters[to_underlying(EGenParams::OutputViewSlot)].InitAsDescriptorTable(1, &outputImageRange);
            rootParameters[to_underlying(EGenParams::BeamASSlot)].InitAsShaderResourceView(0);
            rootParameters[to_underlying(EGenParams::SurfaceASSlot)].InitAsShaderResourceView(1);
            rootParameters[to_underlying(EGenParams::ReadBuffersSlot)].InitAsDescriptorTable(1,&buffersRange);
            rootParameters[to_underlying(EGenParams::TextureMapsSlot)].InitAsDescriptorTable(1,&textureMapsRange);

            CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(rootParameters), rootParameters, 1, &GetLinearSampler());
            desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
            SerializeAndCreateRootSignature(
                desc,
                m_rayRootSignatures[to_underlying(ERootSignatures::Gen)].GetAddressOf()
            );
        }

        // Ray trace any hit nad intersection
        {
            CD3DX12_ROOT_PARAMETER rootParameters[to_underlying(EAnyHitAndIntParams::Count)] = {};

            CD3DX12_DESCRIPTOR_RANGE beamBufferRange{};
            beamBufferRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

            rootParameters[to_underlying(EAnyHitAndIntParams::BeamBufferSlot)].InitAsDescriptorTable(1, &beamBufferRange);

            CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(rootParameters), rootParameters);
            desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
            SerializeAndCreateRootSignature(
                desc,
                m_rayRootSignatures[to_underlying(ERootSignatures::AnyHitAndInt)].GetAddressOf()
            );
        }

    }
}

void PhotonBeamApp::BuildPostRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE texTable0{};
    texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[1] = {};

    slotRootParameter[0].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);

    const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
        0, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
        1,
        slotRootParameter,
        1, 
        &pointWrap,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mPostRootSignature.GetAddressOf())));
}

void PhotonBeamApp::BuildShadersAndInputLayout()
{

    m_rasterizeShaders["standardVS"] = raytrace_helper::CompileShaderLibrary(L"Shaders\\Rasterization.hlsl", L"vs_6_6", L"VS");
    m_rasterizeShaders["opaquePS"] = raytrace_helper::CompileShaderLibrary(L"Shaders\\Rasterization.hlsl", L"ps_6_6", L"PS");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    m_rasterizeShaders["postVS"] = raytrace_helper::CompileShaderLibrary(L"Shaders\\PostColor.hlsl", L"vs_6_6", L"VS");
    m_rasterizeShaders["postPS"] = raytrace_helper::CompileShaderLibrary(L"Shaders\\PostColor.hlsl", L"ps_6_6", L"PS");

    m_AsInstanceBufferResetShader = raytrace_helper::CompileShaderLibrary(L"Shaders\\BeamTracing\\ResetSubBeamInfoBuffer.hlsl", L"cs_6_6", L"main");

    m_beamShaders[to_underlying(EBeamTracingShaders::Miss)] = raytrace_helper::CompileShaderLibrary(L"Shaders\\BeamTracing\\BeamMiss.hlsl", L"lib_6_6");
    m_beamShaders[to_underlying(EBeamTracingShaders::CloseHit)] = raytrace_helper::CompileShaderLibrary(L"Shaders\\BeamTracing\\BeamClosestHit.hlsl", L"lib_6_6");
    m_beamShaders[to_underlying(EBeamTracingShaders::Gen)] = raytrace_helper::CompileShaderLibrary(L"Shaders\\BeamTracing\\BeamGen.hlsl", L"lib_6_6");

    m_rayShaders[to_underlying(ERayTracingShaders::Miss)] = raytrace_helper::CompileShaderLibrary(L"Shaders\\RayTracing\\RayMiss.hlsl", L"lib_6_6");
    m_rayShaders[to_underlying(ERayTracingShaders::BeamAnyHit)] = raytrace_helper::CompileShaderLibrary(L"Shaders\\RayTracing\\RayBeamAnyHit.hlsl", L"lib_6_6");
    m_rayShaders[to_underlying(ERayTracingShaders::BeamInt)] = raytrace_helper::CompileShaderLibrary(L"Shaders\\RayTracing\\RayBeamInt.hlsl", L"lib_6_6");
    m_rayShaders[to_underlying(ERayTracingShaders::SurfaceAnyHit)] = raytrace_helper::CompileShaderLibrary(L"Shaders\\RayTracing\\RaySurfaceAnyHit.hlsl", L"lib_6_6");
    m_rayShaders[to_underlying(ERayTracingShaders::SurfaceInt)] = raytrace_helper::CompileShaderLibrary(L"Shaders\\RayTracing\\RaySurfaceInt.hlsl", L"lib_6_6");
    m_rayShaders[to_underlying(ERayTracingShaders::Gen)] = raytrace_helper::CompileShaderLibrary(L"Shaders\\RayTracing\\RayGen.hlsl", L"lib_6_6");
}

void PhotonBeamApp::LoadScene()
{
    auto Filename = L"media\\cornellBox.gltf";

    m_gltfScene.LoadFile("./media/cornellBox.gltf");

    auto& vertexPositions = m_gltfScene.GetVertexPositions();
    auto& vertexNormals = m_gltfScene.GetVertexNormals();
    auto& vertexUVs = m_gltfScene.GetVertextexcoords0();
    auto& indices = m_gltfScene.GetVertexIndices();

    auto& materials = m_gltfScene.GetMaterials();
    std::vector<GltfShadeMaterial> shadeMaterials;

    for (const auto& m : materials)
    {
        shadeMaterials.emplace_back(
            GltfShadeMaterial{
                m.baseColorFactor,
                m.emissiveFactor,
                m.baseColorTexture,
                m.metallicFactor,
                m.roughnessFactor
            }
        );
    }

    auto& meshes = m_gltfScene.GetPrimMeshes();
    std::vector<PrimMeshInfo> shaderMeshes;
    
    for (const auto& m : meshes)
    {
        shaderMeshes.emplace_back(
            PrimMeshInfo{
                m.firstIndex,
                m.vertexOffset,
                m.materialIndex
            }
        );
    }

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "cornellBox";

    const UINT vbByteSize = (UINT)vertexPositions.size() * sizeof(XMFLOAT3);
    const UINT nbByteSize = (UINT)vertexNormals.size() * sizeof(XMFLOAT3);
    const UINT ubByteSize = (UINT)vertexUVs.size() * sizeof(XMFLOAT2);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);
    const UINT mbByteSize = (UINT)shadeMaterials.size() * sizeof(GltfShadeMaterial);
    const UINT meshBufferByteSize = (UINT)shaderMeshes.size() * sizeof(PrimMeshInfo);

    // upload to cpu is not necessary now
    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertexPositions.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(nbByteSize, &geo->NormalBufferCPU));
    CopyMemory(geo->NormalBufferCPU->GetBufferPointer(), vertexNormals.data(), nbByteSize);

    ThrowIfFailed(D3DCreateBlob(ubByteSize, &geo->UvBufferCPU));
    CopyMemory(geo->UvBufferCPU->GetBufferPointer(), vertexUVs.data(), ubByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);


    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertexPositions.data(), vbByteSize, geo->VertexBufferUploader);

    geo->NormalBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertexNormals.data(), nbByteSize, geo->NormalBufferUploader);

    geo->UvBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertexUVs.data(), ubByteSize, geo->UvBufferUploader);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->MaterialBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), shadeMaterials.data(), mbByteSize, geo->MaterialBufferUploader);

    geo->MeshBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), shaderMeshes.data(), meshBufferByteSize, geo->MeshBufferUploader);

    geo->VertexByteStride = sizeof(XMFLOAT3);
    geo->VertexBufferByteSize = vbByteSize;
    geo->NormalByteStride = sizeof(XMFLOAT3);
    geo->NormalBufferByteSize = nbByteSize;
    geo->UvByteStride = sizeof(XMFLOAT2);
    geo->UvBufferByteSize = ubByteSize;
    geo->IndexFormat = DXGI_FORMAT_R32_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    geo->MaterialByteStride = sizeof(GltfShadeMaterial);
    geo->MaterialBufferByteSize = mbByteSize;

    geo->MeshByteStride = sizeof(PrimMeshInfo);
    geo->MeshBufferByteSize = meshBufferByteSize;

    mGeometries[geo->Name] = std::move(geo);

}

void PhotonBeamApp::CreateTextures()
{
    const static std::array<uint8_t, 4> whiteTexture = { 225, 255, 255, 255 };
    const auto& textureImages = m_gltfScene.GetTextureImages();
    
    size_t numTextures = textureImages.size();
    if (textureImages.empty())
        numTextures = 1;

    if (numTextures > MAX_SHADER_MATERIAL_TEXTURES)
    { 
        numTextures = MAX_SHADER_MATERIAL_TEXTURES;
        MessageBox(
            nullptr, 
            L"Number of texture exeeds the max number of texturs allowed. Modify source code and shader code to increase the maximum", 
            L"Texture Loading Incomplete", MB_OK
        );
    }

    m_textures.reserve(numTextures);
    for (size_t i = 0; i < numTextures; i++)
    {
        auto texture = std::make_unique<Texture>();
        
        const void* imageData = whiteTexture.data();
        uint64_t imageWidth = 1;
        uint32_t imageHeight = 1;

        if (!textureImages.empty() && textureImages[i].image.size() != 0 && textureImages[i].width > 0 && textureImages[i].height > 0)
        {
            auto& gltfImage = textureImages[i];
            imageData = gltfImage.image.data();
            imageWidth = gltfImage.width;
            imageHeight = gltfImage.height;
        }

        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.MipLevels = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.Width = imageWidth;
        textureDesc.Height = imageHeight;
        textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

        auto defaultHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

        ThrowIfFailed(
            md3dDevice->CreateCommittedResource(
                &defaultHeapProp,
                D3D12_HEAP_FLAG_NONE,
                &textureDesc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(texture->Resource.GetAddressOf())
            )
        );

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(
            texture->Resource.Get(), 0, 1
        );

        auto uploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bufferResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

        // Create the GPU upload buffer.
        ThrowIfFailed(
            md3dDevice->CreateCommittedResource(
                &uploadHeapProp,
                D3D12_HEAP_FLAG_NONE,
                &bufferResourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(texture->UploadHeap.GetAddressOf())
            )
        );


        D3D12_SUBRESOURCE_DATA textureData{};
        textureData.pData = imageData;
        textureData.RowPitch = imageWidth * 4;
        textureData.SlicePitch = textureData.RowPitch * imageHeight;

        UpdateSubresources(
            mCommandList.Get(), 
            texture->Resource.Get(), 
            texture->UploadHeap.Get(), 
            0, 
            0, 
            1, 
            &textureData
        );

        auto transition = CD3DX12_RESOURCE_BARRIER::Transition(
            texture->Resource.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, 
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );

        mCommandList->ResourceBarrier(
            1,
            &transition
        );

        m_textures.push_back(std::move(texture));

    }
    
}

void PhotonBeamApp::BuildRenderItems()
{
    auto& primMeshes = m_gltfScene.GetPrimMeshes();
    UINT objCBIndex = 0;

    for (auto& node : m_gltfScene.GetNodes())
    {
        auto rItem = std::make_unique<RenderItem>();
        auto& primitive = primMeshes[node.primMesh];

        rItem->ObjCBIndex = objCBIndex++;
        rItem->World = node.worldMatrix;
        rItem->MaterialIndex = primitive.materialIndex;
        rItem->Geo = mGeometries["cornellBox"].get();
        rItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        rItem->IndexCount = primitive.indexCount;
        rItem->StartIndexLocation = primitive.firstIndex;
        rItem->BaseVertexLocation = primitive.vertexOffset;
        mAllRitems.push_back(std::move(rItem));
    }

    // All the render items are opaque.
    for (auto& e : mAllRitems)
        mOpaqueRitems.push_back(e.get());
}

void PhotonBeamApp::BuildBeamTracingPSOs()
{
    CD3DX12_STATE_OBJECT_DESC beamTracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };


    for (size_t i = 0; i < to_underlying(EBeamTracingShaders::Count); i++)
    {
        auto& shaderBlob = m_beamShaders[i];
        auto lib = beamTracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
        D3D12_SHADER_BYTECODE libdxil{
            reinterpret_cast<BYTE*>(shaderBlob->GetBufferPointer()),
            shaderBlob->GetBufferSize()
        };
        lib->SetDXILLibrary(&libdxil);
    }

    {
        auto hitGroup = beamTracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
        hitGroup->SetClosestHitShaderImport(c_beamShadersExportNames[to_underlying(EBeamTracingShaders::CloseHit)]);
        hitGroup->SetHitGroupExport(c_beamHitGroupNames[0]);
        hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
    }

    auto shaderConfig = beamTracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    UINT payloadSize = sizeof(BeamHitPayload);
    UINT attributeSize = sizeof(BeamHitAttributes);
    shaderConfig->Config(payloadSize, attributeSize);

    {
        using namespace RootSignatueEnums::BeamTrace;

        // globla root signature
        {
            auto globalRootSignature = beamTracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
            globalRootSignature->SetRootSignature(
                m_beamRootSignatures[to_underlying(ERootSignatures::Global)].Get()
            );
        }

        // beam generation root signature
        {
            auto localRootSignature = beamTracingPipeline.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
            localRootSignature->SetRootSignature(
                m_beamRootSignatures[to_underlying(ERootSignatures::Gen)].Get()
            );
            // Shader association
            auto rootSignatureAssociation = beamTracingPipeline.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
            rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
            rootSignatureAssociation->AddExport(c_beamShadersExportNames[to_underlying(EBeamTracingShaders::Gen)]);
        }

        //beam closest hit root signatue
        {
            auto localRootSignature = beamTracingPipeline.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
            localRootSignature->SetRootSignature(
                m_beamRootSignatures[to_underlying(ERootSignatures::CloseHit)].Get()
            );
            // Shader association
            auto rootSignatureAssociation = beamTracingPipeline.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
            rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
            rootSignatureAssociation->AddExports(c_beamHitGroupNames);
        }
        
    }

    auto pipelineConfig = beamTracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();

    pipelineConfig->Config(1);

    PrintStateObjectDesc(beamTracingPipeline);

    ThrowIfFailed(
        md3dDevice->CreateStateObject(
            beamTracingPipeline, 
            IID_PPV_ARGS(m_beamStateObject.GetAddressOf())
        )
    );
}

void PhotonBeamApp::BuildRayTracingPSOs()
{
    // create buffer reset PSO
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC bufferResetPsoDesc = {};
        bufferResetPsoDesc.pRootSignature = m_bufferResetRootSignature.Get();
        bufferResetPsoDesc.CS = {
            reinterpret_cast<BYTE*>(m_AsInstanceBufferResetShader->GetBufferPointer()),
            m_AsInstanceBufferResetShader->GetBufferSize()
        };
        bufferResetPsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        ThrowIfFailed(md3dDevice->CreateComputePipelineState(&bufferResetPsoDesc, IID_PPV_ARGS(mPSOs["bufferReset"].GetAddressOf())));
    }



    CD3DX12_STATE_OBJECT_DESC rayTracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

    for (size_t i = 0; i < to_underlying(ERayTracingShaders::Count); i++)
    {
        auto& shaderBlob = m_rayShaders[i];
        auto lib = rayTracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
        D3D12_SHADER_BYTECODE libdxil{
            reinterpret_cast<BYTE*>(shaderBlob->GetBufferPointer()),
            shaderBlob->GetBufferSize()
        };
        lib->SetDXILLibrary(&libdxil);
    }

    // Beam hit group
    {
        auto hitGroup = rayTracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();

        //hitGroup->SetClosestHitShaderImport(nullptr);
        hitGroup->SetIntersectionShaderImport(c_rayShadersExportNames[to_underlying(ERayTracingShaders::BeamInt)]);
        hitGroup->SetAnyHitShaderImport(c_rayShadersExportNames[to_underlying(ERayTracingShaders::BeamAnyHit)]);
        hitGroup->SetHitGroupExport(c_rayHitGroupNames[to_underlying(ERayHitTypes::Beam)]);
        hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE);
    }

    // Surface hit group
    {
        auto hitGroup = rayTracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();

        hitGroup->SetClosestHitShaderImport(nullptr);
        hitGroup->SetIntersectionShaderImport(c_rayShadersExportNames[to_underlying(ERayTracingShaders::SurfaceInt)]);
        hitGroup->SetAnyHitShaderImport(c_rayShadersExportNames[to_underlying(ERayTracingShaders::SurfaceAnyHit)]);
        hitGroup->SetHitGroupExport(c_rayHitGroupNames[to_underlying(ERayHitTypes::Surface)]);
        hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE);
    }


    auto shaderConfig = rayTracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    UINT payloadSize = sizeof(RayHitPayload);
    UINT attributeSize = sizeof(RayHitAttributes);
    shaderConfig->Config(payloadSize, attributeSize);


    {
        using namespace RootSignatueEnums::RayTrace;

        // globla root signature
        {
            auto globalRootSignature = rayTracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
            globalRootSignature->SetRootSignature(
                m_rayRootSignatures[to_underlying(ERootSignatures::Global)].Get()
            );
        }

        // ray generation root signature
        {
            auto localRootSignature = rayTracingPipeline.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
            localRootSignature->SetRootSignature(
                m_rayRootSignatures[to_underlying(ERootSignatures::Gen)].Get()
            );
            // Shader association
            auto rootSignatureAssociation = rayTracingPipeline.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
            rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
            rootSignatureAssociation->AddExport(c_rayShadersExportNames[to_underlying(EBeamTracingShaders::Gen)]);
        }

        //ray beam any hit and intersection root signatue
        {
            auto localRootSignature = rayTracingPipeline.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
            localRootSignature->SetRootSignature(
                m_rayRootSignatures[to_underlying(ERootSignatures::AnyHitAndInt)].Get()
            );
            // Shader association
            auto rootSignatureAssociation = rayTracingPipeline.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
            rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
            rootSignatureAssociation->AddExports(c_rayHitGroupNames);
        }

    }

    auto pipelineConfig = rayTracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();

    pipelineConfig->Config(1);

    PrintStateObjectDesc(rayTracingPipeline);

    ThrowIfFailed(
        md3dDevice->CreateStateObject(
            rayTracingPipeline,
            IID_PPV_ARGS(m_rayStateObject.GetAddressOf())
        )
    );
}

void PhotonBeamApp::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

    // PSO for opaque objects.
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    opaquePsoDesc.pRootSignature = mRootSignature.Get();
    opaquePsoDesc.VS = {
        reinterpret_cast<BYTE*>(m_rasterizeShaders["standardVS"]->GetBufferPointer()),
        m_rasterizeShaders["standardVS"]->GetBufferSize()
    };
    opaquePsoDesc.PS = {
        reinterpret_cast<BYTE*>(m_rasterizeShaders["opaquePS"]->GetBufferPointer()),
        m_rasterizeShaders["opaquePS"]->GetBufferSize()
    };
    opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    opaquePsoDesc.SampleMask = UINT_MAX;
    opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    opaquePsoDesc.NumRenderTargets = 1;
    opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
    opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    opaquePsoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(mPSOs["raster"].GetAddressOf())));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC postPsoDesc;
    ZeroMemory(&postPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    postPsoDesc.InputLayout = { nullptr, 0 };
    postPsoDesc.pRootSignature = mPostRootSignature.Get();
    postPsoDesc.VS = {
        reinterpret_cast<BYTE*>(m_rasterizeShaders["postVS"]->GetBufferPointer()),
        m_rasterizeShaders["postVS"]->GetBufferSize()
    };
    postPsoDesc.PS = {
        reinterpret_cast<BYTE*>(m_rasterizeShaders["postPS"]->GetBufferPointer()),
        m_rasterizeShaders["postPS"]->GetBufferSize()
    };
    postPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    postPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    postPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    postPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    postPsoDesc.DepthStencilState.DepthEnable = false;
    postPsoDesc.SampleMask = UINT_MAX;
    postPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    postPsoDesc.NumRenderTargets = 1;
    postPsoDesc.RTVFormats[0] = mBackBufferFormat;
    postPsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    postPsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&postPsoDesc, IID_PPV_ARGS(mPSOs["post"].GetAddressOf())));
}

void PhotonBeamApp::BuildFrameResources()
{
    for (int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, (UINT)mAllRitems.size()));
    }
}

void PhotonBeamApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

    auto objectCB = mCurrFrameResource->ObjectCB->Resource();

    // For each render item...
    for (size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];

        auto vertexBufferView = ri->Geo->VertexBufferView();
        auto indexBufferView = ri->Geo->IndexBufferView();

        D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[3] = {
            ri->Geo->VertexBufferView(),
            ri->Geo->NormalBufferView(),
            ri->Geo->UvBufferView()
        };

        cmdList->IASetVertexBuffers(0, 3, vertexBufferViews);
        cmdList->IASetIndexBuffer(&indexBufferView);
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + static_cast<UINT64>(ri->ObjCBIndex) * objCBByteSize;

        cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);

    }
}

void PhotonBeamApp::SetDefaults()
{
    const XMVECTORF32 defaultBeamNearColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    const XMVECTORF32 defaultBeamUnitDistantColor{ 0.895f, 0.966f, 0.966f, 1.0f };

    m_clearColor = Colors::LightSteelBlue;
    m_beamNearColor = defaultBeamNearColor;
    m_beamUnitDistantColor = defaultBeamUnitDistantColor;
    m_beamRadius = 0.8f;
    m_photonRadius = 1.2f;
    m_beamIntensity = 15.0f;
    m_usePhotonMapping = true;
    m_usePhotonBeam = true;
    m_hgAssymFactor = 0.0f;
    m_showDirectColor = false;
    m_airAlbedo = 0.07f;

    m_numBeamSamples = 1600;
    m_numPhotonSamples = 2 * 4 * 2048;


    m_lightPosition = XMFLOAT3{ 0.0f, 0.0f, 0.0f };
    m_lightIntensity = 3.0f;

    m_camearaFOV = 60.0f;
    m_prevCameraFOV = m_camearaFOV;
    m_isBeamMotionOn = true;

    m_pcRay.seed = 231;
    m_pcBeam.seed = 1017;
    m_isRandomSeedChanging = true;
    m_seedUPdateInterval = 50.0f;

}

// Extra UI
void PhotonBeamApp::RenderUI()
{
    const uint32_t minValBeam = 1;
    const uint32_t maxValBeam = m_maxNumBeamSamples;
    const uint32_t minValPhoton = 4 * 4;
    const uint32_t maxValPhoton = m_maxNumPhotonSamples;

    ImGuiH::Panel::Begin();

    auto cameraFloat = mCamera.GetPosition3f();
    ImGui::InputFloat3("Camera Position", &cameraFloat.x, "%.5f", ImGuiItemFlags_Disabled);
    ImGui::SliderFloat("FOV", &m_camearaFOV, 1.0f, 179.f);

    if (m_camearaFOV != m_prevCameraFOV)
    {
        mCamera.SetLens(m_camearaFOV / 180 * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
        m_prevCameraFOV = m_camearaFOV;
    }

    ImGui::ColorEdit3("clear color", (float*)&m_clearColor); // Edit 3 floats representing a color

    ImGui::Checkbox("Ray Tracer mode", &m_useRayTracer);  // Switch between raster and ray tracing


    do
    {
        bool isCollapsed = ImGui::CollapsingHeader("Light");
        if (isCollapsed)
            break;

        ImGui::SliderFloat3("Position", (float*)&m_lightPosition, -20.f, 20.f);

        ImGuiH::Control::Info(
            "",
            "",
            "Use W,A,S,D key and Q,E key to adjust Camera/Light position. Pressing Shift key adjusts light position.",
            ImGuiH::Control::Flags::Disabled
        );

        if (!m_useRayTracer)
        {
            ImGui::SliderFloat("Intensity", &m_lightIntensity, 0.f, 20.f);
            break;
        }

        ImGui::ColorEdit3("Near Color", reinterpret_cast<float*>(&m_beamNearColor), ImGuiColorEditFlags_NoSmallPreview);
        ImGuiH::Control::Color(
            std::string("Near Color"), "Air color near the light source, seen at the eye position",
            reinterpret_cast<float*>(&(m_beamNearColor))
        );

        ImGui::ColorEdit3("Distant Color", reinterpret_cast<float*>(&m_beamUnitDistantColor), ImGuiColorEditFlags_NoSmallPreview);
        ImGuiH::Control::Color(
            std::string("Distant Color"),
            "Air color one unit distance away from the light source, at direction orthogonal from the "
            "line between eye and the light source, seen at eye position.\n"
            "Each color channel will be adjusted to fit between 0.1% to 100% of the value in the same "
            "channel of Near Color\n",
            reinterpret_cast<float*>(&(m_beamUnitDistantColor))
        );

        ImGui::SliderFloat("Air Albedo", &m_airAlbedo, 0.0f, 1.0f);

        //ImGui::SliderFloat("Color Intensity", &helloVk.m_beamColorIntensity, 1.f, 150.f);
        //ImGui::SliderFloat("Beam Radius", &helloVk.m_beamRadius, 0.05f, 5.0f);
        //ImGui::SliderFloat("Surface Photon Radius", &helloVk.m_photonRadius, 0.05f, 5.0f);
        //ImGui::SliderFloat("HG Assymetric Factor", &helloVk.m_hgAssymFactor, -0.99f, 0.99f);

        ImGui::SliderFloat("Light Intensity", &m_beamIntensity, 0.0f, 15.f);

        ImGui::Checkbox("Light Variation On", &m_isRandomSeedChanging);

        ImGuiH::Control::Slider(
            std::string("Light Variation Interval"), "How long does it takes light to changes",
            &m_seedUPdateInterval,
            nullptr,
            ImGuiH::Control::Flags::Normal,
            1.0f, 100.0f
        );

        ImGuiH::Control::Custom(
            "Air Scatter",
            "Light Scattering Coffiecient in Air",
            [&] { return ImGui::InputFloat3("##Eye", (float*)&m_airScatterCoff, "%.5f"); },
            ImGuiH::Control::Flags::Disabled
        );

        ImGuiH::Control::Custom(
            "Air Extinction",
            "Light Extinction Coffiecient in Air",
            [&] { return ImGui::InputFloat3("##Eye", (float*)&m_airExtinctCoff, "%.5f"); },
            ImGuiH::Control::Flags::Disabled
        );

        ImGuiH::Control::Custom(
            "Light Power",
            "Source Light Power",
            [&] { return ImGui::InputFloat3("##Eye", (float*)&m_sourceLight, "%.5f"); },
            ImGuiH::Control::Flags::Disabled
        );

        ImGuiH::Control::Slider(
            std::string("Beam Radius"), "Sampling radius for beams",
            &m_beamRadius,
            nullptr,
            ImGuiH::Control::Flags::Normal,
            0.05f, 5.0f
        );

        ImGuiH::Control::Slider(
            std::string("Photon Radius"),  // Name of the parameter
            "Sampling radius for surface photons",
            &m_photonRadius,
            nullptr,
            ImGuiH::Control::Flags::Normal,
            0.05f, 5.0f
        );

        ImGuiH::Control::Slider(
            std::string("HG Assymetric Factor"),  // Name of the parameter
            "Henyey and Greenstein Assymetric Factor for air.\n"
            "Positive: more front light scattering.\n"
            "Negative: more back light scattering.",
            &m_hgAssymFactor,
            nullptr,
            ImGuiH::Control::Flags::Normal,
            -0.99f, 0.99f
        );

        ImGui::Checkbox("Surface Photon", &m_usePhotonMapping);
        ImGui::Checkbox("Photon Beam", &m_usePhotonBeam);
        ImGui::Checkbox("Show Solid Beam/Surface Color", &m_showDirectColor);

        ImGui::SliderScalar(
            "Sample Beams",
            ImGuiDataType_U32, &m_numBeamSamples,
            &minValBeam,
            &maxValBeam,
            nullptr,
            ImGuiSliderFlags_None
        );
        ImGui::SliderScalar(
            "Sample Photons",
            ImGuiDataType_U32,
            &m_numPhotonSamples,
            &minValPhoton,
            &maxValPhoton,
            nullptr,
            ImGuiSliderFlags_None
        );

        ImGui::Checkbox("Light Motion", &m_isBeamMotionOn);  // Switch between raster and ray tracing

    } while (false);

    if (ImGui::SmallButton("Set Defaults"))
        SetDefaults();


    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGuiH::Panel::End();
}

void PhotonBeamApp::CreateSurfaceBlas()
{
    // Adding all vertex buffers and not transforming their position. 

    auto geo = mGeometries["cornellBox"].get();
    auto vertexBufferView = geo->VertexBufferView();
    auto indexbufferView = geo->IndexBufferView();
    const auto& primMeshes = m_gltfScene.GetPrimMeshes();

    m_surfaceBlasBuffers.clear();
    m_surfaceBlasBuffers.reserve(primMeshes.size());
    for (size_t i = 0; i < primMeshes.size(); i++)
    {
        m_surfaceBlasBuffers.emplace_back(nullptr, nullptr, nullptr );
        const auto& mesh = primMeshes[i];
        ASBuilder::BottomLevelASGenerator generator{};

        generator.AddVertexBuffer(
            geo->VertexBufferGPU.Get(),
            static_cast<UINT64>(mesh.vertexOffset) * vertexBufferView.StrideInBytes,
            mesh.vertexCount,
            vertexBufferView.StrideInBytes,
            geo->IndexBufferGPU.Get(),
            mesh.firstIndex * sizeof(UINT32),
            mesh.indexCount,
            nullptr,
            0
        );

        UINT64 scratchSizeInBytes = 0;
        UINT64 resultSizeInBytes = 0;
        generator.ComputeASBufferSizes(md3dDevice.Get(), false, &scratchSizeInBytes, &resultSizeInBytes);

        m_surfaceBlasBuffers[i].pScratch = raytrace_helper::CreateBuffer(
            md3dDevice.Get(),
            scratchSizeInBytes,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COMMON,
            raytrace_helper::pmDefaultHeapProps
        );
        m_surfaceBlasBuffers[i].pResult = raytrace_helper::CreateBuffer(
            md3dDevice.Get(),
            resultSizeInBytes,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
            raytrace_helper::pmDefaultHeapProps
        );

        generator.Generate(
            mCommandList.Get(),
            m_surfaceBlasBuffers[i].pScratch.Get(),
            m_surfaceBlasBuffers[i].pResult.Get(),
            false,
            nullptr
        );
    }
}

void PhotonBeamApp::CreateSurfaceTlas()
{
    auto identity = MathHelper::Identity4x4();
    for (auto& node : m_gltfScene.GetNodes())
    {
        XMFLOAT4X4 worldMat{};
        XMStoreFloat4x4(&worldMat, XMMatrixTranspose(XMLoadFloat4x4(&node.worldMatrix)));

        auto blasAddress = m_surfaceBlasBuffers[node.primMesh].pResult.Get();
        m_topLevelASGenerator.AddInstance(
            blasAddress,
            worldMat,
            node.primMesh,
            0
        );
    }

    UINT64 scratchSize, resultSize, instanceDescsSize;
    m_topLevelASGenerator.ComputeASBufferSizes(
        md3dDevice.Get(),
        true,
        &scratchSize,
        &resultSize,
        &instanceDescsSize
    );

    m_surfaceTlasBuffers.pScratch = raytrace_helper::CreateBuffer(
        md3dDevice.Get(),
        scratchSize,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COMMON,
        raytrace_helper::pmDefaultHeapProps
    );

    m_surfaceTlasBuffers.pResult = raytrace_helper::CreateBuffer(
        md3dDevice.Get(),
        resultSize,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        raytrace_helper::pmDefaultHeapProps
    );

    m_surfaceTlasBuffers.pInstanceDesc = raytrace_helper::CreateBuffer(
        md3dDevice.Get(),
        instanceDescsSize,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        raytrace_helper::pmUploadHeapProps
    );


    m_topLevelASGenerator.Generate(
        mCommandList.Get(),
        m_surfaceTlasBuffers.pScratch.Get(),
        m_surfaceTlasBuffers.pResult.Get(),
        m_surfaceTlasBuffers.pInstanceDesc.Get()
    );
}

void PhotonBeamApp::CreateBeamBlases()
{
    static const D3D12_RAYTRACING_AABB beamPhotonBoxes[] = {
        { -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 2.0f }, // beam box 
        { -1.0f, -0.1f, -1.0, 1.0f, 0.1f, 1.0f }, // photon box
    };
    static ComPtr<ID3D12Resource> boxUploadBuffer = nullptr;
    static const ComPtr<ID3D12Resource> boxBuffer = d3dUtil::CreateDefaultBuffer(
        md3dDevice.Get(),
        mCommandList.Get(), 
        beamPhotonBoxes, 2 * sizeof(D3D12_RAYTRACING_AABB), 
        boxUploadBuffer
    );

    {
        ASBuilder::BottomLevelASGenerator beamBoxGenerator{};

        beamBoxGenerator.AddAabbBuffer(boxBuffer.Get(), 0, 1);

        UINT64 scratchSizeInBytes = 0;
        UINT64 resultSizeInBytes = 0;
        beamBoxGenerator.ComputeASBufferSizes(md3dDevice.Get(), false, &scratchSizeInBytes, &resultSizeInBytes);

        m_beamBlasBuffers.pScratch = raytrace_helper::CreateBuffer(
            md3dDevice.Get(),
            scratchSizeInBytes,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COMMON,
            raytrace_helper::pmDefaultHeapProps
        );
        m_beamBlasBuffers.pResult = raytrace_helper::CreateBuffer(
            md3dDevice.Get(),
            resultSizeInBytes,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
            raytrace_helper::pmDefaultHeapProps
        );

        beamBoxGenerator.Generate(
            mCommandList.Get(),
            m_beamBlasBuffers.pScratch.Get(),
            m_beamBlasBuffers.pResult.Get(),
            false,
            nullptr
        );
    }

    {
        ASBuilder::BottomLevelASGenerator photonBoxGenerator{};

        photonBoxGenerator.AddAabbBuffer(boxBuffer.Get(), sizeof(D3D12_RAYTRACING_AABB), 1);

        UINT64 scratchSizeInBytes = 0;
        UINT64 resultSizeInBytes = 0;
        photonBoxGenerator.ComputeASBufferSizes(md3dDevice.Get(), false, &scratchSizeInBytes, &resultSizeInBytes);

        m_photonBlasBuffers.pScratch = raytrace_helper::CreateBuffer(
            md3dDevice.Get(),
            scratchSizeInBytes,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COMMON,
            raytrace_helper::pmDefaultHeapProps
        );
        m_photonBlasBuffers.pResult = raytrace_helper::CreateBuffer(
            md3dDevice.Get(),
            resultSizeInBytes,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
            raytrace_helper::pmDefaultHeapProps
        );

        photonBoxGenerator.Generate(
            mCommandList.Get(),
            m_photonBlasBuffers.pScratch.Get(),
            m_photonBlasBuffers.pResult.Get(),
            false,
            nullptr
        );
    }
    
    m_pcBeam.beamBlasAddress = m_beamBlasBuffers.pResult->GetGPUVirtualAddress();
    m_pcBeam.photonBlasAddress = m_photonBlasBuffers.pResult->GetGPUVirtualAddress();
}

uint32_t PhotonBeamApp::AllocateRayTracingDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse)
{
    auto descriptorHeapCpuBase = m_rayTracingDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    if (descriptorIndexToUse >= m_rayTracingDescriptorHeap->GetDesc().NumDescriptors)
    {
        ThrowIfFalse(
            m_rayTracingDescriptorsAllocated < m_rayTracingDescriptorHeap->GetDesc().NumDescriptors 
        );
        descriptorIndexToUse = m_rayTracingDescriptorsAllocated++;
    }
    *cpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeapCpuBase, descriptorIndexToUse, mCbvSrvDescriptorSize);
    return descriptorIndexToUse;
}

uint32_t PhotonBeamApp::AllocateBeamTracingDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse)
{
    auto descriptorHeapCpuBase = m_beamTracingDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    if (descriptorIndexToUse >= m_beamTracingDescriptorHeap->GetDesc().NumDescriptors)
    {
        ThrowIfFalse(
            m_beamTracingDescriptorsAllocated < m_beamTracingDescriptorHeap->GetDesc().NumDescriptors
        );
        descriptorIndexToUse = m_beamTracingDescriptorsAllocated++;
    }
    *cpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeapCpuBase, descriptorIndexToUse, mCbvSrvDescriptorSize);
    return descriptorIndexToUse;
}

void PhotonBeamApp::CreateOffScreenOutputResource()
{
    auto backbufferFormat = mBackBufferFormat;

    // Create the output resource. The dimensions and format should match the swap-chain.
    auto uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        mBackBufferFormat, 
        mClientWidth, 
        mClientHeight, 
        1, 
        1, 
        m4xMsaaState ? 4 : 1,
        m4xMsaaState ? (m4xMsaaQuality - 1) : 0,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
    );

    auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(
        md3dDevice->CreateCommittedResource(
            &defaultHeapProperties, 
            D3D12_HEAP_FLAG_NONE, 
            &uavDesc, 
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            nullptr, 
            IID_PPV_ARGS(&m_offScreenOutput)
        )
    );
    NAME_D3D12_OBJECT(m_offScreenOutput);

    D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle;
    m_offScreenOutputResourceUAVDescriptorHeapIndex = AllocateRayTracingDescriptor(
        &uavDescriptorHandle, 
        m_offScreenOutputResourceUAVDescriptorHeapIndex
    );
    D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
    UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    md3dDevice->CreateUnorderedAccessView(
        m_offScreenOutput.Get(), 
        nullptr, 
        &UAVDesc, 
        uavDescriptorHandle
    );
    m_offScreenOutputDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        m_rayTracingDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), 
        m_offScreenOutputResourceUAVDescriptorHeapIndex, 
        mCbvSrvUavDescriptorSize
    );

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(m_offScreenRtvHeap->GetCPUDescriptorHandleForHeapStart());
    md3dDevice->CreateRenderTargetView(m_offScreenOutput.Get(), nullptr, rtvHeapHandle);

    // create off screen output texture view
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    srvDesc.Format = m_offScreenOutput->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = 1;

    CD3DX12_CPU_DESCRIPTOR_HANDLE srvDescriptorHandle(m_postSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    md3dDevice->CreateShaderResourceView(m_offScreenOutput.Get(), &srvDesc, srvDescriptorHandle);
}

void PhotonBeamApp::CreateBeamBuffers(Microsoft::WRL::ComPtr<ID3D12Resource>& resetValuploadBuffer)
{
    static const PhotonBeamCounter counterResetVal = { 0, 0 };

    // Buffer for reading beam counter
    {
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(
            sizeof(PhotonBeamCounter),
            D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE
        );

        auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);

    }

    //upload buffer  for resetting beam counter
    {
        m_beamCounterReset = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
            mCommandList.Get(), &counterResetVal, sizeof(PhotonBeamCounter), resetValuploadBuffer);
    }

    uint32_t photonBeamHeapIndex{};
    // Buffer for beam data
    {
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(
            sizeof(PhotonBeam) * m_maxNumBeamData, 
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
        );

        auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

        // below statement, and other CreateCommittedResource statement for RWStructuredBuffer creation  give warnings, but I don't know how to solve.
        // A similar statement in Frank Lua's sample code also gives the same warning.
        // Bellow is the actual warning from the staement.
        /*
            D3D12 WARNING: ID3D12Device::CreateCommittedResource: Ignoring InitialState D3D12_RESOURCE_STATE_UNORDERED_ACCESS. 
            Buffers are effectively created in state D3D12_RESOURCE_STATE_COMMON. 
            [ STATE_CREATION WARNING #1328: CREATERESOURCE_STATE_IGNORED]
        */
        ThrowIfFailed(
            md3dDevice->CreateCommittedResource(
                &defaultHeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(&m_beamData)
            )
        );
        NAME_D3D12_OBJECT(m_beamData);

        // set descriptor handle for beam tracing
        D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc{};
        UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
        UAVDesc.Buffer.CounterOffsetInBytes = 0;
        UAVDesc.Buffer.NumElements = m_maxNumBeamData;
        UAVDesc.Buffer.StructureByteStride = sizeof(PhotonBeam);
        UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

        D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle;
        photonBeamHeapIndex = AllocateBeamTracingDescriptor(&uavDescriptorHandle);

        md3dDevice->CreateUnorderedAccessView(
            m_beamData.Get(),
            nullptr,
            &UAVDesc,
            uavDescriptorHandle
        );

        m_beamTracingBeamDataDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
            m_beamTracingDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
            photonBeamHeapIndex,
            mCbvSrvUavDescriptorSize
        );

        // set descriptor handle for ray tracing
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        srvDesc.Buffer.NumElements = m_maxNumBeamData;
        srvDesc.Buffer.StructureByteStride = sizeof(PhotonBeam);

        D3D12_CPU_DESCRIPTOR_HANDLE srvDescriptorHandle;
        uint32_t rayTracingHeapIndex = AllocateRayTracingDescriptor(&srvDescriptorHandle);

        md3dDevice->CreateShaderResourceView(
            m_beamData.Get(),
            &srvDesc,
            srvDescriptorHandle
        );

        m_rayTracingBeamDataDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
            m_rayTracingDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
            rayTracingHeapIndex,
            mCbvSrvUavDescriptorSize
        );
    }

    //Buffer for storing sub beam Accelerated Structure instance info
    {
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(
            sizeof(ShaderRayTracingTopASInstanceDesc) * m_maxNumSubBeamInfo,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
        );

        auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(
            md3dDevice->CreateCommittedResource(
                &defaultHeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                nullptr,
                IID_PPV_ARGS(m_beamAsInstanceDescData.GetAddressOf())
            )
        );
        NAME_D3D12_OBJECT(m_beamAsInstanceDescData);

        D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
        UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
        UAVDesc.Buffer.CounterOffsetInBytes = 0;
        UAVDesc.Buffer.NumElements = m_maxNumSubBeamInfo;
        UAVDesc.Buffer.StructureByteStride = sizeof(ShaderRayTracingTopASInstanceDesc);
        UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

        D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle;
        auto allocatedIndex = AllocateBeamTracingDescriptor(&uavDescriptorHandle);

        ThrowIfFalse(allocatedIndex ==  photonBeamHeapIndex + 1);

        md3dDevice->CreateUnorderedAccessView(
            m_beamAsInstanceDescData.Get(),
            nullptr,
            &UAVDesc,
            uavDescriptorHandle
        );
    }

    // Create beam counter Buffer
    {
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(
            sizeof(PhotonBeamCounter),
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
        );

        auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(
            md3dDevice->CreateCommittedResource(
                &defaultHeapProperties,
                D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS,
                &bufferDesc,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS | D3D12_RESOURCE_STATE_COPY_SOURCE,
                nullptr,
                IID_PPV_ARGS(&m_beamCounter)
            )
        );
        NAME_D3D12_OBJECT(m_beamCounter);

        D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
        UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
        UAVDesc.Buffer.CounterOffsetInBytes = 0;
        UAVDesc.Buffer.NumElements = 1;
        UAVDesc.Buffer.StructureByteStride = sizeof(PhotonBeamCounter);
        UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

        D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle;
        auto allocatedIndex = AllocateBeamTracingDescriptor(&uavDescriptorHandle);

        // allocated index must be photonBeamHeapIndex + 2
        ThrowIfFalse(photonBeamHeapIndex + 2 == allocatedIndex);

        md3dDevice->CreateUnorderedAccessView(
            m_beamCounter.Get(),
            nullptr,
            &UAVDesc,
            uavDescriptorHandle
        );
    }

    // Create scratch buffer for beam TLAS
    {
        nv_helpers_dx12::TopLevelASGenerator generator;

        UINT64 scratchSize, resultSize, instanceDescsSize;
        generator.ComputeASBufferSizes(
            md3dDevice.Get(),
            true,
            &scratchSize,
            &resultSize,
            &instanceDescsSize,
            m_maxNumSubBeamInfo
        );

        m_beamTlasBuffers.pScratch = raytrace_helper::CreateBuffer(
            md3dDevice.Get(),
            scratchSize,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COMMON,
            raytrace_helper::pmDefaultHeapProps
        );
        m_beamTlasBuffers.pResult = raytrace_helper::CreateBuffer(
            md3dDevice.Get(),
            resultSize,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
            raytrace_helper::pmDefaultHeapProps
        );
    }
}

void PhotonBeamApp::BuildBeamTracingShaderTables()
{
    void* beamGenShaderID;
    void* missShaderID;
    void* hitGroupShaderID;

    // A shader name look-up table for shader table debug print out.
    std::unordered_map<void*, std::wstring> shaderIdToStringMap;

    UINT shaderIDSize;
    {
        ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
        ThrowIfFailed(m_beamStateObject.As(&stateObjectProperties));
        
        auto& beamGenShaderName = c_beamShadersExportNames[to_underlying(EBeamTracingShaders::Gen)];
        beamGenShaderID = stateObjectProperties->GetShaderIdentifier(beamGenShaderName);
        shaderIdToStringMap[beamGenShaderID] = beamGenShaderName;

        auto& hitGroupName = c_beamHitGroupNames[to_underlying(EBeamHitTypes::Surface)];
        hitGroupShaderID = stateObjectProperties->GetShaderIdentifier(hitGroupName);
        shaderIdToStringMap[hitGroupShaderID] = hitGroupName;

        auto& missShaderName = c_beamShadersExportNames[to_underlying(EBeamTracingShaders::Miss)];
        missShaderID = stateObjectProperties->GetShaderIdentifier(missShaderName);
        shaderIdToStringMap[missShaderID] = missShaderName;

        shaderIDSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    }
   
    // BeamGen shader table.
    {
        struct {
            D3D12_GPU_VIRTUAL_ADDRESS topLevelAsAddress;
            D3D12_GPU_DESCRIPTOR_HANDLE beamDataDescriptorTable;
        } rootArgs{};

        rootArgs.topLevelAsAddress = m_surfaceTlasBuffers.pResult->GetGPUVirtualAddress();
        rootArgs.beamDataDescriptorTable = m_beamTracingBeamDataDescriptorHandle;
        

        uint32_t numShaderRecords = 1;
        uint32_t shaderRecordSize = shaderIDSize + sizeof(rootArgs);

        raytrace_helper::ShaderTable beamGenShaderTable(md3dDevice.Get(), numShaderRecords, shaderRecordSize, L"BeamGenShaderTable");
        beamGenShaderTable.push_back(raytrace_helper::ShaderRecord(beamGenShaderID, shaderIDSize, &rootArgs, sizeof(rootArgs)));
        beamGenShaderTable.DebugPrint(shaderIdToStringMap);
        m_beamGenShaderTable = beamGenShaderTable.GetResource();
    }

    // Miss shader table.
    {

        UINT numShaderRecords = 1;
        UINT shaderRecordSize = shaderIDSize; // No root arguments

        raytrace_helper::ShaderTable beamMissShaderTable(md3dDevice.Get(), numShaderRecords, shaderRecordSize, L"BeamMissShaderTable");
        beamMissShaderTable.push_back(raytrace_helper::ShaderRecord(missShaderID, shaderIDSize, nullptr, 0));
        
        beamMissShaderTable.DebugPrint(shaderIdToStringMap);
        m_beamMissShaderTableStrideInBytes = beamMissShaderTable.GetShaderRecordSize();
        m_beamMissShaderTable = beamMissShaderTable.GetResource();
    }

    // Hit group shader table.
    {
        struct {
            D3D12_GPU_DESCRIPTOR_HANDLE goemetryDescriptorTable;
            D3D12_GPU_DESCRIPTOR_HANDLE textureDescriptorTable;
        } rootArgs{};

        rootArgs.goemetryDescriptorTable = m_beamTracingVertexDescriptorHandle;
        rootArgs.textureDescriptorTable = m_beamTracingTextureDescriptorHandle;

        UINT numShaderRecords = 1;
        UINT shaderRecordSize = shaderIDSize + sizeof(rootArgs);
        raytrace_helper::ShaderTable beamHitGroupShaderTable(md3dDevice.Get(), numShaderRecords, shaderRecordSize, L"BeamHitGroupShaderTable");

        beamHitGroupShaderTable.push_back(raytrace_helper::ShaderRecord(hitGroupShaderID, shaderIDSize, &rootArgs, sizeof(rootArgs)));

        beamHitGroupShaderTable.DebugPrint(shaderIdToStringMap);
        m_beamHitGroupShaderTableStrideInBytes = beamHitGroupShaderTable.GetShaderRecordSize();
        m_beamHitGroupShaderTable = beamHitGroupShaderTable.GetResource();
    }
}

void PhotonBeamApp::BuildRayTracingShaderTables()
{
    void* rayGenShaderID;
    void* missShaderID;
    void* beamHitGroupShaderID;
    void* surfaceHitGroupShaderID;

    // A shader name look-up table for shader table debug print out.
    std::unordered_map<void*, std::wstring> shaderIdToStringMap;

    UINT shaderIDSize;
    {
        ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
        ThrowIfFailed(m_rayStateObject.As(&stateObjectProperties));

        auto& rayGenShaderName = c_rayShadersExportNames[to_underlying(ERayTracingShaders::Gen)];
        rayGenShaderID = stateObjectProperties->GetShaderIdentifier(rayGenShaderName);
        shaderIdToStringMap[rayGenShaderID] = rayGenShaderName;

        auto& beamHitGroupName = c_rayHitGroupNames[to_underlying(ERayHitTypes::Beam)];
        beamHitGroupShaderID = stateObjectProperties->GetShaderIdentifier(beamHitGroupName);
        shaderIdToStringMap[beamHitGroupShaderID] = beamHitGroupName;

        auto& surfaceHitGroupName = c_rayHitGroupNames[to_underlying(ERayHitTypes::Surface)];
        surfaceHitGroupShaderID = stateObjectProperties->GetShaderIdentifier(surfaceHitGroupName);
        shaderIdToStringMap[surfaceHitGroupShaderID] = surfaceHitGroupName;

        auto& missShaderName = c_rayShadersExportNames[to_underlying(ERayTracingShaders::Miss)];
        missShaderID = stateObjectProperties->GetShaderIdentifier(missShaderName);
        shaderIdToStringMap[missShaderID] = missShaderName;

        shaderIDSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    }

    // RayGen shader table.
    {
        struct {
            D3D12_GPU_DESCRIPTOR_HANDLE outputImageDescriptorTable;
            D3D12_GPU_VIRTUAL_ADDRESS beamAsAddress;
            D3D12_GPU_VIRTUAL_ADDRESS surfaceAsAddress;
            D3D12_GPU_DESCRIPTOR_HANDLE geometryDescriptorTable;
            D3D12_GPU_DESCRIPTOR_HANDLE textureDescriptorTable;
        } rootArgs{};

        rootArgs.outputImageDescriptorTable = m_offScreenOutputDescriptorHandle;
        rootArgs.beamAsAddress = m_beamTlasBuffers.pResult->GetGPUVirtualAddress();
        rootArgs.surfaceAsAddress = m_surfaceTlasBuffers.pResult->GetGPUVirtualAddress();
        rootArgs.geometryDescriptorTable = m_rayTracingNormalDescriptorHandle;
        rootArgs.textureDescriptorTable = m_rayTracingTextureDescriptorHandle;

        uint32_t numShaderRecords = 1;
        uint32_t shaderRecordSize = shaderIDSize + sizeof(rootArgs);

        raytrace_helper::ShaderTable rayGenShaderTable(md3dDevice.Get(), numShaderRecords, shaderRecordSize, L"RayGenShaderTable");
        rayGenShaderTable.push_back(raytrace_helper::ShaderRecord(rayGenShaderID, shaderIDSize, &rootArgs, sizeof(rootArgs)));
        rayGenShaderTable.DebugPrint(shaderIdToStringMap);
        m_rayGenShaderTable = rayGenShaderTable.GetResource();
    }

    // Miss shader table.
    {

        UINT numShaderRecords = 1;
        UINT shaderRecordSize = shaderIDSize; // No root arguments

        raytrace_helper::ShaderTable rayMissShaderTable(md3dDevice.Get(), numShaderRecords, shaderRecordSize, L"RayMissShaderTable");
        rayMissShaderTable.push_back(raytrace_helper::ShaderRecord(missShaderID, shaderIDSize, nullptr, 0));

        rayMissShaderTable.DebugPrint(shaderIdToStringMap);
        m_rayMissShaderTableStrideInBytes = rayMissShaderTable.GetShaderRecordSize();
        m_rayMissShaderTable = rayMissShaderTable.GetResource();
    }

    // Hit group shader table.
    {
        struct {
            D3D12_GPU_DESCRIPTOR_HANDLE beamDataDescriptorTable;
        } rootArgs{};

        rootArgs.beamDataDescriptorTable = m_rayTracingBeamDataDescriptorHandle;

        UINT numShaderRecords = 2;
        UINT shaderRecordSize = shaderIDSize + sizeof(rootArgs);
        raytrace_helper::ShaderTable rayHitGroupShaderTable(md3dDevice.Get(), numShaderRecords, shaderRecordSize, L"RayHitGroupShaderTable");

        rayHitGroupShaderTable.push_back(raytrace_helper::ShaderRecord(beamHitGroupShaderID, shaderIDSize, &rootArgs, sizeof(rootArgs)));
        rayHitGroupShaderTable.push_back(raytrace_helper::ShaderRecord(surfaceHitGroupShaderID, shaderIDSize, &rootArgs, sizeof(rootArgs)));

        rayHitGroupShaderTable.DebugPrint(shaderIdToStringMap);
        m_rayHitGroupShaderTableStrideInBytes = rayHitGroupShaderTable.GetShaderRecordSize();
        m_rayHitGroupShaderTable = rayHitGroupShaderTable.GetResource();
    }
}
