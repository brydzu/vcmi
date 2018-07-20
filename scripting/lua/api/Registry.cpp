/*
 * Registry.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "api/Registry.h"

namespace scripting
{
namespace api
{

Registry::Registry() = default;

Registry * Registry::get()
{
	static std::unique_ptr<Registry> Instance = make_unique<Registry>();
	return Instance.get();
}

void Registry::add(const std::string & name, std::shared_ptr<Registar> item)
{
	data[name] = item;
}

const Registar * Registry::find(const std::string & name) const
{
	auto iter = data.find(name);
	if(iter == data.end())
		return nullptr;
	else
		return iter->second.get();
}

}
}
