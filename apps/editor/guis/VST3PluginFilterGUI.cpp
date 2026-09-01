#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>
#include <QWidget>

#include <atomic>
#include <algorithm>
#include <cstring>
#include <functional>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "helpers/RegistryHelper.h"
#include "helpers/VST3PluginInstance.h"
#include "helpers/VST3PluginModule.h"
#include "widgets/ResizingLineEdit.h"
#include "VST3PluginFilterGUI.h"

#include "pluginterfaces/gui/iplugview.h"

using namespace std;
using namespace Steinberg;

namespace
{
	QString quote(const QString& value)
	{
		QString escaped = value;
		escaped.replace(QStringLiteral("\""), QStringLiteral("\"\""));
		return QStringLiteral("\"") + escaped + QStringLiteral("\"");
	}

	class PluginViewFrame final : public IPlugFrame
	{
	public:
		explicit PluginViewFrame(QWidget* container) : container(container) {}

		tresult PLUGIN_API queryInterface(const TUID interfaceId, void** obj) override
		{
			if (obj == nullptr)
				return kInvalidArgument;
			if (memcmp(interfaceId, FUnknown::iid, sizeof(TUID)) == 0 ||
				memcmp(interfaceId, IPlugFrame::iid, sizeof(TUID)) == 0)
			{
				*obj = static_cast<IPlugFrame*>(this);
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

		tresult PLUGIN_API resizeView(IPlugView* view, ViewRect* newSize) override
		{
			if (view == nullptr || newSize == nullptr || container.isNull())
				return kInvalidArgument;
			if (view->onSize(newSize) != kResultTrue)
				return kResultFalse;
			container->setFixedSize(max(1, newSize->getWidth()), max(1, newSize->getHeight()));
			if (QWidget* window = container->window())
				window->adjustSize();
			return kResultTrue;
		}

	private:
		atomic<uint32> references {1};
		QPointer<QWidget> container;
	};

	class PluginEditorDialog final : public QDialog
	{
	public:
		PluginEditorDialog(QWidget* parent, VST3PluginInstance* instance, function<void()> apply)
			: QDialog(parent), apply(std::move(apply))
		{
			setWindowTitle(QStringLiteral("VST3 plugin"));
			QVBoxLayout* layout = new QVBoxLayout(this);
			container = new QWidget(this);
			container->setAttribute(Qt::WA_NativeWindow);
			container->setMinimumSize(320, 160);
			layout->addWidget(container);

			QDialogButtonBox* buttons = new QDialogButtonBox(
				QDialogButtonBox::Apply | QDialogButtonBox::Close, this);
			layout->addWidget(buttons);
			connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this]() {
				if (this->apply)
					this->apply();
			});
			connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);

			view = owned(instance->createEditorView());
			if (!view || view->isPlatformTypeSupported(kPlatformTypeHWND) != kResultTrue)
			{
				QLabel* message = new QLabel(QStringLiteral("This plugin does not provide a Win32 editor view."), container);
				QVBoxLayout* containerLayout = new QVBoxLayout(container);
				containerLayout->addWidget(message);
				return;
			}

			frame = owned(new PluginViewFrame(container));
			view->setFrame(frame);
			if (view->attached(reinterpret_cast<void*>(container->winId()), kPlatformTypeHWND) != kResultTrue)
			{
				view->setFrame(nullptr);
				view = nullptr;
				return;
			}
			attached = true;
			ViewRect size {};
			if (view->getSize(&size) == kResultTrue)
				container->setFixedSize(max(1, size.getWidth()), max(1, size.getHeight()));
			adjustSize();
		}

		~PluginEditorDialog() override
		{
			if (attached && view)
				view->removed();
			if (view)
				view->setFrame(nullptr);
		}

	private:
		QWidget* container = nullptr;
		IPtr<IPlugView> view;
		IPtr<PluginViewFrame> frame;
		function<void()> apply;
		bool attached = false;
	};
}

VST3PluginFilterGUI::VST3PluginFilterGUI(const wstring& bundlePath, const wstring& classId,
	const wstring& processorState, const wstring& controllerState)
	: classId(classId), processorState(processorState), controllerState(controllerState)
{
	QGridLayout* root = new QGridLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	root->setVerticalSpacing(5);
	root->setColumnStretch(4, 1);
	QLabel* pluginLabel = new QLabel(QStringLiteral("VST3 plugin:"), this);
	pathLineEdit = new ResizingLineEdit(this);
	pathLineEdit->setPlaceholderText(QStringLiteral("Select a .vst3 plugin or paste its path"));
	QPushButton* selectButton = new QPushButton(QStringLiteral("Select plugin"), this);
	classCombo = new QComboBox(this);
	classCombo->setVisible(false);
	openButton = new QPushButton(QStringLiteral("Open panel"), this);
	statusLabel = new QLabel(this);
	statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	latencyLabel = new QLabel(this);
	root->addWidget(pluginLabel, 0, 0);
	root->addWidget(pathLineEdit, 0, 1);
	root->addWidget(selectButton, 0, 2);
	root->addWidget(statusLabel, 1, 0, 1, 2);
	root->addWidget(openButton, 1, 2);
	root->addWidget(latencyLabel, 1, 3);
	root->addWidget(classCombo, 2, 0, 1, 3);

	selectedPath = resolvePath(QString::fromStdWString(bundlePath));
	pathLineEdit->setText(displayPath(selectedPath));

	connect(selectButton, &QPushButton::clicked, this, [this]() { selectBundle(); });
	connect(pathLineEdit, &QLineEdit::editingFinished, this, [this]() { acceptEditedPath(); });
	connect(classCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
		if (!populating)
			selectClass(index);
	});
	connect(openButton, &QPushButton::clicked, this, [this]() { openEditor(); });

	loadSelectedBundle(false);
}

