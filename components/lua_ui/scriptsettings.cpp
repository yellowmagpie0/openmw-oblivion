#include "scriptsettings.hpp"

#include <map>
#include <sol/sol.hpp>

#include "adapter.hpp"
#include "element.hpp"
#include "registerscriptsettings.hpp"

namespace LuaUi
{
    namespace
    {
        std::vector<sol::table> allPages;
        ScriptSettingsPage parse(const sol::table& options)
        {
            auto name = options.get_or("name", std::string());
            auto searchHints = options.get_or("searchHints", std::string());
            auto element = options.get_or<std::shared_ptr<LuaUi::Element>>("element", nullptr);
            if (name.empty())
                Log(Debug::Warning) << "A script settings page has an empty name";
            return { std::move(name), std::move(searchHints), std::move(element) };
        }
    }

    void exchangeSettingsPages(std::vector<sol::table>& pages) noexcept { allPages.swap(pages); }

    size_t scriptSettingsPageCount()
    {
        return allPages.size();
    }

    ScriptSettingsPage scriptSettingsPageAt(size_t index)
    {
        return parse(allPages[index]);
    }

    void registerSettingsPage(const sol::table& options)
    {
        Element::requireSelectedRuntime();
        allPages.push_back(options);
    }

    void removeSettingsPage(const sol::table& options)
    {
        Element::requireSelectedRuntime();
        std::erase_if(allPages, [options](const sol::table& it) { return it == options; });
    }

    void clearSettings()
    {
        Element::requireSelectedRuntime();
        allPages.clear();
    }

    void ensureScriptSettingsPage(size_t index)
    {
        Element::requireSelectedRuntime();
        if (index >= allPages.size())
            return;

        sol::table options = allPages[index];
        auto element = options.get_or<std::shared_ptr<Element>>("element", nullptr);
        if (!element)
        {
            auto layout = options.get<sol::table>("layout");
            element = Element::make(std::move(layout), true, std::nullopt);
            element->create();
            options["element"] = std::move(element);
        }
    }

    void attachPageAt(size_t index, LuaAdapter* adapter)
    {
        Element::requireSelectedRuntime();
        adapter->detach();
        if (index < allPages.size())
        {
            ensureScriptSettingsPage(index);
            ScriptSettingsPage page = parse(allPages[index]);
            if (page.mElement.get())
                adapter->attach(page.mElement);
        }
    }
}
