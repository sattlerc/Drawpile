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
#ifndef DP_NET_CTRL_H
#define DP_NET_CTRL_H

#include "message.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

namespace protocol {

/**
 * @brief A command sent to the server using the Command message
 */
struct ServerCommand {
	QString cmd;
	QJsonArray args;
	QJsonObject kwargs;

	static ServerCommand fromJson(const QJsonDocument &doc);
	QJsonDocument toJson() const;
};

/**
 * @brief A reply or notification from the server received with the Command message
 */
struct ServerReply {
	enum {
		UNKNOWN,
		LOGIN,
		MESSAGE,
		CHAT,
		ALERT,
		ERROR,
		RESULT,
		SESSIONCONF,
		RESET
	} type;
	QString message;
	QJsonObject reply;

	static ServerReply fromJson(const QJsonDocument &doc);
	QJsonDocument toJson() const;
};

/**
 * @brief Server command message
 *
 * This is a general purpose message for sending commands to the server
 * and receiving replies. This is used for (among other things):
 * - the login handshake
 * - setting session parameters (e.g. max user count and password)
 * - sending administration commands (e.g. kick user)
 */
class Command : public Message {
public:
	Command(uint8_t ctx, const QByteArray &msg) : Message(MSG_COMMAND, ctx), m_msg(msg) {}
	Command(uint8_t ctx, const QJsonDocument &doc) : Command(ctx, doc.toJson(QJsonDocument::Compact)) {}
	template<typename T> Command(uint8_t ctx, const T &t) : Command(ctx, t.toJson()) { }

	static Command *deserialize(uint8_t ctxid, const uchar *data, uint len);

	//! Convenience function: make an ERROR type reply message
	static MessagePtr error(const QString &message);

	QJsonDocument doc() const;
	ServerCommand cmd() const { return ServerCommand::fromJson(doc()); }
	ServerReply reply() const { return ServerReply::fromJson(doc()); }

protected:
    int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	QByteArray m_msg;
};

/**
 * @brief Disconnect notification
 *
 * This message is used when closing the connection gracefully. The message queue
 * will automatically close the socket after sending this message.
 */
class Disconnect : public Message {
public:
	enum Reason {
		ERROR,    // client/server error
		KICK,     // user kicked by session operator
		SHUTDOWN, // client/server closed
		OTHER     // other unspecified error
	};

	Disconnect(uint8_t ctx, Reason reason, const QString &message) : Message(MSG_DISCONNECT, ctx),
		_reason(reason), _message(message.toUtf8()) { }

	static Disconnect *deserialize(uint8_t ctx, const uchar *data, uint len);

	/**
	 * Get the reason for the disconnection
	 */
	Reason reason() const { return _reason; }

	/**
	 * Get the disconnect message
	 *
	 * When reason is KICK, this is the name of the operator who kicked this user.
	 */
	QString message() const { return QString::fromUtf8(_message); }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	Reason _reason;
	QByteArray _message;
};

/**
 * @brief Download progress message
 *
 * The StreamPos message is used to inform the client of roughly how many bytes
 * of data to expect. The client can use this to show a download progress bar.
 */
class StreamPos : public Message {
public:
	StreamPos(uint8_t ctx, uint32_t bytes) : Message(MSG_STREAMPOS, ctx),
		_bytes(bytes) {}

	static StreamPos *deserialize(uint8_t ctx, const uchar *data, uint len);

	/**
	 * @brief Number of bytes behind the current stream head
	 * @return byte count
	 */
	uint32_t bytes() const { return _bytes; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint32_t _bytes;
};

/**
 * @brief Ping message
 *
 * This is used for latency measurement as well as a keepalive. Normally, the client
 * should be the one to send the ping messages.
 *
 * The server should return with a Ping with the pong message setenv()
 */
class Ping : public Message {
public:
	Ping(uint8_t ctx, bool pong) : Message(MSG_PING, ctx), _isPong(pong) { }

	static Ping *deserialize(uint8_t ctx, const uchar *data, int len);

	bool isPong() const { return _isPong; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	bool _isPong;
};

}

#endif
