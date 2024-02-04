#pragma once
#include "framework.h"
#include "cpprt.h"
#include "wil.h"
#include "HookHelper.hpp"
#include "dcompPrivates.hpp"

namespace AcrylicEverywhere::uDwmPrivates
{
	inline HANDLE g_hProcessHeap{ GetProcessHeap() };
	inline wil::unique_hmodule g_udwmModule{ nullptr };
	inline std::unordered_map<std::string, HookHelper::OffsetStorage> g_offsetMap{};

	struct CBaseObject
	{
		void* operator new(size_t size)
		{
			return HeapAlloc(g_hProcessHeap, 0, size);
		}
		void operator delete(void* ptr)
		{
			if (ptr)
			{
				HeapFree(g_hProcessHeap, 0, ptr);
			}
		}
		size_t AddRef()
		{
			return InterlockedIncrement(reinterpret_cast<DWORD*>(this) + 2);
		}
		size_t Release()
		{
			auto result{ InterlockedDecrement(reinterpret_cast<DWORD*>(this) + 2) };
			if (!result)
			{
				delete this;
			}
			return result;
		}
		HRESULT QueryInterface(REFIID riid, PVOID* ppvObject) 
		{
			*ppvObject = this;
			return S_OK; 
		}

		template <typename T, typename... Args>
		static HRESULT Create(T** object, Args&&... args) try
		{
			winrt::com_ptr<T> allocatedObject{ new T(std::forward<Args>(args)...), winrt::take_ownership_from_abi };
			THROW_IF_FAILED(allocatedObject->Initialize());

			*object = allocatedObject.detach();
			return S_OK;
		}
		catch(...) { return wil::ResultFromCaughtException(); }

