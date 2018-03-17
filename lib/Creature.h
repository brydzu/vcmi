/*
 * Creature.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

class DLL_LINKAGE Creature
{
public:
	virtual ~Creature() = default;

	virtual int32_t getIndex() const = 0;
	virtual uint32_t getMaxHealth() const = 0;
};
