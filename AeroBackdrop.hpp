#pragma once
#include "DCompBackdrop.hpp"
#include "CrossFadeEffect.hpp"
#include "GaussianBlurEffect.hpp"
#include "ColorSourceEffect.hpp"
#include "OpacityEffect.hpp"
#include "BlendEffect.hpp"
#include "ExposureEffect.hpp"
#include "CompositeEffect.hpp"
#include "SaturationEffect.hpp"
#include "BrightnessEffect.hpp"
#include "TintEffect.hpp"

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
			// the current recipe is modified from @kfh83, @TorutheRedFox, @aubymori
			auto fallbackTintSource{ winrt::make_self<ColorSourceEffect>() };
			fallbackTintSource->SetColor(winrt::Windows::UI::Color
			{ 
				255, 
				static_cast<UCHAR>(min(blurBalance + 0.1f, 1.f) * 255.f),
				static_cast<UCHAR>(min(blurBalance + 0.1f, 1.f) * 255.f),
				static_cast<UCHAR>(min(blurBalance + 0.1f, 1.f) * 255.f),
			});

			auto blackOrTransparentSource{ winrt::make_self<TintEffect>() };
			blackOrTransparentSource->SetInput(winrt::Windows::UI::Composition::CompositionEffectSourceParameter{ L"Backdrop" });
			blackOrTransparentSource->SetColor(D2D1::ColorF(D2D1::ColorF::Black));

			auto colorEffect{ winrt::make_self<ColorSourceEffect>() };
			colorEffect->SetName(L"MainColor");
			colorEffect->SetColor(mainColor);

			auto colorOpacityEffect{ winrt::make_self<OpacityEffect>() };
			colorOpacityEffect->SetName(L"MainColorOpacity");
			colorOpacityEffect->SetInput(*colorEffect);
			colorOpacityEffect->SetOpacity(colorBalance);

			auto gaussianBlurEffect{ winrt::make_self<GaussianBlurEffect>() };
			gaussianBlurEffect->SetName(L"Blur");
			gaussianBlurEffect->SetBorderMode(D2D1_BORDER_MODE_HARD);
			gaussianBlurEffect->SetBlurAmount(blurAmount);
			gaussianBlurEffect->SetInput(winrt::Windows::UI::Composition::CompositionEffectSourceParameter{ L"Backdrop" });

			auto blurredBackdropBalanceEffect{ winrt::make_self<OpacityEffect>() };
			blurredBackdropBalanceEffect->SetName(L"BlurBalance");
			blurredBackdropBalanceEffect->SetOpacity(blurBalance);
			blurredBackdropBalanceEffect->SetInput(*gaussianBlurEffect);

			auto actualBackdropEffect{ winrt::make_self<CompositeStepEffect>() };
			actualBackdropEffect->SetCompositeMode(D2D1_COMPOSITE_MODE_PLUS);
			actualBackdropEffect->SetDestination(*blackOrTransparentSource);
			actualBackdropEffect->SetSource(*blurredBackdropBalanceEffect);

			auto gaussianBlurEffect2{ winrt::make_self<GaussianBlurEffect>() };
			gaussianBlurEffect2->SetBorderMode(D2D1_BORDER_MODE_HARD);
			gaussianBlurEffect2->SetBlurAmount(blurAmount);
			gaussianBlurEffect2->SetInput(winrt::Windows::UI::Composition::CompositionEffectSourceParameter{ L"Backdrop" });

			auto desaturatedBlurredBackdrop{ winrt::make_self<SaturationEffect>() };
			desaturatedBlurredBackdrop->SetSaturation(0.f);
			desaturatedBlurredBackdrop->SetInput(*gaussianBlurEffect2);

			// make animation feel better...
			auto backdropNotTransparentPromised{ winrt::make_self<CompositeStepEffect>() };
			backdropNotTransparentPromised->SetCompositeMode(D2D1_COMPOSITE_MODE_SOURCE_OVER);
			backdropNotTransparentPromised->SetDestination(*fallbackTintSource);
			backdropNotTransparentPromised->SetSource(*desaturatedBlurredBackdrop);

			// if the glowColor is black, then it will produce a completely transparent surface
			auto tintEffect{ winrt::make_self<TintEffect>() };
			tintEffect->SetInput(*backdropNotTransparentPromised);
			tintEffect->SetColor(winrt::Windows::UI::Color{ static_cast<UCHAR>(static_cast<float>(glowColor.A) * glowBalance), glowColor.R, glowColor.G, glowColor.B});

			auto backdropWithAfterGlow{ winrt::make_self<CompositeStepEffect>() };
			backdropWithAfterGlow->SetCompositeMode(D2D1_COMPOSITE_MODE_PLUS);
			backdropWithAfterGlow->SetDestination(*actualBackdropEffect);
			backdropWithAfterGlow->SetSource(*tintEffect);

			auto compositeEffect{ winrt::make_self<CompositeStepEffect>() };
			compositeEffect->SetCompositeMode(D2D1_COMPOSITE_MODE_PLUS);
			compositeEffect->SetDestination(*backdropWithAfterGlow);
			compositeEffect->SetSource(*colorOpacityEffect);


			auto effectBrush{ compositor.CreateEffectFactory(*compositeEffect).CreateBrush() };
			effectBrush.SetSourceParameter(L"Backdrop", compositor.CreateBackdropBrush());

			return effectBrush;
		}
		catch (...) { return nullptr; }

		void STDMETHODCALLTYPE ReloadParameters()
		{
			darkMode_Active_Color = { 255, 116, 184, 252 };
			darkMode_Inactive_Color = { 255, 116, 184, 252 };
			lightMode_Active_Color = { 255, 116, 184, 252 };
			lightMode_Inactive_Color = { 255, 116, 184, 252 };

			darkMode_Active_GlowColor = { 255, 116, 184, 252 };
			darkMode_Inactive_GlowColor = { 255, 116, 184, 252 };
			lightMode_Active_GlowColor = { 255, 116, 184, 252 };
			lightMode_Inactive_GlowColor = { 255, 116, 184, 252 };

			lightMode_Active_ColorBalance = 0.f; // originally the color balance is zero
			lightMode_Inactive_ColorBalance = 0.f;
			darkMode_Active_ColorBalance = 0.f;
			darkMode_Inactive_ColorBalance = 0.f;

			lightMode_Active_GlowBalance = 0.43f;
			lightMode_Inactive_GlowBalance = 0.43f;
			darkMode_Active_GlowBalance = 0.43f;
			darkMode_Inactive_GlowBalance = 0.43f;

			Active_BlurBalance = 0.49f;
			Inactive_BlurBalance = 0.796f; // y = 0.4 * x + 0.6
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

	struct CAccentAeroResources : CDCompResourcesBase
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