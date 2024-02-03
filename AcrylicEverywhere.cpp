#include "pch.h"
#include "HookHelper.hpp"
#include "ThemeHelper.hpp"
#include "SymbolResolver.hpp"
#include "AcrylicEverywhere.hpp"
#include "udwmPrivates.hpp"
#include "BackdropMaterials.hpp"

namespace AcrylicEverywhere
{
	LRESULT CALLBACK WndProc(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam);
	DWORD WINAPI ThreadProc(PVOID parameter);

	struct MyCTopLevelWindow : uDwmPrivates::CTopLevelWindow
	{
		HRESULT STDMETHODCALLTYPE InitializeVisualTreeClone(CTopLevelWindow* topLevelWindow, UINT cloneOptions);
		HRESULT STDMETHODCALLTYPE CloneVisualTree(uDwmPrivates::CTopLevelWindow** topLevelWindow, bool a1, bool a2, bool a3);
		HRESULT STDMETHODCALLTYPE UpdateNCAreaBackground();
		HRESULT STDMETHODCALLTYPE ValidateVisual();
		void STDMETHODCALLTYPE Destructor();
	};
	namespace MyResourceHelper
	{
		thread_local struct CTopLevelWindowNCGeometry
		{
			uDwmPrivates::CTopLevelWindow* windowOfInterest{nullptr};
			uDwmPrivates::CRgnGeometryProxy* const* borderGeometry{ nullptr };
			wil::unique_hrgn borderRgn{ nullptr };
			uDwmPrivates::CRgnGeometryProxy* const* titlebarGeometry{ nullptr };
			wil::unique_hrgn titlebarRgn{ nullptr };
		} g_resourceStorage;

		HRESULT STDMETHODCALLTYPE CreateGeometryFromHRGN(
			HRGN hrgn,
			uDwmPrivates::CRgnGeometryProxy** geometry
		);
	};
	struct MyIWICImagingFactory2 : IWICImagingFactory2
	{
		HRESULT STDMETHODCALLTYPE MyCreateBitmapFromHBITMAP(
			HBITMAP hBitmap,
			HPALETTE hPalette,
			WICBitmapAlphaChannelOption options,
			IWICBitmap** ppIBitmap
		);
	};

	int WINAPI MyFillRect(
		HDC hdc,
		LPRECT pRect,
		HBRUSH hBrush
	);
	int WINAPI MyDrawTextW(
		HDC hdc,
		LPCWSTR lpchText,
		int cchText,
		LPRECT lprc,
		UINT format
	);
	PVOID g_CTopLevelWindow_Destructor_Org{ nullptr };
	PVOID g_CTopLevelWindow_InitializeVisualTreeClone_Org{ nullptr };
	PVOID g_CTopLevelWindow_CloneVisualTree_Org{ nullptr };
	PVOID g_CTopLevelWindow_UpdateNCAreaBackground_Org{ nullptr };
	PVOID g_CTopLevelWindow_ValidateVisual_Org{ nullptr };
	PVOID g_ResourceHelper_CreateGeometryFromHRGN_Org{ nullptr };
	PVOID g_CGlassColorizationParameters_AdjustWindowColorization_Org{ nullptr };
	PVOID g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org{ nullptr };
	PVOID g_FillRect_Org{ nullptr };
	PVOID g_DrawTextW_Org{ nullptr };

	class CBackdropManager
	{
	public:
		std::shared_ptr<CCompositedBackdrop> GetOrCreateBackdrop(uDwmPrivates::CTopLevelWindow* topLevelWindow, bool createIfNecessary = false);
		std::shared_ptr<CCompositedBackdrop> CreateWithGivenBackdrop(uDwmPrivates::CTopLevelWindow* topLevelWindow, std::shared_ptr<CCompositedBackdrop> backdrop);
		std::shared_ptr<CCompositedBackdrop> Remove(uDwmPrivates::CTopLevelWindow* topLevelWindow);
		void Shutdown();
		bool MatchVisualCollection(uDwmPrivates::VisualCollection* visualCollection);
	private:
		std::unordered_map<uDwmPrivates::CTopLevelWindow*, std::shared_ptr<CCompositedBackdrop>> m_backdropMap;
	};
	CBackdropManager g_backdropManager{};
	bool g_outOfLoaderLock{ false };
}

