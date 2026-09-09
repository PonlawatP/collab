#pragma once

namespace CppProject
{
	// TEMPORARY dev-only hook for manually end-to-end testing the co-working command-stream
	// protocol (.senior-mode/plans/2026-09-09-command-stream-protocol.md, Step 10) before real
	// host/join UI exists (Phase 2). Remove once that UI ships.
	//
	// F7 hosts a session on port 24689. F8 connects to 127.0.0.1:24689 as a client. Marks
	// app::cam_work_zoom_goal (the 3D workspace zoom TARGET - cam_work_zoom itself is a
	// per-frame-interpolated display value derived from this goal, so syncing the goal lets each
	// side's own existing smoothing animate its local zoom toward it) as the one syncable member.
	//
	// IMPORTANT: cam_work_zoom_goal is a PIPE-VERIFICATION vehicle only, not a real design
	// decision. In the actual product, each client's viewport/camera navigation (cam_work_*) must
	// stay independent/local per client - do not carry this member into the real syncable-member
	// set once object/timeline/keyframe sync work starts.
	struct DebugTrigger
	{
		// Call once per frame from AppHandler::timerEvent.
		static void Tick();
	};
}
