#include "DeviceManagementDialog.h"

#include <QCoreApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryFile>
#include <QTextStream>
#include <QTreeWidget>
#include <QVBoxLayout>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include "APOMetaTypes.h"
#include "Editor/helpers/GUIHelper.h"
#include "helpers/RegistryHelper.h"
#include "devices/DeviceIdentityResolver.h"
#include "devices/StableDeviceSelector.h"

using namespace std;

DeviceManagementDialog::DeviceManagementDialog(
	const QList<shared_ptr<AbstractAPOInfo>>& outputDevices,
	const QList<shared_ptr<AbstractAPOInfo>>& inputDevices,
	QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Manage audio devices"));
	setWindowFlags(windowFlags().setFlag(Qt::WindowContextHelpButtonHint, false));
	resize(GUIHelper::scale(QSize(700, 430)));

	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->addWidget(new QLabel(tr("Choose the endpoints that should load EqualizerAPO-VST3. Applying changes requires administrator approval."), this));

	treeWidget = new QTreeWidget(this);
	treeWidget->setColumnCount(3);
	treeWidget->setHeaderLabels({tr("Connection"), tr("Device"), tr("Status")});
	treeWidget->setRootIsDecorated(true);
	layout->addWidget(treeWidget, 1);

	QTreeWidgetItem* outputNode = new QTreeWidgetItem(treeWidget, QStringList(tr("Playback devices")));
	outputNode->setExpanded(true);
	addDevices(outputDevices, outputNode);
	QTreeWidgetItem* inputNode = new QTreeWidgetItem(treeWidget, QStringList(tr("Capture devices")));
	inputNode->setExpanded(true);
	addDevices(inputDevices, inputNode);

	vector<EndpointIdentity> catalog;
	for (const auto& info : outputDevices)
		if (!info->getEndpointIdentity().endpointGuid.empty()) catalog.push_back(info->getEndpointIdentity());
	for (const auto& info : inputDevices)
		if (!info->getEndpointIdentity().endpointGuid.empty()) catalog.push_back(info->getEndpointIdentity());
	const wstring bindingsPath = APP_REGPATH L"\\DeviceBindings";
	if (RegistryHelper::keyExists(bindingsPath))
	{
		for (const wstring& bindingId : RegistryHelper::enumSubKeys(bindingsPath))
		{
			wstring key = bindingsPath + L"\\" + bindingId;
			if (!RegistryHelper::valueExists(key, L"Selector") || !RegistryHelper::valueExists(key, L"DesiredInstalled")
				|| RegistryHelper::readDWORDValue(key, L"DesiredInstalled") == 0)
				continue;
			wstring selectorText = RegistryHelper::readValue(key, L"Selector");
			auto parsed = StableDeviceSelectorCodec::parse(selectorText);
			if (parsed.status != StableSelectorParseStatus::Success)
				continue;
			DeviceResolution resolution = DeviceIdentityResolver::resolve(parsed.selector, catalog);
			if (resolution.endpoint == nullptr)
				continue;
			for (int groupIndex = 0; groupIndex < treeWidget->topLevelItemCount(); ++groupIndex)
			{
				QTreeWidgetItem* group = treeWidget->topLevelItem(groupIndex);
				for (int itemIndex = 0; itemIndex < group->childCount(); ++itemIndex)
				{
					QTreeWidgetItem* item = group->child(itemIndex);
					auto info = item->data(0, Qt::UserRole).value<shared_ptr<AbstractAPOInfo>>();
					if (info != nullptr && equalGuid(info->getDeviceGuid(), resolution.endpoint->endpointGuid))
					{
						item->setCheckState(0, Qt::Checked);
						item->setData(0, Qt::UserRole + 1, QString::fromStdWString(selectorText));
						item->setData(0, Qt::UserRole + 2, true);
						updateItem(item);
					}
				}
			}
		}
	}

	for (int i = 0; i < treeWidget->columnCount(); i++)
		treeWidget->resizeColumnToContents(i);
	treeWidget->header()->setStretchLastSection(true);

	QHBoxLayout* visibilityLayout = new QHBoxLayout;
	hideDisabledCheckBox = new QCheckBox(tr("Hide Disabled"), this);
	hideDisabledCheckBox->setChecked(true);
	visibilityLayout->addWidget(hideDisabledCheckBox);
	hideUnpluggedCheckBox = new QCheckBox(tr("Hide unplugged"), this);
	hideUnpluggedCheckBox->setChecked(true);
	visibilityLayout->addWidget(hideUnpluggedCheckBox);
	visibilityLayout->addStretch();
	layout->addLayout(visibilityLayout);

	buttonBox = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Cancel, this);
	QPushButton* soundControlPanelButton = buttonBox->addButton(tr("Open Sound control panel"), QDialogButtonBox::ActionRole);
	applyButton = buttonBox->button(QDialogButtonBox::Apply);
	applyButton->setText(tr("Apply"));
	layout->addWidget(buttonBox);

	connect(treeWidget, &QTreeWidget::itemChanged, this, &DeviceManagementDialog::itemChanged);
	connect(hideDisabledCheckBox, &QCheckBox::toggled, this, [this]() { updateVisibility(); });
	connect(hideUnpluggedCheckBox, &QCheckBox::toggled, this, [this]() { updateVisibility(); });
	connect(soundControlPanelButton, &QPushButton::clicked, this, &DeviceManagementDialog::openSoundControlPanel);
	connect(applyButton, &QPushButton::clicked, this, &DeviceManagementDialog::applyChanges);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	updateVisibility();
	initializing = false;
}

