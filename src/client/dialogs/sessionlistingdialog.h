/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#ifndef SESSIONLISTINGDIALOG_H
#define SESSIONLISTINGDIALOG_H

class Ui_SessionListingDialog;

#include <QDialog>

namespace sessionlisting {
	class SessionListingModel;
	class AnnouncementApi;
}

class ServerDiscoveryModel;
class QSortFilterProxyModel;

namespace dialogs {

class SessionListingDialog : public QDialog
{
	Q_OBJECT
public:
	explicit SessionListingDialog(QWidget *parent=nullptr);
	~SessionListingDialog();

signals:
	void selected(const QUrl &sessionUrl);

private slots:
	void refreshListing();
	void stopNsfmFiltering();

private:
	Ui_SessionListingDialog *_ui;
	sessionlisting::AnnouncementApi *_apiClient;

	QSortFilterProxyModel *_model;
	sessionlisting::SessionListingModel * _sessions;
#ifdef HAVE_DNSSD
	ServerDiscoveryModel *_localservers;
#endif
};

}

#endif // SESSIONLISTINGDIALOG_H
