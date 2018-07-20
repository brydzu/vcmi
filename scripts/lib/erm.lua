local ERM = {}

local y = {}

ERM.getY = function(key)
	y[key] = y[key] or {}
	return y[key]
end

ERM.flag = {}
ERM.v = {}
ERM.z = {}


local __IF = nil


ERM.IF = function(...)
	__IF = __IF or require("core:erm.IF")
	__IF.ERM = ERM
	return __IF
end

return ERM
