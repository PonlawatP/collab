#pragma once
#include "Common.hpp"
#include "Collab/Command.hpp"
#include <QVector>

namespace CppProject
{
	// One instance per active co-working session. Bridges SyncSnapshot's per-tick diff to the
	// network transport (Step 7) and applies incoming remote Commands to local instances.
	struct CommandSink
	{
		// Captures local changes via SyncSnapshot::CaptureTick() and queues them for send. Call
		// once per tick while this sink is active - Step 7 wires this into the app's frame loop.
		void Tick();

		// Removes and returns every locally-generated Command queued for send since the last call.
		QVector<Command> DrainOutgoing();

		// Applies an incoming remote Command: writes the value into the target instance via
		// Object::SetValue, and records it as already-known in SyncSnapshot so the very next
		// Tick() does not see it as a new local change and re-broadcast it back out (loop-safe by
		// construction - no separate suppression flag needed).
		void ApplyRemoteCommand(const Command& command);

		// The sink for the currently active co-working session, or nullptr if none is running.
		static CommandSink* active;

	private:
		QVector<Command> outgoing;
	};
}
