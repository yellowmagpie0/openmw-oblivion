-- Exercise the shipped Lua scripts, with the engine API boundary supplied here.
local root, mode = arg[1], arg[2]
local paused, menu, paralysis, god, stance = false, nil, 0, false, 1
local switches = { controls = true, fighting = true }
local held, axes, callbacks, registrations = {}, {}, {}, {}
local input = {
    ACTION_TYPE = { Range = 1, Boolean = 2 },
    ACTION = setmetatable({}, { __index = function(_, key) return key end }),
    CONTROLLER_AXIS = setmetatable({}, { __index = function(_, key) return key end }),
    registerAction = function(a) registrations[a.key] = a end,
    registerTrigger = function() end,
    registerTriggerHandler = function() end,
    registerActionHandler = function() end,
    getRangeActionValue = function() return 0 end,
    getBooleanActionValue = function(key) return held[key] or false end,
    isActionPressed = function(key) return held[key] or false end,
    getAxisValue = function(key) return axes[key] or 0 end,
    bindAction = function(key, callback) callbacks[key] = callback end,
    actions = {}, triggers = {},
}
local self = { controls = {}, ATTACK_TYPE = { Any = 1, NoAttack = 0 } }
local settings = { get = function() return false end, subscribe = function() end, asTable = function() return {} end }
package.preload['openmw.input'] = function() return input end
package.preload['openmw.self'] = function() return self end
package.preload['openmw.async'] = function() return { callback = function(_, f) return f end } end
package.preload['openmw.storage'] = function() return { playerSection = function() return settings end } end
package.preload['openmw.core'] = function() return {
    isWorldPaused = function() return paused end,
    magic = { EFFECT_TYPE = { Paralyze = 1 } },
} end
package.preload['openmw.debug'] = function() return { isGodMode = function() return god end } end
package.preload['openmw.ui'] = function() return {} end
package.preload['openmw.interfaces'] = function() return { UI = { getMode = function() return menu end } } end
package.preload['openmw.types'] = function() return {
    Actor = {
        STANCE = { Weapon = 1, Spell = 2, Nothing = 0 },
        getStance = function() return stance end,
        activeEffects = function() return { getEffect = function() return { magnitude = paralysis } end } end,
    },
    Player = {
        CONTROL_SWITCH = { Controls = 'controls', Fighting = 'fighting' },
        getControlSwitch = function(_, key) return switches[key] end,
    },
} end
package.preload['openmw.util'] = function() return {} end
if mode == 'controls' then
    local script = dofile(root .. '/files/data/scripts/omw/input/playercontrols.lua')
    assert(registrations.Block.type == input.ACTION_TYPE.Boolean and registrations.Block.defaultValue == false)
    local cases = 0
    for mask = 0, 63 do
        held.Block = mask % 2 >= 1
        switches.fighting = math.floor(mask / 2) % 2 == 0
        paused = math.floor(mask / 4) % 2 >= 1
        menu = math.floor(mask / 8) % 2 >= 1 and 'inventory' or nil
        switches.controls = math.floor(mask / 16) % 2 == 0
        stance = math.floor(mask / 32) % 2 >= 1 and 2 or 1
        self.controls.block = true
        script.engineHandlers.onFrame(0.1)
        assert(self.controls.block == (held.Block and switches.fighting and not paused and menu == nil and switches.controls and stance == 1), mask)
        cases = cases + 1
    end
    paused, menu, stance, switches.controls, switches.fighting, held.Block = false, nil, 1, true, true, true
    paralysis = 1
    script.engineHandlers.onFrame(0.1); assert(self.controls.block == false)
    god = true
    script.engineHandlers.onFrame(0.1); assert(self.controls.block == true)
    script.interface.overrideCombatControls(true)
    self.controls.block = 'owned by custom script'
    script.engineHandlers.onFrame(0.1); assert(self.controls.block == 'owned by custom script')
    self.controls.block = true
    script.engineHandlers.onLoad(nil); assert(self.controls.block == false)
    self.controls.block = true
    script.engineHandlers.onLoad({ sneaking = true }); assert(self.controls.block == false and self.controls.sneak == true)
    assert(script.engineHandlers.onSave().block == nil)
    print('PASS controls ' .. cases .. ' combinations; paralysis/god/override/load/save')
elseif mode == 'bindings' then
    dofile(root .. '/files/data/scripts/omw/input/actionbindings.lua')
    local cases = 0
    for _, trigger in ipairs({ -1, 0, .59, .6, .61, 1 }) do
        for _, mouse in ipairs({ false, true }) do
            for _, preview in ipairs({ false, true }) do
                held.Block, held.TogglePOV, axes.TriggerLeft = mouse, preview, trigger
                assert(callbacks.Block() == (mouse or (not preview and trigger >= .6)))
                cases = cases + 1
            end
        end
    end
    print('PASS bindings ' .. cases .. ' combinations')
else error('unknown mode') end
