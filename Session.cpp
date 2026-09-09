#include "Session.hpp"
#include "Collab/BuildFingerprint.generated.hpp"
#include "Collab/EditLock.hpp"
#include "Collab/Presence.hpp"
#include "Collab/ProjectTransfer.hpp"
#include "Collab/SyncSnapshot.hpp"
#include "Asset/Object.hpp"
#include "Generated/Scripts.hpp" // M_save_id, save_id_find, null_
#include <QDataStream>
#include <QHostAddress>

namespace CppProject
{
	enum FrameType : quint8
	{
		Frame_Hello = 0,         // client -> host: our build fingerprint
		Frame_HelloAccepted = 1, // host -> client: fingerprint matched, ready for Command traffic
		Frame_HelloRejected = 2, // host -> client: fingerprint mismatch (payload = reason), then disconnect
		Frame_Command = 3,
		Frame_Presence = 4,      // peerId (qint64) + position (3x double) - never touches Object
		Frame_ProjectData = 5,   // host -> client, sent BEFORE Frame_HelloAccepted: the host's whole
		                         // project file, so instanceIds in the Command snapshot that follows
		                         // actually resolve on the client
		Frame_EditLock = 6,      // saveId (VarType, NOT instanceId - see Collab/CommandSink.cpp for
		                         // why a raw instanceId means nothing cross-process) + peerId (qint64)
		                         // + locked (quint8 1/0) - never touches Object/Command
	};

	Session::~Session()
	{
		Close();
	}

	bool Session::Host(quint16 port)
	{
		Close();

		server = new QTcpServer(this);
		connect(server, &QTcpServer::newConnection, this, &Session::OnNewConnection);

		if (!server->listen(QHostAddress::Any, port))
		{
			WARNING("Session::Host failed to listen on port " + NumStr(port) + ": " + server->errorString());
			deleteAndReset(server);
			return false;
		}

		return true;
	}

	void Session::ConnectToHost(const QString& address, quint16 port)
	{
		Close();

		QTcpSocket* socket = new QTcpSocket(this);
		connect(socket, &QTcpSocket::connected, this, &Session::OnSocketConnected);
		connect(socket, &QTcpSocket::readyRead, this, &Session::OnReadyRead);
		connect(socket, &QTcpSocket::disconnected, this, &Session::OnSocketDisconnected);
		socket->connectToHost(address, port);
		sockets.append(socket);
	}

	void Session::Close()
	{
		for (QTcpSocket* socket : sockets)
		{
			socket->disconnectFromHost();
			socket->deleteLater();
		}
		sockets.clear();
		recvBuffers.clear();
		handshaked.clear();
		peerNames.clear();
		peerIdsBySocket.clear();

		if (server)
		{
			server->close();
			deleteAndReset(server);
		}
	}

	bool Session::IsConnected() const
	{
		if (IsHost())
			return !handshaked.isEmpty();

		return !sockets.isEmpty() && handshaked.contains(sockets.first());
	}

	void Session::OnSocketConnected()
	{
		QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
		if (!socket)
			return;

		// Client side: say hello with our build fingerprint + display name before any Command
		// traffic. The fingerprint is hex (never contains '\0'), so it's a safe separator.
		SendFrame(socket, Frame_Hello, QByteArray(COLLAB_BUILD_FINGERPRINT) + '\0' + localPeerName.toUtf8());
	}

	void Session::OnNewConnection()
	{
		while (server->hasPendingConnections())
		{
			QTcpSocket* socket = server->nextPendingConnection();
			connect(socket, &QTcpSocket::readyRead, this, &Session::OnReadyRead);
			connect(socket, &QTcpSocket::disconnected, this, &Session::OnSocketDisconnected);
			sockets.append(socket);
			// Host waits for the client's Frame_Hello before treating it as handshaked.
		}
	}

	void Session::OnSocketDisconnected()
	{
		QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
		if (!socket)
			return;

		sockets.removeAll(socket);
		recvBuffers.remove(socket);
		handshaked.remove(socket);
		peerNames.remove(socket);
		if (peerIdsBySocket.contains(socket))
		{
			IntType peerId = peerIdsBySocket.take(socket);
			Presence::Remove(peerId);
			EditLock::RemovePeer(peerId);
		}
		socket->deleteLater();
	}

	void Session::SendFrame(QTcpSocket* socket, quint8 type, const QByteArray& payload)
	{
		QByteArray frame;
		QDataStream frameStream(&frame, QIODevice::WriteOnly);
		frameStream << type << (quint32)payload.size();
		frame += payload;

		socket->write(frame);
	}

