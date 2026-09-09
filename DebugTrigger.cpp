#include "DebugTrigger.hpp"
#include "Collab/Session.hpp"
#include "Collab/SyncSnapshot.hpp"
#include "Generated/GmlFunc.hpp" // keyboard_check_pressed, vk_f7, vk_f8
#include "Generated/Scripts.hpp" // M_cam_work_focus, global::_app

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
