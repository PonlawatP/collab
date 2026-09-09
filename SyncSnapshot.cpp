#include "SyncSnapshot.hpp"
#include "Collab/SyncRegistry.hpp"
#include "Asset/Object.hpp"

namespace CppProject
{
	QHash<QPair<IntType, IntType>, VarType> SyncSnapshot::lastKnown;
	IntType SyncSnapshot::sequence = 0;
	IntType SyncSnapshot::localPeerId = 0;

	QVector<Command> SyncSnapshot::Walk(BoolType forceEmit)
	{
		QVector<Command> changes;

		for (IntType subAssetId : SyncRegistry::GetSyncableSubAssetIds())
		{
			const QSet<IntType>& members = SyncRegistry::GetSyncableMembers(subAssetId);

			for (IntType instanceId : Object::GetAll(subAssetId))
			{
				Object* obj = FindAssetOpt(Object, instanceId);
				if (!obj)
					continue; // instance list and live asset can be momentarily out of sync

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

					DEBUG("Collab: captured change instance=" + NumStr(instanceId) + " member=" + NumStr(memberId) +
						" value=" + value.ToStr() + (forceEmit ? " (snapshot)" : " (tick)"));

					Command cmd;
					cmd.peerId = localPeerId;
					cmd.instanceId = instanceId;
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
