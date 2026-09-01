#include "stdafx.h"
#include "StableDeviceSelector.h"

#include <windows.h>
#include <objbase.h>

#include <charconv>
#include <limits>
#include <map>
#include <set>
#include <sstream>

namespace
{
	bool isUnreserved(unsigned char value)
	{
		return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z')
			|| (value >= '0' && value <= '9') || value == '-' || value == '.' || value == '_' || value == '~';
	}

	std::string toUtf8(const std::wstring& value)
	{
		if (value.empty())
			return {};
		int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
		if (length <= 0)
			return {};
		std::string result(static_cast<size_t>(length), '\0');
		if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), length, nullptr, nullptr) != length)
			return {};
		return result;
	}

	bool fromUtf8(const std::string& value, std::wstring& result)
	{
		if (value.empty())
		{
			result.clear();
			return true;
		}
		int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
		if (length <= 0)
			return false;
		result.resize(static_cast<size_t>(length));
		return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), length) == length;
	}

	std::wstring percentEncode(const std::wstring& value)
	{
		static const wchar_t hex[] = L"0123456789ABCDEF";
		std::wstring result;
		for (unsigned char byte : toUtf8(value))
		{
			if (isUnreserved(byte))
				result.push_back(static_cast<wchar_t>(byte));
			else
			{
				result.push_back(L'%');
				result.push_back(hex[byte >> 4]);
				result.push_back(hex[byte & 0x0f]);
			}
		}
		return result;
	}

	int hexValue(wchar_t value)
	{
		if (value >= L'0' && value <= L'9')
			return value - L'0';
		if (value >= L'a' && value <= L'f')
			return value - L'a' + 10;
		if (value >= L'A' && value <= L'F')
			return value - L'A' + 10;
		return -1;
	}

	bool percentDecode(const std::wstring& value, std::wstring& result)
	{
		std::string bytes;
		bytes.reserve(value.size());
		for (size_t i = 0; i < value.size(); ++i)
		{
			wchar_t character = value[i];
			if (character == L'%')
			{
				if (i + 2 >= value.size())
					return false;
				int high = hexValue(value[i + 1]);
				int low = hexValue(value[i + 2]);
				if (high < 0 || low < 0)
					return false;
				bytes.push_back(static_cast<char>((high << 4) | low));
				i += 2;
			}
			else
			{
				if (character > 0x7f)
					return false;
				bytes.push_back(static_cast<char>(character));
			}
		}
		return fromUtf8(bytes, result) && result.size() <= StableDeviceSelectorCodec::MaxDecodedValueLength;
	}

	template<typename T>
	bool parseUnsigned(const std::wstring& value, T& result)
	{
		if (value.empty())
			return false;
		std::string ascii;
		ascii.reserve(value.size());
		for (wchar_t character : value)
		{
			if (character < L'0' || character > L'9')
				return false;
			ascii.push_back(static_cast<char>(character));
		}
		unsigned long long parsed = 0;
		auto conversion = std::from_chars(ascii.data(), ascii.data() + ascii.size(), parsed, 10);
		if (conversion.ec != std::errc() || conversion.ptr != ascii.data() + ascii.size() || parsed > (std::numeric_limits<T>::max)())
			return false;
		result = static_cast<T>(parsed);
		return true;
	}

	StableSelectorParseResult failure(StableSelectorParseStatus status, const wchar_t* diagnostic)
	{
		StableSelectorParseResult result;
		result.status = status;
		result.diagnostic = diagnostic;
		return result;
	}
}

bool StableDeviceSelectorCodec::isStable(const std::wstring& text)
{
	return text.rfind(L"stable:v", 0) == 0;
}

