#pragma once
#include "framework.h"
#include "cpprt.h"
#include "wil.h"

namespace AcrylicEverywhere::HookHelper
{
	template <typename T, typename Type>
	__forceinline constexpr T union_cast(Type pointer)
	{
		union
		{
			Type real_ptr;
			T fake_ptr;
		} rf{ .real_ptr{pointer} };

		return rf.fake_ptr;
	}

	bool IsBadReadPtr(const void* ptr);
	PVOID WritePointer(PVOID* memoryAddress, PVOID value);
	void WriteMemory(PVOID memoryAddress, const std::function<void()>&& callback);
	PVOID InjectCallbackToThread(DWORD threadId, const std::function<void()>& callback);
	HMODULE GetProcessModule(HANDLE processHandle, std::wstring_view dllPath);

	void WalkIAT(PVOID baseAddress, std::string_view dllName, std::function<bool(PVOID* functionAddress, LPCSTR functionNameOrOrdinal, BOOL importedByName)> callback);
	void WalkDelayloadIAT(PVOID baseAddress, std::string_view dllName, std::function<bool(HMODULE* moduleHandle, PVOID* functionAddress, LPCSTR functionNameOrOrdinal, BOOL importedByName)> callback);

	PVOID* GetIAT(PVOID baseAddress, std::string_view dllName, LPCSTR targetFunctionNameOrOrdinal);
	std::pair<HMODULE*, PVOID*> GetDelayloadIAT(PVOID baseAddress, std::string_view dllName, LPCSTR targetFunctionNameOrOrdinal, bool resolveAPI = false);
	PVOID WriteIAT(PVOID baseAddress, std::string_view dllName, LPCSTR targetFunctionNameOrOrdinal, PVOID detourFunction);
	std::pair<HMODULE, PVOID> WriteDelayloadIAT(PVOID baseAddress, std::string_view dllName, LPCSTR targetFunctionNameOrOrdinal, PVOID detourFunction, std::optional<HMODULE> newModuleHandle = std::nullopt);
	void ResolveDelayloadIAT(const std::pair<HMODULE*, PVOID*>& info, PVOID baseAddress, std::string_view dllName, LPCSTR targetFunctionNameOrOrdinal);

	PUCHAR MatchAsmCode(PUCHAR startAddress, std::pair<UCHAR*, size_t> asmCodeInfo, std::vector<std::pair<size_t, size_t>> asmSliceInfo, size_t maxSearchBytes);
	
	__forceinline PVOID* GetObjectVTable(void* This)
	{
		return reinterpret_cast<PVOID*>(*reinterpret_cast<PVOID*>(This));
	}
	std::pair<UCHAR*, size_t> GetTextSectionInfo(PVOID baseAddress);

	class ImageMapper
	{
		wil::unique_hfile m_fileHandle{ nullptr };
		wil::unique_handle m_fileMapping{ nullptr };
		wil::unique_mapview_ptr<void> m_baseAddress{ nullptr };
	public:
		ImageMapper(std::wstring_view dllPath);
		~ImageMapper() noexcept = default;
		ImageMapper(const ImageMapper&) = delete;
		ImageMapper& operator=(const ImageMapper&) = delete;

		template <typename T = HMODULE>
		__forceinline T GetBaseAddress() const { return reinterpret_cast<T>(m_baseAddress.get()); };
	};

	struct OffsetStorage
	{
		LONGLONG value{ 0 };

		inline bool IsValid() const { return value != 0; }
		template <typename T = PVOID, typename T2 = PVOID>
		inline T To(T2 baseAddress) const { if (baseAddress == 0 || !IsValid()) { return 0; } return reinterpret_cast<T>(RVA_TO_ADDR(baseAddress, value)); }
		template <typename T = PVOID>
		static inline auto From(T baseAddress, T targetAddress) { return OffsetStorage{ LONGLONG(targetAddress) - LONGLONG(baseAddress) }; }
		static inline auto From(PVOID baseAddress, PVOID targetAddress) { if (!baseAddress || !targetAddress) { return OffsetStorage{ 0 }; } return OffsetStorage{ reinterpret_cast<LONGLONG>(targetAddress) - reinterpret_cast<LONGLONG>(baseAddress) }; }
	};

	namespace Detours
	{
		// Call single or multiple Attach/Detach in the callback
		HRESULT Write(const std::function<void()>&& callback);
		// Install an inline hook using Detours
		void Attach(std::string_view dllName, std::string_view funcName, PVOID* realFuncAddr, PVOID hookFuncAddr) noexcept(false);
		void Attach(PVOID* realFuncAddr, PVOID hookFuncAddr) noexcept(false);
		// Uninstall an inline hook using Detours
		void Detach(PVOID* realFuncAddr, PVOID hookFuncAddr) noexcept(false);
	}
}