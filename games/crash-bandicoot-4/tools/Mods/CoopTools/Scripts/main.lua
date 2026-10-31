-- CoopTools v3: written for the Crash 4 co-op project. Read-only: prints information, changes nothing.
-- Every 3 seconds: if the current level (world) changed, print a report automatically.
-- F7 or F8: print the report on demand.

local function safe(f, ...)
    local ok, r = pcall(f, ...)
    if ok then return r end
    return "(error: " .. tostring(r) .. ")"
end

local function name(o)
    return safe(function() if o and o:IsValid() then return o:GetFullName() end return "(none)" end)
end

local function currentWorld()
    -- The world that owns the game state is the level being played.
    local gs = FindFirstOf("GameStateBase")
    if gs and gs:IsValid() then return gs:GetWorld() end
    local pc = FindFirstOf("PlayerController")
    if pc and pc:IsValid() then return pc:GetWorld() end
    return nil
end

local function report(why)
    print("[CoopTools] ===== report (" .. why .. ") =====\n")
    print("[CoopTools] World: " .. name(currentWorld()) .. "\n")
    print("[CoopTools] GameMode: " .. name(FindFirstOf("GameModeBase")) .. "\n")
    print("[CoopTools] GameState: " .. name(FindFirstOf("GameStateBase")) .. "\n")
    for _, pc in ipairs(FindAllOf("PlayerController") or {}) do
        print("[CoopTools] PlayerController: " .. name(pc) .. "\n")
        print("[CoopTools]    Pawn: " .. safe(function() return name(pc.Pawn) end) .. "\n")
        print("[CoopTools]    ViewTarget/Camera: " .. safe(function() return name(pc.PlayerCameraManager) end) .. "\n")
        print("[CoopTools]    Player (LocalPlayer): " .. safe(function() return name(pc.Player) end) .. "\n")
    end
    for _, ch in ipairs(FindAllOf("TfbCombatCharacter") or {}) do
        local n = name(ch)
        if not n:find("Enemy") then
            print("[CoopTools] Character: " .. n .. "\n")
            print("[CoopTools]    Controller: " .. safe(function() return name(ch.Controller) end) .. "\n")
        end
    end
    print("[CoopTools] ===== end =====\n")
end

local lastWorld = ""
LoopAsync(3000, function()
    local ok, err = pcall(function()
        local w = name(currentWorld())
        if w ~= lastWorld then
            lastWorld = w
            report("level changed")
        end
    end)
    if not ok then print("[CoopTools] loop error: " .. tostring(err) .. "\n") end
    return false
end)

local function onKey(k)
    print("[CoopTools] " .. k .. " pressed\n")
    local ok, err = pcall(report, k)
    if not ok then print("[CoopTools] report error: " .. tostring(err) .. "\n") end
end
RegisterKeyBind(Key.F7, function() onKey("F7") end)
RegisterKeyBind(Key.F8, function() onKey("F8") end)
print("[CoopTools] v3 loaded; reports automatically when the level changes; F7/F8 for a report\n")
