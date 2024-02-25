#pragma once
#include "DCompBackdrop.hpp"
#include "CrossFadeEffect.hpp"
#include "GaussianBlurEffect.hpp"
#include "ColorSourceEffect.hpp"
#include "OpacityEffect.hpp"
#include "BlendEffect.hpp"
#include "ExposureEffect.hpp"

namespace AcrylicEverywhere
{
	struct CAeroResources : CDCompResources
	{
		winrt::Windows::UI::Color lightMode_Active_GlowColor{};
		winrt::Windows::UI::Color darkMode_Active_GlowColor{};
		winrt::Windows::UI::Color lightMode_Inactive_GlowColor{};
		winrt::Windows::UI::Color darkMode_Inactive_GlowColor{};

		float lightMode_Active_ColorBalance{};
		float lightMode_Inactive_ColorBalance{};
		float darkMode_Active_ColorBalance{};
		float darkMode_Inactive_ColorBalance{};

		float lightMode_Active_GlowBalance{};
		float lightMode_Inactive_GlowBalance{};
		float darkMode_Active_GlowBalance{};
		float darkMode_Inactive_GlowBalance{};

		float Active_BlurBalance{};
		float Inactive_BlurBalance{};

		float blurAmount{ 3.f };
		bool hostBackdrop{ false };

		static winrt::Windows::UI::Composition::CompositionBrush CreateBrush(
			const winrt::Windows::UI::Composition::Compositor& compositor,
			const winrt::Windows::UI::Color& mainColor,
			const winrt::Windows::UI::Color& glowColor,
			float colorBalance,
			float glowBalance,
			float blurAmount,
			float blurBalance,
			bool hostBackdrop
		) try
		{
			auto colorEffect{ winrt::make_self<ColorSourceEffect>() };
			colorEffect->SetName(L"MainColor");
			colorEffect->SetColor(mainColor);

			auto colorOpacityEffect{ winrt::make_self<OpacityEffect>() };
			colorOpacityEffect->SetName(L"MainColorOpacity");
			colorOpacityEffect->SetInput(*colorEffect);
			colorOpacityEffect->SetOpacity(colorBalance);

			auto glowColorEffect{ winrt::make_self<ColorSourceEffect>() };
			glowColorEffect->SetName(L"GlowColor");
			glowColorEffect->SetColor(glowColor);

			auto glowOpacityEffect{ winrt::make_self<OpacityEffect>() };
			glowOpacityEffect->SetName(L"GlowColorOpacity");
			glowOpacityEffect->SetInput(*glowColorEffect);
			glowOpacityEffect->SetOpacity(glowBalance);

			auto blurredBackdropBalanceEffect{ winrt::make_self<ExposureEffect>() };
			blurredBackdropBalanceEffect->SetName(L"BlurBalance");
			blurredBackdropBalanceEffect->SetExposureAmount(-blurBalance);
			if (hostBackdrop)
			{
				blurredBackdropBalanceEffect->SetInput(winrt::Windows::UI::Composition::CompositionEffectSourceParameter{ L"Backdrop" });
			}
			else
			{
				auto gaussianBlurEffect{ winrt::make_self<GaussianBlurEffect>() };
				gaussianBlurEffect->SetName(L"Blur");
				gaussianBlurEffect->SetBorderMode(D2D1_BORDER_MODE_HARD);
				gaussianBlurEffect->SetBlurAmount(blurAmount);
				gaussianBlurEffect->SetInput(winrt::Windows::UI::Composition::CompositionEffectSourceParameter{ L"Backdrop" });
				blurredBackdropBalanceEffect->SetInput(*gaussianBlurEffect);
			}

			auto colorBlendEffect{ winrt::make_self<BlendEffect>() };
			colorBlendEffect->SetBlendMode(D2D1_BLEND_MODE_MULTIPLY);
			colorBlendEffect->SetBackground(*blurredBackdropBalanceEffect);
			colorBlendEffect->SetForeground(*colorOpacityEffect);

			auto colorBalanceEffect{ winrt::make_self<ExposureEffect>() };
			colorBalanceEffect->SetName(L"ColorBalance");
			colorBalanceEffect->SetExposureAmount(colorBalance / 10.f);
			colorBalanceEffect->SetInput(*colorBlendEffect);

			auto glowBlendEffect{ winrt::make_self<BlendEffect>() };
			glowBlendEffect->SetBlendMode(D2D1_BLEND_MODE_LUMINOSITY);
			glowBlendEffect->SetBackground(*colorBalanceEffect);
			glowBlendEffect->SetForeground(*glowOpacityEffect);

			auto glowBalanceEffect{ winrt::make_self<ExposureEffect>() };
			glowBalanceEffect->SetName(L"GlowBalance");
			glowBalanceEffect->SetExposureAmount(glowBalance / 10.f);
			glowBalanceEffect->SetInput(*glowBlendEffect);

			auto effectBrush{ compositor.CreateEffectFactory(*glowBalanceEffect).CreateBrush() };
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

		void STDMETHODCALLTYPE UpdateParameters()
		{
			darkMode_Active_Color = { 255, 116, 184, 252 };
			darkMode_Inactive_Color = { 255, 116, 184, 252 };
			lightMode_Active_Color = { 255, 116, 184, 252 };
			lightMode_Inactive_Color = { 255, 116, 184, 252 };

			darkMode_Active_GlowColor = { 255, 116, 184, 252 };
			darkMode_Inactive_GlowColor = { 255, 116, 184, 252 };
			lightMode_Active_GlowColor = { 255, 116, 184, 252 };
			lightMode_Inactive_GlowColor = { 255, 116, 184, 252 };

			lightMode_Active_ColorBalance = 0.f;
			lightMode_Inactive_ColorBalance = 0.f;
			darkMode_Active_ColorBalance = 0.f;
			darkMode_Inactive_ColorBalance = 0.f;

			lightMode_Active_GlowBalance = 0.43f;
			lightMode_Inactive_GlowBalance = 0.43f;
			darkMode_Active_GlowBalance = 0.43f;
			darkMode_Inactive_GlowBalance = 0.43f;

			Active_BlurBalance = 0.49f;
			Inactive_BlurBalance = 0.f;
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
				UpdateParameters();
				auto compositor{ interopDCompDevice.as<winrt::Windows::UI::Composition::Compositor>() };

				lightMode_Active_Brush = CreateBrush(
					compositor,
					lightMode_Active_Color,
					lightMode_Active_GlowColor,
					lightMode_Active_ColorBalance,
					lightMode_Active_GlowBalance,
					blurAmount,
					Active_BlurBalance,
					hostBackdrop
				);
				lightMode_Inactive_Brush = CreateBrush(
					compositor,
					lightMode_Inactive_Color,
					lightMode_Inactive_GlowColor,
					lightMode_Inactive_ColorBalance,
					lightMode_Inactive_GlowBalance,
					blurAmount,
					Inactive_BlurBalance,
					hostBackdrop
				);
				darkMode_Active_Brush = CreateBrush(
					compositor,
					darkMode_Active_Color,
					darkMode_Active_GlowColor,
					darkMode_Active_ColorBalance,
					darkMode_Active_GlowBalance,
					blurAmount,
					Active_BlurBalance,
					hostBackdrop
				);
				darkMode_Inactive_Brush = CreateBrush(
					compositor,
					darkMode_Inactive_Color,
					darkMode_Inactive_GlowColor,
					darkMode_Inactive_ColorBalance,
					darkMode_Inactive_GlowBalance,
					blurAmount,
					Inactive_BlurBalance,
					hostBackdrop
				);
			}

			return CDCompResources::GetBrush(useDarkMode, windowActivated);
		}
		catch (...) { return nullptr; }
	};

