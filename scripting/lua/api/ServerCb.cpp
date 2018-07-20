/*
 * ServerCb.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "ServerCb.h"

#include "Registry.h"

namespace scripting
{
namespace api
{

const std::string ServerCbProxy::CLASSNAME = "ServerCb";

const std::vector<ServerCbProxy::Wrapper::RegType> ServerCbProxy::REGISTER =
{
	{
		"commitPackage",
		&ServerCbProxy::commitPackage
	}
};

int ServerCbProxy::commitPackage(lua_State * L, Object * object)
{
	if(lua_isuserdata(L, 1) != 1)
	{
		lua_settop(L, 0);
		return 0;
	}

	lua_getfield(L, 1, "toNetpackLight");
	lua_insert(L, 1);

	int ret = lua_pcall(L, 1, 1, 0);

	if(ret != 0 || !lua_islightuserdata(L, 1))
	{
		lua_settop(L, 0);
		return 0;
	}

	CPackForClient * pack = static_cast<CPackForClient *>(lua_touserdata(L, 1));

	object->commitPackage(pack);

	lua_settop(L, 0);
	return 0;
}


}
}
