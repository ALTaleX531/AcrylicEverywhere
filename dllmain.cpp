// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include "AcrylicEverywhere.hpp"

BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD  dwReason,
    LPVOID lpReserved
)
{
    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
        {
            DisableThreadLibraryCalls(hModule);
            AcrylicEverywhere::Startup();
            AllocConsole();
            break;
        }
        case DLL_PROCESS_DETACH:
        {
            AcrylicEverywhere::Shutdown();
            FreeConsole();
            break;
        }
    }
    return TRUE;
}

extern "C" __declspec(dllexport) int WINAPI Main(
    HWND hWnd,
    HINSTANCE hInstance,
    LPCSTR    lpCmdLine,
    int       nCmdShow
)
{
    return S_OK;
}