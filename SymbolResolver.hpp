#pragma once
#include "pch.h"

namespace AcrylicEverywhere
{
	class SymbolResolver
	{
	public:
		SymbolResolver();
		~SymbolResolver() noexcept;

		// Pass "*!*" to mask to search all.
		HRESULT Walk(std::wstring_view dllName, std::string_view mask, std::function<bool(PSYMBOL_INFO, ULONG)> callback);
		// Return true if symbol successfully loaded.
		bool IsLoaded();
		// Return true if symbol need to be downloaded.
		bool IsInternetRequired();
	private:
		static HMODULE WINAPI MyLoadLibraryExW(
			LPCWSTR lpLibFileName,
			HANDLE  hFile,
			DWORD   dwFlags
		);
		static BOOL CALLBACK EnumSymbolsCallback(
			PSYMBOL_INFO pSymInfo,
			ULONG SymbolSize,
			PVOID UserContext
		);
		static BOOL CALLBACK SymCallback(
			HANDLE hProcess,
			ULONG ActionCode,
			ULONG64 CallbackData,
			ULONG64 UserContext
		);

		PVOID m_LoadLibraryExW_Org{nullptr};
		FILE* m_fpstdout{ stdout };
		bool m_consoleAllocated{ false };
		bool m_symbolsOK{ false };
		bool m_requireInternet{ false };
	};
}