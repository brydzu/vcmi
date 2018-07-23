/*
 * LuaStack.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "LuaStack.h"

namespace scripting
{

LuaStack::LuaStack(lua_State * L_)
	: L(L_)
{

}

void LuaStack::clear()
{
	lua_settop(L, 0);
}

bool LuaStack::tryGetBool(int position, bool & value)
{
	if(!lua_isboolean(L, position))
		return false;

	value = lua_toboolean(L, position);

	return true;
}

bool LuaStack::tryGetInteger(int position, lua_Integer & value)
{
	if(!lua_isnumber(L, position))
		return false;

	value = lua_tointeger(L, position);

	return true;
}

int LuaStack::retNil()
{
	clear();
	return 0;
}

}
