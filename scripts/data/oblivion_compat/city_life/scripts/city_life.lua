local core = require('openmw.core')
local nearby = require('openmw.nearby')
local self = require('openmw.self')
local util = require('openmw.util')

local function form(id)
    return core.getFormId('Oblivion.esm', id)
end

-- Only inspect actors the scene already contains. Looking up a world object
-- by its placed ID here could itself load the actor under test.
local function residentActors(expected)
    local residents = {}
    for _, actor in ipairs(nearby.actors) do
        local ref = expected[actor.recordId]
        if ref then
            assert(not residents[ref] and actor.id == form(ref):match('0x%x+$'),
                'Unexpected or duplicate city NPC: ' .. actor.id)
            residents[ref] = actor
        end
    end
    return residents
end

local market = {
    [form(0x03e92d)] = 0x03e92e, -- Gin-Wulm
    [form(0x015ea7)] = 0x01d208, -- Cicero Verus
    [form(0x016060)] = 0x01c4c2, -- Mandil
    [form(0x01c458)] = 0x01c462, -- Simplicia the Slow
}
local chorrol = {
    [form(0x02acc3)] = 0x0c536b, -- day guard
    [form(0x0222bb)] = 0x0c536a, -- night guard
    [form(0x0234b9)] = 0x0c8b80, -- Honditar
}
local marketCentre = util.vector3(32365, 69823, 3514)
local eastPlaza = util.vector3(34667, 68092, 3608)
local met = false
local settledSince, lastMandil, relievedSince
local elapsed = 0

local function onFrame(dt)
    elapsed = elapsed + dt
    if elapsed < 0.2 or not self.cell or core.isWorldPaused() then return end
    elapsed = 0
    local now = core.getGameTime()
    if self.cell.id == form(0x01bd1f) then
        local actors = residentActors(market)
        local mandil, cicero, simplicia = actors[0x01c4c2], actors[0x01d208], actors[0x01c462]
        if not (actors[0x03e92e] and mandil and (mandil.position - marketCentre):length() <= 1024
            and cicero and (cicero.position - eastPlaza):length() <= 2048
            and simplicia and (simplicia.position - marketCentre):length() <= 512) then
            settledSince = nil
            return
        end
        if not lastMandil or (mandil.position - lastMandil):length() > 2 then settledSince = nil end
        lastMandil = mandil.position
        settledSince = settledSince or now
        if now - settledSince >= 180 then
            print('Market civilians present together')
            core.quit()
        end
    elseif self.cell.id == form(0x00084f) then
        local actors = residentActors(chorrol)
        local day, night = actors[0x0c536b], actors[0x0c536a]
        if day and night and (day.position - night.position):length() <= 160 then met = true end
        if not (met and day and (day.position - day.startingPosition):length() <= 96 and not night
            and math.cos(day.rotation:getYaw() - day.startingRotation:getYaw()) >= math.cos(math.rad(5))) then
            relievedSince = nil
            return
        end
        relievedSince = relievedSince or now
        if now - relievedSince >= 900 and actors[0x0c8b80] then
            print('Chorrol guard relieved and Honditar in town')
            core.quit()
        end
    end
end

return { engineHandlers = { onFrame = onFrame } }
