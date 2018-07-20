/*
 * ServerCb.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "../../../lib/ScriptingService.h"
#include "../../../lib/CGameInfoCallback.h"

#include "../LuaWrapper.h"

namespace scripting
{
namespace api
{

class ServerCbProxy
{
public:
	using Object = ServerCb;
	using Wrapper = OpaqueWrapper<Object, ServerCbProxy>;

	static int commitPackage(lua_State * L, Object * object);

	static const std::vector<Wrapper::RegType> REGISTER;
	static const std::string CLASSNAME;
};

}
}
