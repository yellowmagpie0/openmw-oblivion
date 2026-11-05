-- Read-only camera prerequisite for the existing Forward-effect photo window.
local camera = require('openmw.camera')
local core = require('openmw.core')
local nearby = require('openmw.nearby')
local self = require('openmw.self')
local util = require('openmw.util')
local worldMask = nearby.COLLISION_TYPE.World + nearby.COLLISION_TYPE.HeightMap + nearby.COLLISION_TYPE.Door

local function motion(target, mode, radiansPerUnit)
    assert(type(radiansPerUnit) == 'number' and radiansPerUnit > 0 and radiansPerUnit < math.huge)
    local function units(angle)
        return math.max(-8192, math.min(8192, math.floor(angle / radiansPerUnit + 0.5)))
    end
    if mode == 'level' then return 0, units(-camera.getPitch()) end
    if mode == 'upper' then
        -- Standing transitions can retain the previous physical body's bounds
        -- for a frame. Frame the upper body from the logical actor placement;
        -- the separate rendered-mesh prerequisite still checks real visibility.
        local p, t, c = self.position, target.position, camera.getPosition()
        local dx, dy = t.x-p.x, t.y-p.y
        local f = self.rotation:apply(util.vector3(0, 1, 0))
        local x = units(math.atan2(f.y*dx-f.x*dy, f.x*dx+f.y*dy))
        local pitch = math.atan2(c.z-t.z-100, math.sqrt(dx*dx+dy*dy))
        return x, units(pitch-camera.getPitch())
    end
    assert(mode == 'frame' or mode == 'aim')
    local anchor = mode == 'frame' and target:getBoundingBox().center
        or target.position + util.vector3(0, 0, 100)
    local p = camera.getViewTransform():apply(anchor)
    local x = units(math.atan2(p.x, -p.z))
    local y = mode == 'aim' and 0 or units(-math.atan2(p.y, math.sqrt(p.x*p.x + p.z*p.z)))
    return x, y
end

local function visible(target, screen)
    local bounds = target:getBoundingBox()
    for _, x in ipairs({0, -0.65, 0.65}) do
        for _, y in ipairs({0, -0.65, 0.65}) do
            for _, z in ipairs({0, -0.65, 0.65}) do
                local anchor = bounds.center + util.vector3(
                    x * bounds.halfSize.x, y * bounds.halfSize.y, z * bounds.halfSize.z)
                local pixel = camera.worldToViewportVector(anchor)
                if camera.getViewTransform():apply(anchor).z < 0
                    and pixel.x >= 0 and pixel.x < screen.x
                    and pixel.y >= 0 and pixel.y < screen.y then
                    if not nearby.castRay(camera.getPosition(), anchor, {collisionType = worldMask}).hit then
                        local hit = nearby.castRenderingRay(camera.getPosition(), anchor, {ignore = self}).hitObject
                        if hit and hit.id == target.id then return true end
                    end
                end
            end
        end
    end
    return false
end

-- Require both the head and upper torso inside the viewport, rather than
-- accepting one low-body anchor while the head is cropped above the screen.
local function upperVisible(target, screen)
    local bounds = target:getBoundingBox()
    local top = bounds.center + util.vector3(0, 0, 1.05 * bounds.halfSize.z)
    local projectedTop = camera.worldToViewportVector(top)
    if camera.getViewTransform():apply(top).z >= 0 or projectedTop.x < 16
        or projectedTop.x >= screen.x - 16 or projectedTop.y < 16 or projectedTop.y >= screen.y - 16 then
        return false
    end
    for _, height in ipairs({0.85, 0.35}) do
        -- An extended arm/weapon shifts the box centre beside the head or
        -- chest. Require a real target-mesh hit in each band, without assuming
        -- that the centre line passes through the actor's skin.
        local found = false
        for _, x in ipairs({0, -0.25, 0.25, -0.5, 0.5, -0.75, 0.75}) do
            for _, y in ipairs({0, -0.25, 0.25, -0.5, 0.5, -0.75, 0.75}) do
                local anchor = bounds.center + util.vector3(
                    x * bounds.halfSize.x, y * bounds.halfSize.y, height * bounds.halfSize.z)
                local pixel = camera.worldToViewportVector(anchor)
                if camera.getViewTransform():apply(anchor).z < 0
                    and pixel.x >= 0 and pixel.x < screen.x and pixel.y >= 0 and pixel.y < screen.y
                    and not nearby.castRay(camera.getPosition(), anchor, {collisionType = worldMask}).hit then
                    local hit = nearby.castRenderingRay(camera.getPosition(), anchor, {ignore = self}).hitObject
                    if hit and hit.id == target.id then found = true; break end
                end
            end
            if found then break end
        end
        if not found then return false end
    end
    return true
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

-- A quiet rendered bounding box is a cheap capture prerequisite. The saved
-- physical bodies and the photograph remain the evidence for the actual pose.
local function newSettled(window, minimum, hold, maximumSpeed)
    for _, value in ipairs({window, minimum, hold, maximumSpeed}) do
        assert(type(value) == 'number' and value == value and value >= 0 and value < math.huge)
    end
    assert(hold > 0 and maximumSpeed > 0 and minimum + hold < window)
    local started, previousTime, previousBounds, quietSince
    local function distance(a, b)
        return math.sqrt((a.x-b.x)^2 + (a.y-b.y)^2 + (a.z-b.z)^2)
    end
    return function(target)
        local now = core.getSimulationTime()
        started = started or now
        local elapsed = now - started
        if elapsed < 0 or elapsed >= window then return false, false, elapsed, math.huge end
        local bounds = target:getBoundingBox()
        local speed = math.huge
        if previousTime and now > previousTime then
            speed = math.max(distance(bounds.center, previousBounds.center),
                distance(bounds.halfSize, previousBounds.halfSize)) / (now - previousTime)
            if elapsed >= minimum and speed <= maximumSpeed then
                quietSince = quietSince or now
            else
                quietSince = nil
            end
        elseif previousTime and now < previousTime then
            quietSince = nil
        end
        previousTime, previousBounds = now, bounds
        return quietSince ~= nil and now - quietSince >= hold, true, elapsed, speed
    end
end

local function framedVisible(target, screen)
    local bounds = target:getBoundingBox()
    for _, x in ipairs({-1, 1}) do
        for _, y in ipairs({-1, 1}) do
            for _, z in ipairs({-1, 1}) do
                local anchor = bounds.center + util.vector3(
                    x * bounds.halfSize.x, y * bounds.halfSize.y, z * bounds.halfSize.z)
                local pixel = camera.worldToViewportVector(anchor)
                if camera.getViewTransform():apply(anchor).z >= 0 or pixel.x < 16
                    or pixel.x >= screen.x - 16 or pixel.y < 16 or pixel.y >= screen.y - 16 then
                    return false
                end
            end
        end
    end
    return visible(target, screen)
end

return {new = new, visible = visible, motion = motion, upperVisible = upperVisible,
    newSettled = newSettled, framedVisible = framedVisible}
