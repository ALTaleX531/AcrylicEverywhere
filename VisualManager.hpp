#pragma once
#include "udwmPrivates.hpp"
#include "CompositedBackdrop.hpp"

namespace AcrylicEverywhere
{
	class CVisualManager
	{
	public:
		winrt::com_ptr<CCompositedBackdrop> GetOrCreateBackdrop(uDwmPrivates::CTopLevelWindow* target, bool createIfNecessary = false)
		{
			auto it{ m_backdropMap.find(target) };

			if (createIfNecessary)
			{
				if (it == m_backdropMap.end())
				{
					auto result{ m_backdropMap.emplace(target, winrt::make_self<CCompositedBackdrop>(target)) };
					if (result.second == true) 
					{ 
						it = result.first; 
						it->second->UpdateBackdropType(BackdropType::Aero);
						it->second->UpdateGlassReflectionState(true);
					}
				}
			}

			return it == m_backdropMap.end() ? nullptr : it->second;
		}
		winrt::com_ptr<CCompositedAccentBackdrop> GetOrCreateAccentBackdrop(uDwmPrivates::CTopLevelWindow* target, bool createIfNecessary = false)
		{
			auto it{ m_accentBackdropMap.find(target) };

			if (createIfNecessary)
			{
				if (it == m_accentBackdropMap.end())
				{
					auto result{ m_accentBackdropMap.emplace(target, winrt::make_self<CCompositedAccentBackdrop>(target)) };
					if (result.second == true) 
					{
						it = result.first;
						it->second->UpdateBackdropType(BackdropType::Aero);
						it->second->UpdateGlassReflectionState(true);
					}
				}
			}

			return it == m_accentBackdropMap.end() ? nullptr : it->second;
		}
		winrt::com_ptr<CCompositedBackdrop> TryCloneBackdropForWindow(uDwmPrivates::CTopLevelWindow* src, uDwmPrivates::CTopLevelWindow* dest)
		{
			if (auto backdrop{ GetOrCreateBackdrop(src) }; backdrop)
			{
				auto it{ m_backdropMap.find(dest) };
				auto clonedBackdrop{ winrt::make_self<CCompositedBackdrop>(backdrop.get(), dest)};

				if (it == m_backdropMap.end())
				{
					auto result{ m_backdropMap.emplace(dest, clonedBackdrop) };
					if (result.second == true) { it = result.first; }
				}
				else
				{
					std::swap(
						clonedBackdrop,
						it->second
					);
				}

				return it == m_backdropMap.end() ? nullptr : it->second;
			}

			return nullptr;
		}
		winrt::com_ptr<CCompositedBackdrop> TryCloneAnimatedBackdropForWindow(uDwmPrivates::CTopLevelWindow* src, uDwmPrivates::CSecondaryWindowRepresentation* dest)
		{
			if (auto backdrop{ GetOrCreateBackdrop(src) }; backdrop)
			{
				auto it{ m_animatedBackdropMap.find(dest) };
				if (it != m_animatedBackdropMap.end())
				{
					m_animatedBackdropMap.erase(it);
				}
				auto clonedBackdrop{ winrt::make_self<CCompositedBackdrop>(backdrop.get()) };
				auto result{ m_animatedBackdropMap.emplace(dest, clonedBackdrop) };
				if (result.second == true) { it = result.first; }

				return it == m_animatedBackdropMap.end() ? nullptr : it->second;
			}

			return nullptr;
		}
		winrt::com_ptr<CCompositedAccentBackdrop> TryCloneAnimatedAccentBackdropForWindow(uDwmPrivates::CTopLevelWindow* src, uDwmPrivates::CSecondaryWindowRepresentation* dest)
		{
			if (auto backdrop{ GetOrCreateAccentBackdrop(src) }; backdrop)
			{
				auto it{ m_animatedAccentBackdropMap.find(dest) };
				if (it != m_animatedAccentBackdropMap.end())
				{
					m_animatedAccentBackdropMap.erase(it);
				}
				auto clonedBackdrop{ winrt::make_self<CCompositedAccentBackdrop>(backdrop.get()) };
				auto result{ m_animatedAccentBackdropMap.emplace(dest, clonedBackdrop) };
				if (result.second == true) { it = result.first; }

				return it == m_animatedAccentBackdropMap.end() ? nullptr : it->second;
			}

			return nullptr;
		}
		winrt::com_ptr<CCompositedAccentBackdrop> TryCloneAccentBackdropForWindow(uDwmPrivates::CTopLevelWindow* src, uDwmPrivates::CTopLevelWindow* dest)
		{
			if (auto backdrop{ GetOrCreateAccentBackdrop(src) }; backdrop)
			{
				auto it{ m_accentBackdropMap.find(dest) };
				auto clonedBackdrop{ winrt::make_self<CCompositedAccentBackdrop>(backdrop.get(), dest) };

				if (it == m_accentBackdropMap.end())
				{
					auto result{ m_accentBackdropMap.emplace(dest, clonedBackdrop) };
					if (result.second == true) { it = result.first; }
				}
				else
				{
					std::swap(
						clonedBackdrop,
						it->second
					);
				}

				return it == m_accentBackdropMap.end() ? nullptr : it->second;
			}

			return nullptr;
		}
		void Remove(uDwmPrivates::CTopLevelWindow* target)
		{
			auto it{ m_backdropMap.find(target) };

			if (it != m_backdropMap.end())
			{
				m_backdropMap.erase(it);
			}
		}
		void RemoveAnimatedBackdrop(uDwmPrivates::CSecondaryWindowRepresentation* target)
		{
			auto it{ m_animatedBackdropMap.find(target) };

			if (it != m_animatedBackdropMap.end())
			{
				m_animatedBackdropMap.erase(it);
			}
		}
		void RemoveAnimatedAccentBackdrop(uDwmPrivates::CSecondaryWindowRepresentation* target)
		{
			auto it{ m_animatedAccentBackdropMap.find(target) };

			if (it != m_animatedAccentBackdropMap.end())
			{
				m_animatedAccentBackdropMap.erase(it);
			}
		}
		void RemoveAccent(uDwmPrivates::CTopLevelWindow* target)
		{
			auto it{ m_accentBackdropMap.find(target) };

			if (it != m_accentBackdropMap.end())
			{
				m_accentBackdropMap.erase(it);
			}
		}
		void Shutdown()
		{
			m_backdropMap.clear();
			m_accentBackdropMap.clear();
			m_animatedBackdropMap.clear();
		}
	private:
		std::unordered_map<uDwmPrivates::CTopLevelWindow*, winrt::com_ptr<CCompositedBackdrop>> m_backdropMap{};
		std::unordered_map<uDwmPrivates::CTopLevelWindow*, winrt::com_ptr<CCompositedAccentBackdrop>> m_accentBackdropMap{};
		std::unordered_map<uDwmPrivates::CSecondaryWindowRepresentation*, winrt::com_ptr<CCompositedBackdrop>> m_animatedBackdropMap{};
		std::unordered_map<uDwmPrivates::CSecondaryWindowRepresentation*, winrt::com_ptr<CCompositedAccentBackdrop>> m_animatedAccentBackdropMap{};
	};
}