#include "oblivionui.hpp"

#include <components/vfs/archive.hpp>
#include <components/vfs/file.hpp>
#include <components/vfs/manager.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <sstream>
#include <utility>

namespace Resource
{
    namespace
    {
        enum class Shape { Frame, Fill, Crosshair, Pointer, Move, Door, Page, Left, Right, Close, List, Bookmark, Drop, Hit, Compass };
        enum Edge : unsigned { Top = 1, Bottom = 2, Left = 4, Right = 8, All = 15 };
        struct Texture
        {
            int mWidth = 8;
            int mHeight = 8;
            Shape mShape = Shape::Frame;
            unsigned mEdges = All;
            bool mPressed = false;
        };
        using Pixel = std::array<unsigned char, 4>;
        constexpr Pixel gold{ 205, 177, 116, 255 };
        constexpr Pixel paper{ 223, 210, 171, 255 };
        constexpr Pixel ink{ 45, 37, 25, 255 };
        constexpr Pixel clear{ 0, 0, 0, 0 };

        void integer(std::string& output, std::uint32_t value)
        {
            for (unsigned i = 0; i < 4; ++i)
                output.push_back(static_cast<char>((value >> (i * 8)) & 0xff));
        }

        // Small vector-like primitives rendered into uncompressed RGBA DDS.
        // The source stays editable and independent of proprietary TES3 art.
        std::string draw(const Texture& texture)
        {
            const int w = texture.mWidth, h = texture.mHeight;
            std::string data("DDS ");
            integer(data, 124);
            integer(data, 0x100f); // caps, height, width, pitch, pixel format
            integer(data, h);
            integer(data, w);
            integer(data, w * 4);
            integer(data, 0);
            integer(data, 0);
            for (int i = 0; i < 11; ++i)
                integer(data, 0);
            for (auto value : { 32u, 0x41u, 0u, 32u, 0xffu, 0xff00u, 0xff0000u, 0xff000000u,
                     0x1000u, 0u, 0u, 0u, 0u })
                integer(data, value);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    Pixel pixel = clear;
                    const int cx = x - w / 2, cy = y - h / 2;
                    const double nx = static_cast<double>(cx) / (w / 2);
                    const double ny = static_cast<double>(cy) / (h / 2);
                    switch (texture.mShape)
                    {
                        case Shape::Frame:
                            pixel = texture.mPressed ? Pixel{ 64, 51, 31, 240 } : Pixel{ 26, 24, 20, 235 };
                            if (((texture.mEdges & Top) && y == 1) || ((texture.mEdges & Bottom) && y == h - 2)
                                || ((texture.mEdges & Left) && x == 1) || ((texture.mEdges & Right) && x == w - 2))
                                pixel = gold;
                            break;
                        case Shape::Fill:
                            pixel = { 255, 255, 255, 255 };
                            break;
                        case Shape::Crosshair:
                            if ((std::abs(cx) <= 1 && std::abs(cy) >= 4 && std::abs(cy) <= 10)
                                || (std::abs(cy) <= 1 && std::abs(cx) >= 4 && std::abs(cx) <= 10))
                                pixel = { 245, 238, 215, 255 };
                            break;
                        case Shape::Pointer:
                            if (x >= 5 && x <= 5 + y / 2 && y < 24)
                                pixel = x == 5 || x == 5 + y / 2 || y == 23 ? ink : paper;
                            break;
                        case Shape::Move:
                        case Shape::Compass:
                            if ((std::abs(cx) <= 1 && std::abs(cy) < 13)
                                || (std::abs(cy) <= 1 && std::abs(cx) < 13)
                                || (cy < -6 && std::abs(cx) <= (-cy - 6) / 2))
                                pixel = gold;
                            break;
                        case Shape::Door:
                            if ((x == 9 || x == 23) && y >= 4 && y <= 28)
                                pixel = gold;
                            if ((y == 4 || y == 28) && x >= 9 && x <= 23)
                                pixel = gold;
                            if (x >= 18 && x <= 20 && y >= 15 && y <= 17)
                                pixel = gold;
                            break;
                        case Shape::Page:
                            pixel = paper;
                            if (x < 6 || x >= w - 6 || y < 6 || y >= h - 6)
                                pixel = gold;
                            break;
                        case Shape::Left:
                        case Shape::Right:
                            if (std::abs(cy) <= 10 && std::abs(cx - (texture.mShape == Shape::Left ? -1 : 1)
                                    * (10 - std::abs(cy))) <= 2)
                                pixel = gold;
                            break;
                        case Shape::Close:
                            if (std::abs(cx) < 11 && (std::abs(cx - cy) <= 1 || std::abs(cx + cy) <= 1))
                                pixel = gold;
                            break;
                        case Shape::List:
                            if (std::abs(cx) <= 11 && (std::abs(cy + 8) <= 1 || std::abs(cy) <= 1 || std::abs(cy - 8) <= 1))
                                pixel = gold;
                            break;
                        case Shape::Bookmark:
                            if (std::abs(cx) <= 6 && cy >= -12 && cy <= 12 - std::abs(cx))
                                pixel = gold;
                            break;
                        case Shape::Drop:
                            if ((std::abs(cx) <= 1 && cy >= -12 && cy <= 8)
                                || (cy >= 1 && cy <= 9 && std::abs(cx) == 9 - cy)
                                || (cy == 12 && std::abs(cx) <= 10))
                                pixel = gold;
                            break;
                        case Shape::Hit:
                            pixel = { 150, 0, 0, static_cast<unsigned char>(std::clamp(
                                (std::max(std::abs(nx), std::abs(ny)) - 0.55) * 350., 0., 180.)) };
                            break;
                    }
                    data.append(reinterpret_cast<const char*>(pixel.data()), pixel.size());
                }
            return data;
        }

