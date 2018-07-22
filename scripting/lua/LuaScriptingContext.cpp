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

#include <vstd/StringUtils.h>

#include "api/Registry.h"

#include "api/ServerCb.h"

#include "../../lib/JsonNode.h"
#include "../../lib/NetPacks.h"
#include "../../lib/filesystem/Filesystem.h"

namespace scripting
{

LuaContext::LuaContext(vstd::CLoggerBase * logger_, const Script * source)
	: ContextBase(logger_),
	script(source)
{
	L = luaL_newstate();

	luaopen_base(L);

	luaopen_math(L);
	luaopen_string(L);
	luaopen_table(L);
	luaopen_bit(L);

	cleanupGlobals();

	registerCore();
}

LuaContext::~LuaContext()
{
	lua_close(L);
}

void LuaContext::cleanupGlobals()
{
	lua_pushnil(L);
	lua_setglobal(L, "collectgarbage");

	lua_pushnil(L);
	lua_setglobal(L, "dofile");

	lua_pushnil(L);
	lua_setglobal(L, "load");

	lua_pushnil(L);
	lua_setglobal(L, "loadfile");

	lua_pushnil(L);
	lua_setglobal(L, "loadstring");

	lua_pushnil(L);
	lua_setglobal(L, "print");

	//TODO:
	//string.dump

	//math.random

	//math.randomseed
}

void LuaContext::init(const IGameInfoCallback * cb, const CBattleInfoCallback * battleCb)
{
	icb = cb;
	bicb = battleCb;

	int ret = luaL_loadbuffer(L, script->getSource().c_str(), script->getSource().size(), script->getName().c_str());

	if(ret)
	{
		logger->error("Script '%s' failed to load, error: %s", script->getName(), toStringRaw(-1));
		popAll();
		return;
	}

	ret = lua_pcall(L, 0, 0, 0);

	if(ret)
	{
		logger->error("Script '%s' failed to run, error: %s", script->getName(), toStringRaw(-1));
		popAll();
		return;
	}
}

void LuaContext::error(const std::string & message)
{
	//do not log here
	push(message);
	lua_error(L);
}

int LuaContext::errorRetVoid(const std::string & message)
{
	logger->error(message);
	popAll();
	return 0;
}

JsonNode LuaContext::callGlobal(const std::string & name, const JsonNode & parameters)
{
	lua_getglobal(L, name.c_str());

	if(!lua_isfunction(L, -1))
	{
		boost::format fmt("%s is not a function");
		fmt % name;

		logger->error(fmt.str());

		return JsonUtils::stringNode(fmt.str());
	}

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
	push(cb);
	lua_setglobal(L, "SERVER");

	auto ret = callGlobal(name, parameters);

	lua_pushnil(L);
	lua_setglobal(L, "SERVER");

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
		push(value.String());
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
		value.String() = toStringRaw(-1);
		break;
	case LUA_TTABLE:
		value.clear(); //TODO:
		error("Not implemented");
		break;
	default:
		value.clear();
		break;
	}

	lua_pop(L, 1);
}

void LuaContext::push(const std::string & value)
{
	lua_pushlstring(L, value.c_str(), value.size());
}

void LuaContext::push(lua_CFunction f, void * opaque)
{
	lua_pushlightuserdata(L, opaque);
	lua_pushcclosure(L, f, 1);
}

void LuaContext::push(ServerCb * cb)
{
	api::ServerCbProxy::Wrapper::push(L, cb);
}

void LuaContext::popAll()
{
	lua_settop(L, 0);
}

std::string LuaContext::toStringRaw(int index)
{
	size_t len = 0;
	auto raw = lua_tolstring(L, index, &len);
	return std::string(raw, len);
}

void LuaContext::registerCore()
{
	push(&LuaContext::require, this);
	lua_setglobal(L, "require");

	api::ServerCbProxy::Wrapper::registrator(L);
}

int LuaContext::require(lua_State * L)
{
	LuaContext * self = static_cast<LuaContext *>(lua_touserdata(L, lua_upvalueindex(1)));

	if(!self)
	{
		lua_pushstring(L, "internal error");
		lua_error(L);
		return 0;
	}

	return self->loadModule();
}

int LuaContext::loadModule()
{
	int argc = lua_gettop(L);

	if(argc < 1)
		return errorRetVoid("Module name required");

	if(!lua_isstring(L, 1))
		return errorRetVoid("Module name must be string");

	std::string resourceName = toStringRaw(1);
	popAll();

	if(resourceName.empty())
		return errorRetVoid("Module name is empty");

	auto temp = vstd::split(resourceName, ":");

	std::string scope;
	std::string modulePath;

	if(temp.size() <= 1)
	{
		modulePath = temp.at(0);
	}
	else
	{
		scope = temp.at(0);
		modulePath = temp.at(1);
	}

	if(scope.empty())
	{
		auto registar = api::Registry::get()->find(modulePath);

		if(!registar)
		{
			return errorRetVoid("Module not found: "+modulePath);
		}

		registar->perform(L);
	}
	else if(scope == "core")
	{

	//	boost::algorithm::replace_all(modulePath, boost::is_any_of("\\/ "), "");

		boost::algorithm::replace_all(modulePath, ".", "/");

		auto loader = CResourceHandler::get("core");

		modulePath = "scripts/lib/" + modulePath;

		ResourceID id(modulePath, EResType::LUA);

		if(!loader->existsResource(id))
			return errorRetVoid("Module not found "+modulePath);

		auto rawData = loader->load(id)->readAll();

		auto sourceText = std::string((char *)rawData.first.get(), rawData.second);

		int ret = luaL_loadbuffer(L, sourceText.c_str(), sourceText.size(), modulePath.c_str());

		if(ret)
			return errorRetVoid(toStringRaw(-1));

		ret = lua_pcall(L, 0, LUA_MULTRET, 0);

		if(ret)
		{
			logger->error("Module '%s' failed to run, error: %s", modulePath, toStringRaw(-1));
			popAll();
		}
	}
	else
	{
		//todo: also allow loading scripts from same scope
		return errorRetVoid("No access to scope "+scope);
	}


	return lua_gettop(L);
}

int LuaContext::print(lua_State * L)
{
	//TODO:
}

int LuaContext::printImpl()
{
	//TODO:
}


}
