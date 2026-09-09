#pragma once
#include "Common.hpp"
#include <QByteArray>

namespace CppProject
{
	// Bridges the co-working Session transport to the engine's own project save/load - both
	// confirmed fully synchronous, both operating on the singleton `app`
	// (.senior-mode/plans/2026-09-09-project-join-sync.md's Step 1 findings). Used so a joining
	// client starts on the exact same project as the host, not whatever it happened to have open.
	struct ProjectTransfer
	{
		// Host: saves the CURRENT live project (not the on-disk file - captures unsaved edits) to
		// a temp path and returns its bytes, ready to send to a joining client. Empty on failure.
		static QByteArray CaptureCurrentProject();

		// Client: writes the received bytes to a temp path and loads them as the current project,
		// replacing whatever was open. No unsaved-changes confirmation - calling project_load
		// directly bypasses the toolbar action's own prompt, which is the MVP behavior wanted here
		// (joining silently replaces the client's project).
		static void LoadProject(const QByteArray& data);
	};
}
