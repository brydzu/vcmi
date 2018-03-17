/*
 * mock_scripting_Environment.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "../../../lib/ScriptingService.h"

namespace scripting
{

class EnvironmentMock : public Environment
{
public:
	MOCK_CONST_METHOD0(battle, const BattleCb *());
	MOCK_CONST_METHOD0(game, const GameCb *());
	MOCK_CONST_METHOD0(logger, ::vstd::CLoggerBase *());
};

}
