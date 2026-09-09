#include "CommandSink.hpp"
#include "Collab/EditLock.hpp"
#include "Collab/SyncSnapshot.hpp"
#include "Collab/SyncRegistry.hpp"
#include "Asset/Object.hpp"
#include "Generated/Scripts.hpp" // save_id_find, null_

namespace CppProject
{
	CommandSink* CommandSink::active = nullptr;

	void CommandSink::Tick()
	{
		outgoing += SyncSnapshot::CaptureTick();
	}

	QVector<Command> CommandSink::DrainOutgoing()
	{
		QVector<Command> drained = outgoing;
		outgoing.clear();
		return drained;
	}

	void CommandSink::ApplyRemoteCommand(const Command& command)
	{
		// Defensive: ignore a remote command targeting a member we don't recognize as syncable.
		if (!SyncRegistry::IsSyncable(command.subAssetId, command.memberId))
		{
			WARNING("Collab: dropped remote command - member not marked syncable (subAsset=" +
				NumStr(command.subAssetId) + " member=" + NumStr(command.memberId) + ")");
			return;
		}

		// command.instanceId is the SENDER's own local Asset::id - meaningless here (Asset::id
		// depends on in-process allocation order, not stable across processes even for the
		// identical loaded project). Resolve the real target via the instance's stable save_id
		// instead (Generated/Scripts54.cpp's save_id_find - the same function GML itself uses to
		// find an instance by its saved identity).
		IntType instanceId = VarGetInt(save_id_find(command.saveId));
		if (instanceId == null_)
		{
			WARNING("Collab: dropped remote command - save_id not found locally (save_id=" +
				command.saveId.ToStr() + ")");
			return;
		}

		Object* obj = FindAssetOpt(Object, instanceId);
		if (!obj)
		{
			WARNING("Collab: dropped remote command - save_id resolved to id=" + NumStr(instanceId) +
				" but no local instance exists there");
			return;
		}
		if (obj->subAssetId != command.subAssetId)
		{
			WARNING("Collab: dropped remote command - subAsset mismatch (local=" + NumStr(obj->subAssetId) +
				" remote=" + NumStr(command.subAssetId) + ")");
			return;
		}

		// Edit lock: an instance currently locked by some OTHER peer rejects everyone else's
		// edits at this data level - there's no way to block the conflicting edit's originating
		// click (see .senior-mode/plans/2026-09-10-edit-lock-presence.md's Step 1 findings), so
		// this is what actually keeps the lock holder's work from being overwritten. Checked using
		// the locally-resolved instanceId - EditLock is always keyed in local id space.
		if (EditLock::IsLockedByOther(instanceId, command.peerId))
		{
			DEBUG("Collab: dropped remote command - instance=" + NumStr(instanceId) +
				" is locked by another peer");
			return;
		}

		DEBUG("Collab: applying remote command instance=" + NumStr(instanceId) +
			" member=" + NumStr(command.memberId) + " value=" + command.value.ToStr());

		obj->SetValue(command.memberId, command.value);
		SyncSnapshot::RecordKnownValue(instanceId, command.memberId, command.value);
	}
}