	protected:
		CBaseObject() { *(reinterpret_cast<DWORD*>(this) + 2) = 1; };
		virtual ~CBaseObject() {};
	};

#pragma push_macro("UDWM_CALL_ORG")
#pragma push_macro("UDWM_CALL_ORG_STATIC")
#define UDWM_CALL_ORG(name, ...) [&]{ static const auto s_ptr{ AcrylicEverywhere::uDwmPrivates::g_offsetMap.at(#name).To(AcrylicEverywhere::uDwmPrivates::g_udwmModule.get()) }; return (this->*HookHelper::union_cast<decltype(&name)>(s_ptr))(## __VA_ARGS__); } ()
#define UDWM_CALL_ORG_STATIC(name, ...) [&]{ static const auto s_ptr{AcrylicEverywhere::uDwmPrivates::g_offsetMap.at(#name).To(AcrylicEverywhere::uDwmPrivates::g_udwmModule.get())}; return (HookHelper::union_cast<decltype(&name)>(s_ptr))(## __VA_ARGS__);} ()
#define UDWM_CALL_ORG_BY_TYPE(type, object, name, ...) [&]{ static const auto s_ptr{AcrylicEverywhere::uDwmPrivates::g_offsetMap.at(name).To(AcrylicEverywhere::uDwmPrivates::g_udwmModule.get())}; return (object->*HookHelper::union_cast<type>(s_ptr))(## __VA_ARGS__); } ()
#define UDWM_CALL_ORG_STATIC_BY_TYPE(type, name, ...) [&]{ static const auto s_ptr{AcrylicEverywhere::uDwmPrivates::g_offsetMap.at(name).To(AcrylicEverywhere::uDwmPrivates::g_udwmModule.get())}; return (HookHelper::union_cast<type>(s_ptr))(## __VA_ARGS__); } ()
	struct CWindowList : CBaseObject 
	{
		HRESULT STDMETHODCALLTYPE GetExtendedFrameBounds(
			HWND hwnd,
			RECT* rect
		)
		{
			return UDWM_CALL_ORG(CWindowList::GetExtendedFrameBounds, hwnd, rect);
		}
	};
	struct CBaseGeometryProxy : CBaseObject {};
	struct CCombinedGeometryProxy : CBaseGeometryProxy {};
	struct CRgnGeometryProxy : CBaseGeometryProxy {};
	namespace ResourceHelper
	{
		static HRESULT STDMETHODCALLTYPE CreateCombinedGeometry(CBaseGeometryProxy* geometry1, CBaseGeometryProxy* geometry2, UINT combinedMode, CCombinedGeometryProxy** combinedGeometry)
		{
			return UDWM_CALL_ORG_STATIC(ResourceHelper::CreateCombinedGeometry, geometry1, geometry2, combinedMode, combinedGeometry);
		}
		static HRESULT STDMETHODCALLTYPE CreateGeometryFromHRGN(
			HRGN hrgn,
			uDwmPrivates::CRgnGeometryProxy** geometry
		)
		{
			return UDWM_CALL_ORG_STATIC(ResourceHelper::CreateGeometryFromHRGN, hrgn, geometry);
		}
	}
	struct VisualCollection;
	struct CResourceProxy : CBaseObject {};
	struct CSolidColorLegacyMilBrushProxy : CBaseObject {};
	struct CVisualProxy : CBaseObject 
	{
		HRESULT STDMETHODCALLTYPE SetClip(CBaseGeometryProxy* geometry) { return UDWM_CALL_ORG(CVisualProxy::SetClip, geometry); }
		HRESULT STDMETHODCALLTYPE SetSize(double width, double height) { return UDWM_CALL_ORG(CVisualProxy::SetSize, width, height); }
	};
	struct CVisual : CBaseObject
	{
		VisualCollection* GetVisualCollection() const
		{
			if (SystemHelper::g_buildNumber < 26020)
			{
				return const_cast<VisualCollection*>(reinterpret_cast<VisualCollection const*>(reinterpret_cast<const char*>(this) + 32));
			}
			return const_cast<VisualCollection*>(reinterpret_cast<VisualCollection const*>(reinterpret_cast<const char*>(this) + 144));
		}
		CVisualProxy* GetVisualProxy() const
		{
			return reinterpret_cast<CVisualProxy*>(reinterpret_cast<const ULONG_PTR*>(this)[2]);
		}
		bool AllowVisualTreeClone(bool allow)
		{
			BYTE* properties{nullptr};

			if (SystemHelper::g_buildNumber < 22000)
			{
				properties = &reinterpret_cast<BYTE*>(this)[84];
			}
			else if (SystemHelper::g_buildNumber < 22621)
			{
				properties = &reinterpret_cast<BYTE*>(this)[92];
			}
			else if (SystemHelper::g_buildNumber < 26020)
			{
				properties = &reinterpret_cast<BYTE*>(this)[92];
			}
			else
			{
				properties = &reinterpret_cast<BYTE*>(this)[36];
			}

			bool allowed{false};
			if (properties)
			{
				allowed = (*properties & 8) == 0;
				if (allow)
				{
					*properties = *properties & ~8;
				}
				else
				{
					*properties |= 8;
				}
			}

			return allowed;
		}
		void STDMETHODCALLTYPE SetInsetFromParent(MARGINS* margins)
		{
			return UDWM_CALL_ORG(CVisual::SetInsetFromParent, margins);
		}
		HRESULT STDMETHODCALLTYPE SetSize(const SIZE& size)
		{
			return UDWM_CALL_ORG(CVisual::SetSize, size);
		}
		HRESULT STDMETHODCALLTYPE InitializeVisualTreeClone(CBaseObject* baseObject, UINT cloneOptions)
		{
			return UDWM_CALL_ORG(CVisual::InitializeVisualTreeClone, baseObject, cloneOptions);
		}
		void STDMETHODCALLTYPE Unhide()
		{
			return UDWM_CALL_ORG(CVisual::Unhide, );
		}
		void STDMETHODCALLTYPE Hide()
		{
			return UDWM_CALL_ORG(CVisual::Hide, );
		}
		void STDMETHODCALLTYPE ConnectToParent(bool connect)
		{
			return UDWM_CALL_ORG(CVisual::ConnectToParent, connect);
		}
		void STDMETHODCALLTYPE SetOpacity(double opacity)
		{
			return UDWM_CALL_ORG(CVisual::SetOpacity, opacity);
		}
		void STDMETHODCALLTYPE SetScale(double x, double y)
		{
			return UDWM_CALL_ORG(CVisual::SetScale, x, y);
		}
		HRESULT STDMETHODCALLTYPE SendSetOpacity(double opacity)
		{ 
			return UDWM_CALL_ORG(CVisual::SendSetOpacity, opacity);
		}
		HRESULT STDMETHODCALLTYPE RenderRecursive()
		{
			return UDWM_CALL_ORG(CVisual::RenderRecursive, );
		}
		void STDMETHODCALLTYPE SetDirtyChildren()
		{
			if (SystemHelper::g_buildNumber < 26020)
			{
				return UDWM_CALL_ORG(CVisual::SetDirtyChildren, );
			}
			else
			{
				return UDWM_CALL_ORG_BY_TYPE(decltype(&CVisual::SetDirtyChildren), this, "CContainerVisual");
			}
		}
		static HRESULT WINAPI CreateFromSharedHandle(HANDLE handle, CVisual** visual)
		{
			return UDWM_CALL_ORG_STATIC(CVisual::CreateFromSharedHandle, handle, visual);
		}
		HRESULT STDMETHODCALLTYPE InitializeFromSharedHandle(HANDLE handle) { return UDWM_CALL_ORG(CVisual::InitializeFromSharedHandle, handle); }
		HRESULT WINAPI MoveToFront(bool unknown) { return UDWM_CALL_ORG(CVisual::MoveToFront, unknown); }
		HRESULT STDMETHODCALLTYPE Initialize() { return UDWM_CALL_ORG(CVisual::Initialize, ); }
	};

