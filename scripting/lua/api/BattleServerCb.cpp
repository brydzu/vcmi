/*
 * BattleServerCb.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"

#include "BattleServerCb.h"

#include "../LuaStack.h"

#include "Registry.h"

#include "netpacks/BattleStackMoved.h"

namespace scripting
{
namespace api
{

const std::string BattleServerCbProxy::CLASSNAME = "BattleServerCb";

const std::vector<BattleServerCbProxy::Wrapper::RegType> BattleServerCbProxy::REGISTER =
{
	{
		"moveUnit",
		&BattleServerCbProxy::moveUnit
	}
};

int BattleServerCbProxy::moveUnit(lua_State * L, Object * object)
{
	LuaStack S(L);

	std::shared_ptr<BattleStackMoved> pack;

	if(!S.tryGetShared<netpacks::BattleStackMovedProxy>(1, pack))
		return S.retNil();

	object->apply(pack.get());

	return S.retNil();
}


}
}

