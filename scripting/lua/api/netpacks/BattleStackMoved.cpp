/*
 * api/netpacks/BattleStackMoved.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "BattleStackMoved.h"

#include "../../LuaStack.h"

#include "../Registry.h"

using scripting::api::netpacks::BattleStackMovedProxy;
using scripting::api::RegisterAPI;

VCMI_REGISTER_SCRIPT_API(BattleStackMovedProxy::Wrapper, "netpacks.BattleStackMoved")

namespace scripting
{
namespace api
{
namespace netpacks
{

const std::string BattleStackMovedProxy::CLASSNAME = "netpacks.InfoWindow";

const std::vector<BattleStackMovedProxy::Wrapper::RegType> BattleStackMovedProxy::REGISTER =
{
	{
		"addTileToMove",
		&BattleStackMovedProxy::addTileToMove
	},
	{
		"setUnitId",
		&BattleStackMovedProxy::setUnitId
	},
	{
		"setDistance",
		&BattleStackMovedProxy::setDistance
	},
	{
		"setTeleporting",
		&BattleStackMovedProxy::setTeleporting
	},
	{
		"toNetpackLight",
		&PackForClientProxy<BattleStackMovedProxy>::toNetpackLight
	}
};

int BattleStackMovedProxy::addTileToMove(lua_State * L, std::shared_ptr<Object> object)
{
	LuaStack S(L);
	lua_Integer hex = 0;
	if(!S.tryGetInteger(1, hex))
		return S.retNil();
	object->tilesToMove.emplace_back(hex);
	return S.retNil();
}

int BattleStackMovedProxy::setUnitId(lua_State * L, std::shared_ptr<Object> object)
{
	LuaStack S(L);
	lua_Integer unitId = 0;
	if(!S.tryGetInteger(1, unitId))
		return S.retNil();
	object->stack = static_cast<uint32_t>(unitId);
	return S.retNil();
}

int BattleStackMovedProxy::setDistance(lua_State * L, std::shared_ptr<Object> object)
{
	LuaStack S(L);
	lua_Integer distance = 0;
	if(!S.tryGetInteger(1, distance))
		return S.retNil();
	object->distance = distance;
	return S.retNil();
}

int BattleStackMovedProxy::setTeleporting(lua_State * L, std::shared_ptr<Object> object)
{
	LuaStack S(L);

	S.tryGetBool(1, object->teleporting);

	return S.retNil();
}

}
}
}