	struct VisualCollection : CBaseObject
	{
		HRESULT STDMETHODCALLTYPE RemoveAll() { return UDWM_CALL_ORG(VisualCollection::RemoveAll, ); }
		HRESULT STDMETHODCALLTYPE Remove(CVisual* visual) { return UDWM_CALL_ORG(VisualCollection::Remove, visual); }
		HRESULT STDMETHODCALLTYPE InsertRelative(
			CVisual* visual,
			CVisual* referenceVisual,
			bool insertAfter,
			bool updateNow
		)
		{
			return UDWM_CALL_ORG(VisualCollection::InsertRelative, visual, referenceVisual, insertAfter, updateNow);
		}
	};
	struct ACCENT_POLICY;
	struct CAccent : CBaseObject
	{
		static bool STDMETHODCALLTYPE s_IsPolicyActive(const ACCENT_POLICY* accentPolicy)
		{
			return UDWM_CALL_ORG_STATIC(CAccent::s_IsPolicyActive, accentPolicy);
		}
	};
	struct CWindowData : CBaseObject
	{
		HWND GetHwnd() const
		{
			return reinterpret_cast<const HWND*>(this)[5];
		}
		ACCENT_POLICY* GetAccentPolicy() const
		{
			ACCENT_POLICY* accentPolicy{nullptr};

			if (SystemHelper::g_buildNumber >= 22000)
			{
				accentPolicy = (ACCENT_POLICY*)((ULONGLONG)this + 168);
			}
			else
			{
				accentPolicy = (ACCENT_POLICY*)((ULONGLONG)this + 152);
			}

			return accentPolicy;
		}
		bool IsUsingDarkMode() const
		{
			bool darkMode{ false };

			if (SystemHelper::g_buildNumber < 19041)
			{
				// TO-DO
			}
			else if (SystemHelper::g_buildNumber < 22000)
			{
				darkMode = (reinterpret_cast<BYTE const*>(this)[613] & 8) != 0;
			}
			else if (SystemHelper::g_buildNumber < 22621)
			{
				darkMode = (reinterpret_cast<BYTE const*>(this)[669] & 4) != 0;
			}
			else
			{
				darkMode = (reinterpret_cast<BYTE const*>(this)[677] & 4) != 0; // ok with build 26020
			}

			return darkMode;
		}
		DWORD GetNonClientAttribute() const
		{
			DWORD attribute{ 0 };

			if (SystemHelper::g_buildNumber < 19041)
			{
				// TO-DO
			}
			else if (SystemHelper::g_buildNumber < 22000)
			{
				attribute = *reinterpret_cast<const DWORD*>(reinterpret_cast<BYTE const*>(this) + 608);
			}
			else if (SystemHelper::g_buildNumber < 22621)
			{
				attribute = *reinterpret_cast<const DWORD*>(reinterpret_cast<BYTE const*>(this) + 664);
			}
			else
			{
				attribute = *reinterpret_cast<const DWORD*>(reinterpret_cast<BYTE const*>(this) + 672); // ok with build 26020
			}

			return attribute;
		}
		bool STDMETHODCALLTYPE IsImmersiveWindow() const
		{
			return UDWM_CALL_ORG(CWindowData::IsImmersiveWindow, );
		}
	};
	struct CTopLevelWindow : CVisual
	{
		void GetBorderMargins(MARGINS* margins) const
		{
			if (SystemHelper::g_buildNumber < 19041)
			{
				// TO-DO
			}
			else if (SystemHelper::g_buildNumber < 26020)
			{
				UDWM_CALL_ORG_BY_TYPE(RECT * (STDMETHODCALLTYPE uDwmPrivates::CTopLevelWindow::*)(MARGINS*) const, this, "CTopLevelWindow::GetBorderMargins", margins);
			}
			else
			{
				UDWM_CALL_ORG_BY_TYPE(void (STDMETHODCALLTYPE uDwmPrivates::CTopLevelWindow::*)(MARGINS*) const, this, "CTopLevelWindow::GetFrameMargins", margins);
			}
		}
		CRgnGeometryProxy* const& GetBorderGeometry() const
		{
			CRgnGeometryProxy* const* geometry{ nullptr };

			if (SystemHelper::g_buildNumber < 19041)
			{
				// TO-DO
			}
			else if (SystemHelper::g_buildNumber < 22000)
			{
				geometry = &reinterpret_cast<CRgnGeometryProxy* const*>(this)[69];
			}
			else if (SystemHelper::g_buildNumber < 22621)
			{
				geometry = &reinterpret_cast<CRgnGeometryProxy* const*>(this)[71];
			}
			else if (SystemHelper::g_buildNumber < 26020)
			{
				auto legacyBackgroundVisual{ reinterpret_cast<CVisual* const*>(this)[40] };
				if (legacyBackgroundVisual)
				{
					geometry = &reinterpret_cast<CRgnGeometryProxy* const*>(legacyBackgroundVisual)[40];
				}
			}
			else
			{
				auto legacyBackgroundVisual{ reinterpret_cast<CVisual* const*>(this)[34] };
				if (legacyBackgroundVisual)
				{
					geometry = &reinterpret_cast<CRgnGeometryProxy* const*>(legacyBackgroundVisual)[34];
				}
			}

			return *geometry;
		}
		CRgnGeometryProxy* const& GetTitlebarGeometry() const
		{
			CRgnGeometryProxy* const* geometry{nullptr};

			if (SystemHelper::g_buildNumber < 19041)
			{
				// TO-DO
			}
			else if (SystemHelper::g_buildNumber < 22000)
			{
				geometry = &reinterpret_cast<CRgnGeometryProxy* const*>(this)[70];
			}
			else if (SystemHelper::g_buildNumber < 22621)
			{
				geometry = &reinterpret_cast<CRgnGeometryProxy* const*>(this)[72];
			}
			else if (SystemHelper::g_buildNumber < 26020)
			{
				auto legacyBackgroundVisual{ reinterpret_cast<CVisual* const*>(this)[39] };
				if (legacyBackgroundVisual)
				{
					geometry = &reinterpret_cast<CRgnGeometryProxy* const*>(legacyBackgroundVisual)[39];
				}
			}
			else
			{
				auto legacyBackgroundVisual{ reinterpret_cast<CVisual* const*>(this)[33] };
				if (legacyBackgroundVisual)
				{
					geometry = &reinterpret_cast<CRgnGeometryProxy* const*>(legacyBackgroundVisual)[33];
				}
			}

			return *geometry;
		}
		bool STDMETHODCALLTYPE TreatAsActiveWindow()
		{
			return UDWM_CALL_ORG(CTopLevelWindow::TreatAsActiveWindow, );
		}
		RECT* STDMETHODCALLTYPE GetActualWindowRect(
			RECT* rect,
			char eraseOffset,
			char includeNonClient,
			bool excludeBorderMargins
		)
		{
			return UDWM_CALL_ORG(CTopLevelWindow::GetActualWindowRect, rect, eraseOffset, includeNonClient, excludeBorderMargins);
		}
		CWindowData* GetWindowData() const
		{
			CWindowData* windowData{ nullptr };

			if (SystemHelper::g_buildNumber < 19041)
			{
				// TO-DO
			}
			else if (SystemHelper::g_buildNumber <= 18363)
			{
				windowData = reinterpret_cast<CWindowData* const*>(this)[90];
			}
			else if (SystemHelper::g_buildNumber < 22000)
			{
				windowData = reinterpret_cast<CWindowData* const*>(this)[91];
			}
			else if (SystemHelper::g_buildNumber < 26020)
			{
				windowData = reinterpret_cast<CWindowData* const*>(this)[94];
			}
			else
			{
				windowData = reinterpret_cast<CWindowData* const*>(this)[89];
			}

			return windowData;
		}