	void Session::SendCommand(QTcpSocket* socket, const Command& command)
	{
		QByteArray payload;
		QDataStream payloadStream(&payload, QIODevice::WriteOnly);
		command.Write(payloadStream);

		SendFrame(socket, Frame_Command, payload);
	}

	void Session::SendPresenceFrame(QTcpSocket* socket, IntType peerId, const VecType& position)
	{
		QByteArray payload;
		QDataStream payloadStream(&payload, QIODevice::WriteOnly);
		payloadStream << (qint64)peerId << position.x << position.y << position.z;

		SendFrame(socket, Frame_Presence, payload);
	}

	void Session::SendEditLockFrame(QTcpSocket* socket, const VarType& saveId, IntType peerId, bool locked)
	{
		QByteArray payload;
		QDataStream payloadStream(&payload, QIODevice::WriteOnly);
		WriteVarType(payloadStream, saveId);
		payloadStream << (qint64)peerId << (quint8)(locked ? 1 : 0);

		SendFrame(socket, Frame_EditLock, payload);
	}

	void Session::OnReadyRead()
	{
		QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
		if (!socket)
			return;

		recvBuffers[socket] += socket->readAll();
		ProcessBufferedFrames(socket);
	}

	void Session::ProcessBufferedFrames(QTcpSocket* socket)
	{
		QByteArray& buffer = recvBuffers[socket];
		const int headerSize = (int)(sizeof(quint8) + sizeof(quint32));

		while (true)
		{
			if (buffer.size() < headerSize)
				return;

			quint8 type = 0;
			quint32 length = 0;
			{
				QDataStream headerStream(buffer);
				headerStream >> type >> length;
			}

			if (buffer.size() < (int)(headerSize + length))
				return; // wait for the rest of the frame to arrive

			QByteArray payload = buffer.mid(headerSize, length);
			buffer.remove(0, headerSize + length);

			HandleFrame(socket, type, payload);
		}
	}