StableSelectorParseResult StableDeviceSelectorCodec::parse(const std::wstring& text)
{
	if (!isStable(text))
		return failure(StableSelectorParseStatus::NotStable, L"Not a stable selector");
	if (text.size() > MaxSelectorLength)
		return failure(StableSelectorParseStatus::Invalid, L"Selector exceeds the length limit");

	size_t firstSeparator = text.find(L'|');
	std::wstring prefix = text.substr(0, firstSeparator);
	if (prefix != L"stable:v1")
		return failure(StableSelectorParseStatus::UnsupportedVersion, L"Unsupported stable selector version");
	if (firstSeparator == std::wstring::npos)
		return failure(StableSelectorParseStatus::Invalid, L"Stable selector has no fields");

	std::map<std::wstring, std::wstring> fields;
	std::vector<std::wstring> order;
	for (size_t position = firstSeparator + 1; position <= text.size();)
	{
		size_t next = text.find(L'|', position);
		std::wstring field = text.substr(position, next == std::wstring::npos ? std::wstring::npos : next - position);
		if (field.empty())
			return failure(StableSelectorParseStatus::Invalid, L"Stable selector contains an empty field");
		size_t equals = field.find(L'=');
		if (equals == std::wstring::npos || equals == 0 || field.find(L'=', equals + 1) != std::wstring::npos)
			return failure(StableSelectorParseStatus::Invalid, L"Stable selector contains a malformed field");
		std::wstring key = field.substr(0, equals);
		for (wchar_t character : key)
		{
			if (character < L'a' || character > L'z')
				return failure(StableSelectorParseStatus::Invalid, L"Stable selector contains an invalid key");
		}
		if (fields.find(key) != fields.end())
			return failure(StableSelectorParseStatus::Invalid, L"Stable selector contains a duplicate field");
		fields.emplace(key, field.substr(equals + 1));
		order.push_back(key);
		if (next == std::wstring::npos)
			break;
		position = next + 1;
	}

	const std::set<std::wstring> required = {L"binding", L"endpoint", L"flow", L"connection", L"device", L"fallback"};
	for (const auto& key : required)
	{
		if (fields.find(key) == fields.end())
			return failure(StableSelectorParseStatus::Invalid, L"Stable selector is missing a required field");
	}

	StableSelectorParseResult result;
	result.status = StableSelectorParseStatus::Success;
	result.selector.version = 1;

	auto decode = [&](const wchar_t* key, std::wstring& destination) {
		return percentDecode(fields.at(key), destination);
	};
	if (!decode(L"binding", result.selector.bindingId) || !isCanonicalGuid(result.selector.bindingId))
		return failure(StableSelectorParseStatus::Invalid, L"Stable selector has an invalid binding UUID");
	if (!decode(L"endpoint", result.selector.snapshot.endpointGuid) || !isCanonicalGuid(result.selector.snapshot.endpointGuid))
		return failure(StableSelectorParseStatus::Invalid, L"Stable selector has an invalid endpoint GUID");
	std::wstring flow;
	if (!decode(L"flow", flow) || (result.selector.snapshot.flow = audioDataFlowFromString(flow)) == AudioDataFlow::Unknown)
		return failure(StableSelectorParseStatus::Invalid, L"Stable selector has an invalid flow");
	if (!decode(L"connection", result.selector.snapshot.connectionName) || !decode(L"device", result.selector.snapshot.deviceName))
		return failure(StableSelectorParseStatus::Invalid, L"Stable selector contains malformed UTF-8 text");
	std::wstring fallback;
	if (!decode(L"fallback", fallback) || (fallback != L"name" && fallback != L"none"))
		return failure(StableSelectorParseStatus::Invalid, L"Stable selector has an invalid fallback mode");
	result.selector.allowNameFallback = fallback == L"name";

	auto optionalGuid = [&](const wchar_t* key, std::wstring& destination) {
		auto found = fields.find(key);
		if (found == fields.end())
			return true;
		return percentDecode(found->second, destination) && isCanonicalGuid(destination);
	};
	if (!optionalGuid(L"container", result.selector.snapshot.containerId))
		return failure(StableSelectorParseStatus::Invalid, L"Stable selector has an invalid container GUID");
	if (!optionalGuid(L"association", result.selector.snapshot.association))
		return failure(StableSelectorParseStatus::Invalid, L"Stable selector has an invalid association GUID");

	auto form = fields.find(L"form");
	if (form != fields.end())
	{
		unsigned value = 0;
		if (!parseUnsigned(form->second, value))
			return failure(StableSelectorParseStatus::Invalid, L"Stable selector has an invalid form factor");
		result.selector.snapshot.formFactor = value;
	}
	if (auto channels = fields.find(L"channels"); channels != fields.end() && !parseUnsigned(channels->second, result.selector.snapshot.channelCount))
		return failure(StableSelectorParseStatus::Invalid, L"Stable selector has an invalid channel count");
	if (auto mask = fields.find(L"mask"); mask != fields.end() && !parseUnsigned(mask->second, result.selector.snapshot.channelMask))
		return failure(StableSelectorParseStatus::Invalid, L"Stable selector has an invalid channel mask");

	const std::set<std::wstring> known = {L"binding", L"endpoint", L"container", L"flow", L"form", L"association", L"connection", L"device", L"channels", L"mask", L"fallback"};
	for (const auto& key : order)
	{
		if (known.find(key) == known.end())
		{
			std::wstring decoded;
			if (!percentDecode(fields.at(key), decoded))
				return failure(StableSelectorParseStatus::Invalid, L"Stable selector contains an invalid unknown field");
			result.selector.unknownFields.emplace_back(key, decoded);
		}
	}
	return result;
}

std::wstring StableDeviceSelectorCodec::serialize(const StableDeviceSelector& selector)
{
	std::wostringstream stream;
	stream << L"stable:v1"
		<< L"|binding=" << percentEncode(selector.bindingId)
		<< L"|endpoint=" << percentEncode(selector.snapshot.endpointGuid);
	if (!selector.snapshot.containerId.empty())
		stream << L"|container=" << percentEncode(selector.snapshot.containerId);
	stream << L"|flow=" << audioDataFlowToString(selector.snapshot.flow);
	if (selector.snapshot.formFactor)
		stream << L"|form=" << *selector.snapshot.formFactor;
	if (!selector.snapshot.association.empty())
		stream << L"|association=" << percentEncode(selector.snapshot.association);
	stream << L"|connection=" << percentEncode(selector.snapshot.connectionName)
		<< L"|device=" << percentEncode(selector.snapshot.deviceName);
	if (selector.snapshot.channelCount != 0)
		stream << L"|channels=" << selector.snapshot.channelCount;
	if (selector.snapshot.channelMask != 0)
		stream << L"|mask=" << selector.snapshot.channelMask;
	stream << L"|fallback=" << (selector.allowNameFallback ? L"name" : L"none");
	for (const auto& field : selector.unknownFields)
		stream << L'|' << field.first << L'=' << percentEncode(field.second);
	return stream.str();
}

std::wstring StableDeviceSelectorCodec::createBindingId()
{
	GUID guid = {};
	if (FAILED(CoCreateGuid(&guid)))
		return {};
	wchar_t buffer[39] = {};
	if (StringFromGUID2(guid, buffer, static_cast<int>(std::size(buffer))) != 39)
		return {};
	return buffer;
}
