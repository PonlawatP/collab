#pragma once
#include "Common.hpp"
#include <QHash>
#include <QString>

namespace CppProject
{
	// Tracks which peer currently holds the "edit lock" on which obj_timeline instance. A visual
	// deterrent + data-level guard, NOT a hard input block - no native choke point exists to
	// block a click (.senior-mode/plans/2026-09-10-edit-lock-presence.md's Step 1 findings: all
	// click-to-select hit-testing lives inside Generated/). Deliberately NOT routed through
	// Object/SyncRegistry - this is per-peer ephemeral UI state, not project data (same reasoning
	// as Collab/Presence).
	struct EditLock
	{
		struct Lock
		{
			IntType peerId;
			QString peerName;
		};

		// Records/updates who holds the lock on an instance - used both for our own local
		// selection and for a lock message received over the network.
		static void Set(IntType instanceId, IntType peerId, const QString& peerName);
		static void Clear(IntType instanceId);

		// Removes every lock held by a peer (e.g. on disconnect).
		static void RemovePeer(IntType peerId);

		// True if instanceId is locked by some peer OTHER than peerId - used by
		// CommandSink::ApplyRemoteCommand to reject a conflicting edit instead of applying it.
		static bool IsLockedByOther(IntType instanceId, IntType peerId);

		static QHash<IntType, Lock> GetAll();

	private:
		static QHash<IntType, Lock> locks; // instanceId -> current lock holder
	};
}
