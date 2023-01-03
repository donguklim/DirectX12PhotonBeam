//***************************************************************************************
// PhotonBeamApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//
// Hold down '1' key to view scene in wireframe mode.
//***************************************************************************************

#pragma once
#define NOMINMAX
#include "d3d12.h"
#include <dxcapi.h>

#include "../Common/d3dApp.h"
#include "../Common/Camera.h"
#include "../Common/MathHelper.h"
#include "../Common/UploadBuffer.h"
#include "Nvidia-DXRHelpers/nv_helpers_dx12/TopLevelASGenerator.h"
#include "FrameResource.h"
#include "GltfScene.hpp"



template <typename E>
constexpr typename std::underlying_type<E>::type to_underlying(E e) noexcept 
{
    return static_cast<typename std::underlying_type<E>::type>(e);
}

namespace RootSignatueEnums 
{
    namespace BeamTrace 
    {
        enum class ERootSignatures : uint16_t 
        {
            Global = 0,
            Gen,
            CloseHit,
            Count
        };


        enum class EGlobalParams : uint16_t 
        {
            SceneConstantSlot = 0,
            Count
        };


        enum class EGenParams : uint16_t 
        {
            SurfaceASSlot,
            RWBufferSlot,
            Count
        };


        enum class ECloseHitParams : uint16_t 
        {
            ReadBuffersSlot,
            TextureMapsSlot,
            Count
        };
    }


    namespace RayTrace 
    {
        enum class ERootSignatures : uint16_t 
        {
            Global = 0,
            Gen,
            AnyHitAndInt,
            Count
        };


        enum class EGlobalParams : uint16_t 
        {
            SceneConstantSlot = 0,
            Count
        };


        enum class EGenParams : uint16_t 
        {
            OutputViewSlot = 0,
            BeamASSlot,
            SurfaceASSlot,
            ReadBuffersSlot,
            TextureMapsSlot,
            Count
        };


        enum class EAnyHitAndIntParams : uint16_t 
        {
            BeamBufferSlot,
            Count
        };
    }
}


enum class ERayTracingShaders : uint16_t
{
    Gen = 0,
    BeamInt,
    BeamAnyHit,
    SurfaceInt,
    SurfaceAnyHit,
    Miss,
    Count
};


enum class EBeamTracingShaders : uint16_t
{
    Gen = 0,
    CloseHit,
    Miss,
    Count
};


enum class EBeamHitTypes : uint16_t
{
    Surface = 0,
    Count
};

enum class ERayHitTypes : uint16_t
{
    Beam = 0,
    Surface,
    Count
};


// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
    RenderItem() = default;

    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();

    // Dirty flag indicating the object data has changed and we need to update the constant buffer.
    // Because we have an object cbuffer for each FrameResource, we have to apply the
    // update to each FrameResource.  Thus, when we modify obect data we should set 
    // NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
    int NumFramesDirty = gNumFrameResources;

    // Index into GPU constant buffer corresponding to the ObjectCB for this render item.
    UINT ObjCBIndex = -1;

    UINT MaterialIndex = -1;

    MeshGeometry* Geo = nullptr;

    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

struct AccelerationStructureBuffers
{
    Microsoft::WRL::ComPtr<ID3D12Resource> pScratch;
    Microsoft::WRL::ComPtr<ID3D12Resource> pResult;
    Microsoft::WRL::ComPtr<ID3D12Resource> pInstanceDesc;
};


class PhotonBeamApp : public D3DApp
{

public:
    PhotonBeamApp(HINSTANCE hInstance);
    PhotonBeamApp(const PhotonBeamApp& rhs) = delete;
    PhotonBeamApp& operator=(const PhotonBeamApp& rhs) = delete;
    ~PhotonBeamApp();