VST3PluginFilterGUI::~VST3PluginFilterGUI() = default;

void VST3PluginFilterGUI::store(QString& command, QString& parameters)
{
	command = QStringLiteral("VST3Plugin");
	QString path = selectedPath;
	if (!path.isEmpty())
	{
		QDir localDir(QString::fromStdWString(VST3PluginModule::getDefaultPluginPath()));
		QString relative = QDir::toNativeSeparators(localDir.relativeFilePath(path));
		if (!relative.startsWith(QStringLiteral("..\\")) &&
			!relative.startsWith(QStringLiteral("../")) && relative != QStringLiteral(".."))
			path = relative;
	}
	parameters = QStringLiteral("Bundle %1 ClassID %2").arg(quote(path), quote(QString::fromStdWString(classId)));
	if (!processorState.empty())
		parameters += QStringLiteral(" ProcessorState %1").arg(quote(QString::fromStdWString(processorState)));
	if (!controllerState.empty())
		parameters += QStringLiteral(" ControllerState %1").arg(quote(QString::fromStdWString(controllerState)));
}

void VST3PluginFilterGUI::selectBundle()
{
	QSettings settings;
	QString start = settings.value(QStringLiteral("vst3/lastDir")).toString();
	if (start.isEmpty() && !selectedPath.isEmpty())
		start = QFileInfo(selectedPath).absolutePath();
	if (start.isEmpty())
		start = QString::fromStdWString(VST3PluginModule::getDefaultPluginPath());
	QFileInfo currentFile(selectedPath);
	QString initialPath = selectedPath.isEmpty() ? start : currentFile.absoluteFilePath();
	QFileDialog dialog(this, QStringLiteral("Select VST3 plugin"), initialPath,
		QStringLiteral("VST3 plugins (*.vst3)"));
	dialog.setFileMode(QFileDialog::ExistingFile);
	dialog.setNameFilter(QStringLiteral("VST3 plugins (*.vst3)"));
	if (!selectedPath.isEmpty())
		dialog.selectFile(currentFile.fileName());
	if (dialog.exec() != QDialog::Accepted)
		return;
	QString selected = resolvePath(dialog.selectedFiles().front());
	if (selected.isEmpty())
		return;
	settings.setValue(QStringLiteral("vst3/lastDir"), QFileInfo(selected).absolutePath());
	selectedPath = selected;
	pathLineEdit->setText(displayPath(selectedPath));
	loadSelectedBundle(true);
}

void VST3PluginFilterGUI::acceptEditedPath()
{
	QString editedPath = resolvePath(pathLineEdit->text().trimmed());
	pathLineEdit->setText(displayPath(editedPath));
	if (editedPath == selectedPath)
		return;
	selectedPath = editedPath;
	loadSelectedBundle(true);
}

void VST3PluginFilterGUI::loadSelectedBundle(bool resetState)
{
	instance.reset();
	module.reset();
	openButton->setEnabled(false);
	latencyLabel->clear();
	classCombo->setVisible(false);
	QString path = selectedPath;
	if (resetState)
	{
		classId.clear();
		processorState.clear();
		controllerState.clear();
	}
	if (path.isEmpty())
	{
		updateStatus(QStringLiteral("No VST3 plugin selected."));
		if (resetState)
			emit updateModel();
		return;
	}

	wstring error;
	module = VST3PluginModule::load(path.toStdWString(), &error);
	if (!module)
	{
		updateStatus(QStringLiteral("Load failed: %1").arg(QString::fromStdWString(error)), true);
		if (resetState)
			emit updateModel();
		return;
	}
	const auto& classes = module->getClasses();
	if (classes.empty())
	{
		updateStatus(QStringLiteral("The bundle contains no VST3 audio effects."), true);
		if (resetState)
			emit updateModel();
		return;
	}

	populating = true;
	classCombo->clear();
	for (const VST3PluginClassInfo& info : classes)
	{
		QString label = QString::fromStdWString(info.name);
		if (!info.vendor.empty())
			label += QStringLiteral(" - ") + QString::fromStdWString(info.vendor);
		classCombo->addItem(label, QString::fromStdWString(info.classId));
	}
	int selectedClass = classCombo->findData(QString::fromStdWString(classId));
	if (selectedClass < 0)
		selectedClass = 0;
	classCombo->setCurrentIndex(selectedClass);
	classId = classCombo->currentData().toString().toStdWString();
	populating = false;
	classCombo->setVisible(classes.size() > 1);
	if (resetState)
	{
		emit updateModel();
	}
	openButton->setEnabled(true);
	ACCESS_MASK mask = 0;
	try
	{
		mask = RegistryHelper::getFileAccessForUser(path.toStdWString(), SECURITY_LOCAL_SERVICE_RID);
	}
	catch (RegistryException&)
	{
		mask = 0;
	}
	if ((mask & GENERIC_READ) != GENERIC_READ && (mask & FILE_GENERIC_READ) != FILE_GENERIC_READ)
	{
		updateStatus(selectedClassLabel(), true);
		statusLabel->setToolTip(QStringLiteral("The Windows audio service may not be able to read this plugin."));
	}
	else
	{
		updateStatus(selectedClassLabel());
		statusLabel->setToolTip(QStringLiteral("Ready. Mono and stereo compatibility is checked when the panel opens."));
	}
}

