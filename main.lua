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
        elseif msg == "sleepy" then
            player:sendMessage("waiting 2 seconds and replying!")
            wait(2)
            player:sendMessage("hello from the future!")
        end
    end)
end)

while wait(1) do
    print("Hello world!")
end