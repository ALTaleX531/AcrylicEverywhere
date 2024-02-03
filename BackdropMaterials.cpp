#include "pch.h"
#include "GaussianBlurEffect.hpp"
#include "ColorSourceEffect.hpp"
#include "OpacityEffect.hpp"
#include "BorderEffect.hpp"
#include "BlendEffect.hpp"
#include "CompositeEffect.hpp"
#include "CrossFadeEffect.hpp"
#include "SaturationEffect.hpp"
#include "ExposureEffect.hpp"
#include "BackdropMaterials.hpp"

using namespace AcrylicEverywhere;

CAcrylicResources CAcrylicBackdrop::s_sharedResources{};
CMicaResources CMicaBackdrop::s_sharedResources{};
CAeroResources CAeroBackdrop::s_sharedResources{};
CGlassReflectionResources CGlassReflectionBackdrop::s_sharedResources{};

winrt::Windows::UI::Composition::CompositionBrush CBackdropResources::CreateCrossFadeEffectBrush(
	const winrt::Windows::UI::Composition::Compositor& compositor,
	const winrt::Windows::UI::Composition::CompositionBrush& from,
	const winrt::Windows::UI::Composition::CompositionBrush& to
)
{
	auto crossFadeEffect{ winrt::make_self<CrossFadeEffect>() };
	crossFadeEffect->SetName(L"Crossfade"); // Name to reference when starting the animation.
	crossFadeEffect->SetSource(winrt::Windows::UI::Composition::CompositionEffectSourceParameter{ L"From" });
	crossFadeEffect->SetDestination(winrt::Windows::UI::Composition::CompositionEffectSourceParameter{ L"To" });
	crossFadeEffect->SetWeight(0);

	winrt::Windows::UI::Composition::CompositionEffectBrush crossFadeEffectBrush
	{
		compositor.CreateEffectFactory(
			crossFadeEffect.as<winrt::Windows::Graphics::Effects::IGraphicsEffect>(), {L"Crossfade.Weight"}
		).CreateBrush()
	};
	crossFadeEffectBrush.Comment(L"Crossfade");
	crossFadeEffectBrush.SetSourceParameter(L"From", from);
	crossFadeEffectBrush.SetSourceParameter(L"To", to);
	return crossFadeEffectBrush;
}

winrt::Windows::UI::Composition::ScalarKeyFrameAnimation CBackdropResources::CreateCrossFadeAnimation(
	const winrt::Windows::UI::Composition::Compositor& compositor,
	winrt::Windows::Foundation::TimeSpan const& crossfadeTime
)
{
	auto animation{ compositor.CreateScalarKeyFrameAnimation() };
	auto linearEasing{ compositor.CreateLinearEasingFunction() };
	animation.InsertKeyFrame(0.0f, 0.0f, linearEasing);
	animation.InsertKeyFrame(1.0f, 1.0f, linearEasing);
	animation.Duration(crossfadeTime);
	return animation;
}

winrt::Windows::UI::Composition::CompositionBrush CBackdropResources::ChooseIdealBrush(bool darkMode, bool active)
{
	if (darkMode)
	{
		if (active)
		{
			return darkMode_Active_Brush;
		}
		else
		{
			return darkMode_Inactive_Brush;
		}
	}
	else
	{
		if (active)
		{
			return lightMode_Active_Brush;
		}
		else
		{
			return lightMode_Inactive_Brush;
		}
	}

	return nullptr;
}

winrt::Windows::UI::Composition::CompositionBrush CAcrylicResources::CreateBrush(
	const winrt::Windows::UI::Composition::Compositor& compositor,
	const winrt::Windows::UI::Color& tintColor,
	const winrt::Windows::UI::Color& luminosityColor,
	float tintOpacity,
	float luminosityOpacity,
	float blurAmount
) try
{
	THROW_IF_FAILED(EnsureNoiceSurfaceBrush());

	auto tintColorEffect{ winrt::make_self<ColorSourceEffect>() };
	tintColorEffect->SetName(L"TintColor");
	tintColorEffect->SetColor(tintColor);

	auto tintOpacityEffect{ winrt::make_self<OpacityEffect>() };
	tintOpacityEffect->SetName(L"TintOpacity");
	tintOpacityEffect->SetOpacity(tintOpacity);
	tintOpacityEffect->SetInput(tintColorEffect);

	auto gaussianBlurEffect{ winrt::make_self<GaussianBlurEffect>()};
	gaussianBlurEffect->SetName(L"Blur");
	gaussianBlurEffect->SetBorderMode(D2D1_BORDER_MODE_HARD);
	gaussianBlurEffect->SetBlurAmount(blurAmount);
	winrt::Windows::UI::Composition::CompositionEffectSourceParameter backdropEffectSourceParameter{ L"Backdrop" };
	gaussianBlurEffect->SetInput(backdropEffectSourceParameter);

	auto luminosityColorEffect{ winrt::make_self<ColorSourceEffect>() };
	luminosityColorEffect->SetName(L"LuminosityColor");
	luminosityColorEffect->SetColor(luminosityColor);

	auto luminosityOpacityEffect{ winrt::make_self<OpacityEffect>() };
	luminosityOpacityEffect->SetName(L"LuminosityOpacity");
	luminosityOpacityEffect->SetOpacity(luminosityOpacity);
	luminosityOpacityEffect->SetInput(luminosityColorEffect);

	auto luminosityBlendEffect{ winrt::make_self<BlendEffect>() };
	// NOTE: There is currently a bug where the names of BlendEffectMode::Luminosity and BlendEffectMode::Color are flipped->
	// This should be changed to Luminosity when/if the bug is fixed->
	luminosityBlendEffect->SetBlendMode(D2D1_BLEND_MODE_COLOR);
	luminosityBlendEffect->SetBackground(gaussianBlurEffect);
	luminosityBlendEffect->SetForeground(luminosityOpacityEffect);

	auto colorBlendEffect{ winrt::make_self<BlendEffect>() };
	// NOTE: There is currently a bug where the names of BlendEffectMode::Luminosity and BlendEffectMode::Color are flipped->
	// This should be changed to Color when/if the bug is fixed->
	colorBlendEffect->SetBlendMode(D2D1_BLEND_MODE_LUMINOSITY);
	colorBlendEffect->SetBackground(luminosityBlendEffect);
	colorBlendEffect->SetForeground(tintOpacityEffect);

	auto noiseBorderEffect{ winrt::make_self<BorderEffect>() };
	noiseBorderEffect->SetExtendX(D2D1_BORDER_EDGE_MODE_WRAP);
	noiseBorderEffect->SetExtendY(D2D1_BORDER_EDGE_MODE_WRAP);
	winrt::Windows::UI::Composition::CompositionEffectSourceParameter noiseEffectSourceParameter{ L"Noise" };
	noiseBorderEffect->SetInput(noiseEffectSourceParameter);

	auto noiseOpacityEffect{ winrt::make_self<OpacityEffect>() };
	noiseOpacityEffect->SetName(L"NoiseOpacity");
	noiseOpacityEffect->SetOpacity(0.02f);
	noiseOpacityEffect->SetInput(noiseBorderEffect);

	auto blendEffectOuter{ winrt::make_self<BlendEffect>() };
	blendEffectOuter->SetBlendMode(D2D1_BLEND_MODE_MULTIPLY);
	blendEffectOuter->SetBackground(colorBlendEffect);
	blendEffectOuter->SetForeground(noiseOpacityEffect);

	auto effectBrush
	{ 
		compositor.CreateEffectFactory(
			blendEffectOuter.as<winrt::Windows::Graphics::Effects::IGraphicsEffect>()
		).CreateBrush() 
	};
	effectBrush.SetSourceParameter(L"Backdrop", compositor.CreateBackdropBrush());
	effectBrush.SetSourceParameter(L"Noise", noiceBrush);

	return effectBrush;
}
catch(...) { return nullptr; }

