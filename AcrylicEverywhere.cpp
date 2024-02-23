#include "pch.h"
#include "HookHelper.hpp"
#include "ThemeHelper.hpp"
#include "SymbolResolver.hpp"
#include "AcrylicEverywhere.hpp"
#include "udwmPrivates.hpp"
#include "VisualManager.hpp"

namespace AcrylicEverywhere
{
	DWORD WINAPI ThreadProc(PVOID parameter);

	struct MILCMD
	{
		HWND GetHwnd() const
		{
			return *reinterpret_cast<HWND*>(reinterpret_cast<ULONG_PTR>(this) + 4);
		}
		LPCRECT GetRect() const
		{
			return reinterpret_cast<LPCRECT>(reinterpret_cast<ULONG_PTR>(this) + 12);
		}
	};
	struct MILCMD_DWM_REDIRECTION_ACCENTBLURRECTUPDATE : MILCMD {};
	struct MyCWindowList : uDwmPrivates::CWindowList
	{
		HRESULT STDMETHODCALLTYPE UpdateAccentBlurRect(const MILCMD_DWM_REDIRECTION_ACCENTBLURRECTUPDATE* milCmd);
	};
	struct MyCTopLevelWindow : uDwmPrivates::CTopLevelWindow
	{
		HRESULT STDMETHODCALLTYPE OnClipUpdated();
		HRESULT STDMETHODCALLTYPE OnAccentPolicyUpdated();
		DWORD STDMETHODCALLTYPE GetWindowColorizationColor(UCHAR flags);
		DWORD STDMETHODCALLTYPE GetCaptionColor();
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
	struct MyCAccent : uDwmPrivates::CAccent
	{
		HRESULT STDMETHODCALLTYPE ValidateVisual();
	};
	struct MyCTopLevelWindow3D : uDwmPrivates::CTopLevelWindow3D
	{
		inline static thread_local MyCTopLevelWindow3D* s_window3D{ nullptr };
		HRESULT STDMETHODCALLTYPE EnsureRenderData();
	};
	struct MyCSecondaryWindowRepresentation : uDwmPrivates::CSecondaryWindowRepresentation
	{
		uDwmPrivates::CCachedVisualImageProxy* STDMETHODCALLTYPE CreateCVIForAnimation(
			bool freeze
		);
		void STDMETHODCALLTYPE Destructor();
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
	PVOID g_CWindowList_UpdateAccentBlurRect_Org{ nullptr };
	PVOID g_CTopLevelWindow_GetWindowColorizationColor_Org{ nullptr };
	PVOID g_CTopLevelWindow_GetCaptionColor_Org{ nullptr };
	PVOID g_CTopLevelWindow_OnClipUpdated_Org{ nullptr };
	PVOID g_CTopLevelWindow_OnAccentPolicyUpdated_Org{ nullptr };
	PVOID g_CTopLevelWindow_Destructor_Org{ nullptr };
	PVOID g_CTopLevelWindow_InitializeVisualTreeClone_Org{ nullptr };
	PVOID g_CTopLevelWindow_CloneVisualTree_Org{ nullptr };
	PVOID g_CTopLevelWindow_UpdateNCAreaBackground_Org{ nullptr };
	PVOID g_CTopLevelWindow_ValidateVisual_Org{ nullptr };

	PVOID g_CTopLevelWindow3D_EnsureRenderData_Org{ nullptr };
	PVOID g_CSecondaryWindowRepresentation_CreateCVIForAnimation_Org{ nullptr };
	PVOID g_CSecondaryWindowRepresentation_Destructor_Org{ nullptr };

	PVOID g_ResourceHelper_CreateGeometryFromHRGN_Org{ nullptr };
	PVOID g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org{ nullptr };
	PVOID g_FillRect_Org{ nullptr };
	PVOID g_DrawTextW_Org{ nullptr };

