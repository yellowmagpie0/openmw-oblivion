local core = require('openmw.core')
local types = require('openmw.types')
local self = require('openmw.self')
local I = require('openmw.interfaces')
I.Controls.overrideMovementControls(true)
local elapsed, distance, before, position = 0, 0, nil, nil
local done = false
return { engineHandlers = { onUpdate = function(dt)
    if done or dt <= 0 then return end
    local stat = types.NPC.stats.skills.athletics(self)
    if not before then
        before = { base = stat.base, progress = stat.progress }
        position = self.position
        print('WINDOWS_LIVE_SKILL_BEGIN', before.base, before.progress)
    end
    distance = distance + (self.position - position):length()
    position = self.position
    elapsed = elapsed + dt
    self.controls.jump = false
    self.controls.sneak = false
    self.controls.run = true
    self.controls.sideMovement = 0
    self.controls.movement = elapsed < 3 and 1 or -1
    if elapsed >= 6 then
        self.controls.movement = 0
        self.controls.run = false
        I.Controls.overrideMovementControls(false)
        done = true
        core.sendGlobalEvent('WindowsLiveSkillValidationDone', {
            before = before, after = { base = stat.base, progress = stat.progress },
            elapsed = elapsed, distance = distance,
        })
    end
end } }
