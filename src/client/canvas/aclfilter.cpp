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

#include "aclfilter.h"

#include "core/layer.h"
#include "core/layerstack.h"

#include "../shared/net/meta.h"
#include "../shared/net/meta2.h"
#include "../shared/net/pen.h"
#include "../shared/net/image.h"
#include "../shared/net/layer.h"

namespace canvas {

AclFilter::AclFilter(paintcore::LayerStack *layers, QObject *parent)
	: QObject(parent), m_layers(layers)
{
	Q_ASSERT(layers);
}

void AclFilter::reset(int myId, bool localMode)
{
	for(int i=0;i<256;++i)
		m_users[i] = {false, false};

	for(int i=0;i<m_layers->layerCount();++i)
		m_layers->getLayerByIndex(i)->setAcl(false, QList<uint8_t>());
	m_myId = myId;
	setOperator(localMode);

	m_sessionLocked = false;
	m_localUserLocked = false;
	m_userLayers.clear();
	emit localLockChanged(false);

	setLayerControlLock(false);
	setOwnLayers(false);
	m_lockDefault = false;
}

// Get the ID of the layer's creator. This assumes the ID prefixing convention is used.
static uint8_t layerCreator(uint16_t layerId) {
	return layerId >> 8;
}

bool AclFilter::filterMessage(const protocol::Message &msg)
{
	using namespace protocol;

	// Note: contextId is an unsigned 8 bit value, so we just keep
	// an array of user contexts rather than use a hashmap.
	User u = m_users[msg.contextId()];

	// User list is empty in local mode
	bool isOperator = u.isOperator || (msg.contextId() == m_myId && m_isOperator);

	// First: check if this is an operator-only command
	if(msg.contextId()!=0 && msg.isOpCommand() && !isOperator)
		return false;

	// Special commands that affect access controls
	switch(msg.type()) {
	case MSG_USER_JOIN:
		// Set our own default lock. We can't set the user list item's properties
		// here, since it hasn't been created yet.
		if(msg.contextId() == m_myId && isLockedByDefault())
			setUserLock(true);
		break;

	case MSG_SESSION_OWNER:
		updateSessionOwnership(static_cast<const SessionOwner&>(msg));
		return true;
	case MSG_LAYER_ACL: {
		const auto &lmsg = static_cast<const LayerACL&>(msg);

		if(!isOperator && !(isOwnLayers() && layerCreator(lmsg.id()) == msg.contextId()))
			return false;

		paintcore::Layer *layer = m_layers->getLayer(lmsg.id());
		if(layer) {
			layer->setAcl(lmsg.locked(), lmsg.exclusive());
		} else {
			qWarning("LayerACL: layer %d does not exist!", lmsg.id());
		}

		// Emit this to refresh the UI in case our selected layer was (un)locked.
		// (We don't actually know which layer is selected in the UI here.)
		emit localLockChanged(isLocked());

		return true;
	}
	case MSG_SESSION_ACL: {
		const auto &lmsg = static_cast<const SessionACL&>(msg);

		setSessionLock(lmsg.isSessionLocked());
		setLayerControlLock(lmsg.isLayerControlLocked());
		setOwnLayers(lmsg.isOwnLayers());
		m_lockDefault = lmsg.isLockedByDefault();

		return true;
	}

	case MSG_USER_ACL:
		setUserLock(static_cast<const UserACL&>(msg).ids().contains(m_myId));
		return true;

	case MSG_TOOLCHANGE:
		m_userLayers[msg.contextId()] = static_cast<const ToolChange&>(msg).layer();
		return true;
	default: break;
	}

	// General action filtering when user is locked
	if(msg.isCommand() && (m_sessionLocked || u.isLocked))
		return false;

	// Message specific filtering
	switch(msg.type()) {
	case MSG_LAYER_CREATE:
	case MSG_LAYER_ATTR:
	case MSG_LAYER_RETITLE:
	case MSG_LAYER_DELETE: {
		uint16_t layerId=0;
		if(msg.type() == MSG_LAYER_CREATE) layerId = static_cast<const protocol::LayerCreate&>(msg).id();
		else if(msg.type() == MSG_LAYER_ATTR) layerId = static_cast<const protocol::LayerAttributes&>(msg).id();
		else if(msg.type() == MSG_LAYER_RETITLE) layerId = static_cast<const protocol::LayerRetitle&>(msg).id();
		else if(msg.type() == MSG_LAYER_DELETE) layerId = static_cast<const protocol::LayerDelete&>(msg).id();

		// In OwnLayer mode, users may create, delete and adjust their own layers.
		// Otherwise, session operator privileges are required.
		if(isLayerControlLocked() && !isOperator && !(isOwnLayers() && layerCreator(layerId) == msg.contextId()))
			return false;
		break;
	}

	case MSG_LAYER_ORDER:
		// Reordering is limited to session ops
		if(!isOperator)
			return false;
		break;

	case MSG_PUTIMAGE:
		return !isLayerLockedFor(static_cast<const PutImage&>(msg).layer(), msg.contextId());
	case MSG_FILLRECT:
		return !isLayerLockedFor(static_cast<const FillRect&>(msg).layer(), msg.contextId());
	case MSG_PEN_MOVE:
		return !isLayerLockedFor(m_userLayers[msg.contextId()], msg.contextId());
	default: break;
	}

	return true;
}

bool AclFilter::isLayerLockedFor(int layerId, uint8_t userId) const
{
	paintcore::Layer *l = m_layers->getLayer(layerId);
	if(!l) {
		qWarning("isLayerLockedFor(%d, %d): no such layer!", layerId, userId);
		return false;
	}

	return l->info().isLockedFor(userId);
}

void AclFilter::updateSessionOwnership(const protocol::SessionOwner &msg)
{
	for(int i=0;i<255;++i)
		m_users[i].isOperator = false;
	for(uint8_t id : msg.ids())
		m_users[id].isOperator = true;

	setOperator(msg.ids().contains(m_myId) || m_myId==msg.contextId());
}

void AclFilter::setOperator(bool op)
{
	if(op != m_isOperator) {
		m_isOperator = op;
		emit localOpChanged(op);
	}
}

void AclFilter::setSessionLock(bool lock)
{
	bool wasLocked = isLocked();
	m_sessionLocked = lock;
	if(wasLocked != isLocked())
		emit localLockChanged(isLocked());
}

void AclFilter::setUserLock(bool lock)
{
	bool wasLocked = isLocked();
	m_localUserLocked = lock;
	if(wasLocked != isLocked())
		emit localLockChanged(isLocked());
}

void AclFilter::setLayerControlLock(bool lock)
{
	if(m_layerCtrlLocked != lock) {
		m_layerCtrlLocked = lock;
		emit layerControlLockChanged(lock);
	}
}

void AclFilter::setOwnLayers(bool own)
{
	if(m_ownLayers != own) {
		m_ownLayers = own;
		emit ownLayersChanged(own);
	}
}

uint16_t AclFilter::sessionAclFlags() const
{
	uint16_t flags = 0;
	if(m_sessionLocked)
		flags |= protocol::SessionACL::LOCK_SESSION;

	if(m_layerCtrlLocked)
		flags |= protocol::SessionACL::LOCK_LAYERCTRL;

	if(m_lockDefault)
		flags |= protocol::SessionACL::LOCK_LAYERCTRL;

	if(m_ownLayers)
		flags |= protocol::SessionACL::LOCK_OWNLAYERS;

	return flags;
}

bool AclFilter::canUseLayerControls(int layerId) const
{
	return !isLocalUserLocked() && (isLocalUserOperator() || !isLayerControlLocked() || (isOwnLayers() && layerCreator(layerId) == m_myId));
}

bool AclFilter::canCreateLayer() const
{
	return !isLocalUserLocked() && (isLocalUserOperator() || !isLayerControlLocked() || isOwnLayers());
}

}
