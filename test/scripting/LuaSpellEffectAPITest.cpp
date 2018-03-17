/*
 * LuaSpellEffectAPITest.cpp, part of VCMI engine
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

using namespace ::testing;
using namespace ::scripting;

class LuaSpellEffectAPITest : public Test, public ScriptFixture
{
public:


protected:
	void SetUp() override
	{
		ScriptFixture::setUp();
	}
};

TEST_F(LuaSpellEffectAPITest, ApplicableOnExpert)
{
	JsonNode scriptConfig(JsonNode::JsonType::DATA_STRUCT);

	scriptConfig["source"].String() = "test/lua/SpellEffectAPITest.lua";

	loadScript(scriptConfig);

	context->setGlobal("effectLevel", 3);

	JsonNode params;

	JsonNode ret = context->callGlobal("applicable", params);

	JsonNode expected = JsonUtils::boolNode(true);

	JsonComparer cmp(false);
	cmp.compare("applicable result", ret, expected);

}

TEST_F(LuaSpellEffectAPITest, NotApplicableOnAdvanced)
{
	JsonNode scriptConfig(JsonNode::JsonType::DATA_STRUCT);

	scriptConfig["source"].String() = "test/lua/SpellEffectAPITest.lua";
	loadScript(scriptConfig);

	context->setGlobal("effectLevel", 2);

	JsonNode params;

	JsonNode ret = context->callGlobal("applicable", params);

	JsonNode expected = JsonUtils::boolNode(false);

	JsonComparer cmp(false);
	cmp.compare("applicable result", ret, expected);

}

TEST_F(LuaSpellEffectAPITest, ApplicableOnLeftSideOfField)
{
	JsonNode scriptConfig(JsonNode::JsonType::DATA_STRUCT);

	scriptConfig["source"].String() = "test/lua/SpellEffectAPITest.lua";
	loadScript(scriptConfig);

	context->setGlobal("effectLevel", 1);

	JsonNode params;

	BattleHex hex(2,2);

	JsonNode first;
	first.Vector().push_back(JsonUtils::intNode(hex.hex));
	first.Vector().push_back(JsonNode());

	JsonNode targets;
	targets.Vector().push_back(first);

	params.Vector().push_back(targets);

	JsonNode ret = context->callGlobal("applicableTarget", params);

	JsonNode expected = JsonUtils::boolNode(true);

	JsonComparer cmp(false);
	cmp.compare("applicable result", ret, expected);
}

TEST_F(LuaSpellEffectAPITest, NotApplicableOnRightSideOfField)
{
	JsonNode scriptConfig(JsonNode::JsonType::DATA_STRUCT);

	scriptConfig["source"].String() = "test/lua/SpellEffectAPITest.lua";
	loadScript(scriptConfig);

	context->setGlobal("effectLevel", 1);

	JsonNode params;

	BattleHex hex(11,2);

	JsonNode first;
	first.Vector().push_back(JsonUtils::intNode(hex.hex));
	first.Vector().push_back(JsonUtils::intNode(-1));

	JsonNode targets;
	targets.Vector().push_back(first);

	params.Vector().push_back(targets);

	JsonNode ret = context->callGlobal("applicableTarget", params);

	JsonNode expected = JsonUtils::boolNode(false);

	JsonComparer cmp(false);
	cmp.compare("applicable result", ret, expected);
}

}
