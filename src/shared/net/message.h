/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2015 Calle Laakkonen

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
#ifndef DP_NET_MESSAGE_H
#define DP_NET_MESSAGE_H

#include <QAtomicInt>
#include <QMetaType>

namespace protocol {

/**
 * Drawpile network protocol message types
 */
enum MessageType {
	// Control messages (transparent)
	MSG_COMMAND=0,
	MSG_DISCONNECT,
	MSG_PING,
	MSG_STREAMPOS,

	// Meta messages (transparent)
	MSG_USER_JOIN=32,
	MSG_USER_LEAVE,
	MSG_SESSION_OWNER,

	// Meta messages (opaque)
	MSG_CHAT=64,
	MSG_INTERVAL,
	MSG_LASERTRAIL,
	MSG_MOVEPOINTER,
	MSG_MARKER,
	MSG_USER_ACL,
	MSG_LAYER_ACL,
	MSG_SESSION_ACL,

	// Command messages (opaque)
	MSG_UNDOPOINT=128,
	MSG_CANVAS_RESIZE,
	MSG_LAYER_CREATE,
	MSG_LAYER_ATTR,
	MSG_LAYER_RETITLE,
	MSG_LAYER_ORDER,
	MSG_LAYER_DELETE,
	MSG_LAYER_VISIBILITY,
	MSG_PUTIMAGE,
	MSG_FILLRECT,
	MSG_TOOLCHANGE,
	MSG_PEN_MOVE,
	MSG_PEN_UP,
	MSG_ANNOTATION_CREATE,
	MSG_ANNOTATION_RESHAPE,
	MSG_ANNOTATION_EDIT,
	MSG_ANNOTATION_DELETE,
	MSG_UNDO=255,
};

enum MessageUndoState {
	DONE   = 0x00, /* done/not undone */
	UNDONE = 0x01, /* marked as undone, can be redone */
	GONE   = 0x03  /* marked as undone, cannot be redone */
};

class Message {
	friend class MessagePtr;
public:
	//! Length of the fixed message header
	static const int HEADER_LEN = 4;

	Message(MessageType type, uint8_t ctx): m_type(type), _undone(DONE), m_refcount(0), m_contextid(ctx) {}
	virtual ~Message() {}
	
	/**
	 * @brief Get the type of this message.
	 * @return message type
	 */
	MessageType type() const { return m_type; }
	
	/**
	 * @brief Is this a control message
	 *
	 * Control messages are used for things that are related to the server and not
	 * the session directly (e.g. setting server settings.)
	 */
	bool isControl() const { return m_type < 32; }

	/**
	 * @brief Is this a meta message?
	 *
	 * Meta messages are part of the session, but do not directly affect drawing.
	 * However, some meta message (those related to access controls) do affect
	 * how the command messages are filtered.
	 */
	bool isMeta() const { return m_type >= 31 && m_type < 128; }

	/**
	 * @brief Check if this message type is a command stream type
	 * 
	 * Command stream messages are the messages directly related to drawing.
	 * The canvas can be reconstructed exactly using only command messages.
	 * @return true if this is a drawing command
	 */
	bool isCommand() const { return m_type >= 128; }

	/**
	 * @brief Is this an opaque message
	 *
	 * Opaque messages are those messages that the server does not need to understand and can
	 * merely pass along as binary data.
	 */
	bool isOpaque() const { return m_type >= 64; }

	/**
	 * @brief Is this a recordable message?
	 *
	 * All Meta and Command messages are recordable. Only the Control messages,
	 * which are used just for client/server communications, are ignored.
	 */
	bool isRecordable() const { return m_type >= 32; }

	/**
	 * @brief Get the message length, header included
	 * @return message length in bytes
	 */
	int length() const { return HEADER_LEN + payloadLength(); }

	/**
	 * @brief Get the user context ID of this message
	 *
	 * The ID is 0 for messages that are not related to any user
	 * @return context ID or 0 if not applicable
	 */
	uint8_t contextId() const { return m_contextid; }

	/**
	 * @brief Set the user ID of this message
	 *
	 * @param userid the new user id
	 */
	void setContextId(uint8_t userid) { m_contextid = userid; }

	/**
	 * @brief Does this command need operator privileges to issue?
	 * @return true if user must be session operator to send this
	 */
	virtual bool isOpCommand() const { return false; }

	/**
	 * @brief Is this message type undoable?
	 *
	 * By default, all Command messages are undoable.
	 *
	 * @return true if this action can be undone
	 */
	virtual bool isUndoable() const { return isCommand(); }

	/**
	 * @brief Has this command been marked as undone?
	 *
	 * Note. This is a purely local flag that is not part of the
	 * protocol. It is here to avoid the need to maintain an
	 * external undone action list.
	 *
	 * @return true if this message has been marked as undone
	 */
	MessageUndoState undoState() const { return _undone; }

