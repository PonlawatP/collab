#include "PresenceRenderer.hpp"
#include "Asset/Object.hpp"
#include "Generated/Scripts.hpp" // M_cam_work_from/angle_look_xy/angle_look_z/roll, global::_app
#include "Render/Matrix.hpp"
#include <cmath>

namespace CppProject
{
	namespace
	{
		// Exact copies of the native lengthdir_x/y/z (Gml/MathFunc.cpp, Generated/Scripts33.cpp) -
		// needed here because render_update_camera's view-direction math (below) is built from them.
		RealType LengthDirX(RealType len, RealType dir) { return len * std::cos(DEGTORAD * dir); }
		RealType LengthDirY(RealType len, RealType dir) { return -len * std::sin(DEGTORAD * dir); }
		RealType LengthDirZ(RealType len, RealType dir) { return len * std::sin(DEGTORAD * dir); }
	}

	BoolType PresenceRenderer::WorldToScreen(const VecType& worldPos, int viewportWidth, int viewportHeight,
		QPointF& outPoint)
	{
		if (!global::_app || viewportWidth <= 0 || viewportHeight <= 0)
			return false;

		VarType fromValue, angleLookXYValue, angleLookZValue, rollValue;
		if (!global::_app->TryGetValue(M_cam_work_from, fromValue) ||
			!global::_app->TryGetValue(M_cam_work_angle_look_xy, angleLookXYValue) ||
			!global::_app->TryGetValue(M_cam_work_angle_look_z, angleLookZValue) ||
			!global::_app->TryGetValue(M_cam_work_roll, rollValue))
			return false;

		// Exact replication of render_update_camera (Generated/Scripts50.cpp:378-395) - the actual
		// work-camera view direction/up vector, not the LookAt(from, focus) approximation this
		// started with (which visibly drifted from the real rendered view as the camera moved).
		VecType from = VarGetVec(fromValue);
		RealType angleLookXY = VarGetReal(angleLookXYValue);
		RealType angleLookZ = VarGetReal(angleLookZValue);
		RealType roll = VarGetReal(rollValue);

		VecType to;
		to.x = from.x + LengthDirX(1.0, angleLookXY + 180.0) * LengthDirX(1.0, angleLookZ);
		to.y = from.y + LengthDirY(1.0, angleLookXY + 180.0) * LengthDirX(1.0, angleLookZ);
		to.z = from.z + LengthDirZ(1.0, angleLookZ);

		RealType xx = to.x - from.x;
		RealType yy = to.y - from.y;
		RealType zz = to.z - from.z;
		RealType dirLength = std::sqrt(xx * xx + yy * yy + zz * zz);
		if (dirLength <= 0.0)
			return false; // degenerate view direction

		RealType cx = LengthDirX(1.0, -roll) / dirLength;
		RealType cy = LengthDirY(1.0, -roll);

		VecType up;
		up.x = -cx * xx * zz - cy * yy;
		up.y = cy * xx - cx * yy * zz;
		up.z = cx * (xx * xx + yy * yy);

		Matrix view = Matrix::LookAt(from, to, up);
		RealType aspect = (RealType)viewportWidth / (RealType)viewportHeight;
		// Negate fov/aspect to match this engine's own convention for the work-camera projection
		// (Generated/Scripts50.cpp:431: matrix_build_projection_perspective_fov(-fov, -aspect, ...)).
		Matrix projection = Matrix::Perspective(-45.0, -aspect, 1.0, 100000.0);
		Matrix viewProjection = projection * view;

		VecType clip = viewProjection * VecType(worldPos.x, worldPos.y, worldPos.z, 1.0);
		if (clip.w <= 0.0)
			return false; // behind the camera - do not draw

		RealType ndcX = clip.x / clip.w;
		RealType ndcY = clip.y / clip.w;

		outPoint.setX((ndcX * 0.5 + 0.5) * viewportWidth);
		outPoint.setY((1.0 - (ndcY * 0.5 + 0.5)) * viewportHeight); // NDC up -> screen down

		return true;
	}
}
