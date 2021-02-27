world.onPlayerAdded:listen(function(player)
    print("HELLO " .. player.name .. " FROM LUA!")

    -- so we can hold an upvalue of it :)
    local listener, callback = nil, nil

    callback = function(msg)
        if msg == "up" then
            player:sendMessage("to the moon you go")
            player:moveTo(player.x, player.y, player.z + 10000)
        elseif msg == "praise god" then
            player:sendMessage("your prays have been answered")
            player:setJump(30)
            player:setSpeed(2000)
        elseif msg == "sleepy" then
            player:sendMessage("waiting 2 seconds and replying!")
            wait(2)
            player:sendMessage("hello from the future!")
        elseif msg == "script" then
            player:sendMessage("the next chat you send will be run as a script!")
            listener:disconnect()

            script = player.onChat:wait()

            -- reconnect the event
            listener = player.onChat:listen(callback)

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
end)