    virtual bool Initialize()override;
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)override;
    void InitGui();
    void LoadScene();
    void CheckRaytracingSupport();

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;
    virtual void OnMouseWheel(WPARAM btnState, int delta)override;

    void OnKeyboardInput(const GameTimer& gt);
    void UpdateObjectCBs(const GameTimer& gt);
    void UpdateMainPassCB(const GameTimer& gt);
    void UpdateRayTracingPushConstants();

    void SerializeAndCreateRootSignature(
        D3D12_ROOT_SIGNATURE_DESC& desc,
        ID3D12RootSignature** ppRootSignature
    );
    void CreateTextures();

    void BuildDescriptorHeaps();
    void BuildRasterizeRootSignature();
    void BuildPostRootSignature();
    void BuildShadersAndInputLayout();
    void BuildPSOs();

    uint32_t AllocateRayTracingDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse = UINT_MAX);
    uint32_t AllocateBeamTracingDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse = UINT_MAX);

    void BuildRayTracingDescriptorHeaps();
    void BuildRayTracingRootSignatures();
    void BuildBeamTracingPSOs();
    void BuildRayTracingPSOs();
    void BuildBeamTracingShaderTables();
    void BuildRayTracingShaderTables();

    void CreateOffScreenOutputResource();
    void CreateBeamBuffers(Microsoft::WRL::ComPtr<ID3D12Resource>& resetValuploadBuffer);

    void BuildFrameResources();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
    void RenderUI();
    void SetDefaults();

    void Rasterize();
    void BeamTrace();
    void RayTrace();
    void drawPost();

    void CreateSurfaceBlas();
    void CreateSurfaceTlas();

    void CreateBeamBlases();

    static const CD3DX12_STATIC_SAMPLER_DESC& GetLinearSampler();