	struct CAeroBackdrop : CDCompBackdrop
	{
		inline static CAeroResources s_sharedResources{};

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

	struct CAccentAeroResources
	{

	};
	struct CAccentAeroBackdrop : CAccentDCompBackdrop
	{
		inline static CAccentAeroResources s_sharedResources{};
		bool firstTimeUpdated{ false };
		winrt::Windows::UI::Color currenGlowColor{};

		HRESULT STDMETHODCALLTYPE UpdateBrush(const uDwmPrivates::ACCENT_POLICY& policy) try
		{
			auto compositor{ spriteVisual.Compositor() };
			auto glowColor{ FromAbgr(policy.dwGradientColor) };
			if (
				currenGlowColor != glowColor ||
				firstTimeUpdated == false
			)
			{
				firstTimeUpdated = true;
				currenGlowColor = glowColor;

				spriteVisual.Brush(
					CAeroResources::CreateBrush(
						compositor,
						CAeroBackdrop::s_sharedResources.lightMode_Active_Color,
						{ 255, currenGlowColor.R, currenGlowColor.G, currenGlowColor.B },
						CAeroBackdrop::s_sharedResources.lightMode_Active_ColorBalance,
						static_cast<float>(currenGlowColor.A) / 255.f,
						CAeroBackdrop::s_sharedResources.blurAmount,
						CAeroBackdrop::s_sharedResources.Active_BlurBalance,
						CAeroBackdrop::s_sharedResources.hostBackdrop
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