HRESULT CAcrylicResources::EnsureNoiceSurfaceBrush() try
{
	if (noiceBrush)
	{
		return S_OK;
	}

	auto compositor{ interopDCompDevice.as<winrt::Windows::UI::Composition::Compositor>() };
	winrt::Windows::UI::Composition::CompositionGraphicsDevice graphicsDevice{ nullptr };
	THROW_IF_FAILED(
		compositor.as<ABI::Windows::UI::Composition::ICompositorInterop>()->CreateGraphicsDevice(
			uDwmPrivates::CDesktopManager::s_pDesktopManagerInstance->GetD2DDevice(),
			reinterpret_cast<ABI::Windows::UI::Composition::ICompositionGraphicsDevice**>(winrt::put_abi(graphicsDevice))
		)
	);
	auto compositionSurface
	{
		graphicsDevice.CreateDrawingSurface(
			{ 256.f, 256.f },
			winrt::Windows::Graphics::DirectX::DirectXPixelFormat::R16G16B16A16Float,
			winrt::Windows::Graphics::DirectX::DirectXAlphaMode::Premultiplied
		)
	};
	noiceBrush = compositor.CreateSurfaceBrush(compositionSurface);

	wil::unique_hmodule wuxcModule{ LoadLibraryExW(L"Windows.UI.Xaml.Controls.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE) };
	THROW_LAST_ERROR_IF_NULL(wuxcModule);
	auto resourceHandle{ FindResourceW(wuxcModule.get(), MAKEINTRESOURCE(2000), RT_RCDATA) };
	THROW_LAST_ERROR_IF_NULL(resourceHandle);
	auto globalHandle{ LoadResource(wuxcModule.get(), resourceHandle) };
	THROW_LAST_ERROR_IF_NULL(globalHandle);
	auto cleanUp = wil::scope_exit([&]
	{
		if (globalHandle)
		{
			UnlockResource(globalHandle);
			FreeResource(globalHandle);
		}
	});
	DWORD resourceSize{ SizeofResource(wuxcModule.get(), resourceHandle) };
	THROW_LAST_ERROR_IF(resourceSize == 0);
	auto resourceAddress{ reinterpret_cast<PBYTE>(LockResource(globalHandle)) };
	winrt::com_ptr<IStream> stream{ SHCreateMemStream(resourceAddress, resourceSize), winrt::take_ownership_from_abi };
	THROW_LAST_ERROR_IF_NULL(stream);
	
	winrt::com_ptr<IWICImagingFactory2> wicFactory{ nullptr };
	wicFactory.copy_from(uDwmPrivates::CDesktopManager::s_pDesktopManagerInstance->GetWICFactory());
	winrt::com_ptr<IWICBitmapDecoder> wicDecoder{ nullptr };
	THROW_IF_FAILED(wicFactory->CreateDecoderFromStream(stream.get(), &GUID_VendorMicrosoft, WICDecodeMetadataCacheOnDemand, wicDecoder.put()));
	winrt::com_ptr<IWICBitmapFrameDecode> wicFrame{ nullptr };
	THROW_IF_FAILED(wicDecoder->GetFrame(0, wicFrame.put()));
	winrt::com_ptr<IWICFormatConverter> wicConverter{ nullptr };
	THROW_IF_FAILED(wicFactory->CreateFormatConverter(wicConverter.put()));
	winrt::com_ptr<IWICPalette> wicPalette{ nullptr };
	THROW_IF_FAILED(
		wicConverter->Initialize(
			wicFrame.get(),
			GUID_WICPixelFormat32bppPBGRA,
			WICBitmapDitherTypeNone,
			wicPalette.get(),
			0, WICBitmapPaletteTypeCustom
		)
	);
	winrt::com_ptr<IWICBitmap> wicBitmap{ nullptr };
	THROW_IF_FAILED(wicFactory->CreateBitmapFromSource(wicConverter.get(), WICBitmapCreateCacheOption::WICBitmapNoCache, wicBitmap.put()));

	auto drawingSurfaceInterop{ compositionSurface.as<ABI::Windows::UI::Composition::ICompositionDrawingSurfaceInterop>() };
	POINT offset = { 0, 0 };
	winrt::com_ptr<ID2D1DeviceContext> d2dContext{ nullptr };
	THROW_IF_FAILED(
		drawingSurfaceInterop->BeginDraw(nullptr, IID_PPV_ARGS(d2dContext.put()), &offset)
	);
	d2dContext->Clear();
	winrt::com_ptr<ID2D1Bitmap1> d2dBitmap{ nullptr };
	d2dContext->CreateBitmapFromWicBitmap(
		wicBitmap.get(),
		D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_NONE,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
		),
		d2dBitmap.put()
	);
	d2dContext->DrawBitmap(d2dBitmap.get());
	THROW_IF_FAILED(
		drawingSurfaceInterop->EndDraw()
	);

	return S_OK;
}
CATCH_RETURN()

HRESULT CAcrylicResources::EnsureAcrylicBrush()
{
	if (interopDCompDevice.get() != uDwmPrivates::CDesktopManager::s_pDesktopManagerInstance->GetDCompositionInteropDevice())
	{
		interopDCompDevice.copy_from(uDwmPrivates::CDesktopManager::s_pDesktopManagerInstance->GetDCompositionInteropDevice());

		noiceBrush = nullptr;
		lightMode_Active_Brush = nullptr;
		darkMode_Active_Brush = nullptr;
		lightMode_Inactive_Brush = nullptr;
		darkMode_Inactive_Brush = nullptr;
	}
	if (
		lightMode_Active_Brush && 
		darkMode_Active_Brush &&
		lightMode_Inactive_Brush &&
		darkMode_Inactive_Brush
	)
	{
		return S_OK;
	}

	auto compositor{ interopDCompDevice.as<winrt::Windows::UI::Composition::Compositor>() };
	lightMode_Inactive_Brush = CreateBrush(
		compositor,
		{ 255, 252, 252, 252 },
		{ 255, 252, 252, 252 },
		0.30f,
		0.85f,
		30.f
	);
	darkMode_Inactive_Brush = CreateBrush(
		compositor,
		{ 255, 44, 44, 44 },
		{ 255, 44, 44, 44 },
		0.55f,
		0.96f,
		30.f
	);
	lightMode_Active_Brush = CreateBrush(
		compositor,
		{ 255, 252, 252, 252 },
		{ 255, 252, 252, 252 },
		0.00f,
		0.65f,
		30.f
	);
	darkMode_Active_Brush = CreateBrush(
		compositor,
		{ 255, 44, 44, 44 },
		{ 255, 44, 44, 44 },
		0.15f,
		0.96f,
		30.f
	);

	return S_OK;
}

winrt::Windows::UI::Composition::CompositionBrush CMicaResources::CreateBrush(
	const winrt::Windows::UI::Composition::Compositor& compositor,
	const winrt::Windows::UI::Color& tintColor,
	const winrt::Windows::UI::Color& luminosityColor,
	float tintOpacity,
	float luminosityOpacity
) try
{
	auto tintColorEffect{ winrt::make_self<ColorSourceEffect>() };
	tintColorEffect->SetName(L"TintColor");
	tintColorEffect->SetColor(tintColor);

	auto tintOpacityEffect{ winrt::make_self<OpacityEffect>() };
	tintOpacityEffect->SetName(L"TintOpacity");
	tintOpacityEffect->SetOpacity(tintOpacity);
	tintOpacityEffect->SetInput(tintColorEffect);

	auto luminosityColorEffect{ winrt::make_self<ColorSourceEffect>() };
	luminosityColorEffect->SetName(L"LuminosityColor");
	luminosityColorEffect->SetColor(luminosityColor);

	auto luminosityOpacityEffect{ winrt::make_self<OpacityEffect>() };
	luminosityOpacityEffect->SetName(L"LuminosityOpacity");
	luminosityOpacityEffect->SetOpacity(luminosityOpacity);
	luminosityOpacityEffect->SetInput(luminosityColorEffect);

	auto luminosityBlendEffect{ winrt::make_self<BlendEffect>() };
	// NOTE: There is currently a bug where the names of BlendEffectMode::Luminosity and BlendEffectMode::Color are flipped->
	// This should be changed to Luminosity when/if the bug is fixed->
	luminosityBlendEffect->SetBlendMode(D2D1_BLEND_MODE_COLOR);
	winrt::Windows::UI::Composition::CompositionEffectSourceParameter backdropEffectSourceParameter{ L"BlurredWallpaperBackdrop" };
	luminosityBlendEffect->SetBackground(backdropEffectSourceParameter);
	luminosityBlendEffect->SetForeground(luminosityOpacityEffect);

	auto colorBlendEffect{ winrt::make_self<BlendEffect>() };
	// NOTE: There is currently a bug where the names of BlendEffectMode::Luminosity and BlendEffectMode::Color are flipped->
	// This should be changed to Color when/if the bug is fixed->
	colorBlendEffect->SetBlendMode(D2D1_BLEND_MODE_LUMINOSITY);
	colorBlendEffect->SetBackground(luminosityBlendEffect);
	colorBlendEffect->SetForeground(tintOpacityEffect);

	auto effectBrush
	{
		compositor.CreateEffectFactory(
			colorBlendEffect.as<winrt::Windows::Graphics::Effects::IGraphicsEffect>()
		).CreateBrush()
	};
	effectBrush.SetSourceParameter(L"BlurredWallpaperBackdrop", compositor.TryCreateBlurredWallpaperBackdropBrush());

	return effectBrush;
}
catch (...) { return nullptr; }

HRESULT CMicaResources::EnsureMicaBrush()
{
	if (interopDCompDevice.get() != uDwmPrivates::CDesktopManager::s_pDesktopManagerInstance->GetDCompositionInteropDevice())
	{
		interopDCompDevice.copy_from(uDwmPrivates::CDesktopManager::s_pDesktopManagerInstance->GetDCompositionInteropDevice());

		lightMode_Active_Brush = nullptr;
		darkMode_Active_Brush = nullptr;
		lightMode_Inactive_Brush = nullptr;
		darkMode_Inactive_Brush = nullptr;
	}
	if (
		lightMode_Active_Brush &&
		darkMode_Active_Brush &&
		lightMode_Inactive_Brush &&
		darkMode_Inactive_Brush
	)
	{
		return S_OK;
	}

	auto compositor{ interopDCompDevice.as<winrt::Windows::UI::Composition::Compositor>() };
	lightMode_Inactive_Brush = CreateBrush(
		compositor,
		{ 255, 252, 252, 252 },
		{ 255, 252, 252, 252 },
		0.30f,
		0.85f
	);
	darkMode_Inactive_Brush = CreateBrush(
		compositor,
		{ 255, 44, 44, 44 },
		{ 255, 44, 44, 44 },
		0.55f,
		0.96f
	);
	lightMode_Active_Brush = CreateBrush(
		compositor,
		{ 255, 252, 252, 252 },
		{ 255, 252, 252, 252 },
		0.00f,
		0.65f
	);
	darkMode_Active_Brush = CreateBrush(
		compositor,
		{ 255, 44, 44, 44 },
		{ 255, 44, 44, 44 },
		0.15f,
		0.96f
	);

	return S_OK;
}

winrt::Windows::UI::Composition::CompositionBrush CAeroResources::CreateBrush(
	const winrt::Windows::UI::Composition::Compositor& compositor,
	const winrt::Windows::UI::Color& tintColor,
	float tintOpacity,
	float exposureAmount,
	float blurAmount
)
{
	auto tintColorEffect{ winrt::make_self<ColorSourceEffect>() };
	tintColorEffect->SetName(L"TintColor");
	tintColorEffect->SetColor(tintColor);

	auto tintOpacityEffect{ winrt::make_self<OpacityEffect>() };
	tintOpacityEffect->SetName(L"TintOpacity");
	tintOpacityEffect->SetOpacity(tintOpacity);
	tintOpacityEffect->SetInput(tintColorEffect);

	auto gaussianBlurEffect{ winrt::make_self<GaussianBlurEffect>() };
	gaussianBlurEffect->SetName(L"Blur");
	gaussianBlurEffect->SetBorderMode(D2D1_BORDER_MODE_HARD);
	gaussianBlurEffect->SetBlurAmount(blurAmount);
	winrt::Windows::UI::Composition::CompositionEffectSourceParameter backdropEffectSourceParameter{ L"Backdrop" };
	gaussianBlurEffect->SetInput(backdropEffectSourceParameter);

	auto exposureEffect{ winrt::make_self<ExposureEffect>() };
	exposureEffect->SetName(L"Exposure");
	exposureEffect->SetExposure(exposureAmount);
	exposureEffect->SetInput(gaussianBlurEffect);

	auto compositeEffect{ winrt::make_self<CompositeStepEffect>() };
	compositeEffect->SetCompositeMode(D2D1_COMPOSITE_MODE_SOURCE_OVER);
	compositeEffect->SetDestination(exposureEffect);
	compositeEffect->SetSource(tintOpacityEffect);

	auto effectBrush
	{
		compositor.CreateEffectFactory(
			compositeEffect.as<winrt::Windows::Graphics::Effects::IGraphicsEffect>()
		).CreateBrush()
	};
	effectBrush.SetSourceParameter(L"Backdrop", compositor.CreateBackdropBrush());

	return effectBrush;
}

HRESULT CAeroResources::EnsureAeroBrush()
{
	if (interopDCompDevice.get() != uDwmPrivates::CDesktopManager::s_pDesktopManagerInstance->GetDCompositionInteropDevice())
	{
		interopDCompDevice.copy_from(uDwmPrivates::CDesktopManager::s_pDesktopManagerInstance->GetDCompositionInteropDevice());

		lightMode_Active_Brush = nullptr;
		darkMode_Active_Brush = nullptr;
		lightMode_Inactive_Brush = nullptr;
		darkMode_Inactive_Brush = nullptr;
	}
	if (
		lightMode_Active_Brush &&
		darkMode_Active_Brush &&
		lightMode_Inactive_Brush &&
		darkMode_Inactive_Brush
		)
	{
		return S_OK;
	}

	auto compositor{ interopDCompDevice.as<winrt::Windows::UI::Composition::Compositor>() };
	lightMode_Inactive_Brush = CreateBrush(
		compositor,
		{ 255, 252, 252, 252 },
		0.2f,
		0.15f,
		5.f
	);
	darkMode_Inactive_Brush = CreateBrush(
		compositor,
		{ 255, 44, 44, 44 },
		0.2f,
		-0.15f,
		5.f
	);
	lightMode_Active_Brush = CreateBrush(
		compositor,
		{ 255, 252, 252, 252 },
		0.2f,
		0.0f,
		5.f
	);
	darkMode_Active_Brush = CreateBrush(
		compositor,
		{ 255, 44, 44, 44 },
		0.2f,
		0.0f,
		5.f
	);

	return S_OK;
}

HRESULT CGlassReflectionResources::EnsureGlassSurface() try
{
	if (interopDCompDevice.get() != uDwmPrivates::CDesktopManager::s_pDesktopManagerInstance->GetDCompositionInteropDevice())
	{
		interopDCompDevice.copy_from(uDwmPrivates::CDesktopManager::s_pDesktopManagerInstance->GetDCompositionInteropDevice());

		drawingSurface = nullptr;
	}
	if (drawingSurface)
	{
		return S_OK;
	}

	auto compositor{ interopDCompDevice.as<winrt::Windows::UI::Composition::Compositor>() };
	winrt::Windows::UI::Composition::CompositionGraphicsDevice graphicsDevice{ nullptr };
	THROW_IF_FAILED(
		compositor.as<ABI::Windows::UI::Composition::ICompositorInterop>()->CreateGraphicsDevice(
			uDwmPrivates::CDesktopManager::s_pDesktopManagerInstance->GetD2DDevice(),
			reinterpret_cast<ABI::Windows::UI::Composition::ICompositionGraphicsDevice**>(winrt::put_abi(graphicsDevice))
		)
	);

	{
		GetModuleFileName(wil::GetModuleInstanceHandle(), filePath, MAX_PATH);
		PathCchRemoveFileSpec(filePath, MAX_PATH);
		PathCchAppend(filePath, MAX_PATH, L"data\\glass.png");
	}
	wil::unique_hfile file{ CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0) };
	THROW_LAST_ERROR_IF(!file.is_valid());

	LARGE_INTEGER fileSize{};
	THROW_IF_WIN32_BOOL_FALSE(GetFileSizeEx(file.get(), &fileSize));

	auto buffer{ std::make_unique<BYTE[]>(static_cast<size_t>(fileSize.QuadPart)) };
	THROW_IF_WIN32_BOOL_FALSE(ReadFile(file.get(), buffer.get(), static_cast<DWORD>(fileSize.QuadPart), nullptr, nullptr));
	winrt::com_ptr<IStream> stream{ SHCreateMemStream(buffer.get(), static_cast<UINT>(fileSize.QuadPart)), winrt::take_ownership_from_abi};
	THROW_LAST_ERROR_IF_NULL(stream);

	winrt::com_ptr<IWICImagingFactory2> wicFactory{ nullptr };
	wicFactory.copy_from(uDwmPrivates::CDesktopManager::s_pDesktopManagerInstance->GetWICFactory());
	winrt::com_ptr<IWICBitmapDecoder> wicDecoder{ nullptr };
	THROW_IF_FAILED(wicFactory->CreateDecoderFromStream(stream.get(), &GUID_VendorMicrosoft, WICDecodeMetadataCacheOnDemand, wicDecoder.put()));
	winrt::com_ptr<IWICBitmapFrameDecode> wicFrame{ nullptr };
	THROW_IF_FAILED(wicDecoder->GetFrame(0, wicFrame.put()));
	winrt::com_ptr<IWICFormatConverter> wicConverter{ nullptr };
	THROW_IF_FAILED(wicFactory->CreateFormatConverter(wicConverter.put()));
	winrt::com_ptr<IWICPalette> wicPalette{ nullptr };
	THROW_IF_FAILED(
		wicConverter->Initialize(
			wicFrame.get(),
			GUID_WICPixelFormat32bppPBGRA,
			WICBitmapDitherTypeNone,
			wicPalette.get(),
			0, WICBitmapPaletteTypeCustom
		)
	);
	winrt::com_ptr<IWICBitmap> wicBitmap{ nullptr };
	THROW_IF_FAILED(wicFactory->CreateBitmapFromSource(wicConverter.get(), WICBitmapCreateCacheOption::WICBitmapNoCache, wicBitmap.put()));

	UINT width{0}, height{0};
	THROW_IF_FAILED(wicBitmap->GetSize(&width, &height));
	drawingSurface = graphicsDevice.CreateDrawingSurface(
		{ static_cast<float>(width), static_cast<float>(height) },
		winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
		winrt::Windows::Graphics::DirectX::DirectXAlphaMode::Premultiplied
	);
	auto drawingSurfaceInterop{ drawingSurface.as<ABI::Windows::UI::Composition::ICompositionDrawingSurfaceInterop>() };
	POINT offset = { 0, 0 };
	winrt::com_ptr<ID2D1DeviceContext> d2dContext{ nullptr };
	THROW_IF_FAILED(
		drawingSurfaceInterop->BeginDraw(nullptr, IID_PPV_ARGS(d2dContext.put()), &offset)
	);
	d2dContext->Clear();
	winrt::com_ptr<ID2D1Bitmap1> d2dBitmap{ nullptr };
	d2dContext->CreateBitmapFromWicBitmap(
		wicBitmap.get(),
		D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_NONE,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
		),
		d2dBitmap.put()
	);
	d2dContext->DrawBitmap(d2dBitmap.get());
	THROW_IF_FAILED(
		drawingSurfaceInterop->EndDraw()
	);

	return S_OK;
}
CATCH_RETURN()

