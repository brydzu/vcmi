/*
 * LuaScriptingContext.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "LuaWrapper.h"

#include "../../lib/ScriptHandler.h"
#include "../../lib/CScriptingModule.h"

namespace scripting
{

class LuaContext : public ContextBase
{
public:
	LuaContext(vstd::CLoggerBase * logger_, const Script * source);
	virtual ~LuaContext();

	void init(const IGameInfoCallback * cb, const CBattleInfoCallback * battleCb);

	//no return, Lua internally throws
	void error(const std::string & message);

	//log error and return nil from LuaCFunction
	int errorRetVoid(const std::string & message);

	JsonNode callGlobal(const std::string & name, const JsonNode & parameters) override;
	JsonNode callGlobal(ServerCb * cb, const std::string & name, const JsonNode & parameters) override;
	JsonNode callGlobal(ServerBattleCb * cb, const std::string & name, const JsonNode & parameters) override;

	void setGlobal(const std::string & name, int value) override;
	void setGlobal(const std::string & name, const std::string & value) override;
	void setGlobal(const std::string & name, double value) override;

	void push(const JsonNode & value);
	void pop(JsonNode & value);

	void popAll();

	void push(const std::string & value);
	void push(lua_CFunction f, void * opaque);
	void push(ServerCb * cb);

	std::string toStringRaw(int index);

protected:

private:
	lua_State * L;

	const Script * script;

	const IGameInfoCallback * icb;
	const CBattleInfoCallback * bicb;
	ServerBattleCb * bacb;

	void cleanupGlobals();

	void registerCore();

	//require global function
	static int require(lua_State * L);

	//print global function
	static int print(lua_State * L);

	//require function implementation
	int loadModule();

	int printImpl();

};



}
