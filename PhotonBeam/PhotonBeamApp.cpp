//***************************************************************************************
// PhotonBeamApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//
// Hold down '1' key to view scene in wireframe mode.
//***************************************************************************************
#pragma once

#define NOMINMAX

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// tiny gltf used in GltfScene.hpp uses sprintf, so bellow define is needed
#define _CRT_SECURE_NO_WARNINGS

#include "PhotonBeamApp.hpp"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include <imgui_helper.h>
#include "raytraceHelper.hpp"

#include "Shaders/RaytracingHlslCompat.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

const int gNumFrameResources = 3;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


PhotonBeamApp::PhotonBeamApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
    mLastMousePos = POINT{};
    m_useRayTracer = true;
    m_airScatterCoff = XMVECTORF32{};
    m_airExtinctCoff = XMVECTORF32{};
    m_sourceLight = XMVECTORF32{};

    for (size_t i = 0; i < to_underlying(RootSignatueEnums::BeamTrace::ERootSignatures::Count); i++)
    {
        m_BeamRootSignarues[i] = nullptr;
    }
    for (size_t i = 0; i < to_underlying(RootSignatueEnums::CameraRayTrace::ERootSignatures::Count); i++)
    {
        m_RayRootSignarues[i] = nullptr;
    }

    SetDefaults();


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
    BuildRootSignature();
    BuildPostRootSignature();
    BuildBeamTraceRootSignatures();
    BuildShadersAndInputLayout();

    BuildRenderItems();
    BuildFrameResources();
    BuildDescriptorHeaps();
    BuildPSOs();

    CreateBottomLevelAS();
    CreateTopLevelAS();

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();

    mGeometries["cornellBox"].get()->DisposeUploaders();
    m_gltfScene.destroy();

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
}

void PhotonBeamApp::drawPost(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdListAlloc)
{
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["post"].Get()));

    // Indicate a state transition on the resource usage.
    auto resourceBarrierRender = CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    mCommandList->ResourceBarrier(1, &resourceBarrierRender);


    CD3DX12_CLEAR_VALUE clearValue{ DXGI_FORMAT_R32G32B32_FLOAT, m_clearColor };

    D3D12_RENDER_PASS_BEGINNING_ACCESS renderPassBeginningAccessClear{ D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR, { clearValue } };
    static const D3D12_RENDER_PASS_ENDING_ACCESS renderPassEndingAccessPreserve{ D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE, {} };
    D3D12_RENDER_PASS_RENDER_TARGET_DESC renderPassRenderTargetDesc{
        CurrentBackBufferView(),
        renderPassBeginningAccessClear,
        renderPassEndingAccessPreserve
    };

    static const CD3DX12_CLEAR_VALUE depthStencilClearValue{ DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 1.0f, 0 };
    static const D3D12_RENDER_PASS_ENDING_ACCESS endingNoAccess{ D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS, {} };

    static const D3D12_RENDER_PASS_BEGINNING_ACCESS beginningNoAccess{ D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, {} };

    static const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC renderPassDepthStencilDesc{
        D3D12_CPU_DESCRIPTOR_HANDLE{0},
        beginningNoAccess,
        beginningNoAccess,
        endingNoAccess,
        endingNoAccess
    };

    mCommandList->BeginRenderPass(
        1,
        &renderPassRenderTargetDesc,
        &renderPassDepthStencilDesc,
        D3D12_RENDER_PASS_FLAG_NONE
    );
    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    mCommandList->SetGraphicsRootSignature(mPostRootSignature.Get());

    mCommandList->IASetVertexBuffers(0, 0, nullptr);
    //mCommandList->IASetIndexBuffer(&indexBufferView);
    mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mCommandList->DrawInstanced(3, 1, 0, 0);
    //mCommandList->DrawIndexedInstanced(3, 0, 0);

    ID3D12DescriptorHeap* guiDescriptorHeaps[] = { mGuiDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(guiDescriptorHeaps), guiDescriptorHeaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());
    mCommandList->EndRenderPass();

}

void PhotonBeamApp::Rasterize(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdListAlloc)
{

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    if (mIsWireframe)
    {
        ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque_wireframe"].Get()));
    }
    else
    {
        ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));
    }
    // Indicate a state transition on the resource usage.
    auto resourceBarrierRender = CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    mCommandList->ResourceBarrier(1, &resourceBarrierRender);


    CD3DX12_CLEAR_VALUE clearValue{ DXGI_FORMAT_R32G32B32_FLOAT, m_clearColor };

    D3D12_RENDER_PASS_BEGINNING_ACCESS renderPassBeginningAccessClear{ D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR, { clearValue } };
    static const D3D12_RENDER_PASS_ENDING_ACCESS renderPassEndingAccessPreserve{ D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE, {} };
    D3D12_RENDER_PASS_RENDER_TARGET_DESC renderPassRenderTargetDesc{
        CurrentBackBufferView(),
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

}

