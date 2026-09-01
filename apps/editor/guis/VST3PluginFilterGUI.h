#pragma once

#include <memory>
#include <string>

#include <QString>

#include "Editor/IFilterGUI.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class VST3PluginInstance;
class VST3PluginModule;

class VST3PluginFilterGUI : public IFilterGUI
{
	Q_OBJECT

public:
	VST3PluginFilterGUI(const std::wstring& bundlePath, const std::wstring& classId,
		const std::wstring& processorState, const std::wstring& controllerState);
	~VST3PluginFilterGUI() override;

	void store(QString& command, QString& parameters) override;

private:
	void selectBundle();
	void acceptEditedPath();
	void loadSelectedBundle(bool resetState);
	void selectClass(int index);
	bool initializePlugin();
	void openEditor();
	void captureState();
	void updateStatus(const QString& text, bool error = false);
	QString displayPath(const QString& absolutePath) const;
	QString resolvePath(const QString& path) const;
	QString selectedClassLabel() const;

	QLineEdit* pathLineEdit = nullptr;
	QComboBox* classCombo = nullptr;
	QLabel* statusLabel = nullptr;
	QLabel* latencyLabel = nullptr;
	QPushButton* openButton = nullptr;
	QString selectedPath;
	std::shared_ptr<VST3PluginModule> module;
	std::unique_ptr<VST3PluginInstance> instance;
	std::wstring classId;
	std::wstring processorState;
	std::wstring controllerState;
	bool populating = false;
};
