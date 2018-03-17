/*
 * LuaScriptingContext.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "LuaScriptingContext.h"

#include "../../lib/JsonNode.h"
#include "../../lib/NetPacks.h"

namespace scripting
{


LuaContext::LuaContext(vstd::CLoggerBase * logger_, const Script * source)
	: ContextBase(logger_),
	script(source)
{
	L = luaL_newstate();

	luaopen_base(L); //FIXME: disable filesystem access

	luaopen_math(L);
	luaopen_string(L);
	luaopen_table(L);
}

LuaContext::~LuaContext()
{
	lua_close(L);
}

void LuaContext::init(const IGameInfoCallback * cb, const CBattleInfoCallback * battleCb)
{
	icb = cb;
	bicb = battleCb;

	int ret = luaL_loadbuffer(L, script->getSource().c_str(), script->getSource().size(), script->getName().c_str());

	if(ret)
	{
		logger->error("Script %s failed to load, error: ", script->getName(), lua_tostring(L, -1));

		return;
	}

	ret = lua_pcall(L, 0, 0, 0);

	if(ret)
	{
		logger->error("Script failed to run, error: ", lua_tostring(L, -1));

		return;
	}
}

JsonNode LuaContext::callGlobal(const std::string & name, const JsonNode & parameters)
{
	lua_getglobal(L, name.c_str());

	int argc = parameters.Vector().size();

	if(argc)
	{
		for(int idx = 0; idx < parameters.Vector().size(); idx++)
		{
			push(parameters.Vector()[idx]);
		}
	}

	if(lua_pcall(L, argc, 1, 0))
	{
		std::string error = lua_tostring(L, -1);

		boost::format fmt("LUA function %s failed with message: %s");
		fmt % name % error;

		logger->error(fmt.str());

		return JsonUtils::stringNode(fmt.str());
	}

	JsonNode ret;

	pop(ret);

	return ret;
}

JsonNode LuaContext::callGlobal(ServerCb * cb, const std::string & name, const JsonNode & parameters)
{
	acb = cb;

	auto ret = callGlobal(name, parameters);

	acb = nullptr;

	return ret;
}

JsonNode LuaContext::callGlobal(ServerBattleCb * cb, const std::string & name, const JsonNode & parameters)
{
	bacb = cb;

	auto ret = callGlobal(name, parameters);

	bacb = nullptr;

	return ret;
}

void LuaContext::setGlobal(const std::string & name, int value)
{
	lua_pushinteger(L, static_cast<lua_Integer>(value));
	lua_setglobal(L, name.c_str());
}

void LuaContext::setGlobal(const std::string & name, const std::string & value)
{
	lua_pushlstring(L, value.c_str(), value.size());
	lua_setglobal(L, name.c_str());
}

void LuaContext::setGlobal(const std::string & name, double value)
{
	lua_pushnumber(L, value);
	lua_setglobal(L, name.c_str());
}

void LuaContext::push(const JsonNode & value)
{
	switch(value.getType())
	{
	case JsonNode::JsonType::DATA_BOOL:
		{
			lua_pushboolean(L, value.Bool());
		}
		break;
	case JsonNode::JsonType::DATA_FLOAT:
		{
			lua_pushnumber(L, value.Float());
		}
		break;
	case JsonNode::JsonType::DATA_INTEGER:
		{
			lua_pushinteger(L, value.Integer());
		}
		break;
	case JsonNode::JsonType::DATA_STRUCT:
		{
			lua_newtable(L);

			for(auto & keyValue : value.Struct())
			{
				lua_pushlstring(L, keyValue.first.c_str(), keyValue.first.size());

				push(keyValue.second);

				lua_settable(L, -3);
			}
		}
		break;
	case JsonNode::JsonType::DATA_STRING:
		{
			lua_pushlstring(L, value.String().c_str(), value.String().size());
		}
		break;
	case JsonNode::JsonType::DATA_VECTOR:
		{
			lua_newtable(L);
            for(int idx = 0; idx < value.Vector().size(); idx++)
			{
				lua_pushinteger(L, idx + 1);

				push(value.Vector()[idx]);

				lua_settable(L, -3);
			}
		}
		break;

	default:
		lua_pushnil(L);
		break;
	}
}

void LuaContext::pop(JsonNode & value)
{
	auto type = lua_type(L, -1);

	switch(type)
	{
	case LUA_TNUMBER:
		value.Float() = lua_tonumber(L, -1);
		break;
	case LUA_TBOOLEAN:
		value.Bool() = lua_toboolean(L, -1);
		break;
	case LUA_TSTRING:
		{
			size_t len = 0;

			auto raw = lua_tolstring(L, -1, &len);

			value.String() = std::string(raw, len);
		}

		break;
	case LUA_TTABLE:
		value.clear(); //TODO:
		break;
	default:
		value.clear();
		break;
	}

	lua_pop(L, 1);
}


}
