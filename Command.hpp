#pragma once
#include "Common.hpp"
#include "Type/VarType.hpp"
#include <QDataStream>

namespace CppProject
{
	// A single member-write event, replicated across a co-working session. One Command always
	// represents "instance <instanceId> of sub-asset <subAssetId>, member <memberId>, now equals
	// <value>" — never a delta, so applying it is idempotent and order-independent per member
	// (last sequence wins).
	struct Command
	{
		IntType peerId = 0;     // originating peer/session id
		IntType instanceId = 0; // sender's OWN local Object instance id - meaningless on the
		                        // receiving side (Asset::id depends on in-process allocation
		                        // order, NOT stable across processes even for the identical
		                        // loaded project - confirmed by real cross-process id mismatches).
		                        // Kept only for the sender's own SyncSnapshot::lastKnown bookkeeping.
		VarType saveId;         // the instance's GML save_id (M_save_id) - the actual stable,
		                        // cross-process identifier. The receiver MUST resolve the target
		                        // instance via save_id_find(saveId), never via instanceId directly.
		IntType subAssetId = 0; // Object sub-asset type id (cross-checked against SyncRegistry)
		IntType memberId = 0;   // member id within the object (M_xxx)
		IntType sequence = 0;   // monotonically increasing per-peer sequence number
		VarType value;          // new value

		void Write(QDataStream& stream) const;
		static Command Read(QDataStream& stream);
	};

	// VarType has no existing wire format; these are shared by Command and any future co-working
	// message that needs to move a VarType across the network.
	void WriteVarType(QDataStream& stream, VarType value);
	VarType ReadVarType(QDataStream& stream);
}