void PhotonBeamApp::LightTrace(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdListAlloc)
{

}

void PhotonBeamApp::RayTrace()
{
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), m_clearColor, 0, nullptr);
}

void PhotonBeamApp::Draw(const GameTimer& gt)
{
    // Start the Dear ImGui frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    RenderUI();
    ImGui::Render();

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    Rasterize(cmdListAlloc);

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

    if (GetAsyncKeyState('1') & 0x8000)
        mIsWireframe = true;
    else
        mIsWireframe = false;

    const float dt = gt.DeltaTime();

    if (GetAsyncKeyState('W') & 0x8000)
        mCamera.Walk(10.0f * dt);

    if (GetAsyncKeyState('S') & 0x8000)
        mCamera.Walk(-10.0f * dt);

    if (GetAsyncKeyState('A') & 0x8000)
        mCamera.Strafe(-10.0f * dt);

    if (GetAsyncKeyState('D') & 0x8000)
        mCamera.Strafe(10.0f * dt);

    if (GetAsyncKeyState('Q') & 0x8000)
        mCamera.Pedestal(-10.0f * dt);

    if (GetAsyncKeyState('E') & 0x8000)
        mCamera.Pedestal(10.0f * dt);

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
    mMainPassCB.LightPos = XMFLOAT3(m_lightPosition[0], m_lightPosition[1], m_lightPosition[2]);
    mMainPassCB.lightIntensity = m_lightIntensity;
    mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
    mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
    mMainPassCB.NearZ = mCamera.GetNearZ();
    mMainPassCB.FarZ = mCamera.GetFarZ();
    mMainPassCB.TotalTime = gt.TotalTime();
    mMainPassCB.DeltaTime = gt.DeltaTime();

    auto currPassCB = mCurrFrameResource->PassCB.get();
    currPassCB->CopyData(0, mMainPassCB);
}

void PhotonBeamApp::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC guiHeapDesc = {};
    guiHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    guiHeapDesc.NumDescriptors = 1;
    guiHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&guiHeapDesc,
        IID_PPV_ARGS(&mGuiDescriptorHeap)));

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = static_cast<UINT>(m_textures.size());
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
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

void PhotonBeamApp::BuildRootSignature()
{    
    CD3DX12_DESCRIPTOR_RANGE texTable{};
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, PhotonBeamApp::MAX_NUM_TEXTURES, 0, 0);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[4] = {};

    slotRootParameter[0].InitAsConstantBufferView(0);
    slotRootParameter[1].InitAsConstantBufferView(1);
    slotRootParameter[2].InitAsShaderResourceView(0, 1);
    slotRootParameter[3].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);


    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
        0, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP
    );

    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
        4, 
        slotRootParameter, 
        1, 
        &linearWrap,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

    SerializeAndCreateRootSignature(rootSigDesc, mRootSignature.GetAddressOf());
}