HRESULT STDMETHODCALLTYPE CBackdropEffect::InitializeDCompAndVisual() try
{
	interopDCompDevice.copy_from(uDwmPrivates::CDesktopManager::s_pDesktopManagerInstance->GetDCompositionInteropDevice());

	THROW_IF_FAILED(
		interopDCompDevice->CreateSharedResource(
			IID_PPV_ARGS(interopCompositionTarget.put())
		)
	);
	wil::unique_handle resourceHandle{ nullptr };
	THROW_IF_FAILED(
		interopDCompDevice->OpenSharedResourceHandle(interopCompositionTarget.get(), resourceHandle.put())
	);
	auto interopCompositor{ interopDCompDevice.as<winrt::Windows::UI::Composition::Compositor>() };
	spriteVisual = interopCompositor.CreateSpriteVisual();

	winrt::com_ptr<IDCompositionVisual2> dcompVisual{ nullptr };
	THROW_IF_FAILED(interopDCompDevice->CreateVisual(dcompVisual.put()));
	THROW_IF_FAILED(interopDCompDevice->Commit());

	auto interopVisual{ dcompVisual.as<DCompPrivates::IDCompositionVisualPartnerWinRTInterop>() };
	interopVisual->GetVisualCollection().InsertAtTop(spriteVisual);

	THROW_IF_FAILED(interopCompositionTarget->SetRoot(dcompVisual.get()));
	THROW_IF_FAILED(interopDCompDevice->Commit());
	THROW_IF_FAILED(interopDCompDevice->WaitForCommitCompletion());

	currentBrush = nullptr;
	if (!udwmVisual)
	{
		THROW_IF_FAILED(uDwmPrivates::CVisual::CreateFromSharedHandle(resourceHandle.get(), udwmVisual.put()));
	}
	else
	{
		udwmVisual->GetVisualProxy()->Release();
		THROW_IF_FAILED(udwmVisual->InitializeFromSharedHandle(resourceHandle.get()));
	}

	return S_OK;
}
CATCH_RETURN()

