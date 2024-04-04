﻿#pragma once
#include "DCompBackdrop.hpp"
#include "CrossFadeEffect.hpp"
#include "GaussianBlurEffect.hpp"
#include "ColorSourceEffect.hpp"
#include "OpacityEffect.hpp"
#include "BlendEffect.hpp"

namespace AcrylicEverywhere
{
	struct CBlurResources : CDCompResources
	{
		float lightMode_Active_TintOpacity{};
		float lightMode_Inactive_TintOpacity{};
		float darkMode_Active_TintOpacity{};
		float darkMode_Inactive_TintOpacity{};
		float blurAmount{ 3.f };
		bool hostBackdrop{ false };

		static winrt::Windows::UI::Composition::CompositionBrush CreateBrush(
			const winrt::Windows::UI::Composition::Compositor& compositor,
			const winrt::Windows::UI::Color& tintColor,
			float tintOpacity,
			float blurAmount,
			bool hostBackdrop
		) try
		{
			if (static_cast<float>(tintColor.A) * tintOpacity == 255.f)
			{
				return compositor.CreateColorBrush(tintColor);
			}

			auto tintColorEffect{ winrt::make_self<ColorSourceEffect>() };
			tintColorEffect->SetName(L"TintColor");
			tintColorEffect->SetColor(tintColor);

			auto tintOpacityEffect{ winrt::make_self<OpacityEffect>() };
			tintOpacityEffect->SetName(L"TintOpacity");
			tintOpacityEffect->SetOpacity(tintOpacity);
			tintOpacityEffect->SetInput(*tintColorEffect);

			auto colorBlendEffect{ winrt::make_self<BlendEffect>() };
			colorBlendEffect->SetBlendMode(D2D1_BLEND_MODE_MULTIPLY);
			if (hostBackdrop)
			{
				colorBlendEffect->SetBackground(winrt::Windows::UI::Composition::CompositionEffectSourceParameter{ L"Backdrop" });
			}
			else
			{
				auto gaussianBlurEffect{ winrt::make_self<GaussianBlurEffect>() };
				gaussianBlurEffect->SetName(L"Blur");
				gaussianBlurEffect->SetBorderMode(D2D1_BORDER_MODE_HARD);
				gaussianBlurEffect->SetBlurAmount(blurAmount);
				gaussianBlurEffect->SetInput(winrt::Windows::UI::Composition::CompositionEffectSourceParameter{ L"Backdrop" });
				colorBlendEffect->SetBackground(*gaussianBlurEffect);
			}
			colorBlendEffect->SetForeground(*tintOpacityEffect);

			auto effectBrush{ compositor.CreateEffectFactory(*colorBlendEffect).CreateBrush() };
			if (hostBackdrop)
			{
				effectBrush.SetSourceParameter(L"Backdrop", compositor.CreateHostBackdropBrush());
			}
			else
			{
				effectBrush.SetSourceParameter(L"Backdrop", compositor.CreateBackdropBrush());
			}

			return effectBrush;
		}
		catch (...) { return nullptr; }

