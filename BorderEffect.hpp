﻿#pragma once
#include "CanvasEffect.hpp"

namespace AcrylicEverywhere
{
	class BorderEffect : public CanvasEffect
	{
	public:
		BorderEffect() : CanvasEffect{ CLSID_D2D1Border }
		{
			SetExtendX();
			SetExtendY();
		}
		virtual ~BorderEffect() = default;

		void SetExtendX(D2D1_BORDER_EDGE_MODE extendX = D2D1_BORDER_EDGE_MODE_CLAMP)
		{
			SetProperty(D2D1_BORDER_PROP_EDGE_MODE_X, BoxValue(extendX));
		}
		void SetExtendY(D2D1_BORDER_EDGE_MODE extendY = D2D1_BORDER_EDGE_MODE_CLAMP)
		{
			SetProperty(D2D1_BORDER_PROP_EDGE_MODE_Y, BoxValue(extendY));
		}
	};
}