
#include "PhotonBeamApp.hpp"

#pragma comment(lib, "runtimeobject.lib")

#if (_WIN32_WINNT >= 0x0A00 /*_WIN32_WINNT_WIN10*/)
Microsoft::WRL::Wrappers::RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);
if (FAILED(initialize))
MessageBox(nullptr, L"Failed CoInitialize, which must be initialized for texture resource creation ", L"HR Failed", MB_OK);
#else
HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
if (FAILED(hr))
MessageBox(nullptr, L"Failed CoInitialize, which must be initialized for texture resource creation ", L"HR Failed", MB_OK);
#endif

int main(int, char**)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    HINSTANCE hInstance = GetModuleHandle(NULL);
    PhotonBeamApp theApp(hInstance);

    try
    {
        if (!theApp.Initialize())
            return 0;

        theApp.InitGui();
        theApp.CheckRaytracingSupport();

        return theApp.Run();
    }
    catch (DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
    catch (std::runtime_error& e)
    {
        OutputDebugStringA(e.what());
        return 0;
    }

}