		CVisual* GetVisual() const
		{
			CVisual* visual{ nullptr };

			if (SystemHelper::g_buildNumber < 19041)
			{
				// TO-DO
			}
			else if (SystemHelper::g_buildNumber < 22000)
			{
				visual = reinterpret_cast<CVisual* const*>(this)[33];
			}
			else if (SystemHelper::g_buildNumber < 22621)
			{
				visual = reinterpret_cast<CVisual* const*>(this)[34];
			}
			else if (SystemHelper::g_buildNumber < 26020)
			{
				visual = reinterpret_cast<CVisual* const*>(this)[36];
			}
			else
			{
				visual = reinterpret_cast<CVisual* const*>(this)[31];
			}

			return visual;
		}
		auto GetNCBackgroundVisuals() const
		{
			std::vector<winrt::com_ptr<CVisual>> visuals{};

			if (SystemHelper::g_buildNumber < 19041)
			{
				// TO-DO
			}
			else if (SystemHelper::g_buildNumber < 22000)
			{
				if (reinterpret_cast<CVisual* const*>(this)[34]) // accent
				{
					visuals.emplace_back(nullptr).copy_from(reinterpret_cast<CVisual* const*>(this)[34]);
				}
				if (reinterpret_cast<CVisual* const*>(this)[36]) // legacy
				{
					visuals.emplace_back(nullptr).copy_from(reinterpret_cast<CVisual* const*>(this)[36]);
				}
				if (reinterpret_cast<CVisual* const*>(this)[37]) // client blur
				{
					visuals.emplace_back(nullptr).copy_from(reinterpret_cast<CVisual* const*>(this)[37]);
				}
			}
			else if (SystemHelper::g_buildNumber < 22621)
			{
				if (reinterpret_cast<CVisual* const*>(this)[35]) // accent
				{
					visuals.emplace_back(nullptr).copy_from(reinterpret_cast<CVisual* const*>(this)[35]);
				}
				if (reinterpret_cast<CVisual* const*>(this)[37]) // legacy
				{
					visuals.emplace_back(nullptr).copy_from(reinterpret_cast<CVisual* const*>(this)[37]);
				}
				if (reinterpret_cast<CVisual* const*>(this)[38]) // system backdrop
				{
					visuals.emplace_back(nullptr).copy_from(reinterpret_cast<CVisual* const*>(this)[38]);
				}
				if (reinterpret_cast<CVisual* const*>(this)[39]) // client blur
				{
					visuals.emplace_back(nullptr).copy_from(reinterpret_cast<CVisual* const*>(this)[39]);
				}
			}
			else if (SystemHelper::g_buildNumber < 26020)
			{
				if (reinterpret_cast<CVisual* const*>(this)[37]) // accent
				{
					visuals.emplace_back(nullptr).copy_from(reinterpret_cast<CVisual* const*>(this)[37]);
				}
				if (reinterpret_cast<CVisual* const*>(this)[39]) // legacy
				{
					visuals.emplace_back(nullptr).copy_from(reinterpret_cast<CVisual* const*>(this)[39]);
				}
				if (reinterpret_cast<CVisual* const*>(this)[40]) // system backdrop
				{
					visuals.emplace_back(nullptr).copy_from(reinterpret_cast<CVisual* const*>(this)[40]);
				}
				if (reinterpret_cast<CVisual* const*>(this)[41]) // accent color
				{
					visuals.emplace_back(nullptr).copy_from(reinterpret_cast<CVisual* const*>(this)[41]);
				}
				if (reinterpret_cast<CVisual* const*>(this)[42]) // client blur
				{
					visuals.emplace_back(nullptr).copy_from(reinterpret_cast<CVisual* const*>(this)[42]);
				}
			}
			else
			{
				if (reinterpret_cast<CVisual* const*>(this)[32]) // legacy
				{
					visuals.emplace_back(nullptr).copy_from(reinterpret_cast<CVisual* const*>(this)[32]);
				}
				if (reinterpret_cast<CVisual* const*>(this)[34]) // accent
				{
					visuals.emplace_back(nullptr).copy_from(reinterpret_cast<CVisual* const*>(this)[34]);
				}
				if (reinterpret_cast<CVisual* const*>(this)[35]) // system backdrop
				{
					visuals.emplace_back(nullptr).copy_from(reinterpret_cast<CVisual* const*>(this)[35]);
				}
				if (reinterpret_cast<CVisual* const*>(this)[36]) // accent color
				{
					visuals.emplace_back(nullptr).copy_from(reinterpret_cast<CVisual* const*>(this)[36]);
				}
				if (reinterpret_cast<CVisual* const*>(this)[37]) // client blur
				{
					visuals.emplace_back(nullptr).copy_from(reinterpret_cast<CVisual* const*>(this)[37]);
				}
			}

			return visuals;
		}
		void ShowNCBackgroundVisuals(bool show)
		{
			auto showInternal = [show](CVisual* visual)
			{
				if (show)
				{
					//visual->Unhide();
					visual->SetOpacity(1.);
					visual->SendSetOpacity(1.);
					//visual->ConnectToParent(true);
				}
				else
				{
					//visual->Hide();
					visual->SetOpacity(0.);
					visual->SendSetOpacity(0.);
					//visual->ConnectToParent(false);
				}
			};

			for (auto visual : GetNCBackgroundVisuals())
			{
				showInternal(visual.get());
			}
		}