	bool g_outOfLoaderLock{ false };
	CVisualManager g_visualManager;
}

HRESULT STDMETHODCALLTYPE AcrylicEverywhere::MyCWindowList::UpdateAccentBlurRect(const MILCMD_DWM_REDIRECTION_ACCENTBLURRECTUPDATE* milCmd)
{
	HRESULT hr{ S_OK };

	hr = (this->*HookHelper::union_cast<decltype(&MyCWindowList::UpdateAccentBlurRect)>(g_CWindowList_UpdateAccentBlurRect_Org))(milCmd);
	
	auto lock{ wil::EnterCriticalSection(uDwmPrivates::CDesktopManager::s_csDwmInstance) };
	uDwmPrivates::CWindowData* windowData{ nullptr };
	uDwmPrivates::CTopLevelWindow* window{ nullptr };
	winrt::com_ptr<CCompositedAccentBackdrop> backdrop{ nullptr };

	if (
		SUCCEEDED(hr) &&
		SUCCEEDED(GetSyncedWindowDataByHwnd(milCmd->GetHwnd(), &windowData)) &&
		windowData &&
		(window = windowData->GetWindow()) &&
		window->GetAccent() &&
		(backdrop = g_visualManager.GetOrCreateAccentBackdrop(window))
	)
	{
		backdrop->UpdateClipRegion();
	}

	return hr;
}

HRESULT STDMETHODCALLTYPE AcrylicEverywhere::MyCTopLevelWindow::OnClipUpdated()
{
	HRESULT hr{ S_OK };

	hr = (this->*HookHelper::union_cast<decltype(&MyCTopLevelWindow::OnClipUpdated)>(g_CTopLevelWindow_OnClipUpdated_Org))();

	uDwmPrivates::CWindowData* windowData{ GetWindowData() };
	if (windowData)
	{
		winrt::com_ptr<CCompositedAccentBackdrop> backdrop{ nullptr };
		if (
			uDwmPrivates::IsAccentBlurEnabled(windowData->GetAccentPolicy()) &&
			(backdrop = g_visualManager.GetOrCreateAccentBackdrop(this, true))
		)
		{
			backdrop->UpdateClipRegion();
		}
		else if (!uDwmPrivates::IsAccentBlurEnabled(windowData->GetAccentPolicy()))
		{
			g_visualManager.RemoveAccent(this);
		}
	}

	return hr;
}

HRESULT STDMETHODCALLTYPE AcrylicEverywhere::MyCTopLevelWindow::OnAccentPolicyUpdated()
{
	HRESULT hr{ S_OK };

	hr = (this->*HookHelper::union_cast<decltype(&MyCTopLevelWindow::OnAccentPolicyUpdated)>(g_CTopLevelWindow_OnAccentPolicyUpdated_Org))();

	uDwmPrivates::CWindowData* windowData{ GetWindowData() };
	if (windowData)
	{
		winrt::com_ptr<CCompositedAccentBackdrop> backdrop{ nullptr };
		if (
			uDwmPrivates::IsAccentBlurEnabled(windowData->GetAccentPolicy()) &&
			(backdrop = g_visualManager.GetOrCreateAccentBackdrop(this, true))
		)
		{
			backdrop->UpdateAccentPolicy();
		}
		else if (!uDwmPrivates::IsAccentBlurEnabled(windowData->GetAccentPolicy()))
		{
			g_visualManager.RemoveAccent(this);
		}
	}

	return hr;
}

DWORD STDMETHODCALLTYPE AcrylicEverywhere::MyCTopLevelWindow::GetWindowColorizationColor(UCHAR flags)
{
	winrt::com_ptr<CCompositedBackdrop> backdrop{ nullptr };
	if (backdrop = g_visualManager.GetOrCreateBackdrop(this))
	{
		auto mainBackdrop{ backdrop->GetMainBackdrop() };
		if (mainBackdrop)
		{
			auto color { mainBackdrop->currentColor };
			return ((color.A << 24) | (color.R << 16) | (color.G << 8) | color.B);
		}
	}

	return (this->*HookHelper::union_cast<decltype(&MyCTopLevelWindow::GetWindowColorizationColor)>(g_CTopLevelWindow_GetWindowColorizationColor_Org))(flags);
}

DWORD STDMETHODCALLTYPE AcrylicEverywhere::MyCTopLevelWindow::GetCaptionColor()
{
	winrt::com_ptr<CCompositedBackdrop> backdrop{ nullptr };
	if (backdrop = g_visualManager.GetOrCreateBackdrop(this))
	{
		auto mainBackdrop{ backdrop->GetMainBackdrop() };
		if (mainBackdrop)
		{
			auto color{ mainBackdrop->currentColor };
			return ((color.A << 24) | (color.R << 16) | (color.G << 8) | color.B);
		}
	}

	return (this->*HookHelper::union_cast<decltype(&MyCTopLevelWindow::GetCaptionColor)>(g_CTopLevelWindow_GetCaptionColor_Org))();
}

HRESULT STDMETHODCALLTYPE AcrylicEverywhere::MyCTopLevelWindow::InitializeVisualTreeClone(CTopLevelWindow* topLevelWindow, UINT cloneOptions)
{
	HRESULT hr{ S_OK };

	hr = (this->*HookHelper::union_cast<decltype(&MyCTopLevelWindow::InitializeVisualTreeClone)>(g_CTopLevelWindow_InitializeVisualTreeClone_Org))(topLevelWindow, cloneOptions);
	
	if (SUCCEEDED(hr))
	{
		g_visualManager.TryCloneBackdropForWindow(this, topLevelWindow);
		g_visualManager.TryCloneAccentBackdropForWindow(this, topLevelWindow);
	}

	return hr;
}

HRESULT STDMETHODCALLTYPE AcrylicEverywhere::MyCTopLevelWindow::CloneVisualTree(uDwmPrivates::CTopLevelWindow** topLevelWindow, bool a1, bool a2, bool a3)
{
	HRESULT hr{ S_OK };

	hr = (this->*HookHelper::union_cast<decltype(&MyCTopLevelWindow::CloneVisualTree)>(g_CTopLevelWindow_CloneVisualTree_Org))(topLevelWindow, a1, a3, a3);

	return hr;
}

HRESULT STDMETHODCALLTYPE AcrylicEverywhere::MyCTopLevelWindow::UpdateNCAreaBackground()
{
	HRESULT hr{ S_OK };

	MyResourceHelper::g_resourceStorage.windowOfInterest = this;
	MyResourceHelper::g_resourceStorage.borderGeometry = GetBorderGeometry();
	MyResourceHelper::g_resourceStorage.titlebarGeometry = GetTitlebarGeometry();

	hr = (this->*HookHelper::union_cast<decltype(&MyCTopLevelWindow::UpdateNCAreaBackground)>(g_CTopLevelWindow_UpdateNCAreaBackground_Org))();

	MyResourceHelper::g_resourceStorage.windowOfInterest = nullptr;
	MyResourceHelper::g_resourceStorage.borderGeometry = nullptr;
	MyResourceHelper::g_resourceStorage.titlebarGeometry = nullptr;

	winrt::com_ptr<CCompositedBackdrop> backdrop{ nullptr };
	uDwmPrivates::CWindowData* windowData{ GetWindowData() };

	if (
		windowData && 
		HasNonClientBackground() && 
		!uDwmPrivates::IsAccentBlurEnabled(windowData->GetAccentPolicy()) &&
		(backdrop = g_visualManager.GetOrCreateBackdrop(this, true))
	)
	{
		if (
			MyResourceHelper::g_resourceStorage.borderRgn && 
			MyResourceHelper::g_resourceStorage.titlebarRgn
		)
		{
			wil::unique_hrgn compositedRgn{ CreateRectRgn(0, 0, 0, 0) };
			
			CombineRgn(
				compositedRgn.get(),
				MyResourceHelper::g_resourceStorage.borderRgn.get(),
				MyResourceHelper::g_resourceStorage.titlebarRgn.get(),
				RGN_OR
			);
			backdrop->UpdateClipRegion(compositedRgn.get());
		}
		else
		{
			backdrop->UpdateClipRegion(nullptr);
		}
		backdrop->Update();
	}
	else if (g_visualManager.GetOrCreateBackdrop(this))
	{
		g_visualManager.Remove(this);
	}

	MyResourceHelper::g_resourceStorage.borderRgn.reset();
	MyResourceHelper::g_resourceStorage.titlebarRgn.reset();

	return hr;
}

HRESULT STDMETHODCALLTYPE AcrylicEverywhere::MyCTopLevelWindow::ValidateVisual()
{
	HRESULT hr{ (this->*HookHelper::union_cast<decltype(&MyCTopLevelWindow::ValidateVisual)>(g_CTopLevelWindow_ValidateVisual_Org))() };
	
	uDwmPrivates::CWindowData* windowData{ GetWindowData() };
	if (windowData)
	{
		{
			winrt::com_ptr<CCompositedBackdrop> backdrop{ nullptr };
			if (
				HasNonClientBackground() &&
				(backdrop = g_visualManager.GetOrCreateBackdrop(this, true))
			)
			{
				backdrop->UpdateGlassReflection();
			}
		}
		{
			winrt::com_ptr<CCompositedAccentBackdrop> backdrop{ nullptr };
			if (
				uDwmPrivates::IsAccentBlurEnabled(windowData->GetAccentPolicy()) &&
				(backdrop = g_visualManager.GetOrCreateAccentBackdrop(this, true))
			)
			{
				backdrop->Update();
			}
			else if (!uDwmPrivates::IsAccentBlurEnabled(windowData->GetAccentPolicy()))
			{
				g_visualManager.RemoveAccent(this);
			}
		}
	}

	return hr;
}

void STDMETHODCALLTYPE AcrylicEverywhere::MyCTopLevelWindow::Destructor()
{
	g_visualManager.Remove(this);
	g_visualManager.RemoveAccent(this);
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

HRESULT STDMETHODCALLTYPE AcrylicEverywhere::MyCTopLevelWindow3D::EnsureRenderData()
{
	HRESULT hr{ S_OK };

	s_window3D = this;
	uDwmPrivates::CTopLevelWindow* window{ nullptr };
	winrt::com_ptr<CCompositedBackdrop> backdrop{ nullptr };
	if (
		GetWindowData() &&
		(window = GetWindowData()->GetWindow()) &&
		(backdrop = g_visualManager.GetOrCreateBackdrop(window, this))
	)
	{
		window->ValidateVisual();
	}

	hr = (this->*HookHelper::union_cast<decltype(&MyCTopLevelWindow3D::EnsureRenderData)>(g_CTopLevelWindow3D_EnsureRenderData_Org))();
	s_window3D = nullptr;

	return hr;
}

AcrylicEverywhere::uDwmPrivates::CCachedVisualImageProxy* STDMETHODCALLTYPE AcrylicEverywhere::MyCSecondaryWindowRepresentation::CreateCVIForAnimation(bool freeze)
{
	uDwmPrivates::CCachedVisualImageProxy* result{nullptr};

	auto window{ GetWindowData()->GetWindow() };
	if (
		window &&
		MyCTopLevelWindow3D::s_window3D
	)
	{
		OutputDebugStringW(__FUNCTIONW__);
		auto updateClonedAnimatedBackdrop = [this](auto& backdrop)
		{
			auto pt{ GetOffset() };
			auto udwmVisual
			{
				backdrop->GetSharedVisual().udwmVisual
			};
			udwmVisual->SetOffset(
				{
					-pt.x,
					-pt.y
				}
			);
			THROW_IF_FAILED(udwmVisual->UpdateOffset());

			winrt::com_ptr<uDwmPrivates::CDrawVisualTreeInstruction> instruction{ nullptr };
			THROW_IF_FAILED(
				uDwmPrivates::CDrawVisualTreeInstruction::Create(
					instruction.put(),
					udwmVisual.get()
				)
			);
			MyCTopLevelWindow3D::s_window3D->AddInstruction(
				instruction.get()
			);
		};
		{
			winrt::com_ptr<CCompositedBackdrop> backdrop{ nullptr };
			if ((backdrop = g_visualManager.TryCloneAnimatedBackdropForWindow(window, this)))
			{
				updateClonedAnimatedBackdrop(backdrop);
			}
		}
		{
			winrt::com_ptr<CCompositedAccentBackdrop> backdrop{ nullptr };
			if ((backdrop = g_visualManager.TryCloneAnimatedAccentBackdropForWindow(window, this)))
			{
				updateClonedAnimatedBackdrop(backdrop);
			}
		}
	}
	
	result = (this->*HookHelper::union_cast<decltype(&MyCSecondaryWindowRepresentation::CreateCVIForAnimation)>(g_CSecondaryWindowRepresentation_CreateCVIForAnimation_Org))(freeze);

	return result;
}

void STDMETHODCALLTYPE AcrylicEverywhere::MyCSecondaryWindowRepresentation::Destructor()
{
	g_visualManager.RemoveAnimatedBackdrop(this);
	g_visualManager.RemoveAnimatedAccentBackdrop(this);
	OutputDebugStringW(__FUNCTIONW__);
	(this->*HookHelper::union_cast<decltype(&MyCSecondaryWindowRepresentation::Destructor)>(g_CSecondaryWindowRepresentation_Destructor_Org))();
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
	uDwmPrivates::CDesktopManager::s_csDwmInstance = reinterpret_cast<LPCRITICAL_SECTION>(uDwmPrivates::g_offsetMap.at("CDesktopManager::s_csDwmInstance").To<PVOID>(uDwmPrivates::g_udwmModule.get()));
	g_CWindowList_UpdateAccentBlurRect_Org = uDwmPrivates::g_offsetMap.at("CWindowList::UpdateAccentBlurRect").To(uDwmPrivates::g_udwmModule.get());
	if (SystemHelper::g_buildNumber < 22621)
	{
		g_CTopLevelWindow_GetWindowColorizationColor_Org = uDwmPrivates::g_offsetMap.at("CTopLevelWindow::GetWindowColorizationColor").To(uDwmPrivates::g_udwmModule.get());
	}
	else
	{
		g_CTopLevelWindow_GetCaptionColor_Org = uDwmPrivates::g_offsetMap.at("CTopLevelWindow::GetCaptionColor").To(uDwmPrivates::g_udwmModule.get());
	}
	g_CTopLevelWindow_OnClipUpdated_Org = uDwmPrivates::g_offsetMap.at("CTopLevelWindow::OnClipUpdated").To(uDwmPrivates::g_udwmModule.get());
	g_CTopLevelWindow_OnAccentPolicyUpdated_Org = uDwmPrivates::g_offsetMap.at("CTopLevelWindow::OnAccentPolicyUpdated").To(uDwmPrivates::g_udwmModule.get());
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

	g_CTopLevelWindow3D_EnsureRenderData_Org = uDwmPrivates::g_offsetMap.at("CTopLevelWindow3D::EnsureRenderData").To(uDwmPrivates::g_udwmModule.get());
	g_CSecondaryWindowRepresentation_CreateCVIForAnimation_Org = uDwmPrivates::g_offsetMap.at("CSecondaryWindowRepresentation::CreateCVIForAnimation").To(uDwmPrivates::g_udwmModule.get());
	g_CSecondaryWindowRepresentation_Destructor_Org = uDwmPrivates::g_offsetMap.at("CSecondaryWindowRepresentation::~CSecondaryWindowRepresentation").To(uDwmPrivates::g_udwmModule.get());

	if (SystemHelper::g_buildNumber < 22621)
	{
		g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org = HookHelper::WritePointer(
			&HookHelper::GetObjectVTable(uDwmPrivates::CDesktopManager::s_pDesktopManagerInstance->GetWICFactory())[21],
			HookHelper::union_cast<PVOID>(&MyIWICImagingFactory2::MyCreateBitmapFromHBITMAP)
		);
	}
	HookHelper::Detours::Write([]
	{
		HookHelper::Detours::Attach(&g_CWindowList_UpdateAccentBlurRect_Org, HookHelper::union_cast<PVOID>(&MyCWindowList::UpdateAccentBlurRect));
		if (SystemHelper::g_buildNumber < 22621)
		{
			HookHelper::Detours::Attach(&g_CTopLevelWindow_GetWindowColorizationColor_Org, HookHelper::union_cast<PVOID>(&MyCTopLevelWindow::GetWindowColorizationColor));
		}
		else
		{
			HookHelper::Detours::Attach(&g_CTopLevelWindow_GetCaptionColor_Org, HookHelper::union_cast<PVOID>(&MyCTopLevelWindow::GetCaptionColor));
		}
		HookHelper::Detours::Attach(&g_CTopLevelWindow_OnClipUpdated_Org, HookHelper::union_cast<PVOID>(&MyCTopLevelWindow::OnClipUpdated));
		HookHelper::Detours::Attach(&g_CTopLevelWindow_OnAccentPolicyUpdated_Org, HookHelper::union_cast<PVOID>(&MyCTopLevelWindow::OnAccentPolicyUpdated));
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

		HookHelper::Detours::Attach(&g_CTopLevelWindow3D_EnsureRenderData_Org, HookHelper::union_cast<PVOID>(&MyCTopLevelWindow3D::EnsureRenderData));
		HookHelper::Detours::Attach(&g_CSecondaryWindowRepresentation_CreateCVIForAnimation_Org, HookHelper::union_cast<PVOID>(&MyCSecondaryWindowRepresentation::CreateCVIForAnimation));
		HookHelper::Detours::Attach(&g_CSecondaryWindowRepresentation_Destructor_Org, HookHelper::union_cast<PVOID>(&MyCSecondaryWindowRepresentation::Destructor));
	});
	ThemeHelper::RefreshTheme();

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
		HookHelper::Detours::Detach(&g_CWindowList_UpdateAccentBlurRect_Org, HookHelper::union_cast<PVOID>(&MyCWindowList::UpdateAccentBlurRect));
		if (SystemHelper::g_buildNumber < 22621)
		{
			HookHelper::Detours::Detach(&g_CTopLevelWindow_GetWindowColorizationColor_Org, HookHelper::union_cast<PVOID>(&MyCTopLevelWindow::GetWindowColorizationColor));
		}
		else
		{
			HookHelper::Detours::Detach(&g_CTopLevelWindow_GetCaptionColor_Org, HookHelper::union_cast<PVOID>(&MyCTopLevelWindow::GetCaptionColor));
		}
		HookHelper::Detours::Detach(&g_CTopLevelWindow_OnClipUpdated_Org, HookHelper::union_cast<PVOID>(&MyCTopLevelWindow::OnClipUpdated));
		HookHelper::Detours::Detach(&g_CTopLevelWindow_OnAccentPolicyUpdated_Org, HookHelper::union_cast<PVOID>(&MyCTopLevelWindow::OnAccentPolicyUpdated));
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

		HookHelper::Detours::Detach(&g_CTopLevelWindow3D_EnsureRenderData_Org, HookHelper::union_cast<PVOID>(&MyCTopLevelWindow3D::EnsureRenderData));
		HookHelper::Detours::Detach(&g_CSecondaryWindowRepresentation_CreateCVIForAnimation_Org, HookHelper::union_cast<PVOID>(&MyCSecondaryWindowRepresentation::CreateCVIForAnimation));
		HookHelper::Detours::Detach(&g_CSecondaryWindowRepresentation_Destructor_Org, HookHelper::union_cast<PVOID>(&MyCSecondaryWindowRepresentation::Destructor));
	});
	g_visualManager.Shutdown();

	if (SystemHelper::g_buildNumber < 22621)
	{
		g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org = HookHelper::WritePointer(
			&HookHelper::GetObjectVTable(uDwmPrivates::CDesktopManager::s_pDesktopManagerInstance->GetWICFactory())[21],
			g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org
		);
	}
	g_FillRect_Org = HookHelper::WriteIAT(uDwmPrivates::g_udwmModule.get(), "user32.dll", "FillRect", g_FillRect_Org);
	g_DrawTextW_Org = HookHelper::WriteIAT(uDwmPrivates::g_udwmModule.get(), "user32.dll", "DrawTextW", g_DrawTextW_Org);

	THROW_IF_FAILED(
		uDwmPrivates::CDesktopManager::s_pDesktopManagerInstance->GetDCompositionInteropDevice()->WaitForCommitCompletion()
	);
}