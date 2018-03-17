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

#include <lua.hpp>

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

	JsonNode callGlobal(const std::string & name, const JsonNode & parameters) override;
	JsonNode callGlobal(ServerCb * cb, const std::string & name, const JsonNode & parameters) override;
	JsonNode callGlobal(ServerBattleCb * cb, const std::string & name, const JsonNode & parameters) override;

	void setGlobal(const std::string & name, int value) override;
	void setGlobal(const std::string & name, const std::string & value) override;
	void setGlobal(const std::string & name, double value) override;

	void push(const JsonNode & value);

	void pop(JsonNode & value);

protected:

private:
	lua_State * L;

	const Script * script;

	const IGameInfoCallback * icb;
	const CBattleInfoCallback * bicb;
	ServerCb * acb;
	ServerBattleCb * bacb;
};



}