		bool HasNonClientBackground() const
		{
			auto windowData{ GetWindowData() };
			if ((windowData->GetNonClientAttribute() & 8) == 0)
			{
				return false;
			}
			if (windowData->IsImmersiveWindow())
			{
				return false;
			}

			bool nonClientEmpty{false};
			if (SystemHelper::g_buildNumber < 19041)
			{
				// TO-DO
			}
			else if (SystemHelper::g_buildNumber < 22000)
			{
				nonClientEmpty = !reinterpret_cast<DWORD const*>(this)[153] &&
					!reinterpret_cast<DWORD const*>(this)[154] &&
					!reinterpret_cast<DWORD const*>(this)[155] &&
					!reinterpret_cast<DWORD const*>(this)[156];
			}
			else if (SystemHelper::g_buildNumber < 22621)
			{
				nonClientEmpty = !reinterpret_cast<DWORD const*>(this)[157] &&
					!reinterpret_cast<DWORD const*>(this)[158] &&
					!reinterpret_cast<DWORD const*>(this)[159] &&
					!reinterpret_cast<DWORD const*>(this)[160];
			}
			else if (SystemHelper::g_buildNumber < 26020)
			{
				nonClientEmpty = !reinterpret_cast<DWORD const*>(this)[161] &&
					!reinterpret_cast<DWORD const*>(this)[162] &&
					!reinterpret_cast<DWORD const*>(this)[163] &&
					!reinterpret_cast<DWORD const*>(this)[164];
			}
			else
			{
				nonClientEmpty = !reinterpret_cast<DWORD const*>(this)[151] &&
					!reinterpret_cast<DWORD const*>(this)[152] &&
					!reinterpret_cast<DWORD const*>(this)[153] &&
					!reinterpret_cast<DWORD const*>(this)[154];
			}
			if (nonClientEmpty)
			{
				return false;
			}

			return true;
		}
		bool IsSystemBackdropApplied()
		{
			bool systemBackdropApplied{false};

			if (SystemHelper::g_buildNumber < 19041)
			{
				// TO-DO
			}
			else if (SystemHelper::g_buildNumber < 22000)
			{
				systemBackdropApplied = false;
			}
			else if (SystemHelper::g_buildNumber < 22621)
			{
				systemBackdropApplied = *reinterpret_cast<DWORD*>(reinterpret_cast<ULONG_PTR>(GetWindowData()) + 204);
			}
			else if (SystemHelper::g_buildNumber < 26020)
			{
				systemBackdropApplied = (reinterpret_cast<DWORD const*>(this)[210] <= 3);
			}
			else
			{
				systemBackdropApplied = (reinterpret_cast<DWORD const*>(this)[200] <= 3);
			}

			return systemBackdropApplied;
		}
	};