HRESULT STDMETHODCALLTYPE CBackdropEffect::InitializeVisualTreeClone(CBackdropEffect* backdrop) try
{
	backdrop->currentSize = currentSize;
	backdrop->useDarkMode = useDarkMode;
	backdrop->windowActivated = windowActivated;
	backdrop->spriteVisual.Size(currentSize);
	backdrop->spriteVisual.Brush(currentBrush);

	return S_OK;
}
CATCH_RETURN()

HRESULT STDMETHODCALLTYPE CBackdropEffect::UpdateBackdrop(uDwmPrivates::CTopLevelWindow* topLevelWindow) try
{
	if (interopDCompDevice.get() != uDwmPrivates::CDesktopManager::s_pDesktopManagerInstance->GetDCompositionInteropDevice())
	{
		THROW_IF_FAILED(InitializeDCompAndVisual());
	}
	// Custom resources
	THROW_IF_FAILED(EnsureBackdropResources());
	// Color mode changed
	useDarkMode = topLevelWindow->GetWindowData()->IsUsingDarkMode();
	// Window activation changed
	windowActivated = topLevelWindow->TreatAsActiveWindow();
	// Window size changed
	winrt::Windows::Foundation::Numerics::float2 newSize{ 0.f, 0.f };
	HWND hwnd{ topLevelWindow->GetWindowData()->GetHwnd() };
	if (IsIconic(hwnd))
	{
		newSize = { 0.f, 0.f };
	}
	else
	{
		bool maximized{ static_cast<bool>(IsMaximized(hwnd)) };
		RECT windowRect{};
		THROW_HR_IF_NULL(E_INVALIDARG, topLevelWindow->GetActualWindowRect(&windowRect, false, true, false));
		MARGINS margins{};
		topLevelWindow->GetBorderMargins(&margins);
		newSize =
		{
			static_cast<float>(wil::rect_width(windowRect) + (maximized ? margins.cxLeftWidth + margins.cxRightWidth : 0)),
			static_cast<float>(wil::rect_height(windowRect) + (maximized ? margins.cyTopHeight + margins.cyBottomHeight : 0))
		};
	}
	if (currentSize != newSize)
	{
		currentSize = newSize;
		spriteVisual.Size(currentSize);
		THROW_IF_FAILED(udwmVisual->SetSize({static_cast<LONG>(currentSize.x), static_cast<LONG>(currentSize.y) }));
	}

	return S_OK;
}
CATCH_RETURN()

