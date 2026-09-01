#include "stdafx.h"
#include "EndpointIdentity.h"

#include <cwctype>

std::wstring audioDataFlowToString(AudioDataFlow flow)
{
	switch (flow)
	{
	case AudioDataFlow::Render:
		return L"render";
	case AudioDataFlow::Capture:
		return L"capture";
	default:
		return L"unknown";
	}
}

AudioDataFlow audioDataFlowFromString(const std::wstring& value)
{
	if (value == L"render")
		return AudioDataFlow::Render;
	if (value == L"capture")
		return AudioDataFlow::Capture;
	return AudioDataFlow::Unknown;
}

bool equalGuid(const std::wstring& left, const std::wstring& right)
{
	if (left.size() != right.size())
		return false;
	for (size_t i = 0; i < left.size(); ++i)
	{
		if (towlower(left[i]) != towlower(right[i]))
			return false;
	}
	return true;
}

bool isCanonicalGuid(const std::wstring& value)
{
	if (value.size() != 38 || value.front() != L'{' || value.back() != L'}')
		return false;
	for (size_t i = 1; i + 1 < value.size(); ++i)
	{
		if (i == 9 || i == 14 || i == 19 || i == 24)
		{
			if (value[i] != L'-')
				return false;
		}
		else if (!iswxdigit(value[i]))
		{
			return false;
		}
	}
	return true;
}
