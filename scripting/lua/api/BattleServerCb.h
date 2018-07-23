/*
 * BattleServerCb.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once


#include "../../../lib/ScriptingService.h"
#include "../../../lib/battle/IBattleEventRealizer.h"

#include "../LuaWrapper.h"

namespace scripting
{
namespace api
{

class BattleServerCbProxy
{
public:
	using Object = ServerBattleCb;
	using Wrapper = OpaqueWrapper<Object, BattleServerCbProxy>;

	static const std::vector<Wrapper::RegType> REGISTER;
	static const std::string CLASSNAME;

	static int moveUnit(lua_State * L, Object * object);
};

}
}
