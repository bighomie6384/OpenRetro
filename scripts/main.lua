function makeCallback(player)
    print("HELLO " .. player.name .. " FROM LUA!")

    -- so we can hold an upvalue of it :)
    local listener, callback = nil, nil

    callback = function(msg)
        if msg == "up" then
            player:sendMessage("to the moon you go")
            player:moveTo(player.x, player.y, player.z + 10000)
        elseif msg == "pet" then
            -- should spawn a dog
            local npc = NPC.new(player.x, player.y, player.z, 3053)

            while wait(1) and player:exists() do
                -- grab a random radian offset for the pet to "be around" you
                local radian = math.random()*2*math.pi
                local offX = math.cos(radian) * 750
                local offY = math.sin(radian) * 750
                npc:moveTo(player.x + offX, player.y + offY, player.z)
            end
        elseif msg == "praise god" then
            player:sendMessage("your prays have been answered")
            player:setJump(30)
            player:setSpeed(2000)
        elseif msg == "sleepy" then
            player:sendMessage("waiting 2 seconds and replying!")
            wait(2)
            player:sendMessage("hello from the future!")
        elseif msg == "lost" then
            for i, plr in pairs(world.getNearbyPlayers(player.x, player.y, player.z, 1000)) do
                player:sendMessage(plr.name .. " is near!")
            end
        elseif msg == "script" then
            player:sendMessage("the next chat you send will be run as a script!")
            listener:disconnect()

            script = player.onChat:wait()

            -- reconnect the event
            listener:reconnect()

            -- compile the script and run it!
            local func, err = loadstring(script)

            -- if there was an error, report it! otherwise run the script1
            if not func then
                player:sendMessage("ERR: " .. err)
            else 
                func()
            end
        end
    end

    listener = player.onChat:listen(callback)
end

world.onPlayerAdded:listen(makeCallback)

for i, plr in pairs(world.players) do
    makeCallback(plr)
end

print("Hello world!")