void DeviceManagementDialog::addDevices(
	const QList<shared_ptr<AbstractAPOInfo>>& devices, QTreeWidgetItem* parent)
{
	for (const shared_ptr<AbstractAPOInfo>& info : devices)
	{
		QTreeWidgetItem* item = new QTreeWidgetItem(parent, {
			QString::fromStdWString(info->getConnectionName()),
			QString::fromStdWString(info->getDeviceName()),
			QString()
		});
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(0, info->isInstalled() ? Qt::Checked : Qt::Unchecked);
		item->setData(0, Qt::UserRole, QVariant::fromValue(info));
		updateItem(item);
	}
}

void DeviceManagementDialog::itemChanged(QTreeWidgetItem* item, int column)
{
	if (!initializing && column == 0 && item->childCount() == 0)
		updateItem(item);
}

void DeviceManagementDialog::updateItem(QTreeWidgetItem* item)
{
	shared_ptr<AbstractAPOInfo> info = item->data(0, Qt::UserRole).value<shared_ptr<AbstractAPOInfo>>();
	if (info == nullptr)
		return;

	bool selected = item->checkState(0) == Qt::Checked;
	bool desiredBinding = item->data(0, Qt::UserRole + 2).toBool();
	QString status;
	if (selected && !info->isInstalled() && desiredBinding)
		status = tr("Repair required");
	else if (selected && !info->isInstalled())
		status = tr("Will be installed");
	else if (!selected && info->isInstalled())
		status = tr("Will be removed");
	else if (info->isInstalled() && (info->canBeUpgraded() || info->hasChanges()))
		status = tr("Will be repaired");
	else if (info->isInstalled() && info->isEnhancementsDisabled())
		status = tr("Audio enhancements will be enabled");
	else if (info->isInstalled())
		status = tr("Installed");
	else if (info->isExperimental())
		status = tr("Not installed, experimental");
	else
		status = tr("Not installed");

	if (info->isDefaultDevice())
		status += tr(", default device");
	if (info->isUnplugged())
		status += tr(", unplugged");
	item->setText(2, status);
}

void DeviceManagementDialog::updateVisibility()
{
	for (int groupIndex = 0; groupIndex < treeWidget->topLevelItemCount(); groupIndex++)
	{
		QTreeWidgetItem* group = treeWidget->topLevelItem(groupIndex);
		for (int itemIndex = 0; itemIndex < group->childCount(); itemIndex++)
		{
			QTreeWidgetItem* item = group->child(itemIndex);
			shared_ptr<AbstractAPOInfo> info = item->data(0, Qt::UserRole).value<shared_ptr<AbstractAPOInfo>>();
			item->setHidden((hideDisabledCheckBox->isChecked() && info->isDisabled())
				|| (hideUnpluggedCheckBox->isChecked() && info->isUnplugged()));
		}
	}
}

