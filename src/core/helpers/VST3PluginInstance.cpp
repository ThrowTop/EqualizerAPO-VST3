#include "stdafx.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <vector>

#include <wincrypt.h>

#include "StringHelper.h"
#include "VST3PluginInstance.h"
#include "VST3PluginModule.h"

#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include "pluginterfaces/base/funknownimpl.h"
#include "pluginterfaces/base/ibstream.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/utility/uid.h"

using namespace std;
using namespace Steinberg;
using namespace Steinberg::Vst;

namespace
{
	class ComponentHandler final : public IComponentHandler
	{
	public:
		explicit ComponentHandler(IEditController* controller) : controller(controller) {}

		tresult PLUGIN_API queryInterface(const TUID interfaceId, void** obj) override
		{
			if (obj == nullptr)
				return kInvalidArgument;
			if (memcmp(interfaceId, FUnknown::iid, sizeof(TUID)) == 0 ||
				memcmp(interfaceId, IComponentHandler::iid, sizeof(TUID)) == 0)
			{
				*obj = static_cast<IComponentHandler*>(this);
				addRef();
				return kResultTrue;
			}
			*obj = nullptr;
			return kNoInterface;
		}

		uint32 PLUGIN_API addRef() override { return ++references; }
		uint32 PLUGIN_API release() override
		{
			uint32 result = --references;
			if (result == 0)
				delete this;
			return result;
		}

		tresult PLUGIN_API beginEdit(ParamID) override { return kResultTrue; }
		tresult PLUGIN_API performEdit(ParamID id, ParamValue value) override
		{
			return controller && controller->setParamNormalized(id, value) == kResultTrue
				? kResultTrue : kResultFalse;
		}
		tresult PLUGIN_API endEdit(ParamID) override { return kResultTrue; }
		tresult PLUGIN_API restartComponent(int32) override { return kResultTrue; }

	private:
		atomic<uint32> references {1};
		IEditController* controller;
	};

	class VectorStream final : public IBStream
	{
	public:
		VectorStream() = default;
		explicit VectorStream(vector<uint8_t> bytes) : bytes(move(bytes)) {}

		tresult PLUGIN_API queryInterface(const TUID interfaceId, void** obj) override
		{
			if (obj == nullptr)
				return kInvalidArgument;
			if (memcmp(interfaceId, FUnknown::iid, sizeof(TUID)) == 0 ||
				memcmp(interfaceId, IBStream::iid, sizeof(TUID)) == 0)
			{
				*obj = static_cast<IBStream*>(this);
				addRef();
				return kResultTrue;
			}
			*obj = nullptr;
			return kNoInterface;
		}

		uint32 PLUGIN_API addRef() override { return ++refCount; }
		uint32 PLUGIN_API release() override
		{
			uint32 result = --refCount;
			if (result == 0)
				delete this;
			return result;
		}

		tresult PLUGIN_API read(void* buffer, int32 numBytes, int32* numBytesRead) override
		{
			if (buffer == nullptr || numBytes < 0)
				return kInvalidArgument;
			size_t count = min(static_cast<size_t>(numBytes), bytes.size() - min(position, bytes.size()));
			if (count != 0)
				memcpy(buffer, bytes.data() + position, count);
			position += count;
			if (numBytesRead != nullptr)
				*numBytesRead = static_cast<int32>(count);
			return kResultTrue;
		}

		tresult PLUGIN_API write(void* buffer, int32 numBytes, int32* numBytesWritten) override
		{
			if (buffer == nullptr || numBytes < 0)
				return kInvalidArgument;
			size_t end = position + static_cast<size_t>(numBytes);
			if (end > bytes.size())
				bytes.resize(end);
			if (numBytes != 0)
				memcpy(bytes.data() + position, buffer, static_cast<size_t>(numBytes));
			position = end;
			if (numBytesWritten != nullptr)
				*numBytesWritten = numBytes;
			return kResultTrue;
		}

