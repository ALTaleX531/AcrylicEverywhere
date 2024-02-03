#pragma once
#include "framework.h"
#include "wil.h"

namespace AcrylicEverywhere::ThemeHelper
{
	static HRESULT DrawTextWithGlow(
		HDC hdc,
		LPCWSTR pszText,
		int cchText,
		LPRECT prc,
		UINT dwFlags,
		UINT crText,
		UINT crGlow,
		UINT nGlowRadius,
		UINT nGlowIntensity,
		BOOL bPreMultiply,
		DTT_CALLBACK_PROC actualDrawTextCallback,
		LPARAM lParam
	)
	{
		static const auto s_actualDrawTextWithGlow{ reinterpret_cast<HRESULT(WINAPI*)(HDC, LPCWSTR, int, LPRECT, UINT, UINT, UINT, UINT, UINT, BOOL, DTT_CALLBACK_PROC, LPARAM)>(GetProcAddress(GetModuleHandle(TEXT("UxTheme")), MAKEINTRESOURCEA(126))) };

		if (s_actualDrawTextWithGlow) [[likely]]
		{
			return s_actualDrawTextWithGlow(hdc, pszText, cchText, prc, dwFlags, crText, crGlow, nGlowRadius, nGlowIntensity, bPreMultiply, actualDrawTextCallback, lParam);
		}

		return E_FAIL;
	}

	inline HRESULT DrawThemeContent(
		HDC hdc,
		const RECT& paintRect,
		LPCRECT clipRect,
		LPCRECT excludeRect,
		DWORD additionalFlags,
		std::function<void(HDC, HPAINTBUFFER, RGBQUAD*, int)> callback,
		std::byte alpha = std::byte{ 0xFF },
		bool useBlt = false,
		bool update = true
	) try
	{
		BOOL updateTarget{ FALSE };
		HDC memoryDC{ nullptr };
		HPAINTBUFFER bufferedPaint{ nullptr };
		BLENDFUNCTION blendFunction{ AC_SRC_OVER, 0, std::to_integer<BYTE>(alpha), AC_SRC_ALPHA };
		BP_PAINTPARAMS paintParams{ sizeof(BP_PAINTPARAMS), BPPF_ERASE | additionalFlags, excludeRect, !useBlt ? &blendFunction : nullptr };

		THROW_HR_IF_NULL(E_INVALIDARG, callback);

		SaveDC(hdc);
		auto restore = wil::scope_exit([hdc]{ RestoreDC(hdc, -1); });
		{
			auto cleanUp = wil::scope_exit([&]
			{
				if (bufferedPaint != nullptr)
				{
					EndBufferedPaint(bufferedPaint, updateTarget);
					bufferedPaint = nullptr;
				}
			});

			if (clipRect)
			{
				IntersectClipRect(hdc, clipRect->left, clipRect->top, clipRect->right, clipRect->bottom);
			}
			if (excludeRect)
			{
				ExcludeClipRect(hdc, excludeRect->left, excludeRect->top, excludeRect->right, excludeRect->bottom);
			}

			bufferedPaint = BeginBufferedPaint(hdc, &paintRect, BPBF_TOPDOWNDIB, &paintParams, &memoryDC);
			THROW_HR_IF_NULL(E_FAIL, bufferedPaint);

			{
				auto selectedFont{ wil::SelectObject(memoryDC, GetCurrentObject(hdc, OBJ_FONT)) };
				auto selectedBrush{ wil::SelectObject(memoryDC, GetCurrentObject(hdc, OBJ_BRUSH)) };
				auto selectedPen{ wil::SelectObject(memoryDC, GetCurrentObject(hdc, OBJ_PEN)) };

				int cxRow{ 0 };
				RGBQUAD* buffer{ nullptr };
				THROW_IF_FAILED(GetBufferedPaintBits(bufferedPaint, &buffer, &cxRow));

				callback(memoryDC, bufferedPaint, buffer, cxRow);
			}

			updateTarget = TRUE;
		}
		

		return S_OK;
	}
	CATCH_RETURN()

	static int DrawTextWithAlpha(
		HDC     hdc,
		LPCWSTR lpchText,
		int     cchText,
		LPRECT  lprc,
		UINT    format,
		decltype(&DrawTextW) drawTextProc
	) try
	{
		struct DrawTextContext
		{
			int m_result{ 0 };
			decltype(&DrawTextW) m_drawTextProc{ nullptr };
		} context{ 0, drawTextProc };

		auto drawTextCallback = [](HDC hdc, LPWSTR pszText, int cchText, LPRECT prc, UINT dwFlags, LPARAM lParam) -> int
		{
			return reinterpret_cast<DrawTextContext*>(lParam)->m_result = reinterpret_cast<DrawTextContext*>(lParam)->m_drawTextProc(hdc, pszText, cchText, prc, dwFlags);
		};

		auto callback = [&](HDC memoryDC, HPAINTBUFFER, RGBQUAD*, int)
		{
			THROW_IF_FAILED(
				ThemeHelper::DrawTextWithGlow(
					memoryDC,
					lpchText,
					cchText,
					lprc,
					format,
					GetTextColor(hdc),
					0,
					0,
					0,
					TRUE,
					drawTextCallback,
					(LPARAM)&context
				)
			);
		};

		auto callbackAsFallback = [&](HDC memoryDC, HPAINTBUFFER, RGBQUAD*, int)
		{
			DTTOPTS options =
			{
				sizeof(DTTOPTS),
				DTT_TEXTCOLOR | DTT_COMPOSITED | DTT_CALLBACK,
				GetTextColor(hdc),
				0,
				0,
				0,
				{},
				0,
				0,
				0,
				0,
				FALSE,
				0,
				drawTextCallback,
				(LPARAM)&context
			};
			wil::unique_htheme hTheme{ OpenThemeData(nullptr, TEXT("Composited::Window")) };

			THROW_HR_IF_NULL(E_FAIL, hTheme);
			THROW_IF_FAILED(DrawThemeTextEx(hTheme.get(), memoryDC, 0, 0, lpchText, cchText, format, lprc, &options));
		};

		THROW_HR_IF_NULL(E_INVALIDARG, hdc);

		if (FAILED(ThemeHelper::DrawThemeContent(hdc, *lprc, nullptr, nullptr, 0, callback))) [[unlikely]]
		{
			THROW_IF_FAILED(ThemeHelper::DrawThemeContent(hdc, *lprc, nullptr, nullptr, 0, callbackAsFallback));
		}

		return context.m_result;
	}
	catch(...) { return 0; }
}