std::shared_ptr<AcrylicEverywhere::CCompositedBackdrop> AcrylicEverywhere::CBackdropManager::GetOrCreateBackdrop(uDwmPrivates::CTopLevelWindow* topLevelWindow, bool createIfNecessary)
{
	auto it{ m_backdropMap.find(topLevelWindow) };

	if (createIfNecessary)
	{
		if (it == m_backdropMap.end())
		{
			auto backdrop{ std::make_shared<CCompositedBackdrop>(BackdropType::Aero, true) };
			if (backdrop)
			{
				auto result{ m_backdropMap.emplace(topLevelWindow, backdrop) };
				if (result.second == true)
				{
					it = result.first;
					topLevelWindow->ShowNCBackgroundVisuals(false);
					THROW_IF_FAILED(
						topLevelWindow->GetVisual()->GetVisualCollection()->InsertRelative(backdrop->GetVisual(), nullptr, true, true)
					);
				}
			}
		}
	}

	return it == m_backdropMap.end() ? nullptr : it->second;
}
std::shared_ptr<AcrylicEverywhere::CCompositedBackdrop> AcrylicEverywhere::CBackdropManager::CreateWithGivenBackdrop(uDwmPrivates::CTopLevelWindow* topLevelWindow, std::shared_ptr<CCompositedBackdrop> backdrop)
{
	auto it{ m_backdropMap.find(topLevelWindow) };

	if (it == m_backdropMap.end())
	{
		auto result{ m_backdropMap.emplace(topLevelWindow, backdrop) };
		if (result.second == true)
		{
			it = result.first;
		}
		topLevelWindow->ShowNCBackgroundVisuals(false);
	}
	else
	{
		THROW_IF_FAILED(topLevelWindow->GetVisual()->GetVisualCollection()->Remove(it->second->GetVisual()));
		it->second = backdrop;
	}
	THROW_IF_FAILED(
		topLevelWindow->GetVisual()->GetVisualCollection()->InsertRelative(backdrop->GetVisual(), nullptr, true, true)
	);

	return it->second;
}
std::shared_ptr<AcrylicEverywhere::CCompositedBackdrop> AcrylicEverywhere::CBackdropManager::Remove(uDwmPrivates::CTopLevelWindow* topLevelWindow)
{
	auto it{ m_backdropMap.find(topLevelWindow) };
	std::shared_ptr<CCompositedBackdrop> backdrop{ nullptr };

	if (it != m_backdropMap.end())
	{
		backdrop = it->second;

		topLevelWindow->ShowNCBackgroundVisuals(true);
		THROW_IF_FAILED(topLevelWindow->GetVisual()->GetVisualCollection()->Remove(it->second->GetVisual()));

		m_backdropMap.erase(it);
	}

	return backdrop;
}
bool AcrylicEverywhere::CBackdropManager::MatchVisualCollection(uDwmPrivates::VisualCollection* visualCollection)
{
	for (auto [topLevelWindow, backdrop] : m_backdropMap)
	{
		if (topLevelWindow->GetVisual()->GetVisualCollection() == visualCollection)
		{
			return true;
		}
	}

	return false;
}

void AcrylicEverywhere::CBackdropManager::Shutdown()
{
	for (auto [topLevelWindow, backdrop] : m_backdropMap)
	{
		topLevelWindow->ShowNCBackgroundVisuals(true);
		topLevelWindow->GetVisual()->GetVisualCollection()->Remove(
			backdrop->GetVisual()
		);
	}
	m_backdropMap.clear();
}

