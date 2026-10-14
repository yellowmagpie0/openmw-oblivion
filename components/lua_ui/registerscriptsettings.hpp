#ifndef OPENMW_LUAUI_REGISTERSCRIPTSETTINGS
#define OPENMW_LUAUI_REGISTERSCRIPTSETTINGS

#include <sol/sol.hpp>
#include <vector>

namespace LuaUi
{
    // implemented in scriptsettings.cpp
    void exchangeSettingsPages(std::vector<sol::table>& pages) noexcept;
    void registerSettingsPage(const sol::table& options);
    void clearSettings();
    void removeSettingsPage(const sol::table& options);
}

#endif // !OPENMW_LUAUI_REGISTERSCRIPTSETTINGS