HRESULT STDMETHODCALLTYPE CAcrylicBackdrop::UpdateBackdrop(uDwmPrivates::CTopLevelWindow* topLevelWindow) try
{
	THROW_IF_FAILED(CBackdropEffect::UpdateBackdrop(topLevelWindow));
	auto newBrush{ s_sharedResources.ChooseIdealBrush(useDarkMode, windowActivated) };
	if ( currentBrush != newBrush)
	{
		if (currentBrush)
		{
			auto compositor{ interopDCompDevice.as<winrt::Windows::UI::Composition::Compositor>() };
			auto crossfadeBrush
			{
				CBackdropResources::CreateCrossFadeEffectBrush(
					compositor,
					currentBrush, newBrush
				)
			};
			crossfadeBrush.StartAnimation(
				L"Crossfade.Weight",
				CBackdropResources::CreateCrossFadeAnimation(
					compositor,
					crossFadeTime
				)
			);
			spriteVisual.Brush(crossfadeBrush);
		}
		else
		{
			spriteVisual.Brush(newBrush);
		}
		currentBrush = newBrush;
	}

	return S_OK;
}
CATCH_RETURN()

HRESULT STDMETHODCALLTYPE CAcrylicBackdrop::EnsureBackdropResources()
{
	return s_sharedResources.EnsureAcrylicBrush();
}