void DeviceManagementDialog::openSoundControlPanel()
{
	SHELLEXECUTEINFOW executeInfo = {};
	executeInfo.cbSize = sizeof(executeInfo);
	executeInfo.hwnd = (HWND)winId();
	executeInfo.lpVerb = L"open";
	executeInfo.lpFile = L"control.exe";
	executeInfo.lpParameters = L"mmsys.cpl";
	executeInfo.nShow = SW_SHOWNORMAL;
	if (!ShellExecuteExW(&executeInfo))
		QMessageBox::critical(this, tr("Error"), tr("Could not open the Sound control panel."));
}

void DeviceManagementDialog::applyChanges()
{
	QTemporaryFile manifest(QDir::tempPath() + "/EqualizerAPO-devices-XXXXXX.txt");
	if (!manifest.open())
	{
		QMessageBox::critical(this, tr("Error"), tr("Could not create the temporary device request."));
		return;
	}
	QTextStream stream(&manifest);
	stream.setEncoding(QStringConverter::Utf8);
	for (int groupIndex = 0; groupIndex < treeWidget->topLevelItemCount(); groupIndex++)
	{
		QTreeWidgetItem* group = treeWidget->topLevelItem(groupIndex);
		for (int itemIndex = 0; itemIndex < group->childCount(); itemIndex++)
		{
			QTreeWidgetItem* item = group->child(itemIndex);
			shared_ptr<AbstractAPOInfo> info = item->data(0, Qt::UserRole).value<shared_ptr<AbstractAPOInfo>>();
			QString identity = item->data(0, Qt::UserRole + 1).toString();
			if (identity.isEmpty() && !info->getEndpointIdentity().endpointGuid.empty())
			{
				StableDeviceSelector selector;
				selector.bindingId = StableDeviceSelectorCodec::createBindingId();
				selector.snapshot = info->getEndpointIdentity();
				selector.allowNameFallback = selector.snapshot.formFactor.has_value();
				identity = QString::fromStdWString(StableDeviceSelectorCodec::serialize(selector));
			}
			if (identity.isEmpty())
				identity = "name:" + QString::fromStdWString(info->getDeviceString());
			stream << (info->isInput() ? "capture" : "render") << '\t'
				<< identity << '\t'
				<< (item->checkState(0) == Qt::Checked ? "1" : "0") << '\n';
		}
	}
	stream.flush();
	manifest.flush();
	manifest.close();

	if (runElevatedHelper(QDir::toNativeSeparators(manifest.fileName())))
		accept();
}

bool DeviceManagementDialog::runElevatedHelper(const QString& manifestPath)
{
	QString helperPath = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("DeviceControl.exe");
	if (!QFileInfo::exists(helperPath))
	{
		QMessageBox::critical(this, tr("Error"), tr("DeviceControl.exe is missing from the application directory."));
		return false;
	}

	wstring helper = QDir::toNativeSeparators(helperPath).toStdWString();
	wstring parameters = L"--apply \"" + manifestPath.toStdWString() + L"\"";
	SHELLEXECUTEINFOW executeInfo = {};
	executeInfo.cbSize = sizeof(executeInfo);
	executeInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	executeInfo.hwnd = (HWND)winId();
	executeInfo.lpVerb = L"runas";
	executeInfo.lpFile = helper.c_str();
	executeInfo.lpParameters = parameters.c_str();
	executeInfo.nShow = SW_HIDE;

	if (!ShellExecuteExW(&executeInfo))
	{
		if (GetLastError() != ERROR_CANCELLED)
			QMessageBox::critical(this, tr("Error"), tr("Could not start the elevated device helper."));
		return false;
	}

	WaitForSingleObject(executeInfo.hProcess, INFINITE);
	DWORD exitCode = 1;
	GetExitCodeProcess(executeInfo.hProcess, &exitCode);
	CloseHandle(executeInfo.hProcess);
	if (exitCode != 0)
	{
		QMessageBox::critical(this, tr("Error"), tr("The audio-device changes could not be applied."));
		return false;
	}
	return true;
}