HRESULT STDMETHODCALLTYPE AcrylicEverywhere::MyCTopLevelWindow::InitializeVisualTreeClone(CTopLevelWindow* topLevelWindow, UINT cloneOptions)
{
	HRESULT hr{ S_OK };

	bool cloneAllowed{false};
	auto backdrop{ g_backdropManager.GetOrCreateBackdrop(this) };
	if (backdrop)
	{
		cloneAllowed = backdrop->GetVisual()->AllowVisualTreeClone(false);
	}

	hr = (this->*HookHelper::union_cast<decltype(&MyCTopLevelWindow::InitializeVisualTreeClone)>(g_CTopLevelWindow_InitializeVisualTreeClone_Org))(topLevelWindow, cloneOptions);
	
	if (backdrop)
	{
		backdrop->GetVisual()->AllowVisualTreeClone(cloneAllowed);
		if (SUCCEEDED(hr))
		{
			auto clonedBackdrop{ std::make_shared<CCompositedBackdrop>(BackdropType::Aero, true) };
			backdrop->InitializeVisualTreeClone(clonedBackdrop.get());
			g_backdropManager.CreateWithGivenBackdrop(topLevelWindow, clonedBackdrop);
		}
	}
	
	return hr;
}

HRESULT STDMETHODCALLTYPE AcrylicEverywhere::MyCTopLevelWindow::CloneVisualTree(uDwmPrivates::CTopLevelWindow** topLevelWindow, bool a1, bool a2, bool a3)
{
	HRESULT hr{ S_OK };

	bool cloneAllowed{ false };
	auto backdrop{ g_backdropManager.GetOrCreateBackdrop(this) };
	if (backdrop)
	{
		cloneAllowed = backdrop->GetVisual()->AllowVisualTreeClone(false);
	}

	hr = (this->*HookHelper::union_cast<decltype(&MyCTopLevelWindow::CloneVisualTree)>(g_CTopLevelWindow_CloneVisualTree_Org))(topLevelWindow, a1, a3, a3);

	if (topLevelWindow && *topLevelWindow && backdrop)
	{
		backdrop->GetVisual()->AllowVisualTreeClone(cloneAllowed);
		if (SUCCEEDED(hr))
		{
			auto clonedBackdrop{ std::make_shared<CCompositedBackdrop>(BackdropType::Aero, true) };
			backdrop->InitializeVisualTreeClone(clonedBackdrop.get());
			g_backdropManager.CreateWithGivenBackdrop(*topLevelWindow, clonedBackdrop);
		}
	}

	return hr;
}

HRESULT STDMETHODCALLTYPE AcrylicEverywhere::MyCTopLevelWindow::UpdateNCAreaBackground()
{
	HRESULT hr{ S_OK };

	MyResourceHelper::g_resourceStorage.windowOfInterest = this;
	MyResourceHelper::g_resourceStorage.borderGeometry = &GetBorderGeometry();
	MyResourceHelper::g_resourceStorage.titlebarGeometry = &GetTitlebarGeometry();

	hr = (this->*HookHelper::union_cast<decltype(&MyCTopLevelWindow::UpdateNCAreaBackground)>(g_CTopLevelWindow_UpdateNCAreaBackground_Org))();

	MyResourceHelper::g_resourceStorage.windowOfInterest = nullptr;
	MyResourceHelper::g_resourceStorage.borderGeometry = nullptr;
	MyResourceHelper::g_resourceStorage.titlebarGeometry = nullptr;

	std::shared_ptr<CCompositedBackdrop> backdrop{ nullptr };
	uDwmPrivates::CWindowData* windowData{ GetWindowData() };
	if (windowData && !uDwmPrivates::CAccent::s_IsPolicyActive(windowData->GetAccentPolicy()) && HasNonClientBackground() && (backdrop = g_backdropManager.GetOrCreateBackdrop(this, true)))
	{
		if (MyResourceHelper::g_resourceStorage.borderRgn && MyResourceHelper::g_resourceStorage.titlebarRgn)
		{
			wil::unique_hrgn compositedRgn{ CreateRectRgn(0, 0, 0, 0) };
			
			CombineRgn(
				compositedRgn.get(),
				MyResourceHelper::g_resourceStorage.borderRgn.get(),
				MyResourceHelper::g_resourceStorage.titlebarRgn.get(),
				RGN_OR
			); 
			backdrop->Update(
				this,
				IsSystemBackdropApplied() ? nullptr : compositedRgn.get()
			);
		}
		else
		{
			backdrop->Update(
				this,
				IsSystemBackdropApplied() ? nullptr : backdrop->GetClipRegion()
			);
		}
		ShowNCBackgroundVisuals(false);
	}
	else if (g_backdropManager.GetOrCreateBackdrop(this))
	{
		g_backdropManager.Remove(this);
	}

	MyResourceHelper::g_resourceStorage.borderRgn.reset();
	MyResourceHelper::g_resourceStorage.titlebarRgn.reset();

	return hr;
}