private:

    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;
    UINT mCbvSrvDescriptorSize = 0;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> mPostRootSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_postSrvDescriptorHeap = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mGuiDescriptorHeap = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_offScreenRtvHeap = nullptr;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_beamTracingDescriptorHeap = nullptr;
    uint32_t m_beamTracingDescriptorsAllocated;
    D3D12_GPU_DESCRIPTOR_HANDLE m_beamTracingBeamDataDescriptorHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_beamTracingVertexDescriptorHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_beamTracingTextureDescriptorHandle{};
    
    
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rayTracingDescriptorHeap = nullptr;
    uint32_t m_rayTracingDescriptorsAllocated;
    uint32_t m_offScreenOutputResourceUAVDescriptorHeapIndex;
    D3D12_GPU_DESCRIPTOR_HANDLE m_offScreenOutputDescriptorHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_rayTracingNormalDescriptorHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_rayTracingTextureDescriptorHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_rayTracingBeamDataDescriptorHandle{};


    std::vector<std::unique_ptr<Texture>> m_textures;
    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<IDxcBlob>> m_rasterizeShaders;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

    Microsoft::WRL::ComPtr<ID3D12StateObject> m_beamStateObject = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_beamRootSignatures[to_underlying(RootSignatueEnums::BeamTrace::ERootSignatures::Count)];
    Microsoft::WRL::ComPtr<IDxcBlob> m_beamShaders[to_underlying(EBeamTracingShaders::Count)];
    static const wchar_t* c_beamShadersExportNames[to_underlying(EBeamTracingShaders::Count)];
    static const wchar_t* c_rayHitGroupNames[to_underlying(ERayHitTypes::Count)];
    Microsoft::WRL::ComPtr<ID3D12Resource> m_beamGenShaderTable;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_beamMissShaderTable;
    uint32_t m_beamMissShaderTableStrideInBytes = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_beamHitGroupShaderTable;
    uint32_t m_beamHitGroupShaderTableStrideInBytes = 0;
    

    Microsoft::WRL::ComPtr<ID3D12StateObject> m_rayStateObject = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rayRootSignatures[to_underlying(RootSignatueEnums::RayTrace::ERootSignatures::Count)];
    Microsoft::WRL::ComPtr<IDxcBlob> m_rayShaders[to_underlying(ERayTracingShaders::Count)];
    static const wchar_t* c_rayShadersExportNames[to_underlying(ERayTracingShaders::Count)];
    static const wchar_t* c_beamHitGroupNames[to_underlying(EBeamHitTypes::Count)];
    Microsoft::WRL::ComPtr<ID3D12Resource> m_rayGenShaderTable;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_rayHitGroupShaderTable;
    uint32_t m_rayHitGroupShaderTableStrideInBytes = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_rayMissShaderTable;
    uint32_t m_rayMissShaderTableStrideInBytes = 0;


    Microsoft::WRL::ComPtr<ID3D12Resource> m_offScreenOutput = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_beamCounter = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_beamCounterReset = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_beamCounterRead = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_beamReadDebug = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_beamData = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_beamAsInstanceDescData = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Fence> m_beamCounterFence = nullptr;
    uint64_t m_currentBeamCounterFence = 0;

    Microsoft::WRL::ComPtr<ID3D12Fence> m_beamTlasFence = nullptr;
    uint64_t m_currentBeamTlasFence = 0;
    

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    // List of all the render items.
    std::vector<std::unique_ptr<RenderItem>> mAllRitems;

    // Render items divided by PSO.
    std::vector<RenderItem*> mOpaqueRitems;

    PassConstants mMainPassCB;

    PushConstantRay m_pcRay;
    PushConstantBeam m_pcBeam;

    GltfScene m_gltfScene;

    bool mIsWireframe = false;

    DirectX::XMFLOAT3 mEyePos{};
    DirectX::XMVECTORF32 m_clearColor;

    POINT mLastMousePos;
    Camera mCamera;

    float    m_airAlbedo{ 0.1f };
    float m_beamRadius{ 0.5f };
    float    m_photonRadius{ 0.5f };

    // number of beams and photons shot from the light source
    uint32_t m_numBeamSamples{ 1024 };
    uint32_t m_numPhotonSamples{ 4 * 4 * 1024 };

    // maximum number of beam sources or photon sources shot from the light origin
    const uint32_t m_maxNumBeamSamples{ 2048 };
    const uint32_t m_maxNumPhotonSamples{ 4 * 4 * 4096 };

    // maximum number of beam data allowed. A refraction or reflection of a beam generates a new beam data.
    // max(number of photon samples, number of number of beam samples) * (expected number of scatter  + surface intersection )
    const uint32_t m_maxNumBeamData{ (m_maxNumBeamSamples > m_maxNumPhotonSamples ? m_maxNumBeamSamples : m_maxNumPhotonSamples) * 32 };

    //maximum number of total sub beams allowed. Equivalent to the maximum number of instances in Beam Accelerated Structure.
    // number of beam samples * (expected number of scatter  + surface intersection ) * (expected length of the beam / (radius * 2)) 
    const uint32_t m_maxNumSubBeamInfo{ m_maxNumBeamSamples * 48 + m_maxNumPhotonSamples };

    bool m_useRayTracer;
    DirectX::XMVECTORF32 m_beamNearColor;
    DirectX::XMVECTORF32 m_beamUnitDistantColor;
    DirectX::XMVECTORF32 m_airScatterCoff;
    DirectX::XMVECTORF32 m_airExtinctCoff;
    DirectX::XMVECTORF32 m_sourceLight;
    DirectX::XMVECTORF32 m_lightPosition{ 0.0f, 0.0f, 0.0f };
    float m_lightIntensity;
    float         m_beamIntensity;
    bool          m_usePhotonMapping;
    bool          m_usePhotonBeam;
    float         m_hgAssymFactor;
    bool          m_showDirectColor;
    bool m_createBeamPhotonAS;
    float m_camearaFOV;
    float m_prevCameraFOV;

    std::vector<AccelerationStructureBuffers> m_surfaceBlasBuffers{};
    AccelerationStructureBuffers m_surfaceTlasBuffers{};

    AccelerationStructureBuffers m_beamBlasBuffers{};
    AccelerationStructureBuffers m_photonBlasBuffers{};
    AccelerationStructureBuffers m_beamTlasBuffers{};

    Microsoft::WRL::ComPtr<ID3D12Resource> m_bottomLevelAS; // Storage for the bottom Level AS
    nv_helpers_dx12::TopLevelASGenerator m_topLevelASGenerator;

};

