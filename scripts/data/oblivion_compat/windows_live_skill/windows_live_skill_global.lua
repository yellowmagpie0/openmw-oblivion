local core = require('openmw.core')
return { eventHandlers = { WindowsLiveSkillValidationDone = function(data)
    local ok, err = pcall(function()
        assert(data.elapsed >= 6, 'insufficient simulation time')
        assert(data.distance > 0, 'player did not move')
        assert(data.after.base > data.before.base or
            (data.after.base == data.before.base and data.after.progress > data.before.progress),
            'native Athletics did not advance')
    end)
    if ok then
        print('WINDOWS_LIVE_SKILL_PASS', data.elapsed, data.distance,
            data.before.base, data.before.progress, data.after.base, data.after.progress)
    else
        print('WINDOWS_LIVE_SKILL_FAIL', tostring(err))
    end
    core.quit()
end } }