		tresult PLUGIN_API seek(int64 pos, int32 mode, int64* result) override
		{
			int64 base = 0;
			if (mode == kIBSeekCur)
				base = static_cast<int64>(position);
			else if (mode == kIBSeekEnd)
				base = static_cast<int64>(bytes.size());
			else if (mode != kIBSeekSet)
				return kInvalidArgument;
			int64 next = base + pos;
			if (next < 0)
				return kInvalidArgument;
			position = static_cast<size_t>(next);
			if (result != nullptr)
				*result = next;
			return kResultTrue;
		}

		tresult PLUGIN_API tell(int64* pos) override
		{
			if (pos == nullptr)
				return kInvalidArgument;
			*pos = static_cast<int64>(position);
			return kResultTrue;
		}

		const vector<uint8_t>& data() const { return bytes; }

	private:
		atomic<uint32> refCount {1};
		vector<uint8_t> bytes;
		size_t position = 0;
	};

	vector<uint8_t> decodeBase64(const wstring& text)
	{
		if (text.empty())
			return {};
		DWORD size = 0;
		if (!CryptStringToBinaryW(text.c_str(), 0, CRYPT_STRING_BASE64, nullptr, &size, nullptr, nullptr))
			return {};
		vector<uint8_t> result(size);
		if (!CryptStringToBinaryW(text.c_str(), 0, CRYPT_STRING_BASE64, result.data(), &size, nullptr, nullptr))
			return {};
		result.resize(size);
		return result;
	}

	wstring encodeBase64(const vector<uint8_t>& bytes)
	{
		if (bytes.empty())
			return L"";
		DWORD length = 0;
		CryptBinaryToStringW(bytes.data(), static_cast<DWORD>(bytes.size()),
			CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &length);
		wstring result(length, L'\0');
		if (!CryptBinaryToStringW(bytes.data(), static_cast<DWORD>(bytes.size()),
			CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, result.data(), &length))
			return L"";
		if (!result.empty() && result.back() == L'\0')
			result.pop_back();
		return result;
	}

	bool validClassId(const string& value)
	{
		return value.size() == 32 && all_of(value.begin(), value.end(), [](unsigned char c) { return isxdigit(c) != 0; });
	}
}

VST3PluginInstance::VST3PluginInstance(shared_ptr<VST3PluginModule> module, wstring classId)
	: module(move(module)), classId(move(classId))
{
}

VST3PluginInstance::~VST3PluginInstance()
{
	shutdown();
}