HRESULT STDMETHODCALLTYPE CMicaBackdrop::UpdateBackdrop(uDwmPrivates::CTopLevelWindow* topLevelWindow) try
{
	THROW_IF_FAILED(CBackdropEffect::UpdateBackdrop(topLevelWindow));
	auto newBrush{ s_sharedResources.ChooseIdealBrush(useDarkMode, windowActivated) };
	if (currentBrush != newBrush)
	{
		if (currentBrush)
		{
			auto compositor{ interopDCompDevice.as<winrt::Windows::UI::Composition::Compositor>() };
			auto crossfadeBrush
			{
				CBackdropResources::CreateCrossFadeEffectBrush(
					compositor,
					currentBrush, newBrush
				)
			};
			crossfadeBrush.StartAnimation(
				L"Crossfade.Weight",
				CBackdropResources::CreateCrossFadeAnimation(
					compositor,
					crossFadeTime
				)
			);
			spriteVisual.Brush(crossfadeBrush);
		}
		else
		{
			spriteVisual.Brush(newBrush);
		}
		currentBrush = newBrush;
	}

	return S_OK;
}
CATCH_RETURN()

HRESULT STDMETHODCALLTYPE CMicaBackdrop::EnsureBackdropResources()
{
	return s_sharedResources.EnsureMicaBrush();
}

