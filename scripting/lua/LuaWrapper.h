/*
 * LuaWrapper.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <lua.hpp>

/*
 * Original code is LunaWrapper by nornagon.
 * https://lua-users.org/wiki/LunaWrapper
 * Published under the BSD 2-clause license
 * https://opensource.org/licenses/BSD-2-Clause
 *
 */

namespace scripting
{

template<class T, class U = T> class OpaqueWrapper
{
public:
	using UData = T *;

	static void push(lua_State * L, UData value)
	{
		void * raw = lua_newuserdata(L, sizeof(UData));

		UData * ptr = static_cast<UData *>(raw);
		*ptr = value;

		luaL_getmetatable(L, U::CLASSNAME.c_str());
		lua_setmetatable(L, -2);
	}

	static int registrator(lua_State * L)
	{
		if(luaL_newmetatable(L, U::CLASSNAME.c_str()) != 0)
		{
			lua_pushstring(L, "__index");

			lua_newtable(L);

			lua_Integer index = 0;

			for(auto & reg : U::REGISTER)
			{
				lua_pushstring(L, reg.name);
				lua_pushnumber(L, index);
				lua_pushcclosure(L, &OpaqueWrapper<T, U>::dispatcher, 1);
				lua_settable(L, -3);
				index++;
			}

			lua_settable(L, -3);
		}

		return 1;
	}

	static int dispatcher(lua_State * L)
	{
		int i = (int)lua_tonumber(L, lua_upvalueindex(1));

		void * objPtr = luaL_checkudata(L, 1, U::CLASSNAME.c_str());

		lua_remove(L, 1);

		if(objPtr)
		{
			auto obj = static_cast<UData *>(objPtr);
			return (U::REGISTER[i].functor)(L, *obj);
		}

		return 0;
	}

	struct RegType
	{
		const char * name;
		int(* functor)(lua_State *, UData);
	};

};

template<class T, class U = T> class SharedWrapper
{
public:
	using UData = std::shared_ptr<T>;

	static int registrator(lua_State * L)
	{
		if(luaL_newmetatable(L, U::CLASSNAME.c_str()) != 0)
		{
			lua_Integer index = 0;

			for(auto & reg : U::REGISTER)
			{
				lua_pushstring(L, reg.name);
				lua_pushnumber(L, index);
				lua_pushcclosure(L, &SharedWrapper<T, U>::dispatcher, 1);
				lua_settable(L, -3);
				index++;
			}

			lua_pushstring(L, "__gc");
			lua_pushcfunction(L, &(SharedWrapper<T, U>::destructor));
			lua_settable(L, -3);

			lua_pushstring(L, "__index");
			lua_pushvalue(L, -2);
			lua_settable(L, -3);
		}

		lua_settop(L, 0);

		lua_newtable(L);
		lua_pushstring(L, "new");
		lua_pushcfunction(L, &(SharedWrapper<T, U>::constructor));
		lua_settable(L, -3);

		return 1;
	}

	static int constructor(lua_State * L)
	{
		lua_settop(L, 0);//we do not accept any parameters in constructor

		auto obj = std::make_shared<T>();

		lua_newtable(L);

		void * raw = lua_newuserdata(L, sizeof(UData));

		if(!raw)
		{
			lua_settop(L, 0);
			return 0;
		}

		new(raw) UData(obj);

		luaL_getmetatable(L, U::CLASSNAME.c_str());

		if(!lua_istable(L, -1))
		{
			lua_settop(L, 0);
			return 0;
		}

		lua_setmetatable(L, -2);

		return 1;
	}

	static int dispatcher(lua_State * L)
	{
		int i = (int)lua_tonumber(L, lua_upvalueindex(1));

		void * raw = luaL_checkudata(L, 1, U::CLASSNAME.c_str());

		if(!raw)
		{
			lua_settop(L, 0);
			return 0;
		}

		auto obj = *(static_cast<UData *>(raw));

		lua_remove(L, 1);

		return (U::REGISTER[i].functor)(L, obj);
	}

	static int destructor(lua_State * L)
	{
		void * objPtr = luaL_checkudata(L, 1, U::CLASSNAME.c_str());
		if(objPtr)
		{
			auto obj = static_cast<UData *>(objPtr);
			obj->reset();
		}

		return 0;
	}

	struct RegType
	{
		const char * name;
		int(* functor)(lua_State *, UData);
	};
};

}