	void Session::HandleFrame(QTcpSocket* socket, quint8 type, const QByteArray& payload)
	{
		switch (type)
		{
			case Frame_Hello: // host side: a client just told us its build fingerprint + name
			{
				int sep = payload.indexOf('\0');
				QByteArray fingerprint = sep >= 0 ? payload.left(sep) : payload;
				QString peerName = sep >= 0 ? QString::fromUtf8(payload.mid(sep + 1)) : QString();

				if (fingerprint == QByteArray(COLLAB_BUILD_FINGERPRINT))
				{
					handshaked.insert(socket);
					peerNames[socket] = peerName;
					DEBUG("Collab: client handshaked, name=" + peerName);

					// Send the host's whole project BEFORE anything else, so the joining client is
					// on the same project - the Command snapshot below references instanceIds that
					// only resolve once this has been loaded (ProcessBufferedFrames handles each
					// frame synchronously and in order, so this is guaranteed to run first).
					QByteArray projectData = ProjectTransfer::CaptureCurrentProject();
					if (!projectData.isEmpty())
						SendFrame(socket, Frame_ProjectData, projectData);

					SendFrame(socket, Frame_HelloAccepted, localPeerName.toUtf8());

					// Late joiner: send it the current state of every syncable member so it
					// starts from the real state instead of an empty one, before any live Tick().
					for (const Command& command : SyncSnapshot::CaptureFullSnapshot())
						SendCommand(socket, command);
				}
				else
				{
					SendFrame(socket, Frame_HelloRejected,
						QByteArray("Build fingerprint mismatch - update to the same version and try again"));
					socket->disconnectFromHost();
				}
				break;
			}

			case Frame_HelloAccepted: // client side - payload is the host's display name
				handshaked.insert(socket);
				peerNames[socket] = QString::fromUtf8(payload);
				emit ClientHandshakeAccepted();
				break;

			case Frame_HelloRejected: // client side
				emit ClientHandshakeRejected(QString::fromUtf8(payload));
				socket->disconnectFromHost();
				break;

			case Frame_ProjectData: // client side - arrives before Frame_HelloAccepted
				DEBUG("Collab: received project data (" + NumStr(payload.size()) + " bytes), staged for next Tick()");
				// Do NOT call ProjectTransfer::LoadProject() here - see the field comment on
				// pendingProjectData (Session.hpp) for why loading a project from inside this
				// socket callback corrupted GL state and froze the app.
				pendingProjectData = payload;
				hasPendingProjectData = true;
				break;

			case Frame_Command:
			{
				if (!handshaked.contains(socket))
					break; // ignore Command traffic from a socket that hasn't completed the handshake

				QDataStream payloadStream(payload);
				Command command = Command::Read(payloadStream);
				sink.ApplyRemoteCommand(command);

				if (IsHost()) // relay to every other handshaked client
					for (QTcpSocket* other : sockets)
						if (other != socket && handshaked.contains(other))
							SendCommand(other, command);
				break;
			}

			case Frame_Presence:
			{
				if (!handshaked.contains(socket))
					break; // ignore presence traffic from a socket that hasn't completed the handshake

				qint64 peerId = 0;
				RealType x = 0.0, y = 0.0, z = 0.0;
				QDataStream payloadStream(payload);
				payloadStream >> peerId >> x >> y >> z;

				peerIdsBySocket[socket] = peerId;
				Presence::Set(peerId, peerNames.value(socket), VecType(x, y, z));

				if (IsHost()) // relay to every other handshaked client
					for (QTcpSocket* other : sockets)
						if (other != socket && handshaked.contains(other))
							SendPresenceFrame(other, peerId, VecType(x, y, z));
				break;
			}

			case Frame_EditLock:
			{
				if (!handshaked.contains(socket))
					break; // ignore lock traffic from a socket that hasn't completed the handshake

				QDataStream payloadStream(payload);
				VarType saveId = ReadVarType(payloadStream);
				qint64 peerId = 0;
				quint8 lockedByte = 0;
				payloadStream >> peerId >> lockedByte;

				peerIdsBySocket[socket] = peerId;

				// saveId is the sender's stable identifier - resolve it to OUR OWN local
				// instanceId the same way CommandSink::ApplyRemoteCommand does (a raw instanceId
				// would mean nothing here). EditLock is always keyed by local instanceId.
				IntType instanceId = VarGetInt(save_id_find(saveId));
				if (instanceId == null_)
				{
					WARNING("Collab: edit lock message's save_id not found locally (save_id=" +
						saveId.ToStr() + ")");
					break; // can't resolve it locally, so can't relay it meaningfully either
				}

				DEBUG("Collab: edit lock " + QString(lockedByte ? "acquired" : "released") +
					" instance=" + NumStr(instanceId) + " peerId=" + NumStr(peerId));
				if (lockedByte)
					EditLock::Set(instanceId, peerId, peerNames.value(socket));
				else
					EditLock::Clear(instanceId);

				if (IsHost()) // relay to every other handshaked client, using the ORIGINAL
				              // saveId - each of them must resolve it in their OWN id space
					for (QTcpSocket* other : sockets)
						if (other != socket && handshaked.contains(other))
							SendEditLockFrame(other, saveId, peerId, lockedByte != 0);
				break;
			}
		}
	}

	void Session::ApplyPendingProjectLoad()
	{
		if (!hasPendingProjectData)
			return;

		ProjectTransfer::LoadProject(pendingProjectData);
		pendingProjectData.clear();
		hasPendingProjectData = false;
	}

	void Session::Tick()
	{
		sink.Tick();

		QVector<Command> outgoing = sink.DrainOutgoing();
		if (outgoing.isEmpty())
			return;

		for (QTcpSocket* socket : sockets)
		{
			if (!handshaked.contains(socket))
				continue;

			for (const Command& command : outgoing)
				SendCommand(socket, command);
		}
	}

	void Session::SendPresence(const VecType& position)
	{
		IntType peerId = SyncSnapshot::localPeerId;
		Presence::Set(peerId, localPeerName, position);

		for (QTcpSocket* socket : sockets)
			if (handshaked.contains(socket))
				SendPresenceFrame(socket, peerId, position);
	}

	void Session::BroadcastEditLock(IntType instanceId, bool locked)
	{
		IntType peerId = SyncSnapshot::localPeerId;
		DEBUG("Collab: local edit lock " + QString(locked ? "acquired" : "released") +
			" instance=" + NumStr(instanceId));

		// Local EditLock storage is always keyed by OUR OWN local instanceId - no translation
		// needed here, only for what goes out over the wire (below).
		if (locked)
			EditLock::Set(instanceId, peerId, localPeerName);
		else
			EditLock::Clear(instanceId);

		Object* obj = FindAssetOpt(Object, instanceId);
		VarType saveId;
		if (!obj || !obj->TryGetValue(M_save_id, saveId))
		{
			WARNING("Collab: could not broadcast edit lock - instance=" + NumStr(instanceId) +
				" has no resolvable save_id");
			return;
		}

		for (QTcpSocket* socket : sockets)
			if (handshaked.contains(socket))
				SendEditLockFrame(socket, saveId, peerId, locked);
	}
}