HRESULT STDMETHODCALLTYPE AcrylicEverywhere::MyCTopLevelWindow::ValidateVisual()
{
	std::shared_ptr<CCompositedBackdrop> backdrop{ nullptr };
	CGlassReflectionBackdrop* glassReflection{ nullptr };
	uDwmPrivates::CWindowData* windowData{ GetWindowData() };
	if (windowData && !uDwmPrivates::CAccent::s_IsPolicyActive(windowData->GetAccentPolicy()) && (backdrop = g_backdropManager.GetOrCreateBackdrop(this)) && (glassReflection = backdrop->GetGlassReflection()))
	{
		glassReflection->UpdateBackdrop(this);
	}
	return (this->*HookHelper::union_cast<decltype(&MyCTopLevelWindow::ValidateVisual)>(g_CTopLevelWindow_ValidateVisual_Org))();
}

void STDMETHODCALLTYPE AcrylicEverywhere::MyCTopLevelWindow::Destructor()
{
	g_backdropManager.Remove(this);

	return (this->*HookHelper::union_cast<decltype(&MyCTopLevelWindow::Destructor)>(g_CTopLevelWindow_Destructor_Org))();
}

HRESULT STDMETHODCALLTYPE AcrylicEverywhere::MyResourceHelper::CreateGeometryFromHRGN(HRGN hrgn, uDwmPrivates::CRgnGeometryProxy** geometry)
{
	if (g_resourceStorage.windowOfInterest)
	{
		if (geometry == g_resourceStorage.borderGeometry)
		{
			if (!g_resourceStorage.borderRgn)
			{
				g_resourceStorage.borderRgn.reset(CreateRectRgn(0, 0, 0, 0));
			}
			CopyRgn(g_resourceStorage.borderRgn.get(), hrgn);
		}
		if (geometry == g_resourceStorage.titlebarGeometry)
		{
			if (!g_resourceStorage.titlebarRgn)
			{
				g_resourceStorage.titlebarRgn.reset(CreateRectRgn(0, 0, 0, 0));
			}
			CopyRgn(g_resourceStorage.titlebarRgn.get(), hrgn);
		}
	}
	
	return reinterpret_cast<decltype(&MyResourceHelper::CreateGeometryFromHRGN)>(g_ResourceHelper_CreateGeometryFromHRGN_Org)(hrgn, geometry);
}

HRESULT STDMETHODCALLTYPE AcrylicEverywhere::MyIWICImagingFactory2::MyCreateBitmapFromHBITMAP(
	HBITMAP hBitmap,
	HPALETTE hPalette,
	WICBitmapAlphaChannelOption options,
	IWICBitmap** ppIBitmap
)
{
	return (this->*HookHelper::union_cast<decltype(&MyIWICImagingFactory2::MyCreateBitmapFromHBITMAP)>(g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org))(
		hBitmap, hPalette, WICBitmapUsePremultipliedAlpha, ppIBitmap
	);
}
int WINAPI AcrylicEverywhere::MyFillRect(
	HDC hdc,
	LPRECT pRect,
	HBRUSH hBrush
)
{
	return reinterpret_cast<decltype(&MyFillRect)>(g_FillRect_Org)(hdc, pRect, GetStockBrush(BLACK_BRUSH));
}
int WINAPI AcrylicEverywhere::MyDrawTextW(
	HDC hdc,
	LPCWSTR lpchText,
	int cchText,
	LPRECT lprc,
	UINT format
)
{
	if ((format & DT_CALCRECT) || (format & DT_INTERNAL) || (format & DT_NOCLIP))
	{
		return reinterpret_cast<decltype(&MyDrawTextW)>(g_DrawTextW_Org)(hdc, lpchText, cchText, lprc, format);
	}
	return ThemeHelper::DrawTextWithAlpha(hdc, lpchText, cchText, lprc, format, reinterpret_cast<decltype(&MyDrawTextW)>(g_DrawTextW_Org));
}

