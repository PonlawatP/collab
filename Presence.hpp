#pragma once
#include "Common.hpp"
#include "Type/VecType.hpp"
#include <QHash>
#include <QString>

namespace CppProject
{
	// Per-peer presence state (a display name + a 3D point, e.g. the camera focus each peer is
	// looking at) for the co-working session. Deliberately NOT routed through
	// Object/SyncRegistry/CommandSink - the engine's `app` object is a process-wide singleton, so
	// applying a remote peer's camera position via Object::SetValue would silently overwrite the
	// local user's own camera state. This is a small, parallel, Object-free registry instead.
	struct Presence
	{
		struct Peer
		{
			QString name;
			VecType position;
		};

		// Records/updates a peer's current name+position - used both for our own local presence
		// (peerId = SyncSnapshot::localPeerId) and for a peer's presence received over the network.
		static void Set(IntType peerId, const QString& name, const VecType& position);

		// Removes a peer (e.g. on disconnect).
		static void Remove(IntType peerId);

		// Every known peer except localPeerId - what should be drawn as a floating label.
		static QHash<IntType, Peer> GetRemote(IntType localPeerId);

	private:
		static QHash<IntType, Peer> peers;
	};
}
