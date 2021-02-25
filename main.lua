world.onPlayerAdded(function(player)
    print("HELLO " .. player.name .. " FROM LUA!")

    player:onChat(function(msg)
        if msg == "fuck" then
            player:sendMessage("no swearing on my christian server pls")
            player:moveTo(player.x, player.y, player.z + 10000)
        elseif msg == "praise god" then
            player:sendMessage("your prays have been answered")
            player:setJump(30)
            player:setSpeed(2000)
        end
    end)
end)

print("Hello world!")