LRESULT CALLBACK AcrylicEverywhere::WndProc(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
	return 0;
}

DWORD WINAPI AcrylicEverywhere::ThreadProc(PVOID parameter)
{
	while (!g_outOfLoaderLock)
	{
		Sleep(50);
	}
	Sleep(100);

	SymbolResolver symbolResolver{};
	HRESULT hr = symbolResolver.Walk(L"udwm.dll", "*!*", [](PSYMBOL_INFO symInfo, ULONG symbolSize) -> bool
	{
		auto functionName{ reinterpret_cast<const CHAR*>(symInfo->Name) };
		CHAR fullyUnDecoratedFunctionName[MAX_PATH + 1]{};
		UnDecorateSymbolName(
			functionName, fullyUnDecoratedFunctionName, MAX_PATH,
			UNDNAME_NAME_ONLY
		);
		auto functionOffset{ HookHelper::OffsetStorage::From(symInfo->ModBase, symInfo->Address) };
		uDwmPrivates::g_offsetMap.emplace(std::string{ fullyUnDecoratedFunctionName }, HookHelper::OffsetStorage::From(symInfo->ModBase, symInfo->Address));

		return true;
	});
	if (FAILED(hr))
	{
		return hr;
	}

	uDwmPrivates::CDesktopManager::s_pDesktopManagerInstance = reinterpret_cast<uDwmPrivates::CDesktopManager*>(*uDwmPrivates::g_offsetMap.at("CDesktopManager::s_pDesktopManagerInstance").To<PVOID*>(uDwmPrivates::g_udwmModule.get()));
	g_CTopLevelWindow_UpdateNCAreaBackground_Org = uDwmPrivates::g_offsetMap.at("CTopLevelWindow::UpdateNCAreaBackground").To(uDwmPrivates::g_udwmModule.get());
	g_CTopLevelWindow_ValidateVisual_Org = uDwmPrivates::g_offsetMap.at("CTopLevelWindow::ValidateVisual").To(uDwmPrivates::g_udwmModule.get());
	if (SystemHelper::g_buildNumber > 18363)
	{
		g_CTopLevelWindow_InitializeVisualTreeClone_Org = uDwmPrivates::g_offsetMap.at("CTopLevelWindow::InitializeVisualTreeClone").To(uDwmPrivates::g_udwmModule.get());
	}
	else
	{
		g_CTopLevelWindow_CloneVisualTree_Org = uDwmPrivates::g_offsetMap.at("CTopLevelWindow::CloneVisualTree").To(uDwmPrivates::g_udwmModule.get());
	}
	g_CTopLevelWindow_Destructor_Org = uDwmPrivates::g_offsetMap.at("CTopLevelWindow::~CTopLevelWindow").To(uDwmPrivates::g_udwmModule.get());
	g_ResourceHelper_CreateGeometryFromHRGN_Org = uDwmPrivates::g_offsetMap.at("ResourceHelper::CreateGeometryFromHRGN").To(uDwmPrivates::g_udwmModule.get());
	g_CGlassColorizationParameters_AdjustWindowColorization_Org = uDwmPrivates::g_offsetMap.at("CGlassColorizationParameters::AdjustWindowColorization").To(uDwmPrivates::g_udwmModule.get());

	if (SystemHelper::g_buildNumber < 22621)
	{
		g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org = HookHelper::WritePointer(
			&HookHelper::GetObjectVTable(uDwmPrivates::CDesktopManager::s_pDesktopManagerInstance->GetWICFactory())[21],
			HookHelper::union_cast<PVOID>(&MyIWICImagingFactory2::MyCreateBitmapFromHBITMAP)
		);
	}
	HookHelper::Detours::Write([]
	{
		HookHelper::Detours::Attach(&g_CTopLevelWindow_UpdateNCAreaBackground_Org, HookHelper::union_cast<PVOID>(&MyCTopLevelWindow::UpdateNCAreaBackground));
		HookHelper::Detours::Attach(&g_CTopLevelWindow_ValidateVisual_Org, HookHelper::union_cast<PVOID>(&MyCTopLevelWindow::ValidateVisual));
		if (SystemHelper::g_buildNumber > 18363)
		{
			HookHelper::Detours::Attach(&g_CTopLevelWindow_InitializeVisualTreeClone_Org, HookHelper::union_cast<PVOID>(&MyCTopLevelWindow::InitializeVisualTreeClone));
		}
		else
		{
			HookHelper::Detours::Attach(&g_CTopLevelWindow_CloneVisualTree_Org, HookHelper::union_cast<PVOID>(&MyCTopLevelWindow::CloneVisualTree));
		}
		HookHelper::Detours::Attach(&g_CTopLevelWindow_Destructor_Org, HookHelper::union_cast<PVOID>(&MyCTopLevelWindow::Destructor));
		HookHelper::Detours::Attach(&g_ResourceHelper_CreateGeometryFromHRGN_Org, HookHelper::union_cast<PVOID>(&MyResourceHelper::CreateGeometryFromHRGN));
	});

	return S_OK;
}

