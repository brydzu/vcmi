/*
 * Spell.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

namespace spells
{
struct SchoolInfo;

class DLL_LINKAGE Spell
{
public:
	virtual ~Spell() = default;

	virtual int32_t getIndex() const = 0;
	virtual int32_t getLevel() const = 0;
	virtual boost::logic::tribool getPositiveness() const = 0;
	virtual bool isAdventureSpell() const = 0;

	virtual void forEachSchool(const std::function<void (const SchoolInfo &, bool &)> & cb) const = 0;
};

}
