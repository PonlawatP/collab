#pragma once
#include "Common.hpp"
#include "Type/VecType.hpp"
#include <QPointF>

namespace CppProject
{
	// Projects a 3D world position into 2D viewport-pixel coordinates, using the LOCAL app's own
	// current work-camera - never the remote peer's, since we are projecting into OUR OWN view.
	// Built fresh every call from Object::TryGetValue reads; no matrix state is cached or reused
	// from the engine's own (transient, pass-scoped) GFX matrices.
	//
	// The view direction/up vector is an exact replication of the engine's own
	// render_update_camera (Generated/Scripts50.cpp:378-395), built from cam_work_from +
	// cam_work_angle_look_xy/z + cam_work_roll - an earlier version approximated this as a plain
	// LookAt(cam_work_from, cam_work_focus, up=(0,0,1)), which visibly drifted from the real
	// rendered view as soon as the camera moved (cam_work_focus is the orbit target, not
	// necessarily the same direction as the actual look/roll-adjusted view).
	struct PresenceRenderer
	{
		// viewportWidth/Height are the target GLWidget's own pixel size (the caller already has
		// these in scope at the draw site, e.g. GLWidget::width()/height()). Returns false
		// (leaving outPoint untouched) if the position is behind the camera and must not be drawn.
		static BoolType WorldToScreen(const VecType& worldPos, int viewportWidth, int viewportHeight,
			QPointF& outPoint);
	};
}
