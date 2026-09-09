#include "CommandSink.hpp"
#include "Collab/SyncSnapshot.hpp"
#include "Collab/SyncRegistry.hpp"
#include "Asset/Object.hpp"

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

		Object* obj = FindAssetOpt(Object, command.instanceId);
		if (!obj)
		{
			WARNING("Collab: dropped remote command - no local instance with id=" + NumStr(command.instanceId));
			return;
		}
		if (obj->subAssetId != command.subAssetId)
		{
			WARNING("Collab: dropped remote command - subAsset mismatch (local=" + NumStr(obj->subAssetId) +
				" remote=" + NumStr(command.subAssetId) + ")");
			return;
		}

		DEBUG("Collab: applying remote command instance=" + NumStr(command.instanceId) +
			" member=" + NumStr(command.memberId) + " value=" + command.value.ToStr());

		obj->SetValue(command.memberId, command.value);
		SyncSnapshot::RecordKnownValue(command.instanceId, command.memberId, command.value);
	}
}
