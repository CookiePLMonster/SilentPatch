#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "ParseUtils.hpp"

#include <windows.h>

static std::string WcharToUTF8(std::wstring_view str)
{
	std::string result;

	const int count = WideCharToMultiByte(CP_UTF8, 0, str.data(), str.size(), nullptr, 0, nullptr, nullptr);
	if (count != 0)
	{
		result.resize(count);
		WideCharToMultiByte(CP_UTF8, 0, str.data(), str.size(), result.data(), count, nullptr, nullptr);
	}

	return result;
}

std::optional<int32_t> ParseUtils::TryParseInt(const wchar_t* str)
{
	std::optional<int32_t> result;

	wchar_t* end;
	const int32_t val = wcstol(str, &end, 0);
	if (*end == '\0')
	{
		result.emplace(val);
	}
	return result;
}

std::string ParseUtils::ParseString(const wchar_t* str)
{
	return WcharToUTF8(str);
}
