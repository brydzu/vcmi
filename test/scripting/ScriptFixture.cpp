/*
 * ScriptFixture.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "ScriptFixture.h"

namespace test
{

ScriptFixture::BattleFake::BattleFake(std::shared_ptr<PoolMock> pool_)
	: CBattleInfoCallback(),
	BattleStateMock(),
	pool(pool_)
{
}

void ScriptFixture::BattleFake::setUp()
{
	CBattleInfoCallback::setBattle(this);
}

Pool * ScriptFixture::BattleFake::getContextPool() const
{
	return pool.get();
}


ScriptFixture::ScriptFixture()
{
	//ctor
}

ScriptFixture::~ScriptFixture() = default;

void ScriptFixture::loadScript(const JsonNode & scriptConfig)
{
	subject = VLC->scriptHandler->loadFromJson(scriptConfig, "test");

	context = subject->createContext(&environmentMock);

	EXPECT_CALL(*pool, getContext(_)).WillRepeatedly(Return(context));
}

void ScriptFixture::setUp()
{
	pool = std::make_shared<PoolMock>();

	battleFake = std::make_shared<BattleFake>(pool);
	battleFake->setUp();

	EXPECT_CALL(environmentMock, battle()).WillRepeatedly(Return(battleFake.get()));
	EXPECT_CALL(environmentMock, game()).WillRepeatedly(Return(&infoMock));
	EXPECT_CALL(environmentMock, logger()).WillRepeatedly(Return(&loggerMock));
}


}
