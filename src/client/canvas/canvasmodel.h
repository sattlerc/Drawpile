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

#ifndef CANVASMODEL_H
#define CANVASMODEL_H

#include <QObject>

#include "../shared/net/message.h"

// note: we must include these so the datatypes get registered properly for use in QML
#include "usercursormodel.h"
#include "lasertrailmodel.h"
#include "selection.h"
#include "annotationmodel.h"
#include "core/layerstack.h"

namespace protocol {
	class UserJoin;
	class UserLeave;
	class LaserTrail;
	class MovePointer;
	class Chat;
	class Marker;
}

class QThread;

namespace canvas {

class StateTracker;
class AclFilter;
class UserListModel;
class LayerListModel;
class CommandQueue;

class CanvasModel : public QObject
{
	friend class CommandQueue;
	Q_PROPERTY(paintcore::LayerStack* layerStack READ layerStack CONSTANT)
	Q_PROPERTY(AnnotationModel* annotations READ annotations CONSTANT)
	Q_PROPERTY(UserCursorModel* userCursors READ userCursors CONSTANT)
	Q_PROPERTY(LaserTrailModel* laserTrails READ laserTrails CONSTANT)
	Q_PROPERTY(StateTracker* stateTracker READ stateTracker CONSTANT)
	Q_PROPERTY(Selection* selection READ selection WRITE setSelection NOTIFY selectionChanged)

	Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)

	Q_OBJECT

public:
	explicit CanvasModel(int localUserId, QObject *parent = 0);
	~CanvasModel();

	// These live in a background processing thread.
	// Remember to synchronize when accessing their properties!
	paintcore::LayerStack *layerStack() const { return m_layerstack; }
	StateTracker *stateTracker() const { return m_statetracker; }
	AclFilter *aclFilter() const { return m_aclfilter; }

	// Rest of the models live in the same thread as this object
	UserCursorModel *userCursors() const { return m_usercursors; }
	LaserTrailModel *laserTrails() const { return m_lasers; }
	UserListModel *userlist() const { return m_userlist; }
	LayerListModel *layerlist() const { return m_layerlist; }
	AnnotationModel *annotations() const { return m_annotations; }

	QString title() const { return m_title; }
	void setTitle(const QString &title);

	Selection *selection() const { return m_selection; }
	void setSelection(Selection *selection);

	bool needsOpenRaster() const;
	QImage toImage() const;
	bool save(const QString &filename) const;

	QList<protocol::MessagePtr> generateSnapshot(bool forceNew) const;

	int localUserId() const;

	int getAvailableAnnotationId() const;

	QImage selectionToImage(int layerId) const;
	void pasteFromImage(const QImage &image, const QPoint &defaultPoint, bool forceDefault);

	void connectedToServer(int myUserId);
	void disconnectedFromServer();
	void endPlayback();

	/**
	 * @brief Is the canvas in "online mode"?
	 *
	 * This mainly affects how certain access controls are checked.
	 */
	bool isOnline() const { return m_onlinemode; }

public slots:
	//! Handle a meta/command message received from the server
	void handleCommand(protocol::MessagePtr cmd);

	//! Handle a local drawing command (will be put in the local fork)
	void handleLocalCommand(protocol::MessagePtr cmd);

	void resetCanvas();

	void pickColor(int x, int y, int layer, int diameter=0);

	void setLayerViewMode(int mode);
	void updateLayerViewOptions();

private slots:
	void handleMeta(protocol::MessagePtr cmd);

signals:
	void layerAutoselectRequest(int id);
	void canvasModified();
	void selectionChanged(Selection *selection);
	void selectionRemoved();

	void titleChanged(QString title);
	void imageSizeChanged();

	void colorPicked(const QColor &color);

	void chatMessageReceived(const QString &user, const QString &message, bool announcement, bool action, bool me);
	void markerMessageReceived(const QString &user, const QString &message);

	void userJoined(int id, const QString &name);
	void userLeft(const QString &name);

	void canvasLocked(bool locked);

private slots:
	void onCanvasResize(int xoffset, int yoffset, const QSize &oldsize);

private:
	void metaUserJoin(const protocol::UserJoin &msg);
	void metaUserLeave(const protocol::UserLeave &msg);
	void metaLaserTrail(const protocol::LaserTrail &msg);
	void metaMovePointer(const protocol::MovePointer &msg);
	void metaChat(const protocol::Chat &msg);
	void metaMarkerMessage(const protocol::Marker &msg);

	// Processing thread
	QThread *m_thread;
	CommandQueue *m_cmdqueue;
	AclFilter *m_aclfilter;
	paintcore::LayerStack *m_layerstack;
	StateTracker *m_statetracker;

	// Main thread
	UserListModel *m_userlist;
	LayerListModel *m_layerlist;
	UserCursorModel *m_usercursors;
	LaserTrailModel *m_lasers;
	Selection *m_selection;
	AnnotationModel *m_annotations;

	QString m_title;

	bool m_onlinemode;
};

}

#endif // CANVASSTATE_H
