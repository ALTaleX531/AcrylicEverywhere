#pragma once
#include "udwmPrivates.hpp"

namespace AcrylicEverywhere
{
	struct CBackdropResources
	{
		winrt::com_ptr<DCompPrivates::IDCompositionDesktopDevicePartner> interopDCompDevice{ nullptr };
		winrt::Windows::UI::Composition::CompositionBrush lightMode_Active_Brush{ nullptr };
		winrt::Windows::UI::Composition::CompositionBrush darkMode_Active_Brush{ nullptr };
		winrt::Windows::UI::Composition::CompositionBrush lightMode_Inactive_Brush{ nullptr };
		winrt::Windows::UI::Composition::CompositionBrush darkMode_Inactive_Brush{ nullptr };

		static winrt::Windows::UI::Composition::CompositionBrush CreateCrossFadeEffectBrush(
			const winrt::Windows::UI::Composition::Compositor& compositor,
			const winrt::Windows::UI::Composition::CompositionBrush& from,
			const winrt::Windows::UI::Composition::CompositionBrush& to
		);
		static winrt::Windows::UI::Composition::ScalarKeyFrameAnimation CreateCrossFadeAnimation(
			const winrt::Windows::UI::Composition::Compositor& compositor,
			winrt::Windows::Foundation::TimeSpan const& crossfadeTime
		);
		inline winrt::Windows::UI::Composition::CompositionBrush ChooseIdealBrush(bool darkMode, bool active);
	};

	struct CAcrylicResources : CBackdropResources
	{
		winrt::Windows::UI::Composition::CompositionBrush noiceBrush{ nullptr };

		winrt::Windows::UI::Composition::CompositionBrush CreateBrush(
			const winrt::Windows::UI::Composition::Compositor& compositor,
			const winrt::Windows::UI::Color& tintColor,
			const winrt::Windows::UI::Color& luminosityColor,
			float tintOpacity,
			float luminosityOpacity,
			float blurAmount
		);
		HRESULT EnsureNoiceSurfaceBrush();
		HRESULT EnsureAcrylicBrush();
	};
	struct CMicaResources : CBackdropResources
	{
		winrt::Windows::UI::Composition::CompositionBrush CreateBrush(
			const winrt::Windows::UI::Composition::Compositor& compositor,
			const winrt::Windows::UI::Color& tintColor,
			const winrt::Windows::UI::Color& luminosityColor,
			float tintOpacity,
			float luminosityOpacity
		);
		HRESULT EnsureMicaBrush();
	};
	struct CAeroResources : CBackdropResources
	{
		winrt::Windows::UI::Composition::CompositionBrush CreateBrush(
			const winrt::Windows::UI::Composition::Compositor& compositor,
			const winrt::Windows::UI::Color& tintColor,
			float tintOpacity,
			float exposureAmount,
			float blurAmount
		);
		HRESULT EnsureAeroBrush();
	};
	struct CGlassReflectionResources
	{
		winrt::com_ptr<DCompPrivates::IDCompositionDesktopDevicePartner> interopDCompDevice{ nullptr };
		winrt::Windows::UI::Composition::CompositionDrawingSurface drawingSurface{ nullptr };
		WCHAR filePath[MAX_PATH + 1]{};

		HRESULT EnsureGlassSurface();
	};

	struct CBackdropEffect
	{
		bool useDarkMode{ false };
		bool windowActivated{ false };

		winrt::com_ptr<DCompPrivates::IDCompositionDesktopDevicePartner> interopDCompDevice{ nullptr };
		winrt::com_ptr<uDwmPrivates::CVisual> udwmVisual{ nullptr };
		winrt::com_ptr<DCompPrivates::InteropCompositionTarget> interopCompositionTarget{ nullptr };
		winrt::Windows::UI::Composition::CompositionBrush currentBrush{ nullptr };
		winrt::Windows::UI::Composition::SpriteVisual spriteVisual{ nullptr };
		winrt::Windows::Foundation::Numerics::float2 currentSize{ 0.f, 0.f };
		std::chrono::milliseconds crossFadeTime{ 187 };

		STDMETHOD(InitializeDCompAndVisual)();
		STDMETHOD(InitializeVisualTreeClone)(CBackdropEffect* backdrop);
		STDMETHOD(UpdateBackdrop)(uDwmPrivates::CTopLevelWindow* topLevelWindow);
		STDMETHOD(EnsureBackdropResources)() PURE;
	};

	struct CAcrylicBackdrop : CBackdropEffect
	{
		static CAcrylicResources s_sharedResources;

		STDMETHOD(UpdateBackdrop)(uDwmPrivates::CTopLevelWindow* topLevelWindow) override;
		STDMETHOD(EnsureBackdropResources)() override;
	};
	struct CMicaBackdrop : CBackdropEffect
	{
		static CMicaResources s_sharedResources;

		STDMETHOD(UpdateBackdrop)(uDwmPrivates::CTopLevelWindow* topLevelWindow) override;
		STDMETHOD(EnsureBackdropResources)() override;
	};
	struct CAeroBackdrop : CBackdropEffect
	{
		static CAeroResources s_sharedResources;

		CAeroBackdrop() { crossFadeTime = std::chrono::milliseconds{87}; }
		STDMETHOD(UpdateBackdrop)(uDwmPrivates::CTopLevelWindow* topLevelWindow) override;
		STDMETHOD(EnsureBackdropResources)() override;
	};
	struct CGlassReflectionBackdrop : CBackdropEffect
	{
		static CGlassReflectionResources s_sharedResources;
		winrt::Windows::Foundation::Numerics::float2 relativeOffset{};
		winrt::Windows::Foundation::Numerics::float2 fixedOffset{};
		RECT currentWindowRect{};
		RECT currentMonitorRect{};
		HMONITOR currentMonitor{ nullptr };
		winrt::Windows::UI::Composition::CompositionSurfaceBrush glassSurfaceBrush{ nullptr };

		STDMETHOD(UpdateBackdrop)(uDwmPrivates::CTopLevelWindow* topLevelWindow) override;
		STDMETHOD(EnsureBackdropResources)() override;
	};

	enum class BackdropType
	{
		Acrylic,
		Mica,
		Aero
	};
	class CCompositedBackdrop
	{
		BackdropType m_type{ BackdropType::Acrylic };
		wil::unique_hrgn m_clipRgn{ nullptr };
		winrt::com_ptr<uDwmPrivates::CVisual> m_visual{ nullptr };
		std::unique_ptr<CBackdropEffect> m_backdropEffect{ nullptr };
		std::unique_ptr<CGlassReflectionBackdrop> m_glassReflection{ nullptr };
		
		void ConnectPrimaryBackdropToParent();
		void ConnectGlassReflectionToParent();
		void ConnectBorderFillToParent();
	public:
		CCompositedBackdrop(BackdropType type, bool glassReflection);
		CCompositedBackdrop(CCompositedBackdrop&& backdrop) = delete;
		CCompositedBackdrop(const CCompositedBackdrop& backdrop) = delete;
		~CCompositedBackdrop();
		void InitializeVisualTreeClone(CCompositedBackdrop* backdrop);

		void UpdateGlassReflection(bool enable);
		void UpdateBackdropType(BackdropType type);
		void Update(uDwmPrivates::CTopLevelWindow* window, HRGN hrgn);
		uDwmPrivates::CVisual* GetVisual() const { return m_visual.get(); }
		CGlassReflectionBackdrop* GetGlassReflection() const { return m_glassReflection.get(); };
		HRGN GetClipRegion() const { return m_clipRgn.get(); };
	};
}