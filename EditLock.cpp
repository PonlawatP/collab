#include "EditLock.hpp"

namespace CppProject
{
	QHash<IntType, EditLock::Lock> EditLock::locks;

	void EditLock::Set(IntType instanceId, IntType peerId, const QString& peerName)
	{
		locks[instanceId] = { peerId, peerName };
	}

	void EditLock::Clear(IntType instanceId)
	{
		locks.remove(instanceId);
	}

	void EditLock::RemovePeer(IntType peerId)
	{
		for (auto it = locks.begin(); it != locks.end();)
		{
			if (it.value().peerId == peerId)
				it = locks.erase(it);
			else
				++it;
		}
	}

	bool EditLock::IsLockedByOther(IntType instanceId, IntType peerId)
	{
		auto it = locks.constFind(instanceId);
		return it != locks.constEnd() && it.value().peerId != peerId;
	}

	QHash<IntType, EditLock::Lock> EditLock::GetAll()
	{
		return locks;
	}
}