void AcrylicEverywhere::Startup()
{
	auto cleanUp = wil::scope_exit([] { g_outOfLoaderLock = true; });

	if (!GetModuleHandleW(L"dwm.exe"))
	{
		return;
	}

	uDwmPrivates::g_udwmModule.reset(LoadLibraryExW(L"udwm.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32));
	// WriteIAT
	CreateThread(nullptr, 0, ThreadProc, nullptr, 0, nullptr);
	g_FillRect_Org = HookHelper::WriteIAT(uDwmPrivates::g_udwmModule.get(), "user32.dll", "FillRect", MyFillRect);
	g_DrawTextW_Org = HookHelper::WriteIAT(uDwmPrivates::g_udwmModule.get(), "user32.dll", "DrawTextW", MyDrawTextW);
}

void AcrylicEverywhere::Shutdown()
{
	if (!GetModuleHandleW(L"dwm.exe"))
	{
		return;
	}

	HookHelper::Detours::Write([]
	{
		HookHelper::Detours::Detach(&g_CTopLevelWindow_UpdateNCAreaBackground_Org, HookHelper::union_cast<PVOID>(&MyCTopLevelWindow::UpdateNCAreaBackground));
		HookHelper::Detours::Detach(&g_CTopLevelWindow_ValidateVisual_Org, HookHelper::union_cast<PVOID>(&MyCTopLevelWindow::ValidateVisual));
		if (SystemHelper::g_buildNumber > 18363)
		{
			HookHelper::Detours::Detach(&g_CTopLevelWindow_InitializeVisualTreeClone_Org, HookHelper::union_cast<PVOID>(&MyCTopLevelWindow::InitializeVisualTreeClone));
		}
		else
		{
			HookHelper::Detours::Detach(&g_CTopLevelWindow_CloneVisualTree_Org, HookHelper::union_cast<PVOID>(&MyCTopLevelWindow::CloneVisualTree));
		}
		HookHelper::Detours::Detach(&g_CTopLevelWindow_Destructor_Org, HookHelper::union_cast<PVOID>(&MyCTopLevelWindow::Destructor));
		HookHelper::Detours::Detach(&g_ResourceHelper_CreateGeometryFromHRGN_Org, HookHelper::union_cast<PVOID>(&MyResourceHelper::CreateGeometryFromHRGN));
	});
	g_backdropManager.Shutdown();

	if (SystemHelper::g_buildNumber < 22621)
	{
		g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org = HookHelper::WritePointer(
			&HookHelper::GetObjectVTable(uDwmPrivates::CDesktopManager::s_pDesktopManagerInstance->GetWICFactory())[21],
			g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org
		);
	}
	g_FillRect_Org = HookHelper::WriteIAT(uDwmPrivates::g_udwmModule.get(), "user32.dll", "FillRect", g_FillRect_Org);
	g_DrawTextW_Org = HookHelper::WriteIAT(uDwmPrivates::g_udwmModule.get(), "user32.dll", "DrawTextW", g_DrawTextW_Org);
}