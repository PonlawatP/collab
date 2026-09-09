#pragma once

namespace CppProject
{
	// TEMPORARY dev-only hook for manually end-to-end testing the co-working command-stream
	// protocol (.senior-mode/plans/2026-09-09-command-stream-protocol.md, Step 10) before real
	// host/join UI exists (Phase 2). Remove once that UI ships.
	//
	// F7 hosts a session on port 24689. F8 connects to 127.0.0.1:24689 as a client. See
	// .senior-mode/plans/2026-09-09-project-join-sync.md - a joining client now receives the
	// host's whole project over the connection.
	struct DebugTrigger
	{
		// Call once per frame from AppHandler::timerEvent, BEFORE the per-window render loop.
		// Drives Command/Presence sync - nothing here needs a bound GFX surface/shader.
		static void Tick();

		// Call once per frame from INSIDE AppHandler::timerEvent's per-window loop, after that
		// window's GFX surface/shader are bound (see Session::ApplyPendingProjectLoad's comment
		// for why this can't run from Tick() above). A no-op unless a project was just received.
		static void RenderTick();
	};
}
