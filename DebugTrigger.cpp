#include "DebugTrigger.hpp"
#include "Collab/Session.hpp"
#include "Collab/SyncRegistry.hpp"
#include "Collab/SyncSnapshot.hpp"
#include "Generated/GmlFunc.hpp" // keyboard_check_pressed, vk_f7, vk_f8
#include "Generated/Scripts.hpp" // M_cam_work_focus, ID_obj_timeline, M_world_pos, global::_app, global::tl_edit, null_

namespace CppProject
{
	static Session* debugSession = nullptr;

	void DebugTrigger::RenderTick()
	{
		if (debugSession)
			debugSession->ApplyPendingProjectLoad();
	}

	void DebugTrigger::Tick()
	{
		// NOTE: cam_work_zoom_goal is deliberately NOT marked syncable (anymore). It was only ever
		// a pipe-verification vehicle for the command-stream protocol test (see
		// .senior-mode/plans/2026-09-09-command-stream-protocol.md) - viewport/camera navigation
		// must stay independent per client. Presence (Collab/Presence, floating peer name) is the
		// real mechanism for "where is the other person" now.

		static BoolType syncableRegistered = false;
		if (!syncableRegistered)
		{
			// obj_timeline::world_pos is a live member - read directly by render_world_tl for
			// rendering, not re-derived from keyframes at render time (confirmed via
			// .senior-mode/plans/2026-09-10-edit-lock-presence.md's Step 1 findings) - so it's
			// safe to sync the same way M_cam_work_zoom_goal was already proven to work.
			SyncRegistry::MarkSyncable(ID_obj_timeline, M_world_pos);
			syncableRegistered = true;
		}

		if (!debugSession && keyboard_check_pressed(vk_f7))
		{
			debugSession = new Session();
			debugSession->localPeerName = "Host"; // TODO Phase 2: real name entry UI, hardcoded for now
			SyncSnapshot::localPeerId = 1;
			if (debugSession->Host(24689))
				DEBUG("Collab debug: hosting on port 24689 (F7), name=" + debugSession->localPeerName);
			else
				deleteAndReset(debugSession);
		}
		else if (!debugSession && keyboard_check_pressed(vk_f8))
		{
			debugSession = new Session();
			debugSession->localPeerName = "Client"; // TODO Phase 2: real name entry UI, hardcoded for now
			SyncSnapshot::localPeerId = 2;
			debugSession->ConnectToHost("127.0.0.1", 24689);
			DEBUG("Collab debug: connecting to 127.0.0.1:24689 (F8), name=" + debugSession->localPeerName);
		}

		if (debugSession)
		{
			debugSession->Tick();

			// Edit lock: poll global::tl_edit for a local selection change - no native push-based
			// hook exists (selection lives entirely inside Generated/, see
			// .senior-mode/plans/2026-09-10-edit-lock-presence.md's Step 1 findings) - and
			// broadcast acquire/release accordingly.
			static IntType lastKnownSelection = null_;
			if (global::tl_edit != lastKnownSelection)
			{
				if (lastKnownSelection != null_)
					debugSession->BroadcastEditLock(lastKnownSelection, false);
				if (global::tl_edit != null_)
					debugSession->BroadcastEditLock(global::tl_edit, true);
				lastKnownSelection = global::tl_edit;
			}

			// PAUSED (2026-09-09): presence broadcast turned off while
			// .senior-mode/plans/2026-09-09-project-join-sync.md is worked on - the floating
			// label's projection math was still visibly wrong and testing it meaningfully needs
			// both sides on the same project first anyway. Resume by uncommenting once that plan
			// lands - see .senior-mode/plans/2026-09-09-presence-indicator.md.
			// VarType focusValue;
			// if (global::_app && global::_app->TryGetValue(M_cam_work_focus, focusValue))
			// 	debugSession->SendPresence(VarGetVec(focusValue));
		}
	}
}
