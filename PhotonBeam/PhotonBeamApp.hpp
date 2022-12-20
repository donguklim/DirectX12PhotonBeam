//***************************************************************************************
// PhotonBeamApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//
// Hold down '1' key to view scene in wireframe mode.
//***************************************************************************************

#pragma once
#define NOMINMAX

#include "../Common/d3dApp.h"
#include "../Common/Camera.h"
#include "../Common/MathHelper.h"
#include "../Common/UploadBuffer.h"
#include "../Common/GeometryGenerator.h"
#include "FrameResource.h"
#include "GltfScene.hpp"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

struct GltfShadeMaterial
{
    XMFLOAT4 pbrBaseColorFactor;
    XMFLOAT3 emissiveFactor;
    int  pbrBaseColorTexture;
    float metallic;
    float roughness;
    UINT   padding[2];
};

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
    RenderItem() = default;

    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    XMFLOAT4X4 World = MathHelper::Identity4x4();

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

    GltfScene m_gltfScene;

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

    void BuildDescriptorHeaps();
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
    void RenderUI();
    void SetDefaults();
    void Rasterize(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdListAlloc);
    void LightTrace(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdListAlloc);
    void drawPost();

private:

    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    ComPtr<ID3D12RootSignature> mPostRootSignature = nullptr;

    ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

    ComPtr<ID3D12DescriptorHeap> mGuiDescriptorHeap = nullptr;

    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
    std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
    std::vector<D3D12_INPUT_ELEMENT_DESC> mPostInputLayout;

    // List of all the render items.
    std::vector<std::unique_ptr<RenderItem>> mAllRitems;

    // Render items divided by PSO.
    std::vector<RenderItem*> mOpaqueRitems;

    PassConstants mMainPassCB;

    bool mIsWireframe = false;

    XMFLOAT3 mEyePos{};
    XMVECTORF32 m_clearColor;

    POINT mLastMousePos;
    Camera mCamera;

    float    m_airAlbedo{ 0.1f };
    float m_beamRadius{ 0.5f };
    float    m_photonRadius{ 0.5f };
    uint32_t m_numBeamSamples{ 1024 };
    uint32_t m_numPhotonSamples{ 4 * 4 * 1024 };

    unsigned int m_maxNumBeamSamples;
    unsigned int m_maxNumPhotonSamples;
    bool m_useRayTracer;
    XMVECTORF32 m_beamNearColor;
    XMVECTORF32 m_beamUnitDistantColor;
    XMVECTORF32 m_airScatterCoff;
    XMVECTORF32 m_airExtinctCoff;
    XMVECTORF32 m_sourceLight;
    XMVECTORF32 m_lightPosition{ 0.0f, 0.0f, 0.0f };
    float m_lightIntensity;
    float         m_beamIntensity;
    bool          m_usePhotonMapping;
    bool          m_usePhotonBeam;
    float         m_hgAssymFactor;
    bool          m_showDirectColor;
    bool m_createBeamPhotonAS;
    float m_camearaFOV;
    float m_prevCameraFOV;
};

