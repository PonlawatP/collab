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
		IntType instanceId = 0; // target Object instance id
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
