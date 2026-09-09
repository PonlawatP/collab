#include "ProjectTransfer.hpp"
#include "Asset/Scope.hpp"
#include "Generated/Scripts.hpp" // project_save, project_load, struct app, global::_app
#include <QDir>
#include <QFile>

namespace CppProject
{
	static StringType TempProjectPath()
	{
		return StringType(QDir::tempPath() + "/mineimator_collab_project.miproject");
	}

	QByteArray ProjectTransfer::CaptureCurrentProject()
	{
		if (!global::_app)
			return QByteArray();

		StringType path = TempProjectPath();
		project_save(ScopeAny(Scope<app>(global::_app)), { VarType(path) });

		QFile file((QString)path);
		if (!file.open(QFile::ReadOnly))
		{
			WARNING("Collab: ProjectTransfer failed to read saved project at " + (QString)path);
			return QByteArray();
		}
		return file.readAll();
	}

	void ProjectTransfer::LoadProject(const QByteArray& data)
	{
		if (!global::_app || data.isEmpty())
			return;

		StringType path = TempProjectPath();
		QFile file((QString)path);
		if (!file.open(QFile::WriteOnly | QFile::Truncate))
		{
			WARNING("Collab: ProjectTransfer failed to write received project to " + (QString)path);
			return;
		}
		file.write(data);
		file.close();

		project_load(ScopeAny(Scope<app>(global::_app)), { VarType(path) });

		// project_load itself doesn't set this - every other project/scene-mutating action does
		// (e.g. Generated/Scripts46.cpp:399-400), because the interactive viewport is a
		// progressive/accumulating render (Generated/Scripts50.cpp:551) that reuses the previous
		// frame's surface unless told the scene changed. Without this, the viewport keeps showing
		// whatever was on screen before the load, indefinitely.
		global::render_samples = -IntType(1);
		global::render_samples_clear = true;

		DEBUG("Collab: loaded joined project from " + (QString)path);
	}
}
