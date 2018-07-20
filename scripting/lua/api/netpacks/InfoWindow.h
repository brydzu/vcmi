/*
 * api/netpacks/InfoWindow.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "PackForClient.h"

namespace scripting
{
namespace api
{
namespace netpacks
{

class InfoWindowProxy
{
public:
	using Object = InfoWindow;
	using Wrapper = SharedWrapper<Object, InfoWindowProxy>;

	static int addReplacement(lua_State * L, std::shared_ptr<Object> object);
	static int addText(lua_State * L, std::shared_ptr<Object> object);

	static const std::vector<Wrapper::RegType> REGISTER;
	static const std::string CLASSNAME;
};

}
}
}