		void STDMETHODCALLTYPE ReloadParameters()
		{
			darkMode_Active_Color = { 255, 32, 32, 32 };
			darkMode_Inactive_Color = { 255, 32, 32, 32 };
			lightMode_Active_Color = { 255, 243, 243, 243 };
			lightMode_Inactive_Color = { 255, 243, 243, 243 };

			lightMode_Active_TintOpacity = 0.8f;
			lightMode_Inactive_TintOpacity = 0.8f;
			darkMode_Active_TintOpacity = 0.5f;
			darkMode_Inactive_TintOpacity = 0.5f;
		}
		winrt::Windows::UI::Composition::CompositionBrush STDMETHODCALLTYPE GetBrush(bool useDarkMode, bool windowActivated) override try
		{
			auto is_device_valid = [&]()
			{
				if (!interopDCompDevice) return false;

				BOOL valid{ FALSE };
				THROW_IF_FAILED(
					interopDCompDevice.as<IDCompositionDevice>()->CheckDeviceState(
						&valid
					)
				);

				return valid == TRUE;
			};
			if (!is_device_valid())
			{
				interopDCompDevice.copy_from(
					uDwmPrivates::CDesktopManager::s_pDesktopManagerInstance->GetDCompositionInteropDevice()
				);
				ReloadParameters();
				auto compositor{ interopDCompDevice.as<winrt::Windows::UI::Composition::Compositor>() };

				lightMode_Active_Brush = CreateBrush(
					compositor,
					lightMode_Active_Color,
					lightMode_Active_TintOpacity,
					blurAmount,
					hostBackdrop
				);
				lightMode_Inactive_Brush = CreateBrush(
					compositor,
					lightMode_Inactive_Color,
					lightMode_Inactive_TintOpacity,
					blurAmount,
					hostBackdrop
				);
				darkMode_Active_Brush = CreateBrush(
					compositor,
					darkMode_Active_Color,
					darkMode_Active_TintOpacity,
					blurAmount,
					hostBackdrop
				);
				darkMode_Inactive_Brush = CreateBrush(
					compositor,
					darkMode_Inactive_Color,
					darkMode_Inactive_TintOpacity,
					blurAmount,
					hostBackdrop
				);
			}

			return CDCompResources::GetBrush(useDarkMode, windowActivated);
		}
		catch (...) { return nullptr; }
	};

	struct CBlurBackdrop : CDCompBackdrop
	{
		inline static CBlurResources s_sharedResources{};

		STDMETHOD(UpdateColorizationColor)(
			bool useDarkMode,
			bool windowActivated
			) override
		{
			if (useDarkMode)
			{
				if (windowActivated) { currentColor = s_sharedResources.darkMode_Active_Color; }
				else { currentColor = s_sharedResources.darkMode_Inactive_Color; }
			}
			else
			{
				if (windowActivated) { currentColor = s_sharedResources.lightMode_Active_Color; }
				else { currentColor = s_sharedResources.lightMode_Inactive_Color; }
			}

			return S_OK;
		}
		HRESULT STDMETHODCALLTYPE Update(
			bool useDarkMode,
			bool windowActivated
		) try
		{
			THROW_IF_FAILED(UpdateColorizationColor(useDarkMode, windowActivated));
			THROW_IF_FAILED(
				TryCrossFadeToNewBrush(
					spriteVisual.Compositor(),
					s_sharedResources.GetBrush(useDarkMode, windowActivated)
				)
			);

			return S_OK;
		}
		CATCH_RETURN()
	};

	struct CAccentBlurResources : CDCompResourcesBase
	{

	};
	struct CAccentBlurBackdrop : CAccentDCompBackdrop
	{
		inline static CAccentBlurResources s_sharedResources{};
		bool firstTimeUpdated{ false };
		winrt::Windows::UI::Color currenTintColor{};

		HRESULT STDMETHODCALLTYPE UpdateBrush(const uDwmPrivates::ACCENT_POLICY& policy) try
		{
			auto compositor{ spriteVisual.Compositor() };
			auto tintColor{ FromAbgr(policy.dwGradientColor) };
			if (
				currenTintColor != tintColor ||
				firstTimeUpdated == false
			)
			{
				firstTimeUpdated = true;
				currenTintColor = tintColor;

				spriteVisual.Brush(
					CBlurResources::CreateBrush(
						compositor,
						tintColor,
						1.f,
						CBlurBackdrop::s_sharedResources.blurAmount,
						CBlurBackdrop::s_sharedResources.hostBackdrop
					)
				);
			}

			return S_OK;
		}
		CATCH_RETURN()

		HRESULT STDMETHODCALLTYPE Update(const uDwmPrivates::ACCENT_POLICY& policy) override try
		{
			THROW_IF_FAILED(UpdateBrush(policy));

			return S_OK;
		}
		CATCH_RETURN()
	};
}