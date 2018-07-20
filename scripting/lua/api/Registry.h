/*
 * Registry.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <lua.hpp>

#define VCMI_REGISTER_SCRIPT_API(Type, Name) \
namespace\
{\
RegisterAPI<Type> _register(Name);\
}\
\

namespace scripting
{
namespace api
{

class Registar
{
public:
	virtual ~Registar() = default;

	virtual void perform(lua_State * L) const = 0;
};

class Registry
{
public:
	Registry();

	static Registry * get();

	const Registar * find(const std::string & name) const;
	void add(const std::string & name, std::shared_ptr<Registar> item);
private:
	std::map<std::string, std::shared_ptr<Registar>> data;
};

template<typename T>
class RegistarT : public Registar
{
public:
	void perform(lua_State * L) const override
	{
		T::registrator(L);
	}
};

template<typename T>
class RegisterAPI
{
public:
	RegisterAPI(const std::string & name)
	{
		auto r = std::make_shared<RegistarT<T>>();
		Registry::get()->add(name, r);
	}
};

}
}