void PhotonBeamApp::BuildBeamTraceRootSignatures()
{
    // Global Root Signature
    // This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
    {
        using namespace RootSignatueEnums::BeamTrace;

        CD3DX12_ROOT_PARAMETER rootParameters[to_underlying(EGlobalParams::Count)] = {};
        rootParameters[to_underlying(EGlobalParams::SceneConstantSlot)].InitAsConstantBufferView(0);
        CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
        SerializeAndCreateRootSignature(
            globalRootSignatureDesc, 
            m_BeamRootSignarues[to_underlying(ERootSignatures::Global)].GetAddressOf()
        );
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

    m_rayTraceShaders["beamMiss"] = raytrace_helper::CompileShaderLibrary(L"Shaders\\BeamTracing\\BeamMiss.hlsl", L"lib_6_6");
    m_rayTraceShaders["beamCHit"] = raytrace_helper::CompileShaderLibrary(L"Shaders\\BeamTracing\\BeamClosestHit.hlsl", L"lib_6_6");
    m_rayTraceShaders["beamGen"] = raytrace_helper::CompileShaderLibrary(L"Shaders\\BeamTracing\\BeamGen.hlsl", L"lib_6_6");

    m_rayTraceShaders["rayBeamAnyHit"] = raytrace_helper::CompileShaderLibrary(L"Shaders\\CameraRayTracing\\RayBeamAnyHit.hlsl", L"lib_6_6");
    m_rayTraceShaders["rayBeamInt"] = raytrace_helper::CompileShaderLibrary(L"Shaders\\CameraRayTracing\\RayBeamInt.hlsl", L"lib_6_6");
    m_rayTraceShaders["raySurfaceAnyInt"] = raytrace_helper::CompileShaderLibrary(L"Shaders\\CameraRayTracing\\RaySurfaceAnyHit.hlsl", L"lib_6_6");
    m_rayTraceShaders["raySurfaceInt"] = raytrace_helper::CompileShaderLibrary(L"Shaders\\CameraRayTracing\\RaySurfaceInt.hlsl", L"lib_6_6");
    m_rayTraceShaders["rayGen"] = raytrace_helper::CompileShaderLibrary(L"Shaders\\CameraRayTracing\\RayGen.hlsl", L"lib_6_6");
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

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "cornellBox";

    const UINT vbByteSize = (UINT)vertexPositions.size() * sizeof(XMFLOAT3);
    const UINT nbByteSize = (UINT)vertexNormals.size() * sizeof(XMFLOAT3);
    const UINT ubByteSize = (UINT)vertexUVs.size() * sizeof(XMFLOAT2);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);
    const UINT mbByteSize = (UINT)shadeMaterials.size() * sizeof(GltfShadeMaterial);

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

    geo->VertexByteStride = sizeof(XMFLOAT3);
    geo->VertexBufferByteSize = vbByteSize;
    geo->NormalByteStride = sizeof(XMFLOAT3);
    geo->NormalBufferByteSize = nbByteSize;
    geo->UvByteStride = sizeof(XMFLOAT2);
    geo->UvBufferByteSize = ubByteSize;
    geo->IndexFormat = DXGI_FORMAT_R32_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    geo->MaterialByteStride = sizeof(shadeMaterials);
    geo->MaterialBufferByteSize = mbByteSize;

    mGeometries[geo->Name] = std::move(geo);

}

void PhotonBeamApp::CreateTextures()
{
    const static std::array<uint8_t, 4> whiteTexture = { 225, 255, 255, 255 };
    const auto& textureImages = m_gltfScene.GetTextureImages();
    
    size_t numTextures = textureImages.size();
    if (textureImages.empty())
        numTextures = 1;

    if (numTextures > PhotonBeamApp::MAX_NUM_TEXTURES)
    { 
        numTextures = PhotonBeamApp::MAX_NUM_TEXTURES;
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

void PhotonBeamApp::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

    // PSO for opaque objects.
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    opaquePsoDesc.pRootSignature = mRootSignature.Get();
    opaquePsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(m_rasterizeShaders["standardVS"]->GetBufferPointer()),
        m_rasterizeShaders["standardVS"]->GetBufferSize()
    };
    opaquePsoDesc.PS =
    {
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
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

    // PSO for opaque wireframe objects.
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
    opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC postPsoDesc;
    ZeroMemory(&postPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    postPsoDesc.InputLayout = { nullptr, 0 };
    postPsoDesc.pRootSignature = mPostRootSignature.Get();
    postPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(m_rasterizeShaders["postVS"]->GetBufferPointer()),
        m_rasterizeShaders["postVS"]->GetBufferSize()
    };
    postPsoDesc.PS =
    {
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
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&postPsoDesc, IID_PPV_ARGS(&mPSOs["post"])));
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
    const XMVECTORF32 defaultBeamUnitDistantColor{ 0.816f, 0.906f, 0.906f, 1.0f };

    m_clearColor = Colors::LightSteelBlue;
    m_beamNearColor = defaultBeamNearColor;
    m_beamUnitDistantColor = defaultBeamUnitDistantColor;
    m_beamRadius = 0.6f;
    m_photonRadius = 1.0f;
    m_beamIntensity = 15.0f;
    m_usePhotonMapping = true;
    m_usePhotonBeam = true;
    m_hgAssymFactor = 0.0f;
    m_showDirectColor = false;
    m_airAlbedo = 0.06f;

    m_numBeamSamples = 1024;
    m_numPhotonSamples = 4 * 4 * 2048;

    m_lightPosition = XMVECTORF32{ 0.0f, 0.0f, 0.0f };
    m_lightIntensity = 10.0f;
    m_createBeamPhotonAS = true;
    m_camearaFOV = 60.0f;
    m_prevCameraFOV = m_camearaFOV;

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

        ImGui::SliderFloat("Light Intensity", &m_beamIntensity, 0.0f, 300.f);


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

        if (ImGui::SmallButton("Refresh Beam"))
            m_createBeamPhotonAS = true;

        ImGuiH::Control::Info(
            "",
            "",
            "Click Refresh Beam to fully reflect changed parameters, some parameters do not get fully reflected before the click",
            ImGuiH::Control::Flags::Disabled
        );

    } while (false);

    if (ImGui::SmallButton("Set Defaults"))
        SetDefaults();

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGuiH::Panel::End();
}


