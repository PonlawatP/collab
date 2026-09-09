#include "Presence.hpp"

namespace CppProject
{
	QHash<IntType, Presence::Peer> Presence::peers;

	void Presence::Set(IntType peerId, const QString& name, const VecType& position)
	{
		Peer& peer = peers[peerId];
		peer.name = name;
		peer.position = position;
	}

	void Presence::Remove(IntType peerId)
	{
		peers.remove(peerId);
	}

	QHash<IntType, Presence::Peer> Presence::GetRemote(IntType localPeerId)
	{
		QHash<IntType, Peer> remote;
		for (auto it = peers.constBegin(); it != peers.constEnd(); ++it)
			if (it.key() != localPeerId)
				remote.insert(it.key(), it.value());

		return remote;
	}
}
