#include "KeyframeSync.hpp"
#include "Collab/EditLock.hpp"
#include "Collab/SyncSnapshot.hpp"
#include "Asset/Object.hpp"
#include "Asset/DataStructure.hpp" // List
#include "Type/ArrType.hpp"
#include <QList>
#include "Generated/Scripts.hpp" // ID_obj_timeline, ID_obj_keyframe, M_save_id, M_keyframe_list,
                                  // M_position, M_value, tl_keyframe_add, tl_update_values,
                                  // tl_update_matrix, tl_update_length, app_update_tl_edit,
                                  // ds_list_delete_value, save_id_find, null_

namespace CppProject
{
	QHash<IntType, QHash<IntType, KeyframeSync::KnownKeyframe>> KeyframeSync::lastKnownKeys;
	IntType KeyframeSync::sequence = 0;
	QHash<QPair<IntType, IntType>, IntType> KeyframeSync::remoteKeyframeMap;

	QVector<Command> KeyframeSync::CaptureTick()
	{
		QVector<Command> changes;

		for (IntType timelineId : Object::GetAll(ID_obj_timeline))
		{
			// Same rule as SyncSnapshot: only broadcast structural/value changes on a timeline
			// the local peer actively holds the edit lock on - otherwise every peer who happens
			// to have the same timeline open would independently "discover" and rebroadcast the
			// same keyframe.
			if (!EditLock::IsLockedByPeer(timelineId, SyncSnapshot::localPeerId))
				continue;

			Object* tlObj = FindAssetOpt(Object, timelineId);
			if (!tlObj)
				continue;

			VarType timelineSaveId;
			if (!tlObj->TryGetValue(M_save_id, timelineSaveId))
				continue;

			VarType listHandleVar;
			if (!tlObj->TryGetValue(M_keyframe_list, listHandleVar))
				continue;

			List* list = FindAssetOpt(List, VarGetInt(listHandleVar));

			QHash<IntType, KnownKeyframe> currentKeys; // local keyframe instanceId -> position+value
			if (list)
			{
				for (IntType i = 0; i < list->vec.size(); i++)
				{
					IntType kfId = VarGetInt(list->Value(i));
					Object* kfObj = FindAssetOpt(Object, kfId);
					VarType posVar, valueVar;
					if (!kfObj || !kfObj->TryGetValue(M_position, posVar))
						continue;
					kfObj->TryGetValue(M_value, valueVar);
					currentKeys[kfId] = { VarGetReal(posVar), valueVar };
				}
			}

			// The FIRST time this timeline is observed (just locked for the first time this
			// session), every one of its pre-existing keyframes would otherwise look "new" and
			// get sent as a CreatedMarker - but every peer already has them from the
			// whole-project-file transfer at join time, so that would insert a full set of
			// DUPLICATE keyframes (tl_keyframe_add's dedup would just shift them to nearby
			// positions rather than reject them) on every other peer. Seed the baseline silently
			// instead - only genuine INCREMENTAL changes from this point on should broadcast.
			BoolType firstObservation = !lastKnownKeys.contains(timelineId);
			QHash<IntType, KnownKeyframe>& lastKeys = lastKnownKeys[timelineId];
			if (firstObservation)
			{
				lastKeys = currentKeys;
				continue;
			}

			// Removed: a keyframe instance we tracked before is gone now. Identity on the wire is
			// (peerId, this sender-local kfId) - the receiver never needs the position to find
			// its target, only to know WHICH of ITS mapped local keyframes to drop.
			for (auto it = lastKeys.begin(); it != lastKeys.end();)
			{
				if (currentKeys.contains(it.key()))
				{
					++it;
					continue;
				}

				Command cmd;
				cmd.peerId = SyncSnapshot::localPeerId;
				cmd.instanceId = it.key(); // sender-local keyframe id - the wire identity here
				cmd.saveId = timelineSaveId;
				cmd.subAssetId = ID_obj_keyframe;
				cmd.memberId = RemovedMarker;
				cmd.sequence = ++sequence;

				DEBUG("Collab: captured keyframe removed timeline=" + NumStr(timelineId) +
					" kf=" + NumStr(it.key()));

				changes.append(cmd);
				it = lastKeys.erase(it);
			}

			// Created or changed (position and/or value) since last tick - always addressed by
			// the SAME sender-local kfId, so a move is just "same id, new position in the
			// payload", not a separate remove+create round trip.
			for (auto it = currentKeys.constBegin(); it != currentKeys.constEnd(); ++it)
			{
				auto prevIt = lastKeys.constFind(it.key());
				BoolType isNew = (prevIt == lastKeys.constEnd());
				BoolType changed = isNew ||
					prevIt.value().position != it.value().position ||
					!(prevIt.value().value == it.value().value);
				if (!changed)
					continue;

				ArrType payload;
				payload.Append(VarType(it.value().position));
				payload.Append(it.value().value);

				Command cmd;
				cmd.peerId = SyncSnapshot::localPeerId;
				cmd.instanceId = it.key(); // sender-local keyframe id - the wire identity here
				cmd.saveId = timelineSaveId;
				cmd.subAssetId = ID_obj_keyframe;
				cmd.memberId = CreatedMarker;
				cmd.sequence = ++sequence;
				cmd.value = VarType(payload);

				DEBUG("Collab: captured keyframe " + QString(isNew ? "created" : "updated") +
					" timeline=" + NumStr(timelineId) + " kf=" + NumStr(it.key()) +
					" position=" + NumStr(it.value().position));

				changes.append(cmd);
				lastKeys[it.key()] = it.value();
			}
		}

		return changes;
	}

