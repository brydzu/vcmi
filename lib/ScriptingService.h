/*
 * ScriptingService.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

class Services;
class JsonNode;
class IGameInfoCallback;
class IGameEventRealizer;
class IBattleEventRealizer;
class CBattleInfoCallback;

namespace scripting
{

using BattleCb = ::CBattleInfoCallback;
using GameCb = ::IGameInfoCallback;
using ServerCb = ::IGameEventRealizer;
using ServerBattleCb = ::IBattleEventRealizer;

class DLL_LINKAGE Environment
{
public:
	virtual ~Environment() = default;

	virtual const BattleCb * battle() const = 0;
	virtual const GameCb * game() const = 0;
	virtual ::vstd::CLoggerBase * logger() const = 0;
};

class DLL_LINKAGE Context
{
public:
	virtual ~Context() = default;

	virtual JsonNode callGlobal(const std::string & name, const JsonNode & parameters) = 0;
	virtual JsonNode callGlobal(ServerCb * cb, const std::string & name, const JsonNode & parameters) = 0;
	virtual JsonNode callGlobal(ServerBattleCb * cb, const std::string & name, const JsonNode & parameters) = 0;

	virtual void setGlobal(const std::string & name, int value) = 0;
	virtual void setGlobal(const std::string & name, const std::string & value) = 0;
	virtual void setGlobal(const std::string & name, double value) = 0;
};

class DLL_LINKAGE Script
{
public:
	virtual ~Script() = default;

	virtual const std::string & getName() const = 0;
	virtual const std::string & getSource() const = 0;

	virtual std::shared_ptr<Context> createContext(const Environment * env) const = 0;
};

class DLL_LINKAGE Service
{
public:
	virtual ~Service() = default;

	virtual void performRegistration(Services * services) const = 0;
};

class DLL_LINKAGE Pool
{
public:
	virtual ~Pool() = default;

	virtual std::shared_ptr<Context> getContext(const Script * script) = 0;

};

}
