#pragma once
#include "Common.hpp"
#include "Collab/Command.hpp"
#include <QHash>
#include <QPair>
#include <QVector>

namespace CppProject
{
	// Syncs obj_keyframe CREATION/REMOVAL/VALUE-EDIT/MOVE on a timeline's keyframe_list. This is a
	// structural + per-instance concern - SyncSnapshot/SyncRegistry only ever handle per-member
	// VALUE changes on a fixed set of already-existing top-level instances, never an instance
	// appearing/disappearing, nor a dynamic list of instances that isn't known ahead of time (same
	// category of thing as "Option B: reconstruct the project via Command" from the original
	// co-working advise, deliberately not built at the time). See
	// .senior-mode/plans/2026-09-10-edit-lock-presence.md, "Keyframe-structure sync" section.
	//
	// obj_keyframe has no save_id of its own - save_id/save_id_find is reserved for top-level
	// project assets (timelines, materials, templates, resources), never per-timeline child
	// records (confirmed: grepped every DefineObjectMember(ID_obj_keyframe, ...) - no M_save_id).
	//
	// Identity scheme (revised after a real crash - see plan's real-world test findings): a
	// keyframe's WIRE identity is (peerId, sender's local keyframe instanceId) - reusing
	// Command::instanceId, normally "meaningless on the receiving side" for value-sync commands,
	// but perfectly fine here since KeyframeSync's own ApplyRemoteCommand branch never treats it
	// as a receiver-side lookup key directly, only as an opaque tag to find/create a matching
	// LOCAL keyframe via `remoteKeyframeMap`. This replaced an earlier position-keyed design
	// (delete-old-key + create-new-key to represent a move) that destroyed and recreated the
	// native obj_keyframe every single tick while a keyframe was being dragged, which crashed the
	// receiver (a stale `keyframe_current`/`keyframe_next` left pointing at a just-destroyed
	// instance, dereferenced via a REQUIRED lookup inside tl_update_values/tl_update_matrix -
	// "[FATAL ERROR] Invalid id 0 in Find:86"). The current scheme reassigns an EXISTING mapped
	// keyframe's position in place (ds_list_delete_value + tl_keyframe_add with its own existing
	// id, never destroying it) whenever it moves, so there is no churn and no dangling reference
	// during a drag - `command.saveId` now only needs to resolve the OWNER TIMELINE.
	struct KeyframeSync
	{
		// Diffs every obj_timeline instance the local peer actively holds the edit lock on
		// against its last-known keyframe state, and returns one Command per keyframe that
		// appeared, disappeared, or changed value/position since the previous call. Call once per
		// tick, same cadence as SyncSnapshot::CaptureTick(). No full-snapshot variant: a joining
		// client already gets every existing keyframe as part of the whole-project-file transfer
		// (.senior-mode/plans/2026-09-09-project-join-sync.md) - this only needs to replicate
		// INCREMENTAL structural/value changes made while the session is live.
		static QVector<Command> CaptureTick();

		// Applies a remote keyframe create/update/move/remove Command - resolves the owner
		// timeline via save_id_find, then either repositions/updates an already-tracked local
		// keyframe (via remoteKeyframeMap) or creates a new one (via tl_keyframe_add, reusing its
		// dedup/list-insert/timeline_length bookkeeping), or destroys one - then re-runs the same
		// tl_update_values/tl_update_matrix/tl_update_length/app_update_tl_edit tail every local
		// keyframe-list-mutating action already calls (e.g. action_tl_keyframes_create,
		// Generated/Scripts12.cpp:477-479) - without it the timeline's cached interpolation state
		// (keyframe_current/next/*_values) never gets recomputed, so scrubbing across the change
		// appears to do nothing.
		static void ApplyRemoteCommand(const Command& command);

		// Drops every (peerId, ...) mapping entry for a disconnected peer - call from the same
		// place EditLock::RemovePeer is already called (Collab/Session.cpp's OnSocketDisconnected)
		// so a reconnecting peer's fresh local instanceIds never collide with stale entries.
		static void RemovePeer(IntType peerId);

		// Sentinel memberId values distinguishing a structural Command from a normal per-member
		// value Command. Never collide with a real GML M_xxx id - every M_xxx id is >= 0 (highest
		// currently defined, grepped from Generated/Scripts.hpp, is 2791).
		static constexpr IntType CreatedMarker = -1001;
		static constexpr IntType RemovedMarker = -1002;

	private:
		struct KnownKeyframe
		{
			RealType position;
			VarType value;
		};

		// Local-only bookkeeping (SENDER side): owner timeline's local instanceId -> {local
		// keyframe instanceId -> last known position+value}. Purely for detecting
		// appear/disappear/change locally - never serialized.
		static QHash<IntType, QHash<IntType, KnownKeyframe>> lastKnownKeys;
		static IntType sequence;

		// RECEIVER-side bookkeeping: (originating peerId, sender's local keyframe instanceId) ->
		// OUR local keyframe instanceId. This is what lets a move/value-update find and reuse the
		// same local obj_keyframe instead of destroying and recreating it.
		static QHash<QPair<IntType, IntType>, IntType> remoteKeyframeMap;
	};
}