HRESULT STDMETHODCALLTYPE CAeroBackdrop::UpdateBackdrop(uDwmPrivates::CTopLevelWindow* topLevelWindow) try
{
	THROW_IF_FAILED(CBackdropEffect::UpdateBackdrop(topLevelWindow));
	auto newBrush{ s_sharedResources.ChooseIdealBrush(useDarkMode, windowActivated) };
	if (currentBrush != newBrush)
	{
		if (currentBrush)
		{
			auto compositor{ interopDCompDevice.as<winrt::Windows::UI::Composition::Compositor>() };
			auto crossfadeBrush
			{
				CBackdropResources::CreateCrossFadeEffectBrush(
					compositor,
					currentBrush, newBrush
				)
			};
			crossfadeBrush.StartAnimation(
				L"Crossfade.Weight",
				CBackdropResources::CreateCrossFadeAnimation(
					compositor,
					crossFadeTime
				)
			);
			spriteVisual.Brush(crossfadeBrush);
		}
		else
		{
			spriteVisual.Brush(newBrush);
		}
		currentBrush = newBrush;
	}

	return S_OK;
}
CATCH_RETURN()

HRESULT STDMETHODCALLTYPE CAeroBackdrop::EnsureBackdropResources()
{
	return s_sharedResources.EnsureAeroBrush();
}

HRESULT STDMETHODCALLTYPE CGlassReflectionBackdrop::UpdateBackdrop(uDwmPrivates::CTopLevelWindow* topLevelWindow) try
{
	THROW_IF_FAILED(CBackdropEffect::UpdateBackdrop(topLevelWindow));
	RECT windowRect{};
	THROW_HR_IF_NULL(E_INVALIDARG, topLevelWindow->GetActualWindowRect(&windowRect, false, true, false));
	if (currentBrush != glassSurfaceBrush)
	{
		spriteVisual.Brush(glassSurfaceBrush);
		currentBrush = glassSurfaceBrush;
	}
	HWND hwnd{ topLevelWindow->GetWindowData()->GetHwnd() };
	HMONITOR monitor{ MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST) };
	THROW_LAST_ERROR_IF_NULL(monitor);

	RECT monitorRect{};
	MONITORINFO mi{ sizeof(MONITORINFO) };
	THROW_IF_WIN32_BOOL_FALSE(GetMonitorInfoW(monitor, &mi));

	if (currentMonitor != monitor || !EqualRect(&currentMonitorRect, &mi.rcMonitor))
	{
		auto surfaceSize{ s_sharedResources.drawingSurface.SizeInt32() };
		auto scaleFactor
		{
			[&]()
			{
				if (
					static_cast<float>(surfaceSize.Width) / static_cast<float>(surfaceSize.Height) <=
					static_cast<float>(wil::rect_width(mi.rcMonitor)) / static_cast<float>(wil::rect_height(mi.rcMonitor))
				)
				{
					return static_cast<float>(wil::rect_width(mi.rcMonitor)) / static_cast<float>(surfaceSize.Width);
				}
				else
				{
					return static_cast<float>(wil::rect_height(mi.rcMonitor)) / static_cast<float>(surfaceSize.Height);
				}

				return 1.f;
			} ()
		};
		glassSurfaceBrush.Scale(
			winrt::Windows::Foundation::Numerics::float2{ scaleFactor, scaleFactor }
		);
		winrt::Windows::Foundation::Numerics::float2 scaledSize
		{
			static_cast<float>(surfaceSize.Width) * scaleFactor,
			static_cast<float>(surfaceSize.Height) * scaleFactor
		};

		fixedOffset = 
		{ 
			(static_cast<float>(wil::rect_width(mi.rcMonitor)) - scaledSize.x) / 2.f,
			(static_cast<float>(wil::rect_height(mi.rcMonitor)) - scaledSize.y) / 2.f
		};
		currentMonitor = monitor;
		currentMonitorRect = mi.rcMonitor;
	}
	if ((currentWindowRect.left != windowRect.left) || (currentWindowRect.top != windowRect.top))
	{
		relativeOffset.x += static_cast<float>(currentWindowRect.left - windowRect.left);
		relativeOffset.y += static_cast<float>(currentWindowRect.top - windowRect.top);
		
		MARGINS margins{};
		topLevelWindow->GetBorderMargins(&margins);
		glassSurfaceBrush.Offset(
			winrt::Windows::Foundation::Numerics::float2
			{
				-static_cast<float>(windowRect.left - mi.rcMonitor.left) - 
				static_cast<float>(relativeOffset.x * 0.25f) +
				static_cast<float>(IsMaximized(hwnd) ? margins.cxLeftWidth : 0) +
				fixedOffset.x,
				-static_cast<float>(windowRect.top - mi.rcMonitor.top) - 
				static_cast<float>(relativeOffset.y * 0.25f) +
				static_cast<float>(IsMaximized(hwnd) ? margins.cyTopHeight : 0) +
				fixedOffset.y	
			}
		);

		currentWindowRect = windowRect;
	}

	return S_OK;
}
CATCH_RETURN()

HRESULT STDMETHODCALLTYPE CGlassReflectionBackdrop::EnsureBackdropResources() try
{
	THROW_IF_FAILED(s_sharedResources.EnsureGlassSurface());
	if (!glassSurfaceBrush || interopDCompDevice.get() != uDwmPrivates::CDesktopManager::s_pDesktopManagerInstance->GetDCompositionInteropDevice())
	{
		interopDCompDevice.copy_from(uDwmPrivates::CDesktopManager::s_pDesktopManagerInstance->GetDCompositionInteropDevice());
		glassSurfaceBrush = interopDCompDevice.as<winrt::Windows::UI::Composition::Compositor>().CreateSurfaceBrush(s_sharedResources.drawingSurface);
		glassSurfaceBrush.Stretch(winrt::Windows::UI::Composition::CompositionStretch::None);
		glassSurfaceBrush.HorizontalAlignmentRatio(0.f);
		glassSurfaceBrush.VerticalAlignmentRatio(0.f);
	}

	return S_OK;
}
CATCH_RETURN()


