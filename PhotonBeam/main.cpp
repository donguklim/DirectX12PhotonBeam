
#include "PhotonBeamApp.hpp"

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
