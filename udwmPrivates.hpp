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
		virtual ~CBaseObject() {};
	};

#pragma push_macro("UDWM_CALL_ORG")
#pragma push_macro("UDWM_CALL_ORG_STATIC")
#define UDWM_CALL_ORG(name, ...) [&]{ static const auto s_ptr{ AcrylicEverywhere::uDwmPrivates::g_offsetMap.at(#name).To(AcrylicEverywhere::uDwmPrivates::g_udwmModule.get()) }; return (this->*HookHelper::union_cast<decltype(&name)>(s_ptr))(## __VA_ARGS__); } ()
#define UDWM_CALL_ORG_STATIC(name, ...) [&]{ static const auto s_ptr{AcrylicEverywhere::uDwmPrivates::g_offsetMap.at(#name).To(AcrylicEverywhere::uDwmPrivates::g_udwmModule.get())}; return (HookHelper::union_cast<decltype(&name)>(s_ptr))(## __VA_ARGS__);} ()
#define UDWM_CALL_ORG_BY_TYPE(type, object, name, ...) [&]{ static const auto s_ptr{AcrylicEverywhere::uDwmPrivates::g_offsetMap.at(name).To(AcrylicEverywhere::uDwmPrivates::g_udwmModule.get())}; return (object->*HookHelper::union_cast<type>(s_ptr))(## __VA_ARGS__); } ()
#define UDWM_CALL_ORG_STATIC_BY_TYPE(type, name, ...) [&]{ static const auto s_ptr{AcrylicEverywhere::uDwmPrivates::g_offsetMap.at(name).To(AcrylicEverywhere::uDwmPrivates::g_udwmModule.get())}; return (HookHelper::union_cast<type>(s_ptr))(## __VA_ARGS__); } ()
	struct CBaseGeometryProxy : CBaseObject {};
	struct CBaseTransformProxy : CBaseObject {};
	struct CCombinedGeometryProxy : CBaseGeometryProxy {};
	struct CRgnGeometryProxy : CBaseGeometryProxy 
	{
		HRESULT STDMETHODCALLTYPE Update(LPCRECT rectangles, UINT count)
		{
			return UDWM_CALL_ORG(CRgnGeometryProxy::Update, rectangles, count);
		}
	};
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
	struct CBaseLegacyMilBrushProxy : CBaseObject {};
	struct CSolidColorLegacyMilBrushProxy : CBaseLegacyMilBrushProxy {};
	struct CVisualProxy : CBaseObject 
	{
		HRESULT STDMETHODCALLTYPE SetTransform(CBaseTransformProxy* transfrom) { return UDWM_CALL_ORG(CVisualProxy::SetTransform, transfrom); }
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
			return reinterpret_cast<CVisualProxy* const*>(this)[2];
		}
		CVisual* GetParent() const
		{
			return reinterpret_cast<CVisual* const*>(this)[3];
		}
		HRESULT STDMETHODCALLTYPE SetParent(CVisual* visual)
		{
			return UDWM_CALL_ORG(CVisual::SetParent, visual);
		}
		bool IsCloneAllowed() const
		{
			const BYTE* properties{ nullptr };

			if (SystemHelper::g_buildNumber < 22000)
			{
				properties = &reinterpret_cast<BYTE const*>(this)[84];
			}
			else if (SystemHelper::g_buildNumber < 22621)
			{
				properties = &reinterpret_cast<BYTE const*>(this)[92];
			}
			else if (SystemHelper::g_buildNumber < 26020)
			{
				properties = &reinterpret_cast<BYTE const*>(this)[92];
			}
			else
			{
				properties = &reinterpret_cast<BYTE const*>(this)[36];
			}

			bool allowed{ false };
			if (properties)
			{
				allowed = (*properties & 8) == 0;
			}

			return allowed;
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
		bool IsVisible()
		{
			if (SystemHelper::g_buildNumber < 22000)
			{
				return *(reinterpret_cast<DWORD*>(this) + 22) == 0;
			}
			else if (SystemHelper::g_buildNumber < 26020)
			{
				return *(reinterpret_cast<DWORD*>(this) + 24) == 0;
			}
			else
			{
				return *(reinterpret_cast<DWORD*>(this) + 10) == 0;
			}

			return true;
		}
		void STDMETHODCALLTYPE SetInsetFromParent(MARGINS* margins)
		{
			return UDWM_CALL_ORG(CVisual::SetInsetFromParent, margins);
		}
		HRESULT STDMETHODCALLTYPE SetSize(const SIZE& size)
		{
			return UDWM_CALL_ORG(CVisual::SetSize, size);
		}
		void STDMETHODCALLTYPE SetOffset(const POINT& pt)
		{
			return UDWM_CALL_ORG(CVisual::SetOffset, pt);
		}
		HRESULT STDMETHODCALLTYPE UpdateOffset()
		{
			return UDWM_CALL_ORG(CVisual::UpdateOffset, );
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
		HRESULT STDMETHODCALLTYPE ConnectToParent(bool connect)
		{
			return UDWM_CALL_ORG(CVisual::ConnectToParent, connect);
		}
		void STDMETHODCALLTYPE SetOpacity(double opacity)
		{
			return UDWM_CALL_ORG(CVisual::SetOpacity, opacity);
		}
		HRESULT STDMETHODCALLTYPE UpdateOpacity()
		{
			return UDWM_CALL_ORG(CVisual::UpdateOpacity, );
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
		void Cloak(bool cloak)
		{
			if (!cloak)
			{
				SetOpacity(1.);
				UpdateOpacity();
			}
			else
			{
				SetOpacity(0.);
				UpdateOpacity();
			}
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
			bool connectNow
		)
		{
			return UDWM_CALL_ORG(VisualCollection::InsertRelative, visual, referenceVisual, insertAfter, connectNow);
		}
	};

	struct IRenderDataBuilder : IUnknown
	{
		STDMETHOD(DrawBitmap)(UINT bitmapHandleTableIndex) PURE;
		STDMETHOD(DrawGeometry)(UINT geometryHandleTableIndex, UINT brushHandleTableIndex) PURE;
		STDMETHOD(DrawImage)(const D2D1_RECT_F& rect, UINT imageHandleTableIndex) PURE;
		STDMETHOD(DrawMesh2D)(UINT meshHandleTableIndex, UINT brushHandleTableIndex) PURE;
		STDMETHOD(DrawRectangle)(const D2D1_RECT_F* rect, UINT brushHandleTableIndex) PURE;
		STDMETHOD(DrawTileImage)(UINT imageHandleTableIndex, const D2D1_RECT_F& rect, float opacity, const D2D1_POINT_2F& point) PURE;
		STDMETHOD(DrawVisual)(UINT visualHandleTableIndex) PURE;
		STDMETHOD(Pop)() PURE;
		STDMETHOD(PushTransform)(UINT transformHandleTableInfex) PURE;
		STDMETHOD(DrawSolidRectangle)(const D2D1_RECT_F& rect, const D2D1_COLOR_F& color) PURE;
	};
	struct CRenderDataInstruction : CBaseObject
	{
		STDMETHOD(WriteInstruction)(
			IRenderDataBuilder* builder,
			const struct CVisual* visual
		) PURE;
	};
	struct CDrawGeometryInstruction : CRenderDataInstruction
	{
		static HRESULT STDMETHODCALLTYPE Create(CBaseLegacyMilBrushProxy* brush, CBaseGeometryProxy* geometry, CDrawGeometryInstruction** instruction)
		{
			return UDWM_CALL_ORG_STATIC(CDrawGeometryInstruction::Create, brush, geometry, instruction);
		}
	};
	struct CRenderDataVisual : CVisual 
	{
		HRESULT STDMETHODCALLTYPE AddInstruction(CRenderDataInstruction* instruction)
		{
			return UDWM_CALL_ORG(CRenderDataVisual::AddInstruction, instruction);
		}
		HRESULT STDMETHODCALLTYPE ClearInstructions()
		{
			return UDWM_CALL_ORG(CRenderDataVisual::ClearInstructions, );
		}
	};
	struct CCanvasVisual : CRenderDataVisual 
	{
		static HRESULT STDMETHODCALLTYPE Create(CCanvasVisual** visual)
		{
			return UDWM_CALL_ORG_STATIC(CCanvasVisual::Create, visual);
		}
	};
	class CEmptyDrawInstruction : public CRenderDataInstruction
	{
		DWORD m_refCount{ 1 };
	public:
		HRESULT STDMETHODCALLTYPE Initialize() { return S_OK; }
		STDMETHOD(WriteInstruction)(
			IRenderDataBuilder* builder,
			const struct CVisual* visual
			) override
		{
			return S_OK;
		}
	};
	class CDrawVisualTreeInstruction : public CRenderDataInstruction
	{
		DWORD m_refCount{ 1 };
		winrt::com_ptr<CVisual> m_visual{ nullptr };
	public:
		CDrawVisualTreeInstruction(CVisual* visual) : CRenderDataInstruction{} { m_visual.copy_from(visual); }
		HRESULT STDMETHODCALLTYPE Initialize() { return S_OK; }
		STDMETHOD(WriteInstruction)(
			IRenderDataBuilder* builder,
			const struct CVisual* visual
		) override
		{
			UINT visualHandleTableIndex{ 0 };
			auto visualProxy{ m_visual->GetVisualProxy() };
			if (visualProxy)
			{
				visualHandleTableIndex = *reinterpret_cast<UINT*>(
					*reinterpret_cast<ULONG_PTR*>(reinterpret_cast<ULONG_PTR>(visualProxy) + 16) + 24ull
				);
			}

			return builder->DrawVisual(visualHandleTableIndex);
		}
	};
	struct ACCENT_POLICY
	{
		DWORD AccentState;
		DWORD AccentFlags;
		DWORD dwGradientColor;
		DWORD dwAnimationId;
	};
	bool IsAccentBlurEnabled(const ACCENT_POLICY* accentPolicy)
	{
		return accentPolicy->AccentState > 2 && accentPolicy->AccentState < 5;
	}
	struct CAccent : CVisual
	{
		static bool STDMETHODCALLTYPE s_IsPolicyActive(const ACCENT_POLICY* accentPolicy)
		{
			return UDWM_CALL_ORG_STATIC(CAccent::s_IsPolicyActive, accentPolicy);
		}
		CBaseGeometryProxy* const& GetClipGeometry() const
		{
			CBaseGeometryProxy* const* geometry{ nullptr };

			if (SystemHelper::g_buildNumber < 22000)
			{
				geometry = &reinterpret_cast<CBaseGeometryProxy* const*>(this)[52];
			}
			else if (SystemHelper::g_buildNumber < 22621)
			{
				geometry = &reinterpret_cast<CBaseGeometryProxy* const*>(this)[53];
			}
			else if (SystemHelper::g_buildNumber < 26020)
			{
				geometry = &reinterpret_cast<CBaseGeometryProxy* const*>(this)[48];
			}
			else
			{
				geometry = &reinterpret_cast<CBaseGeometryProxy* const*>(this)[42];
			}

			return *geometry;
		}
	};
	struct CTopLevelWindow;
	struct CTopLevelWindow3D;
	struct CWindowData : CBaseObject
	{
		HWND GetHwnd() const
		{
			return reinterpret_cast<const HWND*>(this)[5];
		}
		POINT GetOffsetToOwner()
		{
			return UDWM_CALL_ORG(CWindowData::GetOffsetToOwner, );
		}
		CTopLevelWindow* GetWindow() const
		{
			CTopLevelWindow* window{nullptr};

			if (SystemHelper::g_buildNumber < 22000)
			{
				window = reinterpret_cast<CTopLevelWindow* const*>(this)[48];
			}
			else
			{
				window = reinterpret_cast<CTopLevelWindow* const*>(this)[55];
			}
			return window;
		}
		CTopLevelWindow3D* GetWindow3D() const
		{
			CTopLevelWindow3D* window{ nullptr };

			if (SystemHelper::g_buildNumber < 22000)
			{
				window = reinterpret_cast<CTopLevelWindow3D* const*>(this)[59];
			}
			else
			{
				window = reinterpret_cast<CTopLevelWindow3D* const*>(this)[56];
			}
			return window;
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
	struct CWindowList : CBaseObject
	{
		HRESULT STDMETHODCALLTYPE GetExtendedFrameBounds(
			HWND hwnd,
			RECT* rect
		)
		{
			return UDWM_CALL_ORG(CWindowList::GetExtendedFrameBounds, hwnd, rect);
		}
		HRESULT STDMETHODCALLTYPE GetSyncedWindowDataByHwnd(HWND hwnd, CWindowData** windowData)
		{
			return UDWM_CALL_ORG(CWindowList::GetSyncedWindowDataByHwnd, hwnd, windowData);
		}
		void STDMETHODCALLTYPE RegisterAccentState(CWindowData* data, UINT accentState)
		{
			return UDWM_CALL_ORG(CWindowList::RegisterAccentState, data, accentState);
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
		HRESULT STDMETHODCALLTYPE SetWindowOffscreen(bool offscreen)
		{
			return UDWM_CALL_ORG(CTopLevelWindow::SetWindowOffscreen, offscreen);
		}
		bool STDMETHODCALLTYPE TreatAsActiveWindow()
		{
			return UDWM_CALL_ORG(CTopLevelWindow::TreatAsActiveWindow, );
		}
		HRESULT STDMETHODCALLTYPE ValidateVisual()
		{
			return UDWM_CALL_ORG(CTopLevelWindow::ValidateVisual, );
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
		CAccent* GetAccent() const
		{
			CAccent* accent{ nullptr };

			if (SystemHelper::g_buildNumber < 22000)
			{
				accent = reinterpret_cast<CAccent* const*>(this)[34];
			}
			else if (SystemHelper::g_buildNumber < 22621)
			{
				accent = reinterpret_cast<CAccent* const*>(this)[35];
			}
			else if (SystemHelper::g_buildNumber < 26020)
			{
				accent = reinterpret_cast<CAccent* const*>(this)[37];
			}
			else
			{
				accent = reinterpret_cast<CAccent* const*>(this)[34];
			}

			return accent;
		}
		CVisual* GetLegacyVisual() const
		{
			CVisual* visual{ nullptr };

			if (SystemHelper::g_buildNumber < 22000)
			{
				visual = reinterpret_cast<CVisual* const*>(this)[36];
			}
			else if (SystemHelper::g_buildNumber < 22621)
			{
				visual = reinterpret_cast<CVisual* const*>(this)[37];
			}
			else if (SystemHelper::g_buildNumber < 26020)
			{
				visual = reinterpret_cast<CVisual* const*>(this)[39];
			}
			else
			{
				visual = reinterpret_cast<CVisual* const*>(this)[40];
			}

			return visual;
		}
		CVisual* GetClientBlurVisual() const
		{
			CVisual* visual{ nullptr };

			if (SystemHelper::g_buildNumber < 22000)
			{
				visual = reinterpret_cast<CVisual* const*>(this)[37];
			}
			else if (SystemHelper::g_buildNumber < 22621)
			{
				visual = reinterpret_cast<CVisual* const*>(this)[39];
			}
			else if (SystemHelper::g_buildNumber < 26020)
			{
				visual = reinterpret_cast<CVisual* const*>(this)[37];
			}
			else
			{
				visual = reinterpret_cast<CVisual* const*>(this)[40];
			}

			return visual;
		}
		CVisual* GetSystemBackdropVisual() const
		{
			CVisual* visual{ nullptr };

			if (SystemHelper::g_buildNumber < 22000)
			{
			}
			else if (SystemHelper::g_buildNumber < 22621)
			{
				visual = reinterpret_cast<CVisual* const*>(this)[38];
			}
			else if (SystemHelper::g_buildNumber < 26020)
			{
				visual = reinterpret_cast<CVisual* const*>(this)[40];
			}
			else
			{
				visual = reinterpret_cast<CVisual* const*>(this)[35];
			}

			return visual;
		}
		CVisual* GetAccentColorVisual() const
		{
			CVisual* visual{ nullptr };

			if (SystemHelper::g_buildNumber < 22000)
			{
			}
			else if (SystemHelper::g_buildNumber < 22621)
			{
			}
			else if (SystemHelper::g_buildNumber < 26020)
			{
				visual = reinterpret_cast<CVisual* const*>(this)[41];
			}
			else
			{
				visual = reinterpret_cast<CVisual* const*>(this)[36];
			}

			return visual;
		}
		auto GetNCBackgroundVisuals() const
		{
			std::vector<winrt::com_ptr<CVisual>> visuals{};

			if (GetLegacyVisual())
			{
				visuals.emplace_back(nullptr).copy_from(GetLegacyVisual());
			}
			if (GetAccent())
			{
				visuals.emplace_back(nullptr).copy_from(GetAccent());
			}
			if (GetClientBlurVisual())
			{
				visuals.emplace_back(nullptr).copy_from(GetClientBlurVisual());
			}
			if (GetSystemBackdropVisual())
			{
				visuals.emplace_back(nullptr).copy_from(GetSystemBackdropVisual());
			}
			if (GetAccentColorVisual())
			{
				visuals.emplace_back(nullptr).copy_from(GetAccentColorVisual());
			}

			return visuals;
		}
		bool IsNCBackgroundVisualsCloneAllAllowed()
		{
			for (const auto& visual : GetNCBackgroundVisuals())
			{
				if (!visual->IsCloneAllowed())
				{
					return false;
				}
			}

			return true;
		}
		CSolidColorLegacyMilBrushProxy* const* GetBorderMilBrush() const
		{
			CSolidColorLegacyMilBrushProxy* const* brush{ nullptr };

			if (SystemHelper::g_buildNumber < 19041)
			{
				// TO-DO
			}
			else if (SystemHelper::g_buildNumber < 22000)
			{
				brush = &reinterpret_cast<CSolidColorLegacyMilBrushProxy* const*>(this)[94];
			}
			else if (SystemHelper::g_buildNumber < 22621)
			{
				brush = &reinterpret_cast<CSolidColorLegacyMilBrushProxy* const*>(this)[98];
			}
			else if (SystemHelper::g_buildNumber < 26020)
			{
				auto legacyBackgroundVisual{ reinterpret_cast<CVisual* const*>(this)[39] };
				if (legacyBackgroundVisual)
				{
					brush = &reinterpret_cast<CSolidColorLegacyMilBrushProxy* const*>(legacyBackgroundVisual)[38];
				}
			}
			else
			{
				auto legacyBackgroundVisual{ reinterpret_cast<CVisual* const*>(this)[34] };
				if (legacyBackgroundVisual)
				{
					brush = &reinterpret_cast<CSolidColorLegacyMilBrushProxy* const*>(legacyBackgroundVisual)[32];
				}
			}

			return brush;
		}
		CBaseGeometryProxy* const& GetClipGeometry() const
		{
			CBaseGeometryProxy* const* geometry{ nullptr };

			if (SystemHelper::g_buildNumber < 22000)
			{
				geometry = &reinterpret_cast<CBaseGeometryProxy* const*>(this)[46];
			}
			else
			{
				geometry = &reinterpret_cast<CBaseGeometryProxy* const*>(this)[53];
			}

			return *geometry;
		}
		CRgnGeometryProxy* const* GetBorderGeometry() const
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
				auto legacyBackgroundVisual{ reinterpret_cast<CVisual* const*>(this)[39] };
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

			return geometry;
		}
		CRgnGeometryProxy* const* GetTitlebarGeometry() const
		{
			CRgnGeometryProxy* const* geometry{ nullptr };

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

			return geometry;
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
	struct CSecondaryWindowRepresentation
	{
		CWindowData* GetWindowData() const
		{
			return reinterpret_cast<CWindowData* const*>(this)[8];
		}
		CWindowData* GetOwnedWindowData() const
		{
			return reinterpret_cast<CWindowData* const*>(this)[4];
		}
		CVisual* GetCachedVisual() const
		{
			return reinterpret_cast<CVisual* const*>(this)[6];
		}
		CVisual* GetVisual() const
		{
			return reinterpret_cast<CVisual* const*>(this)[7];
		}
		POINT GetOffset() const
		{
			return
			{
				*(reinterpret_cast<LONG const*>(this) + 22),
				*(reinterpret_cast<LONG const*>(this) + 24)
			};
		}
		RECT GetRect() const
		{
			return 
			{
				*(reinterpret_cast<LONG const*>(this) + 23),
				*(reinterpret_cast<LONG const*>(this) + 25),
				*(reinterpret_cast<LONG const*>(this) + 20),
				*(reinterpret_cast<LONG const*>(this) + 21)
			};
		}
	};
	struct CCachedVisualImageProxy : CBaseObject 
	{
		HRESULT STDMETHODCALLTYPE Freeze(bool freeze)
		{
			return UDWM_CALL_ORG(CCachedVisualImageProxy::Freeze, freeze);
		}
		HRESULT STDMETHODCALLTYPE Snapshot(LPCRECT lprc)
		{
			return UDWM_CALL_ORG(CCachedVisualImageProxy::Snapshot, lprc);
		}
		HRESULT STDMETHODCALLTYPE Update(
			const D2D1_RECT_F& rect, 
			const SIZE& size, 
			struct CRectResourceProxy* rectResource,
			struct CSizeResourceProxxy* sizeResource,
			CVisualProxy* visualProxy,
			UINT brushMappingMode
		)
		{
			return UDWM_CALL_ORG(
				CCachedVisualImageProxy::Update,
				rect,
				size,
				rectResource,
				sizeResource,
				visualProxy,
				brushMappingMode
			);
		}
	};
	struct CTopLevelWindow3D : CRenderDataVisual
	{
		CWindowData* GetWindowData() const
		{
			CWindowData* windowData{ nullptr };

			if (SystemHelper::g_buildNumber < 22000)
			{
				windowData = reinterpret_cast<CWindowData* const*>(this)[41];
			}
			else
			{
				windowData = reinterpret_cast<CWindowData* const*>(this)[42];
			}

			return windowData;
		}
		CCachedVisualImageProxy** GetCachedVisualImage()
		{
			return reinterpret_cast<CCachedVisualImageProxy**>(this) + 68;
		}
	};

	struct CDesktopManager
	{
		inline static CDesktopManager* s_pDesktopManagerInstance{ nullptr };
		inline static LPCRITICAL_SECTION s_csDwmInstance{ nullptr };

		bool IsVanillaTheme() const
		{
			return reinterpret_cast<bool const*>(this)[25];
		}
		CWindowList* GetWindowList() const
		{
			return reinterpret_cast<CWindowList* const*>(this)[61];
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
				factory = reinterpret_cast<IWICImagingFactory2* const*>(this)[39];
			}
			else if (SystemHelper::g_buildNumber < 22621)
			{
				factory = reinterpret_cast<IWICImagingFactory2* const*>(this)[30];
			}
			else if (SystemHelper::g_buildNumber < 26020)
			{
				factory = reinterpret_cast<IWICImagingFactory2* const*>(this)[31];
			}
			else
			{
				factory = reinterpret_cast<IWICImagingFactory2* const*>(this)[30];
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