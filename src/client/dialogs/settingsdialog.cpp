/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2015 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"
#include "main.h"
#include "dialogs/settingsdialog.h"
#include "dialogs/certificateview.h"
#include "export/ffmpegexporter.h" // for setting ffmpeg path
#include "widgets/keysequenceedit.h"
#include "utils/icon.h"
#include "utils/customshortcutmodel.h"
#include "utils/listservermodel.h"
#include "utils/listserverdelegate.h"
#include "utils/networkaccess.h"
#include "utils/settings.h"
#include "../shared/util/announcementapi.h"

#include "ui_settings.h"

#include <QSettings>
#include <QMessageBox>
#include <QHeaderView>
#include <QStyledItemDelegate>
#include <QItemEditorFactory>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QSslCertificate>
#include <QSortFilterProxyModel>
#include <QPointer>

#include <QDebug>

class KeySequenceEditFactory : public QItemEditorCreatorBase
{
public:
	QWidget *createWidget(QWidget *parent) const
	{
		return new widgets::KeySequenceEdit(parent);
	}

	QByteArray valuePropertyName() const
	{
		return "keySequence";
	}
};

namespace dialogs {

/**
 * Construct a settings dialog. The actions in the list should have
 * a "defaultshortcut" property for reset to default to work.
 *
 * @param actions list of customizeable actions (for shortcut editing)
 * @param parent parent widget
 */
SettingsDialog::SettingsDialog(QWidget *parent)
	: QDialog(parent)
{
	_ui = new Ui_SettingsDialog;
	_ui->setupUi(this);

	connect(_ui->buttonBox, SIGNAL(accepted()), this, SLOT(rememberSettings()));
	connect(_ui->buttonBox, SIGNAL(accepted()), this, SLOT(saveCertTrustChanges()));
	connect(_ui->buttonBox->button(QDialogButtonBox::Reset), SIGNAL(clicked()), this, SLOT(resetSettings()));

	connect(_ui->pickFfmpeg, &QToolButton::clicked, [this]() {
		QString path = QFileDialog::getOpenFileName(this, tr("Set ffmepg path"), _ui->ffmpegpath->text(),
#ifdef Q_OS_WIN
			tr("Executables (%1)").arg("*.exe") + ";;" +
#endif
			QApplication::tr("All files (*)")
		);
		if(!path.isEmpty())
			_ui->ffmpegpath->setText(path);
	});

	connect(_ui->pickRecordingFolder, &QToolButton::clicked, [this]() {
		QString path = QFileDialog::getExistingDirectory(this, tr("Recording folder"), _ui->recordingFolder->text());
		if(!path.isEmpty())
			_ui->recordingFolder->setText(path);
	});

	connect(_ui->notificationVolume, &QSlider::valueChanged, [this](int val) {
		if(val>0)
			_ui->volumeLabel->setText(QString::number(val) + "%");
		else
			_ui->volumeLabel->setText(tr("off", "notifications sounds"));
	});

	// Get available languages
	_ui->languageBox->addItem(tr("Default"), QString());
	_ui->languageBox->addItem(QStringLiteral("English"), QStringLiteral("en"));

	const QLocale localeC = QLocale::c();
	QStringList locales;
	for(const QString &datapath : DrawpileApp::dataPaths()) {
		QStringList files = QDir(datapath + "/i18n").entryList(QStringList("drawpile_*.qm"), QDir::Files, QDir::Name);
		for(const QString &file : files) {
			QString localename = file.mid(9, file.length() - 3 - 9);
			QLocale locale(localename);
			if(locale != localeC && !locales.contains(localename)) {
				locales << localename;
				_ui->languageBox->addItem(locale.nativeLanguageName(), localename);
			}
		}
	}

	// Editable shortcuts
	_customShortcuts = new CustomShortcutModel(this);
	auto filteredShortcuts = new QSortFilterProxyModel(this);
	filteredShortcuts->setSourceModel(_customShortcuts);
	connect(_ui->shortcutFilter, &QLineEdit::textChanged, filteredShortcuts, &QSortFilterProxyModel::setFilterFixedString);
	filteredShortcuts->setFilterCaseSensitivity(Qt::CaseInsensitive);
	_ui->shortcuts->setModel(filteredShortcuts);
	_ui->shortcuts->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	_ui->shortcuts->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

	// QKeySequence editor delegate
	QStyledItemDelegate *keyseqdel = new QStyledItemDelegate(this);
	QItemEditorFactory *itemeditorfactory = new QItemEditorFactory;
	itemeditorfactory->registerEditor(QVariant::nameToType("QKeySequence"), new KeySequenceEditFactory);
	keyseqdel->setItemEditorFactory(itemeditorfactory);
	_ui->shortcuts->setItemDelegateForColumn(1, keyseqdel);

	// Known hosts list
	connect(_ui->knownHostList, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(viewCertificate(QListWidgetItem*)));
	connect(_ui->knownHostList, SIGNAL(itemSelectionChanged()), this, SLOT(certificateSelectionChanged()));
	connect(_ui->trustKnownHosts, SIGNAL(clicked()), this, SLOT(markTrustedCertificates()));
	connect(_ui->removeKnownHosts, SIGNAL(clicked()), this, SLOT(removeCertificates()));
	connect(_ui->importTrustedButton, SIGNAL(clicked()), this, SLOT(importTrustedCertificate()));

	QStringList pemfilter; pemfilter << "*.pem";
	QDir knownHostsDir(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/known-hosts/");

	for(const QString &filename : knownHostsDir.entryList(pemfilter, QDir::Files)) {
		auto *i = new QListWidgetItem(filename.left(filename.length()-4), _ui->knownHostList);
		i->setData(Qt::UserRole, false);
		i->setData(Qt::UserRole+1, knownHostsDir.absoluteFilePath(filename));
	}

	QDir trustedHostsDir(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/trusted-hosts/");
	QIcon trustedIcon("builtin:trusted.svg");
	for(const QString &filename : trustedHostsDir.entryList(pemfilter, QDir::Files)) {
		auto *i = new QListWidgetItem(trustedIcon, filename.left(filename.length()-4), _ui->knownHostList);
		i->setData(Qt::UserRole, true);
		i->setData(Qt::UserRole+1, trustedHostsDir.absoluteFilePath(filename));
	}

	// Session listing server list
	_listservers = new sessionlisting::ListServerModel(false, this);
	_ui->listserverview->setModel(_listservers);
	_ui->listserverview->setItemDelegate(new sessionlisting::ListServerDelegate(this));

	connect(_ui->addListServer, &QPushButton::clicked, this, &SettingsDialog::addListingServer);
	connect(_ui->removeListServer, &QPushButton::clicked, this, &SettingsDialog::removeListingServer);

	// Load configuration
	restoreSettings();
}

SettingsDialog::~SettingsDialog()
{
	delete _ui;
}

void SettingsDialog::resetSettings()
{
	QMessageBox::StandardButton b = QMessageBox::question(
				this,
				tr("Reset settings"),
				tr("Clear all settings?")
				);
	if(b==QMessageBox::Yes) {
		QSettings().clear();
		restoreSettings();
		rememberSettings();
	}
}

void SettingsDialog::restoreSettings()
{
	QSettings cfg;

  cfg.beginGroup("settings/whiteboard");
  _ui->whiteboardPenSize->setValue(cfg.value("penSize", 1).toInt());
  _ui->whiteboardEraserSize->setValue(cfg.value("eraserSize", 15).toInt());
  _ui->whiteboardFingerSize->setValue(cfg.value("fingerSize", 2).toInt());
  cfg.endGroup();

	cfg.beginGroup("notifications");
	_ui->notificationVolume->setValue(cfg.value("volume", 40).toInt());
	_ui->notifChat->setChecked(cfg.value("chat", true).toBool());
	_ui->notifMarker->setChecked(cfg.value("marker", true).toBool());
	_ui->notifLogin->setChecked(cfg.value("login", true).toBool());
	_ui->notifLock->setChecked(cfg.value("lock", true).toBool());
	cfg.endGroup();

	cfg.beginGroup("settings");
	{
		QVariant langOverride = cfg.value("language", QString());
		for(int i=1;i<_ui->languageBox->count();++i) {
			if(_ui->languageBox->itemData(i) == langOverride) {
				_ui->languageBox->setCurrentIndex(i);
				break;
			}
		}
	}

	_ui->autosaveInterval->setValue(cfg.value("autosave", 5000).toInt() / 1000);
	cfg.endGroup();

	cfg.beginGroup("settings/input");
	_ui->tabletSupport->setChecked(cfg.value("tabletevents", true).toBool());
	_ui->tabletBugWorkaround->setChecked(cfg.value("tabletbugs", false).toBool());
#ifdef Q_OS_MAC
	// Gesture scrolling is always enabled on Macs
	_ui->touchscroll->setChecked(true);
	_ui->touchscroll->setEnabled(false);
#else
	_ui->touchscroll->setChecked(cfg.value("touchscroll", true).toBool());
#endif
	_ui->touchpinch->setChecked(cfg.value("touchpinch", true).toBool());
	_ui->touchtwist->setChecked(cfg.value("touchtwist", true).toBool());
	cfg.endGroup();

	cfg.beginGroup("settings/recording");
	_ui->recordpause->setChecked(cfg.value("recordpause", true).toBool());
	_ui->minimumpause->setValue(cfg.value("minimumpause", 0.5).toFloat());
	_ui->ffmpegpath->setText(FfmpegExporter::getFfmpegPath());
	_ui->recordingFolder->setText(utils::settings::recordingFolder());
	cfg.endGroup();

	cfg.beginGroup("settings/animation");
	_ui->onionskinsBelow->setValue(cfg.value("onionskinsbelow", 4).toInt());
	_ui->onionskinsAbove->setValue(cfg.value("onionskinsabove", 4).toInt());
	_ui->onionskinTint->setChecked(cfg.value("onionskintint", true).toBool());
	_ui->backgroundlayer->setChecked(cfg.value("backgroundlayer", true).toBool());
	cfg.endGroup();

	cfg.beginGroup("settings/server");
	_ui->serverport->setValue(cfg.value("port",DRAWPILE_PROTO_DEFAULT_PORT).toInt());
	_ui->historylimit->setValue(cfg.value("historylimit", 0).toDouble());
	_ui->connTimeout->setValue(cfg.value("timeout", 60).toInt());
#ifdef HAVE_DNSSD
	_ui->dnssd->setChecked(cfg.value("dnssd", true).toBool());
#else
	_ui->dnssd->setEnabled(false);
#endif
#ifdef HAVE_UPNP
	_ui->useupnp->setChecked(cfg.value("upnp", true).toBool());
#else
	_ui->useupnp->setEnabled(false);
#endif
	cfg.endGroup();

	_ui->showNsfm->setChecked(cfg.value("listservers/nsfm", false).toBool());

	_customShortcuts->loadShortcuts();
}

void SettingsDialog::rememberSettings()
{
	QSettings cfg;

  cfg.beginGroup("settings/whiteboard");
  cfg.setValue("penSize", _ui->whiteboardPenSize->value());
  cfg.setValue("eraserSize", _ui->whiteboardEraserSize->value());
  cfg.setValue("fingerSize", _ui->whiteboardFingerSize->value());
  cfg.endGroup();

  // Remember notification settings
	cfg.beginGroup("notifications");
	cfg.setValue("volume", _ui->notificationVolume->value());
	cfg.setValue("chat", _ui->notifChat->isChecked());
	cfg.setValue("marker", _ui->notifMarker->isChecked());
	cfg.setValue("login", _ui->notifLogin->isChecked());
	cfg.setValue("lock", _ui->notifLock->isChecked());
	cfg.endGroup();

	// Remember general settings
	cfg.setValue("settings/language", _ui->languageBox->itemData(_ui->languageBox->currentIndex()));
	cfg.setValue("settings/autosave", _ui->autosaveInterval->value() * 1000);

	cfg.beginGroup("settings/input");
	cfg.setValue("tabletevents", _ui->tabletSupport->isChecked());
	cfg.setValue("tabletbugs", _ui->tabletBugWorkaround->isChecked());
	cfg.setValue("touchscroll", _ui->touchscroll->isChecked());
	cfg.setValue("touchpinch", _ui->touchpinch->isChecked());
	cfg.setValue("touchtwist", _ui->touchtwist->isChecked());
	cfg.endGroup();

	cfg.beginGroup("settings/recording");
	cfg.setValue("recordpause", _ui->recordpause->isChecked());
	cfg.setValue("minimumpause", _ui->minimumpause->value());
	FfmpegExporter::setFfmpegPath(_ui->ffmpegpath->text().trimmed());
	cfg.setValue("folder", _ui->recordingFolder->text());
	cfg.endGroup();

	cfg.beginGroup("settings/animation");
	cfg.setValue("onionskinsbelow", _ui->onionskinsBelow->value());
	cfg.setValue("onionskinsabove", _ui->onionskinsAbove->value());
	cfg.setValue("onionskintint", _ui->onionskinTint->isChecked());
	cfg.setValue("backgroundlayer", _ui->backgroundlayer->isChecked());
	cfg.endGroup();

	// Remember server settings
	cfg.beginGroup("settings/server");
	if(_ui->serverport->value() == DRAWPILE_PROTO_DEFAULT_PORT)
		cfg.remove("port");
	else
		cfg.setValue("port", _ui->serverport->value());

	cfg.setValue("historylimit", _ui->historylimit->value());
	cfg.setValue("timeout", _ui->connTimeout->value());
	cfg.setValue("dnssd", _ui->dnssd->isChecked());
	cfg.setValue("upnp", _ui->useupnp->isChecked());

	cfg.endGroup();

	_customShortcuts->saveShortcuts();
	_listservers->saveServers();
	cfg.setValue("listservers/nsfm", _ui->showNsfm->isChecked());

	static_cast<DrawpileApp*>(qApp)->notifySettingsChanged();
}

void SettingsDialog::saveCertTrustChanges()
{
	// Delete removed certificates
	for(const QString &certfile : _removeCerts) {
		QFile(certfile).remove();
	}

	// Move selected certs to trusted certs
	QDir trustedDir = QDir(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/trusted-hosts/");
	trustedDir.mkpath(".");

	for(const QString &certfile : _trustCerts) {
		QString certname = certfile.mid(certfile.lastIndexOf('/')+1);
		QFile(certfile).rename(trustedDir.absoluteFilePath(certname));
	}

	// Save imported certificates
	for(const QSslCertificate &cert : _importCerts) {
		QString hostname = cert.subjectInfo(QSslCertificate::CommonName).at(0);

		QFile f(trustedDir.absoluteFilePath(hostname + ".pem"));
		if(!f.open(QFile::WriteOnly)) {
			qWarning() << "error opening" << f.fileName() << f.errorString();
			continue;
		}

		f.write(cert.toPem());
	}
}

void SettingsDialog::viewCertificate(QListWidgetItem *item)
{
	QString filename;
	if(item->data(Qt::UserRole+2).isNull())
		filename = item->data(Qt::UserRole+1).toString();
	else // read imported cert from original file
		filename = item->data(Qt::UserRole+2).toString();

	QList<QSslCertificate> certs = QSslCertificate::fromPath(filename);
	if(certs.isEmpty()) {
		qWarning() << "Certificate" << filename << "not found!";
		return;
	}

	CertificateView *cv = new CertificateView(item->text(), certs.at(0), this);
	cv->setAttribute(Qt::WA_DeleteOnClose);
	cv->show();
}

void SettingsDialog::certificateSelectionChanged()
{
	const QItemSelectionModel *sel = _ui->knownHostList->selectionModel();
	if(sel->selectedIndexes().isEmpty()) {
		_ui->trustKnownHosts->setEnabled(false);
		_ui->removeKnownHosts->setEnabled(false);
	} else {
		bool cantrust = false;
		for(const QModelIndex &idx : sel->selectedIndexes()) {
			if(!idx.data(Qt::UserRole).toBool()) {
				cantrust = true;
				break;
			}
		}
		_ui->trustKnownHosts->setEnabled(cantrust);
		_ui->removeKnownHosts->setEnabled(true);
	}
}

void SettingsDialog::markTrustedCertificates()
{
	QIcon trustedIcon("builtin:trusted.svg");
	for(QListWidgetItem *item : _ui->knownHostList->selectedItems()) {
		if(!item->data(Qt::UserRole).toBool()) {
			_trustCerts.append(item->data(Qt::UserRole+1).toString());
			item->setIcon(trustedIcon);
			item->setData(Qt::UserRole, true);
		}
	}
	_ui->trustKnownHosts->setEnabled(false);
}

void SettingsDialog::removeCertificates()
{
	for(QListWidgetItem *item : _ui->knownHostList->selectedItems()) {
		QString path = item->data(Qt::UserRole+1).toString();
		if(path.isEmpty()) {
			foreach(const QSslCertificate &imported, _importCerts) {
				if(imported.subjectInfo(QSslCertificate::CommonName).at(0) == item->text())
					_importCerts.removeOne(imported);
			}
		} else {
			_trustCerts.removeAll(path);
			_removeCerts.append(path);
		}

		delete item;
	}
}

void SettingsDialog::importTrustedCertificate()
{
	QString path = QFileDialog::getOpenFileName(this, tr("Import trusted certificate"), QString(),
		tr("Certificates (%1)").arg("*.pem *.crt *.cer") + ";;" +
		QApplication::tr("All files (*)")
	);

	if(path.isEmpty())
		return;

	QList<QSslCertificate> certs = QSslCertificate::fromPath(path);
	if(certs.isEmpty() || certs.at(0).isNull()) {
		QMessageBox::warning(this, tr("Import trusted certificate"), tr("Invalid certificate!"));
		return;
	}

	if(certs.at(0).subjectInfo(QSslCertificate::CommonName).isEmpty()) {
		QMessageBox::warning(this, tr("Import trusted certificate"), tr("Certificate common name not set!"));
		return;
	}

	_importCerts.append(certs.at(0));

	QIcon trustedIcon("builtin:trusted.svg");
	auto *i = new QListWidgetItem(trustedIcon, certs.at(0).subjectInfo(QSslCertificate::CommonName).at(0), _ui->knownHostList);
	i->setData(Qt::UserRole, true);
	i->setData(Qt::UserRole+2, path);
}

void SettingsDialog::addListingServer()
{
	QString urlstr = QInputDialog::getText(this, tr("Add public listing server"), "URL");
	if(!urlstr.isEmpty()) {
		QUrl url(urlstr);
		if(!url.isValid()) {
			QMessageBox::warning(this, tr("Add public listing server"), tr("Invalid URL!"));
			return;
		}

		auto *api = new sessionlisting::AnnouncementApi;

		QPointer<SettingsDialog> self(this);

		connect(api, &sessionlisting::AnnouncementApi::error, [self, api](QString error) {
			QMessageBox::warning(self, tr("Add public listing server"), error);
			api->deleteLater();
		});

		connect(api, &sessionlisting::AnnouncementApi::serverInfo, [self, url, api](sessionlisting::ListServerInfo info) {
			if(!self.isNull()) {
				self->_listservers->addServer(info.name, url.toString(), info.description);

				if(info.faviconUrl == "drawpile") {
					self->_listservers->setFavicon(url.toString(), QIcon("builtin:drawpile.png").pixmap(128, 128).toImage());
				} else {
					QUrl favicon(info.faviconUrl);
					if(favicon.isValid()) {
						networkaccess::getImage(favicon, nullptr, [self, url](const QImage &image, const QString &) {
							if(!self.isNull() && !image.isNull()) {
								self->_listservers->setFavicon(url.toString(), image);
							}
						});
					}
				}
			}
			api->deleteLater();
		});

		api->getApiInfo(url);
	}
}

void SettingsDialog::removeListingServer()
{
	QModelIndex selection = _ui->listserverview->selectionModel()->currentIndex();
	if(selection.isValid()) {
		_listservers->removeRow(selection.row());
	}
}

}

