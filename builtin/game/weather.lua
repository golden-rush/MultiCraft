--[[
	From Weather mod
	Copyright (C) Jeija (2013)
	Copyright (C) HybridDog (2015)
	Copyright (C) theFox6 (2018)
	Copyright (C) MultiCraft Development Team (2019)

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation; either version 3.0 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
]]

if not core.settings:get_bool("weather") then
	return
end

weather = {type = "none", wind = vector.new(0, 0, 0)}
local cloud_height = tonumber(core.settings:get("cloud_height"))


local file_name = core.get_worldpath() .. "/weather"
local file = io.open(file_name, "r")
if file ~= nil then
	local saved_weather = core.deserialize(file:read("*a"))
	io.close(file)
	if type(saved_weather) == "table" then
		weather = saved_weather
	end
end

core.register_on_shutdown(function()
	local file = io.open(file_name, "w")
	file:write(core.serialize(weather))
	io.close(file)
end)


--
-- Registration of weather types
--

weather_type = {}
weather_type.registered = {}
function weather_type.register(id, def)
	local ndef = table.copy(def)
	weather_type.registered[id] = ndef
end

-- Rain
weather_type.register("rain", {
	falling_speed = 6,
	amount = 7,
	size = 25,
	height = 3,
	vertical = true,
	texture = "weather_rain.png"
})

-- Snow
weather_type.register("snow", {
	falling_speed = 3,
	amount = 5,
	size = 20,
	height = 2,
	texture = "weather_snow.png"
})


--
-- Change of weather
--

local function weather_change()
	if weather.type == "none" then
		for id, _ in pairs(weather_type.registered) do
			if math.random(1, 3) == 1 then
				weather.wind = {}
				weather.wind.x = math.random(0, 8)
				weather.wind.y = 0
				weather.wind.z = math.random(0, 8)
				weather.type = id
			end
		end
		core.after(math.random(60, 300), weather_change)
	else
		weather.type = "none"
		core.after(math.random(1800, 3600), weather_change)
	end
end
core.after(math.random(600, 1800), weather_change)


--
-- Processing players
--

core.register_globalstep(function()
	local current_downfall = weather_type.registered[weather.type]
	if current_downfall == nil then return end

	for _, player in pairs(core.get_connected_players()) do
		if not player:is_player() then return end
		local ppos = vector.round(player:get_pos())
		ppos.y = ppos.y + 1.5
		-- Higher than clouds
		if not core.is_valid_pos(ppos) or ppos.y > cloud_height or ppos.y < -8 then return end
		-- Inside water
		local head_inside = core.get_node_or_nil(ppos)
		local def_inside = head_inside and core.registered_nodes[head_inside.name]
		if def_inside and def_inside.drawtype == "liquid" then return end
		-- Too dark, probably not under the sky
		local light = minetest.get_node_light(ppos, 0.5)
		if light and light < 12 then return end

		local wind_pos = vector.multiply(weather.wind, -1)
		local minp = vector.add(vector.add(ppos, {x = -8, y = current_downfall.height, z = -8}), wind_pos)
		local maxp = vector.add(vector.add(ppos, {x =  8, y = current_downfall.height, z =  8}), wind_pos)
		local vel = {x = weather.wind.x, y = -current_downfall.falling_speed, z = weather.wind.z}
		local vert = current_downfall.vertical or false

		core.add_particlespawner({
			amount = current_downfall.amount,
			time = 0.1,
			minpos = minp,
			maxpos = maxp,
			minvel = vel,
			maxvel = vel,
			minsize = current_downfall.size,
			maxsize = current_downfall.size,
			collisiondetection = true,
			collision_removal = true,
			vertical = vert,
			texture = current_downfall.texture,
			glow = 1,
			playername = player:get_player_name()
		})
	end
end)


--
-- Snow will cover the blocks and melt after some time
--

if core.settings:get_bool(("weather_snow_covers")) then
	-- Temp node to start the node timer
	core.register_node(":snow_cover", {
		tiles = {"blank.png"},
		drawtype = "signlike",
		buildable_to = true,
		groups = {not_in_creative_inventory = 1, dig_immediate = 3},
		on_construct = function(pos)
			core.get_node_timer(pos):start(math.random(60, 180))
			core.swap_node(pos, {name = "default:snow"})
		end
	})

	core.register_abm({
		nodenames = {"group:crumbly", "group:snappy", "group:cracky", "group:choppy"},
		neighbors = {"air"},
		interval = 15,
		chance = 500,
		catch_up = false,
		action = function(pos, node)
			if weather.type == "snow" then
				if pos.y > cloud_height then return end
				if pos.y < -8 then return end
				if core.registered_nodes[node.name].drawtype == "normal"
				or core.registered_nodes[node.name].drawtype == "allfaces_optional" then
					pos.y = pos.y + 1
					if core.get_node(pos).name ~= "air" then return end
					local light_day = core.get_node_light(pos, 0.5)
					local light_night = core.get_node_light(pos, 0)
					if  light_day   and light_day  == 15
					and light_night and light_night < 10 then
						core.add_node(pos, {name = "snow_cover"})
					end
				end
			end
		end
	})
end
