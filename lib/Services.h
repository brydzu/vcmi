/*
 * Services.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

class CreatureService;
class ScriptingService;

namespace spells
{
	class SpellService;

	namespace effects
	{
		class Registry;
	}
}

namespace scripting
{
	class Service;
}

class DLL_LINKAGE Services
{
public:
	virtual ~Services() = default;

	virtual const CreatureService * creatureService() const = 0;
	virtual const scripting::Service * scriptingService() const = 0;
	virtual const spells::SpellService * spellService() const = 0;
	virtual const spells::effects::Registry * spellEffects() const = 0;
	virtual spells::effects::Registry * spellEffects() = 0;
};
