/*
 * ScriptFixture.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <vstd/RNG.h>
#include "../../lib/JsonNode.h"
#include "../../lib/HeroBonus.h"
#include "../../lib/ScriptHandler.h"
#include "../../lib/NetPacksBase.h"
#include "../../lib/battle/CBattleInfoCallback.h"

#include "../mock/mock_IGameEventRealizer.h"
#include "../mock/mock_IGameInfoCallback.h"
#include "../mock/mock_battle_IBattleState.h"
#include "../mock/mock_scripting_Pool.h"
#include "../mock/mock_scripting_Environment.h"
#include "../mock/mock_vstd_CLoggerBase.h"

#include "../JsonComparer.h"

namespace test
{

using namespace ::testing;
using namespace ::scripting;

class ScriptFixture
{
public:
	class BattleFake : public CBattleInfoCallback, public BattleStateMock
	{
		std::shared_ptr<PoolMock> pool;
	public:
		BattleFake(std::shared_ptr<PoolMock> pool_);

		void setUp();

		scripting::Pool * getContextPool() const override;
	};

	std::shared_ptr<PoolMock> pool;

	std::shared_ptr<ScriptImpl> subject;
	std::shared_ptr<Context> context;

	std::shared_ptr<BattleFake> battleFake;

	EnvironmentMock environmentMock;
	StrictMock<IGameInfoCallbackMock> infoMock;
	StrictMock<IGameEventRealizerMock> applierMock;
    LoggerMock loggerMock;

	ScriptFixture();
	virtual ~ScriptFixture();

	void loadScript(const JsonNode & scriptConfig);
protected:
	void setUp();

private:
};


}
