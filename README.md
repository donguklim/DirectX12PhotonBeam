# DirectX12PhotonBeam
Photon beam rendering algorithm implemented with DirectX12 and RTX

### Link to video
[![Link to video](https://img.youtube.com/vi/Tia85zQ_XLM/0.jpg)](https://youtu.be/Tia85zQ_XLM)

## Background
This is a portfolio project made with intention to implement Photon Beam rendering algorithm using DXR and DirectX 12. 

Photon Beam is a rendering algorithm developed by Disney for descriging radiance 
due to [volumetric participating media](## "Volumetric objects where light scatters or gets absorbed. EX: water, fog, fire, smoke, cloud, dusty wind, murky air").


This application loads a GLTF 3D model file. 
BRDF equation used for surface refelections on the 3D model follows [BRDF in GLTF specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#appendix-b-brdf-implementation) for surface reflection.


For rendering, this application uses 

- Photon beam method for volumetric participating media radiance estimation
- Photon mapping method for surface radiance
- Simple raytracing for specular reflection on 3D model objects with zero roughness.

Watch above video for checking how the implementation works.

For more detailed background of the techinques used, you may check the references.


## Vulkan Implementation

https://github.com/donguklim/vk_raytracing_tutorial_KHR/tree/master/photon_beam

There is Vulkan version of the implementation, which is made earlier than this DirectX 12 version. 

Vulkan version lacks light motion, and light variation features. Only a fixed light simulation is used for every frame generated.


## References

 - ### Photon Mapping
    - Jensen, Henrik. (2001). A Practical Guide to Global Illumination using Photon Maps.
 - ### Photon Beam
    - Derek Nowrouzezahrai, Jared Johnson, Andrew Selle, Dylan Lacewell, Michael Kaschalk, Wojciech Jarosz. A programmable system for artistic volumetric lighting. ACM Transactions on Graphics (Proceedings of SIGGRAPH), 30(4):29:1–29:8, August 2011.
    - Wojciech Jarosz, Derek Nowrouzezahrai, Iman Sadeghi, Henrik Wann Jensen. A comprehensive theory of volumetric radiance estimation using photon points and beams. ACM Transactions on Graphics (Presented at SIGGRAPH), 30(1):5:1–5:19, January 2011.
 - ### BRDF Sampling
    - https://www.astro.umd.edu/~jph/HG_note.pdf    &nbsp; [[pdf file backup]](reference_backup/HG_note.pdf)
    - https://agraphicsguynotes.com/posts/sample_microfacet_brdf/   &nbsp; [[webpage backup as pdf file]](reference_backup/Importance_Sampling_techniques_for_GGX.pdf)
    - https://schuttejoe.github.io/post/ggximportancesamplingpart1/     &nbsp; [[webpage backup as pdf file]](reference_backup/sampling_with_microfacet_brdf.pdf)

## Third Party Libraries 

 - "Introduction to 3D Game Programming with DirectX 12" book - [Sample codes in `Common` folder](https://github.com/d3dcoder/d3d12book/tree/4cfd00afa59210a272f62caf0660478d18b9ffed/Common).
	- [Common/](./Common) - The original codes have been modified.
	
 - [Microsoft Official DirectX header files](https://github.com/microsoft/DirectX-Headers/tree/9ca4839a1b49aeac56c86036212dc035b1cf4a09/include/)
	- [third-party/directx-headers](./third-party/directx-headers)
 - tiny gltf - [`stb_image.h`, `stb_image_write.h`, `json.hpp`, `tiny_gltf.h`](https://github.com/syoyo/tinygltf/tree/aa613a1f572c8b9c676a4c0a1d6e5445bf5760f5)
	- [third-party/tiny-gltf](./third-party/directx-headers)
 - Microsoft DirectX Samples - [DirectXRaytracingHelper.h](https://github.com/microsoft/DirectX-Graphics-Samples/blob/0aa79bad78992da0b6a8279ddb9002c1753cb849/Samples/Desktop/D3D12Raytracing/src/D3D12RaytracingProceduralGeometry/DirectXRaytracingHelper.h)
	- [third-party/microsoft-directx-graphics-samples](./third-party/microsoft-directx-graphics-samples)
 - [Nvidia DX12 Raytracing tutorial](https://developer.nvidia.com/rtx/raytracing/dxr/dx12-raytracing-tutorial-part-1) - DXR helper files ([Download Link](https://developer.nvidia.com/rtx/raytracing/dxr/tutorial/Files/DXRHelpers.zip))
	- [third-party/Nvidia-DXRHelpers](./third-party/Nvidia-DXRHelpers)