void VST3PluginFilterGUI::selectClass(int index)
{
	if (index < 0)
		return;
	instance.reset();
	classId = classCombo->itemData(index).toString().toStdWString();
	processorState.clear();
	controllerState.clear();
	latencyLabel->clear();
	updateStatus(selectedClassLabel());
	emit updateModel();
}

bool VST3PluginFilterGUI::initializePlugin()
{
	if (!module || classId.empty())
		return false;
	wstring error;
	instance = make_unique<VST3PluginInstance>(module, classId);
	if (!instance->initialize(48000.0, 2048, 1, processorState, controllerState, &error))
	{
		instance = make_unique<VST3PluginInstance>(module, classId);
		if (!instance->initialize(48000.0, 2048, 2, processorState, controllerState, &error))
		{
			instance.reset();
			updateStatus(QStringLiteral("Initialization failed: %1").arg(QString::fromStdWString(error)), true);
			return false;
		}
	}
	unsigned latencySamples = instance->getLatencySamples();
	if (latencySamples == 0)
	{
		latencyLabel->setText(QStringLiteral("No reported latency"));
		latencyLabel->setToolTip(QStringLiteral(
			"The plugin reports zero host-compensated latency. Internal algorithmic delay may still exist."));
	}
	else
	{
		latencyLabel->setText(QStringLiteral("%1 samples / %2 ms")
			.arg(latencySamples).arg(latencySamples * 1000.0 / 48000.0, 0, 'f', 2));
		latencyLabel->setToolTip(QStringLiteral("Latency reported by the plugin."));
	}
	return true;
}

void VST3PluginFilterGUI::openEditor()
{
	if (!initializePlugin())
		return;
	{
		PluginEditorDialog dialog(this, instance.get(), [this]() { captureState(); });
		dialog.exec();
	}
	captureState();
	instance.reset();
}

void VST3PluginFilterGUI::captureState()
{
	if (!instance)
		return;
	processorState = instance->captureProcessorState();
	controllerState = instance->captureControllerState();
	updateStatus(selectedClassLabel());
	statusLabel->setToolTip(QStringLiteral("Plugin state applied."));
	emit updateModel();
}

void VST3PluginFilterGUI::updateStatus(const QString& text, bool error)
{
	statusLabel->setText(text);
	QPalette palette = statusLabel->palette();
	palette.setColor(QPalette::WindowText, error ? QColor(210, 70, 70) : palette.color(QPalette::Text));
	statusLabel->setPalette(palette);
}

QString VST3PluginFilterGUI::displayPath(const QString& absolutePath) const
{
	if (absolutePath.isEmpty())
		return {};
	QDir localDir(QString::fromStdWString(VST3PluginModule::getDefaultPluginPath()));
	QString relative = QDir::toNativeSeparators(localDir.relativeFilePath(absolutePath));
	if (!relative.startsWith(QStringLiteral("..\\")) &&
		!relative.startsWith(QStringLiteral("../")) && relative != QStringLiteral(".."))
		return relative;
	return QDir::toNativeSeparators(absolutePath);
}

QString VST3PluginFilterGUI::resolvePath(const QString& path) const
{
	if (path.isEmpty())
		return {};
	QString nativePath = QDir::fromNativeSeparators(path);
	if (QDir::isRelativePath(nativePath))
		nativePath = QDir(QString::fromStdWString(VST3PluginModule::getDefaultPluginPath())).absoluteFilePath(nativePath);
	return QDir::toNativeSeparators(QFileInfo(nativePath).absoluteFilePath());
}

QString VST3PluginFilterGUI::selectedClassLabel() const
{
	int index = classCombo->currentIndex();
	return index >= 0 ? classCombo->itemText(index) : QFileInfo(selectedPath).baseName();
}
