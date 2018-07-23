include_directories(${Boost_INCLUDE_DIRS} ${LUA_INCLUDE_DIR} ${CMAKE_HOME_DIRECTORY} ${CMAKE_HOME_DIRECTORY}/include ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_HOME_DIRECTORY}/lib)

set(lib_SRCS
		StdInc.cpp
		LuaScriptModule.cpp
		LuaScriptingContext.cpp
		LuaSpellEffect.cpp
		LuaStack.cpp

		api/BattleServerCb.cpp
		api/Registry.cpp
		api/ServerCb.cpp

		api/netpacks/BattleStackMoved.cpp
		api/netpacks/InfoWindow.cpp
)

add_library(vcmiLua SHARED ${lib_SRCS})
target_link_libraries(vcmiLua ${Boost_LIBRARIES} ${LUA_LIBRARY} vcmi)

vcmi_set_output_dir(vcmiLua "scripting")

set_target_properties(vcmiLua PROPERTIES ${PCH_PROPERTIES})
cotire(vcmiLua)

install(TARGETS vcmiLua DESTINATION ${SCRIPTING_LIB_DIR})
