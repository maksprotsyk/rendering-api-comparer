#include "ModelInstanceBase.h"

namespace Engine::Visual
{
	////////////////////////////////////////////////////////////////////////

	ModelInstanceBase::ModelInstanceBase(const std::string& id): m_id(id)
	{
	}

	////////////////////////////////////////////////////////////////////////

	const std::string& ModelInstanceBase::GetId() const
	{
		return m_id;
	}

	////////////////////////////////////////////////////////////////////////
}