	// Re-runs the same tail every local action that mutates a timeline's keyframe_list already
	// calls (e.g. action_tl_keyframes_create/_remove, Generated/Scripts12.cpp:477-479/946-951) -
	// without it the timeline's cached interpolation state (keyframe_current/next/*_values, and
	// ultimately matrix_render) never gets recomputed, so scrubbing across the change on this
	// peer's own independent timeline appears to do nothing.
	static void RefreshTimelineAfterKeyframeChange(obj_timeline* tl)
	{
		Scope<obj_timeline> tlSelfScope(tl);
		ScopeAny tlScope = ScopeAny(tlSelfScope);
		tl_update_values(tlScope);
		tl_update_matrix(tlScope);
		tl_update_length();

		// app_update_tl_edit expects `self` bound to the APP/editor context (it reads UI-state
		// members like frame_editor/timeline_editor/select_kf_single that only exist on `app`),
		// never the timeline itself - every real call site (e.g. action_tl_keyframes_create,
		// Generated/Scripts12.cpp:479) passes through its OWN outer `self`, which is always the
        // editor context there, not an obj_timeline. Passing the timeline scope here instead (the
		// original bug) makes `sInt(frame_editor)` read a nonexistent member off obj_timeline,
		// which resolves to 0, and `withOne (obj_tab, sInt(frame_editor), ...)`
		// (Generated/Scripts19.cpp:228) then does a REQUIRED lookup for id=0 and hard-crashes -
		// "[FATAL ERROR] Invalid id 0 in Find:86" (Asset/Asset.hpp:86).
		app_update_tl_edit(ScopeAny(Scope<app>(global::_app)));
	}

