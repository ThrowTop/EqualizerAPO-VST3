#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "pluginterfaces/base/smartpointer.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/gui/iplugview.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/processdata.h"

class VST3PluginModule;

class VST3PluginInstance
{
public:
	VST3PluginInstance(std::shared_ptr<VST3PluginModule> module, std::wstring classId);
	~VST3PluginInstance();

	bool initialize(double sampleRate, unsigned maxFrameCount, unsigned channelCount,
		const std::wstring& processorState, const std::wstring& controllerState,
		std::wstring* error = nullptr);
	bool process(float** output, float** input, unsigned frameCount);
	void shutdown();

	unsigned getLatencySamples() const;
	bool isInitialized() const;
	Steinberg::Vst::IEditController* getController() const;
	Steinberg::IPlugView* createEditorView() const;
	std::wstring captureProcessorState() const;
	std::wstring captureControllerState() const;

private:
	bool restoreState(const std::wstring& processorState, const std::wstring& controllerState,
		std::wstring* error);
	bool connectController(std::wstring* error);
	static void setError(std::wstring* target, const wchar_t* message);

	std::shared_ptr<VST3PluginModule> module;
	std::wstring classId;
	Steinberg::IPtr<Steinberg::Vst::HostApplication> host;
	Steinberg::IPtr<Steinberg::Vst::IComponent> component;
	Steinberg::IPtr<Steinberg::Vst::IAudioProcessor> processor;
	Steinberg::IPtr<Steinberg::Vst::IEditController> controller;
	Steinberg::IPtr<Steinberg::Vst::IComponentHandler> componentHandler;
	Steinberg::IPtr<Steinberg::Vst::IConnectionPoint> componentConnection;
	Steinberg::IPtr<Steinberg::Vst::IConnectionPoint> controllerConnection;
	Steinberg::Vst::HostProcessData processData;
	unsigned maxFrameCount = 0;
	unsigned channelCount = 0;
	unsigned latencySamples = 0;
	bool componentInitialized = false;
	bool controllerInitialized = false;
	bool active = false;
	bool processing = false;
};
