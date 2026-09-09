#pragma once
#include "Common.hpp"
#include "Collab/CommandSink.hpp"
#include <QHash>
#include <QObject>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>
#include <QVector>

namespace CppProject
{
	// TCP host/client transport for a co-working session. Frames each message with a 1-byte type
	// tag and a 4-byte length prefix around its payload. The host acts as an authoritative relay:
	// every Command a client sends is applied locally, then rebroadcast to every OTHER connected
	// client (star topology). Before any Command traffic is accepted, each connection completes a
	// build-fingerprint handshake (Step 8) - a client whose build doesn't match the host's is
	// rejected and disconnected.
	struct Session : QObject
	{
		Q_OBJECT

	public:
		~Session();

		// Starts listening for clients on the given port. Returns false if the port couldn't be bound.
		bool Host(quint16 port);

		// Connects to a host at the given address/port as a client.
		void ConnectToHost(const QString& address, quint16 port);

		// Closes the session: host stops listening and disconnects all clients; client disconnects.
		void Close();

		bool IsHost() const { return server != nullptr; }

		// True once the build-fingerprint handshake has completed (host: at least one client
		// accepted; client: accepted by the host) - only handshaked connections exchange Commands.
		bool IsConnected() const;

		// Call once per frame/tick: captures local changes via the session's CommandSink and
		// sends them out (to all handshaked clients if hosting, to the host if a client).
		void Tick();

		// Our own display name, exchanged with the other side during the handshake. Hardcoded by
		// the caller for now (Collab/DebugTrigger sets "Host"/"Client") - a real name entry UI is
		// Phase 2. Must be set before Host()/ConnectToHost().
		QString localPeerName = "Peer";

		// Display name of the peer on the other end of a handshaked socket, or empty if unknown
		// (host: per connected client; client: the host's name, via its one socket).
		QString PeerName(QTcpSocket* socket) const { return peerNames.value(socket); }

		CommandSink sink;

	signals:
		// Client-side only: fires once the host accepts or rejects our build-fingerprint handshake.
		void ClientHandshakeAccepted();
		void ClientHandshakeRejected(QString reason);

	private slots:
		void OnSocketConnected();
		void OnNewConnection();
		void OnReadyRead();
		void OnSocketDisconnected();

	private:
		void SendFrame(QTcpSocket* socket, quint8 type, const QByteArray& payload);
		void SendCommand(QTcpSocket* socket, const Command& command);
		void ProcessBufferedFrames(QTcpSocket* socket);
		void HandleFrame(QTcpSocket* socket, quint8 type, const QByteArray& payload);

		QTcpServer* server = nullptr;
		QVector<QTcpSocket*> sockets; // host: one per connected client; client: the single socket to host
		QHash<QTcpSocket*, QByteArray> recvBuffers;
		QSet<QTcpSocket*> handshaked; // sockets that completed the build-fingerprint handshake
		QHash<QTcpSocket*, QString> peerNames; // handshaked socket -> the other side's display name
	};
}
