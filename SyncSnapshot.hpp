#pragma once
#include "Common.hpp"
#include "Collab/Command.hpp"
#include <QHash>
#include <QPair>
#include <QVector>

namespace CppProject
{
	// Once per tick, diffs every syncable member of every live instance against its last known
	// value and produces a Command for each real change. Doesn't care how the write physically
	// happened (a proxy, a raw pointer, whatever) - it only looks at current vs. last-known state,
	// so it works uniformly across every member-write pattern the codebase actually uses.
	struct SyncSnapshot
	{
		// Call once per frame/tick while a co-working session is active. Returns one Command per
		// member whose value changed since the previous call.
		static QVector<Command> CaptureTick();

		// For a client that just joined: every syncable member's CURRENT value, regardless of
		// whether it changed recently - so a late joiner starts from the real state instead of an
		// empty one. Also seeds lastKnown with these values so the next CaptureTick() doesn't
		// immediately re-detect and re-send them as "changes".
		static QVector<Command> CaptureFullSnapshot();

		// Used by CommandSink when applying a remote Command: records the value as already known
		// so the next CaptureTick() does not mistake it for a new local change and re-broadcast it.
		static void RecordKnownValue(IntType instanceId, IntType memberId, const VarType& value);

		static IntType localPeerId; // stamped on every Command CaptureTick()/CaptureFullSnapshot() produces

	private:
		// Shared walk over every syncable member of every live instance. forceEmit=false (Tick)
		// only emits a Command where the value differs from lastKnown; forceEmit=true (full
		// snapshot) emits one for every existing value regardless.
		static QVector<Command> Walk(BoolType forceEmit);

		static QHash<QPair<IntType, IntType>, VarType> lastKnown; // (instanceId, memberId) -> last seen value
		static IntType sequence;
	};
}
