-- Read-only camera prerequisite for the existing Forward-effect photo window.
local camera = require('openmw.camera')
local core = require('openmw.core')
local nearby = require('openmw.nearby')
local self = require('openmw.self')
local util = require('openmw.util')

local function visible(target, screen)
    for _, x in ipairs({-15, 0, 15}) do
        for _, y in ipairs({-15, 0, 15}) do
            for _, z in ipairs({6, 20, 40, 80, 100, 120}) do
                local anchor = target.position + util.vector3(x, y, z)
                local pixel = camera.worldToViewportVector(anchor)
                if camera.getViewTransform():apply(anchor).z < 0
                    and pixel.x >= 0 and pixel.x < screen.x
                    and pixel.y >= 0 and pixel.y < screen.y then
                    local hit = nearby.castRenderingRay(camera.getPosition(), anchor, {ignore = self}).hitObject
                    if hit and hit.id == target.id then return true end
                end
            end
        end
    end
    return false
end

local function new(window)
    assert(type(window) == 'number' and window == window and window > 0 and window < math.huge,
        'Forward capture needs a finite positive source window')
    local started
    return function(target, screen)
        local now = core.getSimulationTime()
        started = started or now
        local elapsed = now - started
        if elapsed < 0 or elapsed >= window then return false, false end
        return visible(target, screen), true
    end
end

return {new = new, visible = visible}
