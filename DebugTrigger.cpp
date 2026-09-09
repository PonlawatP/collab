#include "DebugTrigger.hpp"
#include "Collab/Session.hpp"
#include "Collab/SyncRegistry.hpp"
#include "Collab/SyncSnapshot.hpp"
#include "Generated/GmlFunc.hpp" // keyboard_check_pressed, vk_f7, vk_f8
#include "Generated/Scripts.hpp" // ID_app, M_cam_work_zoom, global::_app

namespace CppProject
{
	static Session* debugSession = nullptr;

	void DebugTrigger::Tick()
	{
		static BoolType syncableRegistered = false;
		if (!syncableRegistered)
		{
			// cam_work_zoom itself is re-derived every frame by app_update_work_camera() (it lerps
			// toward cam_work_zoom_goal - see Generated/Scripts19.cpp), so syncing it directly
			// fights that local per-frame interpolation and gets immediately overwritten on the
			// receiving side. Sync the GOAL instead - each side's own existing smoothing then
			// animates its local cam_work_zoom toward it naturally.
			SyncRegistry::MarkSyncable(ID_app, M_cam_work_zoom_goal);
			syncableRegistered = true;

			if (global::_app)
			{
				VarType value;
				BoolType found = global::_app->TryGetValue(M_cam_work_zoom_goal, value);
				DEBUG("Collab debug: app instance id=" + NumStr(global::_app->id) +
					" subAssetId=" + NumStr(global::_app->subAssetId) +
					" cam_work_zoom_goal found=" + QString(found ? "true" : "false") +
					" value=" + value.ToStr());
			}
			else
			{
				WARNING("Collab debug: global::_app is null at registration time");
			}
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
			debugSession->Tick();
	}
}
