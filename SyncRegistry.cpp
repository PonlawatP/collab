#include "SyncRegistry.hpp"

namespace CppProject
{
	QHash<IntType, QSet<IntType>> SyncRegistry::syncableMembers;

	void SyncRegistry::MarkSyncable(IntType subAssetId, IntType memberId)
	{
		syncableMembers[subAssetId].insert(memberId);
	}

	bool SyncRegistry::IsSyncable(IntType subAssetId, IntType memberId)
	{
		auto it = syncableMembers.constFind(subAssetId);
		return it != syncableMembers.constEnd() && it->contains(memberId);
	}

	const QSet<IntType>& SyncRegistry::GetSyncableMembers(IntType subAssetId)
	{
		static const QSet<IntType> empty;
		auto it = syncableMembers.constFind(subAssetId);
		return it != syncableMembers.constEnd() ? it.value() : empty;
	}

	QList<IntType> SyncRegistry::GetSyncableSubAssetIds()
	{
		return syncableMembers.keys();
	}
}
