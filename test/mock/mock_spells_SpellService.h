/*
 * mock_spells_Spell.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "../../lib/spells/Magic.h"

namespace spells
{

class SpellServiceMock : public SpellService
{
public:
	MOCK_CONST_METHOD1(getSpell, const Spell *(const SpellID &));

};

}