bool VST3PluginInstance::initialize(double sampleRate, unsigned maxFrames, unsigned channels,
	const wstring& processorState, const wstring& controllerState, wstring* error)
{
	shutdown();
	if (!module || (channels != 1 && channels != 2) || maxFrames == 0)
	{
		setError(error, L"VST3 supports exact mono or stereo processing only.");
		return false;
	}

	string idText = StringHelper::toString(classId, CP_UTF8);
	if (!validClassId(idText))
	{
		setError(error, L"The VST3 class ID is invalid.");
		return false;
	}
	auto uid = VST3::UID::fromString(idText);
	if (!uid)
	{
		setError(error, L"The VST3 class ID could not be decoded.");
		return false;
	}

	host = owned(new HostApplication());
	auto factory = module->getModule()->getFactory();
	factory.setHostContext(host);
	component = factory.createInstance<IComponent>(*uid);
	if (!component)
	{
		setError(error, L"The selected VST3 audio component was not found.");
		return false;
	}
	if (component->initialize(host) != kResultOk)
	{
		setError(error, L"The VST3 component failed to initialize.");
		return false;
	}
	componentInitialized = true;
	processor = U::cast<IAudioProcessor>(component);
	if (!processor || processor->canProcessSampleSize(kSample32) != kResultTrue)
	{
		setError(error, L"The VST3 plugin does not support 32-bit floating-point audio.");
		return false;
	}

	if (!connectController(error) || !restoreState(processorState, controllerState, error))
		return false;

	int32 inputBusses = component->getBusCount(kAudio, kInput);
	int32 outputBusses = component->getBusCount(kAudio, kOutput);
	if (inputBusses < 1 || outputBusses < 1)
	{
		setError(error, L"The VST3 plugin is not an insert audio effect.");
		return false;
	}

	SpeakerArrangement mainArrangement = channels == 1 ? SpeakerArr::kMono : SpeakerArr::kStereo;
	vector<SpeakerArrangement> inputArrangements(static_cast<size_t>(inputBusses), SpeakerArr::kEmpty);
	vector<SpeakerArrangement> outputArrangements(static_cast<size_t>(outputBusses), SpeakerArr::kEmpty);
	inputArrangements[0] = mainArrangement;
	outputArrangements[0] = mainArrangement;
	if (processor->setBusArrangements(inputArrangements.data(), inputBusses,
		outputArrangements.data(), outputBusses) != kResultTrue)
	{
		setError(error, L"The VST3 plugin rejected the endpoint's mono/stereo bus layout.");
		return false;
	}

	for (int32 i = 0; i < inputBusses; ++i)
		component->activateBus(kAudio, kInput, i, i == 0 ? 1 : 0);
	for (int32 i = 0; i < outputBusses; ++i)
		component->activateBus(kAudio, kOutput, i, i == 0 ? 1 : 0);

	ProcessSetup setup {kRealtime, kSample32, static_cast<int32>(maxFrames), sampleRate};
	if (processor->setupProcessing(setup) != kResultOk)
	{
		setError(error, L"The VST3 plugin rejected the processing setup.");
		return false;
	}
	if (!processData.prepare(*component, 0, kSample32))
	{
		setError(error, L"The VST3 processing buffers could not be prepared.");
		return false;
	}
	processData.processMode = kRealtime;
	processData.symbolicSampleSize = kSample32;
	processData.inputParameterChanges = nullptr;
	processData.outputParameterChanges = nullptr;
	processData.inputEvents = nullptr;
	processData.outputEvents = nullptr;
	processData.processContext = nullptr;

	if (component->setActive(true) != kResultOk)
	{
		setError(error, L"The VST3 plugin could not be activated.");
		return false;
	}
	active = true;
	tresult processingResult = processor->setProcessing(true);
	if (processingResult != kResultOk && processingResult != kNotImplemented)
	{
		setError(error, L"The VST3 plugin could not start processing.");
		return false;
	}
	processing = true;
	maxFrameCount = maxFrames;
	channelCount = channels;
	latencySamples = processor->getLatencySamples();
	return true;
}

bool VST3PluginInstance::process(float** output, float** input, unsigned frameCount)
{
	if (!processing || frameCount > maxFrameCount)
		return false;
	if (!processData.setChannelBuffers(kInput, 0, input, static_cast<int32>(channelCount)) ||
		!processData.setChannelBuffers(kOutput, 0, output, static_cast<int32>(channelCount)))
		return false;
	processData.numSamples = static_cast<int32>(frameCount);
	processData.inputs[0].silenceFlags = 0;
	processData.outputs[0].silenceFlags = 0;
	return processor->process(processData) == kResultOk;
}

void VST3PluginInstance::shutdown()
{
	if (processing && processor)
		processor->setProcessing(false);
	processing = false;
	if (active && component)
		component->setActive(false);
	active = false;
	processData.unprepare();

	if (componentConnection && controllerConnection)
	{
		componentConnection->disconnect(controllerConnection);
		controllerConnection->disconnect(componentConnection);
	}
	componentConnection = nullptr;
	controllerConnection = nullptr;
	if (controller)
		controller->setComponentHandler(nullptr);
	componentHandler = nullptr;

	if (controllerInitialized && controller)
	{
		IPtr<IPluginBase> pluginBase = U::cast<IPluginBase>(controller);
		if (pluginBase)
			pluginBase->terminate();
	}
	controllerInitialized = false;
	controller = nullptr;
	processor = nullptr;
	if (componentInitialized && component)
		component->terminate();
	componentInitialized = false;
	component = nullptr;
	host = nullptr;
	latencySamples = 0;
	maxFrameCount = 0;
	channelCount = 0;
}