	/**
	 * @brief Mark this message as undone
	 *
	 * Note. Not all messages are undoable. This function
	 * does nothing if this message type doesn't support undoing.
	 *
	 * @param undone new undo flag state
	 */
	void setUndoState(MessageUndoState undo) { if(isUndoable()) _undone = undo; }

	/**
	 * @brief Serialize this message
	 *
	 * The data buffer must be long enough to hold length() bytes.
	 * @param data buffer where to write the message
	 * @return number of bytes written (should always be length())
	 */
	int serialize(char *data) const;

	/**
	 * @brief get the length of the message from the given data
	 *
	 * Data buffer should be at least two bytes long
	 * @param data data buffer
	 * @return length
	 */
	static int sniffLength(const char *data);

	/**
	 * @brief deserialize a message from data buffer
	 *
	 * The provided buffer should contain at least sniffLength(data)
	 * bytes. The parameter buflen is the maximum length of the buffer.
	 * If the announced length of the message is less than the buffer
	 * length, a null pointer is returned.
	 *
	 * If the message type is unrecognized or the message content is
	 * determined to be invalid, a null pointer is returned.
	 *
	 * @param data input data buffer
	 * @param buflen length of the data buffer
	 * @param decodeOpaque automatically decode opaque messages rather than returning OpaqueMessage
	 * @return message or 0 if type is unknown
	 */
	static Message *deserialize(const uchar *data, int buflen, bool decodeOpaque);

	/**
	 * @brief Check if this message has the same content as the other one
	 * @param m
	 * @return
	 */
	bool equals(const Message &m) const;

protected:
	/**
	 * @brief Get the length of the message payload
	 * @return payload length in bytes
	 */
	virtual int payloadLength() const = 0;

	/**
	 * @brief Serialize the message payload
	 * @param data data buffer
	 * @return number of bytes written (should always be the same as payloadLenth())
	 */
	virtual int serializePayload(uchar *data) const = 0;

	/**
	 * @brief Check if the other message has identical payload
	 *
	 * The default implementation calls serializePayload and does a bytewise comparison
	 * on that. Subclasses should override this with a more efficient check.
	 *
	 * @param m
	 * @return true if payloads are equal
	 */
	virtual bool payloadEquals(const Message &m) const;

private:
	const MessageType m_type;
	MessageUndoState _undone;
	QAtomicInt m_refcount;
	uint8_t m_contextid;
};

/**
 * @brief Base class for messages without a payload
 */
template<class M> class ZeroLengthMessage : public Message {
public:
	ZeroLengthMessage(MessageType type, uint8_t ctx) : Message(type, ctx) { }

	static M *deserialize(uint8_t ctx, const uchar *data, int buflen) {
		Q_UNUSED(data);
		if(buflen!=0)
			return nullptr;
		return new M(ctx);
	}

protected:
	int payloadLength() const { return 0; }
	int serializePayload(uchar *data) const { Q_UNUSED(data); return 0; }
	bool payloadEquals(const Message &m) const { Q_UNUSED(m); return true; }
};

/**
* @brief A reference counting pointer for Messages
*
* This object is the length of a normal pointer so it can be used
* efficiently with QList.
*/
class MessagePtr {
public:
	MessagePtr() : m_ptr(nullptr) { }

	/**
	 * @brief Take ownership of the given raw Message pointer.
	 *
	 * The message will be deleted when reference count falls to zero.
	 * @param msg
	 */
	explicit MessagePtr(Message *msg)
		: m_ptr(msg)
	{
		if(m_ptr) {
			Q_ASSERT(m_ptr->m_refcount.load()==0);
			m_ptr->m_refcount.ref();
		}
	}

	MessagePtr(const MessagePtr &ptr) : m_ptr(ptr.m_ptr) {
		if(m_ptr)
			m_ptr->m_refcount.ref();
	}

	~MessagePtr()
	{
		if(m_ptr && !m_ptr->m_refcount.deref())
			delete m_ptr;
	}

	MessagePtr &operator=(const MessagePtr &msg)
	{
		if(msg.m_ptr != m_ptr) {
			if(m_ptr && !m_ptr->m_refcount.deref())
				delete m_ptr;

			m_ptr = msg.m_ptr;

			if(m_ptr)
				m_ptr->m_refcount.ref();
		}
		return *this;
	}

	Message &operator*() const { return *m_ptr; }
	Message *operator ->() const { return m_ptr; }

	template<class msgtype> msgtype &cast() const { return static_cast<msgtype&>(*m_ptr); }

	bool isNull() const { return m_ptr == nullptr; }

	bool equals(const MessagePtr &m) const {
		if(isNull() || m.isNull())
			return false;
		return m_ptr->equals(*m);
	}

private:
	Message *m_ptr;
};

void registerTypes();

}

Q_DECLARE_TYPEINFO(protocol::MessagePtr, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(protocol::MessagePtr)

#endif