	void KeyframeSync::ApplyRemoteCommand(const Command& command)
	{
		IntType timelineId = VarGetInt(save_id_find(command.saveId));
		if (timelineId == null_)
		{
			WARNING("Collab: dropped remote keyframe command - owner timeline save_id not found locally");
			return;
		}

		obj_timeline* tl = ObjTypeOpt(obj_timeline, timelineId);
		if (!tl)
			return;

		if (EditLock::IsLockedByOther(timelineId, command.peerId))
		{
			DEBUG("Collab: dropped remote keyframe command - timeline locked by another peer");
			return;
		}

		QPair<IntType, IntType> mapKey(command.peerId, command.instanceId);

		if (command.memberId == RemovedMarker)
		{
			auto it = remoteKeyframeMap.find(mapKey);
			if (it == remoteKeyframeMap.end())
				return; // never tracked, or already removed

			// keyframe_event_destroy (Generated/Scripts33.cpp:490), run from ~obj_keyframe() via
			// this delete, already removes the instance from its owner's keyframe_list itself -
			// same as instance_destroy()'s own "delete obj;" (Gml/AssetFunc.cpp:38).
			delete FindAssetOpt(Object, it.value());
			remoteKeyframeMap.erase(it);
			RefreshTimelineAfterKeyframeChange(tl);
			return;
		}

		if (command.memberId != CreatedMarker)
			return; // not a keyframe-structure command we recognize

		if (!command.value.IsArray() || command.value.Arr().Size() < 2)
		{
			WARNING("Collab: dropped remote keyframe-created command - malformed payload");
			return;
		}
		RealType position = VarGetReal(command.value.Arr().Value(0));
		VarType kfValue = command.value.Arr().Value(1);

		auto mapIt = remoteKeyframeMap.find(mapKey);
		if (mapIt != remoteKeyframeMap.end())
		{
			Object* kfObj = FindAssetOpt(Object, mapIt.value());
			if (kfObj)
			{
				// Already tracked - reposition IN PLACE if it moved (never destroy/recreate: an
				// earlier position-keyed design did exactly that on every tick of a drag, which
				// crashed the receiver - see this file's header comment) and update its value.
				VarType curPosVar;
				RealType curPos = kfObj->TryGetValue(M_position, curPosVar) ? VarGetReal(curPosVar) : position;
				if (curPos != position)
				{
					VarType listHandleVar;
					if (tl->TryGetValue(M_keyframe_list, listHandleVar))
					{
						ds_list_delete_value(VarGetInt(listHandleVar), VarType(mapIt.value()));
						tl_keyframe_add(ScopeAny(Scope<obj_timeline>(tl)), VarType(position), mapIt.value());

						// tl_keyframe_add's own dedup logic (Generated/Scripts66.cpp:998-1013)
						// silently SHIFTS the requested position forward if this receiver's
						// timeline already has a DIFFERENT keyframe sitting exactly there - which
						// can only happen if this peer's keyframe set has drifted from the
						// sender's (e.g. a keyframe it created/moved locally, not yet observed by
						// the sender). Log it - a shift here means the two sides now disagree on
						// where this keyframe actually is.
						VarType landedPosVar;
						RealType landedPos = kfObj->TryGetValue(M_position, landedPosVar) ? VarGetReal(landedPosVar) : position;
						if (landedPos != position)
							WARNING("Collab: keyframe reposition landed at " + NumStr(landedPos) +
								" instead of requested " + NumStr(position) +
								" (local dedup-shift - kf=" + NumStr(mapIt.value()) + ")");
					}
				}
				DEBUG("Collab: applying remote keyframe updated timeline=" + NumStr(timelineId) +
					" position=" + NumStr(position) + " local kf=" + NumStr(mapIt.value()));
				kfObj->SetValue(M_value, kfValue);
				RefreshTimelineAfterKeyframeChange(tl);
				return;
			}
			// Mapped local keyframe vanished some other way (e.g. locally deleted) - fall through
			// and recreate it below, replacing the stale mapping.
			remoteKeyframeMap.erase(mapIt);
		}

		// Not tracked under this wire identity yet - but BOTH peers started from the identical
		// project file at join time (.senior-mode/plans/2026-09-09-project-join-sync.md), so any
		// keyframe that already existed before this collab session ever synced it (a "baseline"
		// keyframe) sits at the SAME position on every peer's copy, under NO mapping at all - it
		// was seeded into the SENDER's own lastKnownKeys silently on first observation
        // (this file's "New known gap" section above) and never given a wire identity. The first
		// time such a keyframe is actually edited and its Command arrives here, blindly creating
		// a new local obj_keyframe would DUPLICATE it right next to the original (confirmed by a
		// real test: server warned "landed at 6 instead of requested 5" - the position-5 create
		// collided with the timeline's own pre-existing position-5 keyframe and got dedup-shifted
		// instead of updating it). Adopt an existing UNMAPPED local keyframe at the exact target
		// position instead of creating a new one, whenever one exists.
		IntType adoptedKfId = null_;
		{
			VarType listHandleVar;
			List* list = tl->TryGetValue(M_keyframe_list, listHandleVar) ? FindAssetOpt(List, VarGetInt(listHandleVar)) : nullptr;
			if (list)
			{
				QList<IntType> claimedIds = remoteKeyframeMap.values();
				for (IntType i = 0; i < list->vec.size(); i++)
				{
					IntType candidateId = VarGetInt(list->Value(i));
					if (claimedIds.contains(candidateId))
						continue; // already the mapped target of some other wire identity
					Object* candidateObj = FindAssetOpt(Object, candidateId);
					VarType candidatePosVar;
					if (candidateObj && candidateObj->TryGetValue(M_position, candidatePosVar) &&
						VarGetReal(candidatePosVar) == position)
					{
						adoptedKfId = candidateId;
						break;
					}
				}
			}
		}

		if (adoptedKfId != null_)
		{
			DEBUG("Collab: adopting existing local keyframe timeline=" + NumStr(timelineId) +
				" position=" + NumStr(position) + " local kf=" + NumStr(adoptedKfId) +
				" as the target of this wire identity (baseline keyframe, never synced before)");
			remoteKeyframeMap[mapKey] = adoptedKfId;
			FindAssetOpt(Object, adoptedKfId)->SetValue(M_value, kfValue);
			RefreshTimelineAfterKeyframeChange(tl);
			return;
		}

		// Genuinely brand new. Create first and set its value BEFORE calling tl_keyframe_add with
		// an explicit kf id - this mirrors the engine's OWN "already-built keyframe" call pattern
		// (Generated/Scripts12.cpp:670-698: `(new obj_keyframe)->id` populated, then
		// `tl_keyframe_add(self, pos, newkf)`), which skips tl_keyframe_add's kf<0 branch that
		// would otherwise seed the value from the LOCAL timeline's own current live value instead
		// of the value actually received from the remote peer.
		Object* kfObj = new obj_keyframe();
		kfObj->SetValue(M_value, kfValue);

		DEBUG("Collab: applying remote keyframe created timeline=" + NumStr(timelineId) +
			" position=" + NumStr(position) + " new local kf=" + NumStr(kfObj->id));

		tl_keyframe_add(ScopeAny(Scope<obj_timeline>(tl)), VarType(position), kfObj->id);
		remoteKeyframeMap[mapKey] = kfObj->id;
		RefreshTimelineAfterKeyframeChange(tl);
	}

	void KeyframeSync::RemovePeer(IntType peerId)
	{
		for (auto it = remoteKeyframeMap.begin(); it != remoteKeyframeMap.end();)
		{
			if (it.key().first == peerId)
				it = remoteKeyframeMap.erase(it);
			else
				++it;
		}
	}
}