        class UiFile final : public VFS::File
        {
            std::string mName;
            std::string mBytes;
            const VFS::Manager* mVfs = nullptr;
            VFS::Path::Normalized mAlias;
        public:
            UiFile(std::string name, Texture texture) : mName(std::move(name)), mBytes(draw(texture)) {}
            UiFile(std::string name, const VFS::Manager& vfs, std::string_view alias)
                : mName(std::move(name)), mVfs(&vfs), mAlias(alias) {}
            Files::IStreamPtr open() override
            {
                if (mVfs)
                    return mVfs->get(mAlias);
                return std::make_unique<std::istringstream>(mBytes, std::ios::in | std::ios::binary);
            }
            std::filesystem::file_time_type getLastModified() const override { return {}; }
            std::string getStem() const override { return std::filesystem::path(mName).stem().string(); }
        };

        class UiArchive final : public VFS::Archive
        {
            std::map<VFS::Path::Normalized, std::unique_ptr<UiFile>, std::less<>> mFiles;
            void add(std::string name, Texture texture)
            {
                mFiles.emplace(VFS::Path::Normalized(name), std::make_unique<UiFile>(name, texture));
            }
        public:
            explicit UiArchive(const VFS::Manager& vfs)
            {
                const std::pair<std::string_view, unsigned> edges[]{ { "top", Top }, { "bottom", Bottom },
                    { "left", Left }, { "right", Right }, { "top_left", Top | Left },
                    { "top_right", Top | Right }, { "bottom_left", Bottom | Left },
                    { "bottom_right", Bottom | Right }, { "center", 0 }, { "middle", 0 } };
                for (const auto prefix : { "menu_rightbuttonup_", "menu_rightbuttondown_", "menu_head_block_",
                         "menu_button_frame_", "menu_thick_border_", "menu_thin_border_" })
                    for (const auto& [suffix, mask] : edges)
                    {
                        const std::string base = "textures/" + std::string(prefix) + std::string(suffix);
                        const bool pressed = std::string_view(prefix) == "menu_rightbuttondown_";
                        add(base + ".dds", { 8, 8, Shape::Frame, mask, pressed });
                        if (suffix.find('_') != std::string_view::npos)
                            add(base + "_corner.dds", { 8, 8, Shape::Frame, mask, pressed });
                    }
                add("textures/menu_bar_gray.dds", { 16, 8, Shape::Fill });
                add("textures/menu_small_energy_bar_top.dds", { 8, 4, Shape::Frame, Top });
                add("textures/menu_small_energy_bar_bottom.dds", { 8, 4, Shape::Frame, Bottom });
                add("textures/menu_small_energy_bar_vert.dds", { 4, 8, Shape::Frame, Left | Right });
                add("textures/target.dds", { 32, 32, Shape::Crosshair });
                add("textures/tx_cursor.dds", { 32, 32, Shape::Pointer });
                add("textures/tx_cursormove.dds", { 32, 32, Shape::Move });
                add("textures/cursor_drop_ground.dds", { 32, 32, Shape::Drop });
                add("textures/door_icon.dds", { 32, 32, Shape::Door });
                add("textures/compass.dds", { 32, 32, Shape::Compass });
                add("textures/player_hit_01.dds", { 256, 256, Shape::Hit });
                add("textures/tx_menubook.dds", { 512, 512, Shape::Page });
                add("textures/scroll.dds", { 512, 512, Shape::Page });
                for (const auto& [name, shape] : std::initializer_list<std::pair<std::string_view, Shape>>{
                         { "prev", Shape::Left }, { "next", Shape::Right }, { "cancel", Shape::Close },
                         { "close", Shape::Close }, { "journal", Shape::List }, { "topics", Shape::List },
                         { "take", Shape::Drop }, { "bookmark", Shape::Bookmark } })
                    add("textures/tx_menubook_" + std::string(name) + "_idle.dds", { 64, 32, shape });
                add("textures/tx_menubook_bookmark.dds", { 32, 32, Shape::Bookmark });
                for (const auto& [name, target] : std::initializer_list<std::pair<std::string_view, std::string_view>>{
                         { "icons/k/stealth_handtohand.dds", "textures/menus/icons/weapons/handtohand.dds" },
                         { "icons/tx_goldicon.dds", "textures/menus/icons/clutter/icongold.dds" },
                         { "icons/k/stealth_sneak.dds", "textures/menus/class/attributes/load_image_sneak_small.dds" } })
                    mFiles.emplace(VFS::Path::Normalized(name), std::make_unique<UiFile>(std::string(name), vfs, target));
            }
            void listResources(VFS::FileMap& out) override
            {
                for (const auto& [name, file] : mFiles)
                    out.insert_or_assign(name, file.get());
            }
            bool contains(VFS::Path::NormalizedView file) const override { return mFiles.contains(file); }
            std::string getDescription() const override { return "OpenMW Oblivion shared UI"; }
        };
    }

    std::unique_ptr<VFS::Archive> makeOblivionUiArchive(const VFS::Manager& vfs)
    {
        return std::make_unique<UiArchive>(vfs);
    }
}