void CCompositedBackdrop::ConnectPrimaryBackdropToParent()
{
	if (m_backdropEffect)
	{
		THROW_IF_FAILED(m_backdropEffect->InitializeDCompAndVisual());
		THROW_IF_FAILED(m_visual->GetVisualCollection()->InsertRelative(m_backdropEffect->udwmVisual.get(), nullptr, true, true));
	}
}
void CCompositedBackdrop::ConnectGlassReflectionToParent()
{
	if (m_glassReflection)
	{
		THROW_IF_FAILED(m_glassReflection->InitializeDCompAndVisual());
		THROW_IF_FAILED(m_visual->GetVisualCollection()->InsertRelative(m_glassReflection->udwmVisual.get(), m_backdropEffect->udwmVisual.get(), true, true));
	}
}
void CCompositedBackdrop::ConnectBorderFillToParent()
{

}

CCompositedBackdrop::CCompositedBackdrop(BackdropType type, bool glassReflection) : m_type{type}
{
	if (SystemHelper::g_buildNumber < 26020)
	{
		THROW_IF_FAILED(
			UDWM_CALL_ORG_STATIC_BY_TYPE(
				HRESULT(STDMETHODCALLTYPE*)(uDwmPrivates::CVisual**),
				"CVisual::Create",
				m_visual.put()
			)
		);
	}
	else
	{
		THROW_IF_FAILED(
			UDWM_CALL_ORG_STATIC_BY_TYPE(
				HRESULT(STDMETHODCALLTYPE*)(uDwmPrivates::CVisual**),
				"CContainerVisual::Create",
				m_visual.put()
			)
		);
	}

	UpdateBackdropType(m_type);
	UpdateGlassReflection(glassReflection);
	// ...

	MARGINS margins{ 0, 0, 0, 0 };
	m_visual->SetInsetFromParent(&margins);
	m_visual->SetDirtyChildren();
}

void CCompositedBackdrop::InitializeVisualTreeClone(CCompositedBackdrop* backdrop)
{
	if (m_backdropEffect)
	{
		THROW_IF_FAILED(m_backdropEffect->InitializeVisualTreeClone(backdrop->m_backdropEffect.get()));
	}
	if (m_glassReflection)
	{
		THROW_IF_FAILED(m_glassReflection->InitializeVisualTreeClone(backdrop->m_glassReflection.get()));
	}
	// ...

	if (backdrop->m_clipRgn)
	{
		backdrop->m_clipRgn.reset(CreateRectRgn(0, 0, 0, 0));
		THROW_LAST_ERROR_IF_NULL(backdrop->m_clipRgn);
		THROW_LAST_ERROR_IF(CopyRgn(backdrop->m_clipRgn.get(), m_clipRgn.get()) == ERROR);

		wil::com_ptr<uDwmPrivates::CRgnGeometryProxy> geometry{ nullptr };
		THROW_IF_FAILED(
			uDwmPrivates::ResourceHelper::CreateGeometryFromHRGN(
				backdrop->m_clipRgn.get(),
				geometry.put()
			)
		);
		THROW_IF_FAILED(backdrop->m_visual->GetVisualProxy()->SetClip(geometry.get()));
	}
	else
	{
		THROW_IF_FAILED(backdrop->m_visual->GetVisualProxy()->SetClip(nullptr));
	}
}

CCompositedBackdrop::~CCompositedBackdrop()
{
	m_visual->GetVisualCollection()->RemoveAll();
}

void CCompositedBackdrop::UpdateGlassReflection(bool enable)
{
	if (enable && !m_glassReflection)
	{
		m_glassReflection = std::make_unique<CGlassReflectionBackdrop>();
		ConnectGlassReflectionToParent();
	}
	if (!enable && m_glassReflection)
	{
		THROW_IF_FAILED(m_visual->GetVisualCollection()->Remove(m_glassReflection->udwmVisual.get()));
		m_glassReflection.reset(nullptr);
	}
}
void CCompositedBackdrop::UpdateBackdropType(BackdropType type)
{
	m_type = type;

	if (m_backdropEffect)
	{
		THROW_IF_FAILED(m_visual->GetVisualCollection()->Remove(m_backdropEffect->udwmVisual.get()));
		m_backdropEffect.reset();
	}

	switch (m_type)
	{
	case BackdropType::Acrylic:
	{
		m_backdropEffect = std::make_unique<CAcrylicBackdrop>();
		break;
	}
	case BackdropType::Mica:
	{
		m_backdropEffect = std::make_unique<CMicaBackdrop>();
		break;
	}
	case BackdropType::Aero:
	{
		m_backdropEffect = std::make_unique<CAeroBackdrop>();
		break;
	}
	default:
		break;
	}

	ConnectPrimaryBackdropToParent();
}
void CCompositedBackdrop::Update(uDwmPrivates::CTopLevelWindow* window, HRGN hrgn)
{
	if (m_backdropEffect)
	{
		m_backdropEffect->UpdateBackdrop(window);
	}
	if (m_glassReflection)
	{
		m_glassReflection->UpdateBackdrop(window);
	}
	// ...
	
	// Geometry changed
	if (hrgn != m_clipRgn.get())
	{
		if (hrgn && !EqualRgn(hrgn, m_clipRgn.get()))
		{
			m_clipRgn.reset(CreateRectRgn(0, 0, 0, 0));
			THROW_LAST_ERROR_IF_NULL(m_clipRgn);
			CopyRgn(m_clipRgn.get(), hrgn);

			wil::com_ptr<uDwmPrivates::CRgnGeometryProxy> geometry{ nullptr };
			THROW_IF_FAILED(
				uDwmPrivates::ResourceHelper::CreateGeometryFromHRGN(
					m_clipRgn.get(),
					geometry.put()
				)
			);
			THROW_IF_FAILED(m_visual->GetVisualProxy()->SetClip(geometry.get()));
		}
		else if (!hrgn && m_clipRgn)
		{
			m_clipRgn.reset();
			THROW_IF_FAILED(m_visual->GetVisualProxy()->SetClip(nullptr));
		}
	}
}