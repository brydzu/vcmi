/*
 * LuaStack.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <lua.hpp>

namespace scripting
{

class LuaStack
{
public:
	LuaStack(lua_State * L_);

	void clear();

	bool tryGetBool(int position, bool & value);
	bool tryGetInteger(int position, lua_Integer & value);

	template<typename Proxy>
	bool tryGetShared(int position, std::shared_ptr<typename Proxy::Object> & value)
	{
		void * raw = luaL_checkudata(L, position, Proxy::CLASSNAME.c_str());

		if(!raw)
			return false;

		value = *(static_cast<std::shared_ptr<typename Proxy::Object> *>(raw));
		return true;
	}

	int retNil();

private:
	lua_State * L;
};

}