	struct CDesktopManager
	{
		inline static CDesktopManager* s_pDesktopManagerInstance{ nullptr };

		CWindowList* GetWindowList() const
		{
			return reinterpret_cast<CWindowList*>(reinterpret_cast<PVOID const*>(this)[61]);
		}
		IWICImagingFactory2* GetWICFactory() const
		{
			IWICImagingFactory2* factory{ nullptr };

			if (SystemHelper::g_buildNumber < 19041)
			{
				// TO-DO
			}
			else if (SystemHelper::g_buildNumber < 22000)
			{
				factory = reinterpret_cast<IWICImagingFactory2*>(reinterpret_cast<PVOID const*>(this)[39]);
			}
			else if (SystemHelper::g_buildNumber < 22621)
			{
				factory = reinterpret_cast<IWICImagingFactory2*>(reinterpret_cast<PVOID const*>(this)[30]);
			}
			else if (SystemHelper::g_buildNumber < 26020)
			{
				factory = reinterpret_cast<IWICImagingFactory2*>(reinterpret_cast<PVOID const*>(this)[31]);
			}
			else
			{
				factory = reinterpret_cast<IWICImagingFactory2*>(reinterpret_cast<PVOID const*>(this)[30]);
			}

			return factory;
		}
		ID2D1Device* GetD2DDevice() const
		{
			ID2D1Device* d2dDevice{nullptr};

			if (SystemHelper::g_buildNumber < 19041)
			{
				// TO-DO
			}
			else if (SystemHelper::g_buildNumber < 22000)
			{
				d2dDevice = reinterpret_cast<ID2D1Device* const*>(this)[29];
			}
			else if (SystemHelper::g_buildNumber < 22621)
			{
				d2dDevice = reinterpret_cast<ID2D1Device**>(reinterpret_cast<void* const*>(this)[6])[3];
			}
			else if (SystemHelper::g_buildNumber < 26020)
			{
				d2dDevice = reinterpret_cast<ID2D1Device**>(reinterpret_cast<void* const*>(this)[7])[3];
			}
			else
			{
				d2dDevice = reinterpret_cast<ID2D1Device**>(reinterpret_cast<void* const*>(this)[7])[4];
			}

			return d2dDevice;
		}
		DCompPrivates::IDCompositionDesktopDevicePartner* GetDCompositionInteropDevice() const
		{
			DCompPrivates::IDCompositionDesktopDevicePartner* interopDevice{ nullptr };

			if (SystemHelper::g_buildNumber < 19041)
			{
				// TO-DO
			}
			else if (SystemHelper::g_buildNumber < 22000)
			{
				interopDevice = reinterpret_cast<DCompPrivates::IDCompositionDesktopDevicePartner* const*>(this)[27];

				BOOL valid{ FALSE };
				winrt::com_ptr<IDCompositionDevice> desktopDevice{ nullptr };
				THROW_IF_FAILED(interopDevice->QueryInterface(desktopDevice.put()));
				THROW_IF_FAILED(desktopDevice->CheckDeviceState(&valid));
				if (!valid)
				{
					UDWM_CALL_ORG_STATIC_BY_TYPE(void(STDMETHODCALLTYPE*)(), "CDesktopManager::HandleInteropDeviceLost", );
				}
			}
			else if (SystemHelper::g_buildNumber < 22621)
			{
				interopDevice = reinterpret_cast<DCompPrivates::IDCompositionDesktopDevicePartner**>(reinterpret_cast<void* const*>(this)[5])[4];
			}
			else
			{
				interopDevice = reinterpret_cast<DCompPrivates::IDCompositionDesktopDevicePartner**>(reinterpret_cast<void* const*>(this)[6])[4]; // ok with build 26020
			}

			return interopDevice;
		}
	};
#pragma pop_macro("UDWM_CALL_ORG_STATIC")
#pragma pop_macro("UDWM_CALL_ORG")
}