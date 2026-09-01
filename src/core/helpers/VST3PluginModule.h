#pragma once

#include <memory>
#include <string>
#include <vector>

namespace VST3 { namespace Hosting { class Module; } }

struct VST3PluginClassInfo
{
	std::wstring classId;
	std::wstring name;
	std::wstring vendor;
	std::wstring version;
};

class VST3PluginModule
{
public:
	static std::shared_ptr<VST3PluginModule> load(const std::wstring& bundlePath, std::wstring* error = nullptr);
	static std::wstring getDefaultPluginPath();
	static std::vector<std::wstring> getInstalledModulePaths();

	const std::wstring& getBundlePath() const;
	const std::vector<VST3PluginClassInfo>& getClasses() const;
	std::shared_ptr<VST3::Hosting::Module> getModule() const;

private:
	VST3PluginModule(std::wstring bundlePath, std::shared_ptr<VST3::Hosting::Module> module);

	std::wstring bundlePath;
	std::shared_ptr<VST3::Hosting::Module> module;
	std::vector<VST3PluginClassInfo> classes;
};
