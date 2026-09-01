#pragma once

#include <memory>

#include <QDialog>
#include <QList>

#include "AbstractAPOInfo.h"

class QDialogButtonBox;
class QCheckBox;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

class DeviceManagementDialog : public QDialog
{
	Q_OBJECT

public:
	explicit DeviceManagementDialog(
		const QList<std::shared_ptr<AbstractAPOInfo>>& outputDevices,
		const QList<std::shared_ptr<AbstractAPOInfo>>& inputDevices,
		QWidget* parent = nullptr);

private slots:
	void itemChanged(QTreeWidgetItem* item, int column);
	void openSoundControlPanel();
	void applyChanges();

private:
	void addDevices(const QList<std::shared_ptr<AbstractAPOInfo>>& devices, QTreeWidgetItem* parent);
	void updateItem(QTreeWidgetItem* item);
	void updateVisibility();
	bool runElevatedHelper(const QString& manifestPath);

	QTreeWidget* treeWidget;
	QCheckBox* hideDisabledCheckBox;
	QCheckBox* hideUnpluggedCheckBox;
	QDialogButtonBox* buttonBox;
	QPushButton* applyButton;
	bool initializing = true;
};