void PhotonBeamApp::CreateBottomLevelAS() {

    nv_helpers_dx12::BottomLevelASGenerator bottomLevelAS{};
    // Adding all vertex buffers and not transforming their position. 

    auto geo = mGeometries["cornellBox"].get();
    auto vertexBufferView = geo->VertexBufferView();
    auto indexbufferView = geo->IndexBufferView();
    const auto& primMeshes = m_gltfScene.GetPrimMeshes();
    for (const auto& mesh : primMeshes)
    {
        bottomLevelAS.AddVertexBuffer(
            geo->VertexBufferGPU.Get(),
            static_cast<UINT64>(mesh.vertexOffset) * vertexBufferView.StrideInBytes,
            vertexBufferView.SizeInBytes / vertexBufferView.StrideInBytes - mesh.vertexOffset,
            vertexBufferView.StrideInBytes,
            geo->IndexBufferGPU.Get(),
            mesh.firstIndex * sizeof(UINT16),
            mesh.indexCount,
            nullptr,
            0
        );
    }

    UINT64 scratchSizeInBytes = 0;
    UINT64 resultSizeInBytes = 0;
    bottomLevelAS.ComputeASBufferSizes(md3dDevice.Get(), false, &scratchSizeInBytes, &resultSizeInBytes);

    m_bottomLevelASBuffers.pScratch = raytrace_helper::CreateBuffer(
        md3dDevice.Get(),
        scratchSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COMMON,
        raytrace_helper::pmDefaultHeapProps
    );
    m_bottomLevelASBuffers.pResult = raytrace_helper::CreateBuffer(
        md3dDevice.Get(),
        resultSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        raytrace_helper::pmDefaultHeapProps
    );

    bottomLevelAS.Generate(
        mCommandList.Get(),
        m_bottomLevelASBuffers.pScratch.Get(),
        m_bottomLevelASBuffers.pResult.Get(),
        false,
        nullptr
    );
}

void PhotonBeamApp::CreateTopLevelAS() {


    for (auto& node : m_gltfScene.GetNodes())
    {
        m_topLevelASGenerator.AddInstance(
            m_bottomLevelASBuffers.pResult.Get(),
            XMLoadFloat4x4(&node.worldMatrix),
            static_cast<UINT>(node.primMesh),
            static_cast<UINT>(0)
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

    m_topLevelASBuffers.pScratch = raytrace_helper::CreateBuffer(
        md3dDevice.Get(),
        scratchSize,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COMMON,
        raytrace_helper::pmDefaultHeapProps
    );

    m_topLevelASBuffers.pResult = raytrace_helper::CreateBuffer(
        md3dDevice.Get(),
        resultSize,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        raytrace_helper::pmDefaultHeapProps
    );

    m_topLevelASBuffers.pInstanceDesc = raytrace_helper::CreateBuffer(
        md3dDevice.Get(),
        instanceDescsSize,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        raytrace_helper::pmUploadHeapProps
    );


    m_topLevelASGenerator.Generate(
        mCommandList.Get(),
        m_topLevelASBuffers.pScratch.Get(),
        m_topLevelASBuffers.pResult.Get(),
        m_topLevelASBuffers.pInstanceDesc.Get()
    );

    return;
}

void PhotonBeamApp::BuildBeamSignatures()
{

    nv_helpers_dx12::RootSignatureGenerator rsc;
    rsc.AddHeapRangesParameter(
        {
            {
                0 /*u0*/,
                1 /*1 descriptor */,
                0 /*use the implicit register space 0*/,
                D3D12_DESCRIPTOR_RANGE_TYPE_UAV /* UAV representing the output buffer*/,
                0 /*heap slot where the UAV is defined*/
            },
            {
                0 /*t0*/,
                1,
                0,
                D3D12_DESCRIPTOR_RANGE_TYPE_SRV /*Top-level acceleration structure*/,
                1
            }
        }
    );
    //rsc.Generate(md3dDevice.Get(), true, m_beamGenSignature.GetAddressOf());


}
