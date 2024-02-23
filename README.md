# AcrylicEverywhere
Upstream implementation of the [DWMBlurGlass](https://github.com/Maplespe/DWMBlurGlass) custom blur method.  
  
In theory, the custom blur method is supported as low as Windows 10 1607, but it requires a lot of work, and the current code for the project supports as low as Windows 10 2004.   

## Remarks
The name is a bit inaccurate, it should be called `TranslucentEverywhere`. At first I was only going to implement Acrylic backdrop effect, but based on Maplespe's request, I added more backdrop effects, such as Aero, Mica, Blur, GlassReflection, etc.   

This project is not directly usable for production purposes, but you can easily develop it.

## To-Do
1. Fix crash caused by uninjecting dwm with unfinished crossfade animation
2. Fix glass reflection abnormal offset
3. Optimize aero effect recipe to make it more accurate
4. Optimize animated real-time blur backdrop in Windows 11 (not quite important)
5. Optimize program compatibility

## Dependencies and References
### [Microsoft Research Detours Package](https://github.com/microsoft/Detours)  
Detours is a software package for monitoring and instrumenting API calls on Windows.  
### [VC-LTL - An elegant way to compile lighter binaries.](https://github.com/Chuyu-Team/VC-LTL5)  
VC-LTL is an open source CRT library based on the MS VCRT that reduce program binary size and say goodbye to Microsoft runtime DLLs, such as msvcr120.dll, api-ms-win-crt-time-l1-1-0.dll and other dependencies.  
### [Windows Implementation Libraries (WIL)](https://github.com/Microsoft/wil)  
The Windows Implementation Libraries (WIL) is a header-only C++ library created to make life easier for developers on Windows through readable type-safe C++ interfaces for common Windows coding patterns.  
### [Interop Compositor](https://blog.adeltax.com/interopcompositor-and-coredispatcher/)
Saved me some decompiling and reverse engineering time thanks to ADeltaX's blog!
### [Win32Acrylic](https://github.com/ALTaleX531/Win32Acrylic)
Win2D sucks!