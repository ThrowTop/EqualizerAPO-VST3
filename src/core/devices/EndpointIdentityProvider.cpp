#include "stdafx.h"
#include "EndpointIdentityProvider.h"

#include <windows.h>
#include <initguid.h>
#include <audioclient.h>
#include <propkeydef.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <propvarutil.h>

#include <algorithm>

namespace
{
	// This is the endpoint device-name property already consumed from the
	// MMDevices property cache by DeviceAPOInfo. Windows exposes it through the
	// endpoint property store but does not publish a symbolic SDK constant.
	const PROPERTYKEY EapoEndpointDeviceName = {
		{0xb3f8fa53, 0x0004, 0x438e, {0x90, 0x03, 0x51, 0xa4, 0x6e, 0x13, 0x9b, 0xfc}}, 6
	};

	std::wstring guidString(const GUID& guid)
	{
		wchar_t buffer[39] = {};
		return StringFromGUID2(guid, buffer, static_cast<int>(std::size(buffer))) == 39 ? buffer : L"";
	}

	std::wstring readString(IPropertyStore* store, const PROPERTYKEY& key)
	{
		PROPVARIANT value;
		PropVariantInit(&value);
		std::wstring result;
		if (SUCCEEDED(store->GetValue(key, &value)))
		{
			if (value.vt == VT_LPWSTR && value.pwszVal != nullptr)
				result = value.pwszVal;
			else if (value.vt == VT_BSTR && value.bstrVal != nullptr)
				result = value.bstrVal;
		}
		PropVariantClear(&value);
		return result;
	}

	std::wstring readGuid(IPropertyStore* store, const PROPERTYKEY& key)
	{
		PROPVARIANT value;
		PropVariantInit(&value);
		std::wstring result;
		if (SUCCEEDED(store->GetValue(key, &value)))
		{
			if (value.vt == VT_CLSID && value.puuid != nullptr)
				result = guidString(*value.puuid);
			else if (value.vt == VT_LPWSTR && value.pwszVal != nullptr && isCanonicalGuid(value.pwszVal))
				result = value.pwszVal;
		}
		PropVariantClear(&value);
		return result;
	}

	std::optional<unsigned> readUnsigned(IPropertyStore* store, const PROPERTYKEY& key)
	{
		PROPVARIANT value;
		PropVariantInit(&value);
		std::optional<unsigned> result;
		if (SUCCEEDED(store->GetValue(key, &value)))
		{
			if (value.vt == VT_UI4)
				result = value.ulVal;
			else if (value.vt == VT_I4 && value.lVal >= 0)
				result = static_cast<unsigned>(value.lVal);
		}
		PropVariantClear(&value);
		return result;
	}

	void readFormat(IPropertyStore* store, EndpointIdentity& identity)
	{
		PROPVARIANT value;
		PropVariantInit(&value);
		if (SUCCEEDED(store->GetValue(PKEY_AudioEngine_DeviceFormat, &value)) && value.vt == VT_BLOB
			&& value.blob.pBlobData != nullptr && value.blob.cbSize >= sizeof(WAVEFORMATEX))
		{
			const WAVEFORMATEX* format = reinterpret_cast<const WAVEFORMATEX*>(value.blob.pBlobData);
			identity.channelCount = format->nChannels;
			if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE && value.blob.cbSize >= sizeof(WAVEFORMATEXTENSIBLE))
				identity.channelMask = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format)->dwChannelMask;
		}
		PropVariantClear(&value);
		if (identity.channelMask == 0)
		{
			auto mask = readUnsigned(store, PKEY_AudioEndpoint_PhysicalSpeakers);
			if (mask)
				identity.channelMask = *mask;
		}
	}

	void enumerateFlow(IMMDeviceEnumerator* enumerator, EDataFlow flow, std::vector<EndpointIdentity>& result)
	{
		IMMDeviceCollection* collection = nullptr;
		const DWORD states = DEVICE_STATE_ACTIVE | DEVICE_STATE_DISABLED | DEVICE_STATE_UNPLUGGED;
		if (FAILED(enumerator->EnumAudioEndpoints(flow, states, &collection)) || collection == nullptr)
			return;
		UINT count = 0;
		if (SUCCEEDED(collection->GetCount(&count)))
		{
			for (UINT index = 0; index < count; ++index)
			{
				IMMDevice* device = nullptr;
				if (FAILED(collection->Item(index, &device)) || device == nullptr)
					continue;
				EndpointIdentity identity;
				identity.flow = flow == eCapture ? AudioDataFlow::Capture : AudioDataFlow::Render;
				LPWSTR id = nullptr;
				if (SUCCEEDED(device->GetId(&id)) && id != nullptr)
				{
					identity.mmDeviceId = id;
					CoTaskMemFree(id);
				}
				IPropertyStore* store = nullptr;
				if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store)) && store != nullptr)
				{
					identity.endpointGuid = readString(store, PKEY_AudioEndpoint_GUID);
					identity.containerId = readGuid(store, PKEY_Device_ContainerId);
					identity.formFactor = readUnsigned(store, PKEY_AudioEndpoint_FormFactor);
					identity.association = readGuid(store, PKEY_AudioEndpoint_Association);
					identity.connectionName = readString(store, PKEY_Device_DeviceDesc);
					identity.deviceName = readString(store, EapoEndpointDeviceName);
					readFormat(store, identity);
					store->Release();
				}
				device->Release();
				if (!identity.endpointGuid.empty())
					result.push_back(std::move(identity));
			}
		}
		collection->Release();
	}
}

std::vector<EndpointIdentity> EndpointIdentityProvider::enumerate()
{
	std::vector<EndpointIdentity> result;
	HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	IMMDeviceEnumerator* enumerator = nullptr;
	if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
		__uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator))) && enumerator != nullptr)
	{
		enumerateFlow(enumerator, eRender, result);
		enumerateFlow(enumerator, eCapture, result);
		enumerator->Release();
	}
	if (initialized == S_OK || initialized == S_FALSE)
		CoUninitialize();
	std::sort(result.begin(), result.end(), [](const EndpointIdentity& left, const EndpointIdentity& right) {
		if (left.flow != right.flow)
			return left.flow < right.flow;
		return _wcsicmp(left.endpointGuid.c_str(), right.endpointGuid.c_str()) < 0;
	});
	return result;
}

bool EndpointIdentityProvider::findByEndpointGuid(const std::vector<EndpointIdentity>& catalog, const std::wstring& endpointGuid, EndpointIdentity& identity)
{
	for (const auto& candidate : catalog)
	{
		if (equalGuid(candidate.endpointGuid, endpointGuid))
		{
			identity = candidate;
			return true;
		}
	}
	return false;
}
