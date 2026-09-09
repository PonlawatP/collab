#pragma once
#include "Common.hpp"
#include <QSet>

namespace CppProject
{
	// Tracks which Object members are opted into co-working sync. Nothing is synced by default —
	// a member only gets broadcast as a Command once explicitly marked syncable here.
	struct SyncRegistry
	{
		// Marks a member as syncable for a given object sub-asset type. Safe to call multiple times.
		static void MarkSyncable(IntType subAssetId, IntType memberId);

		// Returns whether a member on a given object sub-asset type is syncable.
		static bool IsSyncable(IntType subAssetId, IntType memberId);

		// Returns all syncable member ids registered for a given object sub-asset type.
		static const QSet<IntType>& GetSyncableMembers(IntType subAssetId);

		// Returns every sub-asset type id that has at least one syncable member registered - used
		// by SyncSnapshot to know which object types to visit each tick without scanning all of them.
		static QList<IntType> GetSyncableSubAssetIds();

	private:
		static QHash<IntType, QSet<IntType>> syncableMembers; // subAssetId -> set of syncable memberIds
	};
}