unsigned VST3PluginInstance::getLatencySamples() const { return latencySamples; }
bool VST3PluginInstance::isInitialized() const { return processing; }
IEditController* VST3PluginInstance::getController() const { return controller; }

IPlugView* VST3PluginInstance::createEditorView() const
{
	return controller ? controller->createView(ViewType::kEditor) : nullptr;
}

wstring VST3PluginInstance::captureProcessorState() const
{
	if (!component)
		return L"";
	IPtr<VectorStream> stream = owned(new VectorStream());
	return component->getState(stream.get()) == kResultTrue ? encodeBase64(stream->data()) : L"";
}

wstring VST3PluginInstance::captureControllerState() const
{
	if (!controller)
		return L"";
	IPtr<VectorStream> stream = owned(new VectorStream());
	return controller->getState(stream.get()) == kResultTrue ? encodeBase64(stream->data()) : L"";
}

bool VST3PluginInstance::restoreState(const wstring& processorState, const wstring& controllerState, wstring* error)
{
	if (!processorState.empty())
	{
		vector<uint8_t> bytes = decodeBase64(processorState);
		if (bytes.empty())
		{
			setError(error, L"The VST3 processor state is not valid base64 data.");
			return false;
		}
		IPtr<VectorStream> componentStream = owned(new VectorStream(bytes));
		if (component->setState(componentStream.get()) != kResultTrue)
		{
			setError(error, L"The VST3 plugin rejected its processor state.");
			return false;
		}
		if (controller)
		{
			componentStream->seek(0, IBStream::kIBSeekSet, nullptr);
			controller->setComponentState(componentStream.get());
		}
	}
	if (controller && !controllerState.empty())
	{
		vector<uint8_t> bytes = decodeBase64(controllerState);
		if (bytes.empty())
		{
			setError(error, L"The VST3 controller state is not valid base64 data.");
			return false;
		}
		IPtr<VectorStream> controllerStream = owned(new VectorStream(move(bytes)));
		if (controller->setState(controllerStream.get()) != kResultTrue)
		{
			setError(error, L"The VST3 plugin rejected its controller state.");
			return false;
		}
	}
	return true;
}

bool VST3PluginInstance::connectController(wstring* error)
{
	IEditController* sharedController = nullptr;
	if (component->queryInterface(IEditController::iid, reinterpret_cast<void**>(&sharedController)) == kResultTrue)
	{
		controller = owned(sharedController);
		componentHandler = owned(new ComponentHandler(controller));
		if (controller->setComponentHandler(componentHandler) != kResultTrue)
		{
			setError(error, L"The VST3 controller rejected the host component handler.");
			return false;
		}
		return true;
	}

	TUID controllerId {};
	if (component->getControllerClassId(controllerId) != kResultTrue)
		return true;
	controller = module->getModule()->getFactory().createInstance<IEditController>(VST3::UID(controllerId));
	if (!controller)
	{
		setError(error, L"The VST3 controller could not be created.");
		return false;
	}
	IPtr<IPluginBase> pluginBase = U::cast<IPluginBase>(controller);
	if (!pluginBase || pluginBase->initialize(host) != kResultOk)
	{
		setError(error, L"The VST3 controller failed to initialize.");
		return false;
	}
	controllerInitialized = true;
	componentConnection = U::cast<Steinberg::Vst::IConnectionPoint>(component);
	controllerConnection = U::cast<Steinberg::Vst::IConnectionPoint>(controller);
	if (componentConnection && controllerConnection)
	{
		if (componentConnection->connect(controllerConnection) != kResultTrue ||
			controllerConnection->connect(componentConnection) != kResultTrue)
		{
			setError(error, L"The VST3 component and controller could not be connected.");
			return false;
		}
	}
	componentHandler = owned(new ComponentHandler(controller));
	if (controller->setComponentHandler(componentHandler) != kResultTrue)
	{
		setError(error, L"The VST3 controller rejected the host component handler.");
		return false;
	}
	return true;
}

void VST3PluginInstance::setError(wstring* target, const wchar_t* message)
{
	if (target != nullptr)
		*target = message;
}
