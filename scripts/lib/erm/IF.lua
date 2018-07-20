local InfoWindow = require("netpacks.InfoWindow")

local IF = {}

local IF_M = function(x, message)

	local pack = InfoWindow.new()

	local onMatch1 = function (key1, key2)
		key2 = tonumber(key2)

		if key1 == 'X' then
			return x[key2]
		end

		if key1 == 'V' then
			return IF.ERM.v[key2]
		end

		if key1 == 'Z' then
			return IF.ERM.z[key2]
		end

		return nil
	end

	message = string.gsub(message, "%%([FVWXYZ])(%d+)", onMatch1)

	message = string.gsub(message, "(%%)(%%)", "%1")

	pack:addText(message)

	SERVER:commitPackage(pack)

end

IF.M = function(self, x, ...)
	local argc = select('#', ...)

	if argc == 1 then
		return IF_M(x, ...)
	end
end

return IF
