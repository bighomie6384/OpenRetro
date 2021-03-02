--[[
    This is a demo lua script that will make a pet which follows you around.
]]--

function onJoin(plr)
    -- spawn the dog
    local pet = NPC.new(plr.x, plr.y, plr.z, 3053)
    local listener = nil
    pet.wander = true

    pet.onDestroy:listen(function(pet)
        print("pet gone!")
    end)

    -- register chat commands for the dog
    listener = plr.onChat:listen(function(msg)
        print("got msg " .. msg)
        if msg == "pet come" then
            print("moving pet")
            pet.wander = false
            pet:moveTo(plr.x, plr.y, plr.z)
        elseif msg == "pet walk" then
            pet.wander = true
        elseif msg == "pet die" then
            pet:destroy()
            listener:disconnect()
        end
    end)

    while wait(3) and pet:exists() do
        if pet.wander then
            pet:moveTo(pet.x + math.random(2000)-1000, pet.y + math.random(2000)-1000, pet.z)
        end
    end
end

-- When a player joins the server, the callback function will be called
world.onPlayerAdded:listen(onJoin)

for i, plr in pairs(world.players) do
    onJoin(plr)
end