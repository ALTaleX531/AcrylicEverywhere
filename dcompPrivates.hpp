#pragma once
#include "framework.h"
#include "winrt.h"
#include "SystemHelper.hpp"
#include "HookHelper.hpp"

namespace AcrylicEverywhere::DCompPrivates
{
	DECLARE_INTERFACE_IID_(InteropCompositionTarget, IUnknown, "EACDD04C-117E-4E17-88F4-D1B12B0E3D89")
	{
		STDMETHOD(SetRoot)(THIS_ IN IDCompositionVisual * visual) PURE;
	};

	struct IDCompositionDesktopDevicePartner : IDCompositionDesktopDevice 
	{
		HRESULT CreateSharedResource(REFIID riid, PVOID* ppvObject)
		{
			return (this->*HookHelper::union_cast<decltype(&IDCompositionDesktopDevicePartner::CreateSharedResource)>(HookHelper::GetObjectVTable(this)[27]))(
				riid, ppvObject
			);
		}
		HRESULT OpenSharedResourceHandle(InteropCompositionTarget* interopTarget, HANDLE* resourceHandle)
		{
			return (this->*HookHelper::union_cast<decltype(&IDCompositionDesktopDevicePartner::OpenSharedResourceHandle)>(HookHelper::GetObjectVTable(this)[28]))(
				interopTarget, resourceHandle
			);
		}
	};

	MIDL_INTERFACE("fe93b735-e574-4a5d-a21a-f705c21945fa")
	IDCompositionVisualPartnerWinRTInterop : IDCompositionVisual3
	{
		auto GetVisualCollection()
		{
			using GetVisualCollection_t = HRESULT(STDMETHODCALLTYPE IDCompositionVisualPartnerWinRTInterop::*)(PVOID* collection);

			winrt::Windows::UI::Composition::VisualCollection collection{ nullptr };

			if (SystemHelper::g_buildNumber < 22000)
			{
				THROW_IF_FAILED(
					(this->*HookHelper::union_cast<GetVisualCollection_t>(HookHelper::GetObjectVTable(this)[44]))(
						winrt::put_abi(collection)
					)
				);
			}
			else
			{
				THROW_IF_FAILED(
					(this->*HookHelper::union_cast<GetVisualCollection_t>(HookHelper::GetObjectVTable(this)[45]))(
						winrt::put_abi(collection)
					)
				);
			}

			return collection;
		}
	};
}