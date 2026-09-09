#include "SyncSnapshot.hpp"
#include "Collab/EditLock.hpp"
#include "Collab/SyncRegistry.hpp"
#include "Asset/Object.hpp"
#include "Generated/Scripts.hpp" // M_save_id

namespace CppProject
{
	QHash<QPair<IntType, IntType>, VarType> SyncSnapshot::lastKnown;
	IntType SyncSnapshot::sequence = 0;
	IntType SyncSnapshot::localPeerId = 0;

	QVector<Command> SyncSnapshot::Walk(BoolType forceEmit)
	{
		QVector<Command> changes;

		// While the timeline is actually playing back, a locked instance's matrix_render changes
		// every tick as a side effect of playback, not editing - EditLock::IsLockedByPeer alone
		// can't tell the two apart, since the lock stays held the whole time the object is simply
		// selected/focused. Still update lastKnown below (so nothing looks "changed" the instant
		// playback stops) but never emit a Command for it - otherwise the lock holder would
		// broadcast their own playback motion to every other peer even though nobody is editing.
		BoolType suppressPlaybackEmit = !forceEmit && global::_app && global::_app->timeline_playing > 0;

		for (IntType subAssetId : SyncRegistry::GetSyncableSubAssetIds())
		{
			const QSet<IntType>& members = SyncRegistry::GetSyncableMembers(subAssetId);

			for (IntType instanceId : Object::GetAll(subAssetId))
			{
				Object* obj = FindAssetOpt(Object, instanceId);
				if (!obj)
					continue; // instance list and live asset can be momentarily out of sync

				// Only broadcast an instance's state while WE actively hold its edit lock (i.e.
				// we are the one dragging/editing it right now). Without this, ANY change to a
				// syncable member - including one caused merely by playing/scrubbing the timeline,
				// which changes a timeline instance's rendered transform every frame without
				// anyone editing anything - would broadcast and make playback bleed across peers
				// instead of staying fully independent per client.
				if (!EditLock::IsLockedByPeer(instanceId, localPeerId))
					continue;

				// The receiver can NEVER resolve command.instanceId directly (Asset::id depends on
				// in-process allocation order, not stable across processes) - it must resolve via
				// save_id_find(saveId) instead, so every Command for this instance needs its
				// save_id attached. Skip the instance entirely if it doesn't have one - nothing
				// downstream could ever apply it anyway.
				VarType saveId;
				if (!obj->TryGetValue(M_save_id, saveId))
					continue;

				for (IntType memberId : members)
				{
					VarType value;
					if (!obj->TryGetValue(memberId, value))
						continue; // dynamic member never assigned on this instance - nothing to diff yet

					QPair<IntType, IntType> key(instanceId, memberId);
					auto it = lastKnown.find(key);
					BoolType unchanged = (it != lastKnown.end() && it.value() == value);
					if (unchanged && !forceEmit)
						continue;

					lastKnown[key] = value;

					if (suppressPlaybackEmit)
						continue; // tracked into lastKnown above, but playback-driven - not a real edit

					DEBUG("Collab: captured change instance=" + NumStr(instanceId) + " member=" + NumStr(memberId) +
						" value=" + value.ToStr() + (forceEmit ? " (snapshot)" : " (tick)"));

					Command cmd;
					cmd.peerId = localPeerId;
					cmd.instanceId = instanceId;
					cmd.saveId = saveId;
					cmd.subAssetId = subAssetId;
					cmd.memberId = memberId;
					cmd.sequence = ++sequence;
					cmd.value = value;
					changes.append(cmd);
				}
			}
		}

		return changes;
	}

	QVector<Command> SyncSnapshot::CaptureTick()
	{
		return Walk(false);
	}

	QVector<Command> SyncSnapshot::CaptureFullSnapshot()
	{
		return Walk(true);
	}

	void SyncSnapshot::RecordKnownValue(IntType instanceId, IntType memberId, const VarType& value)
	{
		lastKnown[QPair<IntType, IntType>(instanceId, memberId)] = value;
	}
}
