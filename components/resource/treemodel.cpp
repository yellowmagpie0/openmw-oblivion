#include "treemodel.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <istream>
#include <limits>
#include <numbers>

#include <osg/AlphaFunc>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/LOD>
#include <osg/Material>
#include <osg/Texture2D>

#include <components/misc/endianness.hpp>
#include <components/misc/rng.hpp>
#include <components/vfs/manager.hpp>

#include "imagemanager.hpp"

namespace
{
    using VFS::Path::Normalized;
    using VFS::Path::NormalizedView;
    using UvQuad = std::array<osg::Vec2f, 4>;
    constexpr float pi = std::numbers::pi_v<float>;

    // Only the material header and leaf composite coordinates are needed. The
    // remaining procedural parameters are intentionally opaque to this renderer.
    struct Materials
    {
        Normalized mBark;
        std::vector<UvQuad> mLeafRects;
    };

    template <class T>
    T readNumber(std::string_view bytes, std::size_t offset)
    {
        T result;
        std::memcpy(&result, bytes.data() + offset, sizeof(T));
        return Misc::fromLittleEndian(result);
    }

    Materials readMaterials(std::istream& stream)
    {
        Materials result;
        stream.seekg(0, std::ios::end);
        const auto size = stream.tellg();
        if (size < 32 || size > 1024 * 1024)
            return result;
        std::string bytes(static_cast<std::size_t>(size), '\0');
        stream.seekg(0);
        if (!stream.read(bytes.data(), bytes.size()) || bytes.compare(8, 12, "__IdvSpt_02_") != 0
            || readNumber<std::uint32_t>(bytes, 24) != 2000)
            return result;

        const auto length = readNumber<std::uint32_t>(bytes, 28);
        if (length > bytes.size() - 32)
            return result;
        const Normalized authoringPath(std::string_view(bytes).substr(32, length));
        result.mBark = Normalized(NormalizedView(authoringPath).filename());

        // LE section 10000 (composite maps), followed by 10002 (leaf UVs).
        constexpr std::string_view leafSection("\x10\x27\0\0\x12\x27\0\0", 8);
        const auto pos = bytes.find(leafSection, 32 + length);
        if (pos == std::string::npos || bytes.size() - pos < 12)
            return result;
        const auto count = readNumber<std::uint32_t>(bytes, pos + 8);
        if (count > 256 || count > (bytes.size() - pos - 12) / 32)
            return result;
        for (std::size_t i = 0; i < count; ++i)
        {
            UvQuad rect;
            for (std::size_t j = 0; j < rect.size(); ++j)
            {
                const auto offset = pos + 12 + 32 * i + 8 * j;
                const float u = readNumber<float>(bytes, offset);
                const float v = readNumber<float>(bytes, offset + 4);
                if (!(u >= 0 && u <= 1 && v >= 0 && v <= 1))
                    return result;
                // SPT lists top-right, top-left, bottom-left, bottom-right.
                rect[(j + 2) % 4].set(u, 1 - v);
            }
            result.mLeafRects.push_back(rect);
        }
        return result;
    }

    osg::ref_ptr<osg::Image> textureImage(
        NormalizedView directory, NormalizedView file, const VFS::Manager& vfs, Resource::ImageManager& images)
    {
        if (file.empty())
            return nullptr;
        Normalized path = file.value().starts_with("textures/") ? Normalized(file) : directory / file;
        path.changeExtension(VFS::Path::ExtensionView("dds"));
        if (!vfs.exists(path))
            return nullptr;
        auto image = images.getImage(path);
        return image == images.getWarningImage() ? nullptr : image;
    }

    osg::ref_ptr<osg::StateSet> material(osg::Image* image, bool cutout)
    {
        osg::ref_ptr<osg::StateSet> state = new osg::StateSet;
        osg::ref_ptr<osg::Material> mat = new osg::Material;
        mat->setColorMode(osg::Material::OFF);
        mat->setDiffuse(
            osg::Material::FRONT_AND_BACK, image ? osg::Vec4f(1, 1, 1, 1) : osg::Vec4f(0.35f, 0.27f, 0.19f, 1));
        state->setAttributeAndModes(mat, osg::StateAttribute::ON);
        if (image)
        {
            osg::ref_ptr<osg::Texture2D> texture = new osg::Texture2D(image);
            const auto wrap = cutout ? osg::Texture::CLAMP_TO_EDGE : osg::Texture::REPEAT;
            texture->setWrap(osg::Texture::WRAP_S, wrap);
            texture->setWrap(osg::Texture::WRAP_T, wrap);
            state->setTextureAttributeAndModes(0, texture, osg::StateAttribute::ON);
        }
        if (cutout)
        {
            state->setAttributeAndModes(new osg::AlphaFunc(osg::AlphaFunc::GREATER, 0.36f), osg::StateAttribute::ON);
            state->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);
        }
        return state;
    }

    struct Mesh
    {
        osg::ref_ptr<osg::Vec3Array> mPositions = new osg::Vec3Array;
        osg::ref_ptr<osg::Vec3Array> mNormals = new osg::Vec3Array;
        osg::ref_ptr<osg::Vec2Array> mUv = new osg::Vec2Array;
        osg::ref_ptr<osg::DrawElementsUShort> mTriangles = new osg::DrawElementsUShort(GL_TRIANGLES);

        void vertex(osg::Vec3f pos, osg::Vec3f normal, osg::Vec2f uv)
        {
            mPositions->push_back(pos);
            mNormals->push_back(normal);
            mUv->push_back(uv);
        }

        void quad(unsigned a, unsigned b, unsigned c, unsigned d)
        {
            for (unsigned i : { a, b, c, a, c, d })
                mTriangles->push_back(static_cast<unsigned short>(i));
        }

        osg::ref_ptr<osg::Geode> finish(osg::StateSet* state) const
        {
            osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
            geometry->setUseDisplayList(false);
            geometry->setUseVertexBufferObjects(true);
            geometry->setVertexArray(mPositions);
            geometry->setNormalArray(mNormals, osg::Array::BIND_PER_VERTEX);
            geometry->setTexCoordArray(0, mUv);
            geometry->addPrimitiveSet(mTriangles);
            osg::ref_ptr<osg::Geode> geode = new osg::Geode;
            geode->setStateSet(state);
            geode->addDrawable(geometry);
            return geode;
        }
    };

    struct Slice
    {
        float mLeft = 0.5f;
        float mRight = 0.5f;
        float radius() const { return (mRight - mLeft) / 2; }
    };

    // Sample once per species. The shipped sprite gives us the crown outline,
    // including clear trunks, asymmetric crowns and low spreading shrubs.
    struct Silhouette
    {
        std::array<Slice, 40> mSlices;
        float mLeft = 1;
        float mRight = 0;
        float mBottom = 1;
        float mTop = 0;
    };

    Silhouette measure(const osg::Image* image)
    {
        Silhouette shape;
        constexpr int columns = 128;
        const int rows = static_cast<int>(shape.mSlices.size());
        for (int y = 0; y < rows; ++y)
        {
            const float z = (y + 0.5f) / rows;
            float left = 1, right = 0;
            for (int x = 0; x < columns; ++x)
            {
                const float u = (x + 0.5f) / columns;
                const bool solid = image
                    ? image->getColor(
                               static_cast<unsigned>(u * image->s()), static_cast<unsigned>((1 - z) * image->t()))
                            .a()
                        > 0.25f
                    : std::abs(u - 0.5f) < std::max(0.015f, 0.38f * std::sin(pi * (z - 0.23f) / 0.77f));
                if (solid)
                {
                    left = std::min(left, static_cast<float>(x) / columns);
                    right = static_cast<float>(x + 1) / columns;
                }
            }
            if (left <= right)
            {
                shape.mSlices[y] = { left, right };
                shape.mLeft = std::min(shape.mLeft, left);
                shape.mRight = std::max(shape.mRight, right);
                shape.mBottom = std::min(shape.mBottom, static_cast<float>(y) / rows);
                shape.mTop = static_cast<float>(y + 1) / rows;
            }
        }
        return shape;
    }

    osg::Vec3f radial(float angle)
    {
        return { std::cos(angle), std::sin(angle), 0 };
    }

    // Tapered tube with a duplicated seam for wrapping bark.
    void limb(Mesh& wood, osg::Vec3f start, osg::Vec3f end, float base, float tip, unsigned sides)
    {
        osg::Vec3f axis = end - start;
        const float length = axis.normalize();
        osg::Vec3f side = axis ^ (std::abs(axis.z()) < 0.9f ? osg::Vec3f(0, 0, 1) : osg::Vec3f(0, 1, 0));
        side.normalize();
        const osg::Vec3f other = axis ^ side;
        const unsigned first = static_cast<unsigned>(wood.mPositions->size());
        for (unsigned j = 0; j <= sides; ++j)
        {
            const float u = static_cast<float>(j) / sides;
            const osg::Vec3f n = side * std::cos(2 * pi * u) + other * std::sin(2 * pi * u);
            wood.vertex(start + n * base, n, { u, length / (8 * base) });
            wood.vertex(end + n * tip, n, { u, 0 });
            if (j)
            {
                const unsigned i = first + j * 2;
                wood.quad(i - 2, i, i + 1, i - 1);
            }
        }
    }

    void card(
        Mesh& mesh, osg::Vec3f center, osg::Vec3f horizontal, osg::Vec3f vertical, osg::Vec3f normal, const UvQuad& uv)
    {
        const unsigned i = static_cast<unsigned>(mesh.mPositions->size());
        mesh.vertex(center - horizontal - vertical, normal, uv[0]);
        mesh.vertex(center + horizontal - vertical, normal, uv[1]);
        mesh.vertex(center + horizontal + vertical, normal, uv[2]);
        mesh.vertex(center - horizontal + vertical, normal, uv[3]);
        mesh.quad(i, i + 1, i + 2, i + 3);
    }

    osg::ref_ptr<osg::Node> impostor(const Silhouette& shape, float w, float h, int planes, osg::StateSet* state)
    {
        // Crop empty margins, retaining one sample of padding on each edge.
        const float l = std::max(0.f, shape.mLeft - 1.f / 128), r = std::min(1.f, shape.mRight + 1.f / 128);
        const float b = std::max(0.f, shape.mBottom - 0.025f), t = std::min(1.f, shape.mTop + 0.025f);
        const UvQuad uv{ osg::Vec2f(l, 1 - b), { r, 1 - b }, { r, 1 - t }, { l, 1 - t } };
        Mesh mesh;
        for (int i = 0; i < planes; ++i)
        {
            const auto axis = radial(pi * i / planes);
            card(mesh, axis * ((r + l - 1) * w / 2) + osg::Vec3f(0, 0, (t + b) * h / 2), axis * ((r - l) * w / 2),
                { 0, 0, (t - b) * h / 2 }, { 0, 0, 1 }, uv);
        }
        return mesh.finish(state);
    }

    osg::ref_ptr<osg::Node> branches(const Silhouette& shape, float w, float h, osg::StateSet* bark,
        osg::StateSet* foliageState, const std::vector<UvQuad>& tiles, Misc::Rng::Generator rng, bool coarse)
    {
        Mesh wood, foliage;
        const auto random = [&] { return Misc::Rng::rollProbability(rng); };
        const float top = shape.mTop * h;
        const float bottom = shape.mBottom * h;
        const float crownWidth = (shape.mRight - shape.mLeft) * w;
        const auto trunk = [&](float z) {
            const auto& row = shape.mSlices[std::clamp(static_cast<int>(z * shape.mSlices.size()), 0, 39)];
            return osg::Vec3f((row.mRight + row.mLeft - 1) * 0.23f * w, 0, z * h);
        };
        const float root
            = std::clamp(shape.mSlices[std::min(39, static_cast<int>(shape.mBottom * 40) + 1)].radius() * w,
                top * 0.009f, top * 0.043f);
        // Connected rings keep both the bark UVs and the surface continuous
        // through bends. Independent tapered cylinders leave seams at every joint.
        const unsigned sides = coarse ? 5 : 9;
        constexpr std::array trunkLevels{ 0.f, 0.055f, 0.17f, 0.34f, 0.5f, 0.67f, 0.84f, 1.f };
        for (const float t : trunkLevels)
        {
            const auto center = trunk(std::lerp(shape.mBottom, shape.mTop, t));
            const float radius = root * (1.01f - t + 0.35f * std::max(0.f, 1.f - t / 0.055f));
            const unsigned first = static_cast<unsigned>(wood.mPositions->size());
            for (unsigned j = 0; j <= sides; ++j)
            {
                const float u = static_cast<float>(j) / sides;
                const auto n = radial(2 * pi * u);
                wood.vertex(center + n * radius, n, { u, (bottom - center.z()) / (10 * root) });
                if (first && j)
                {
                    const unsigned k = first + j;
                    wood.quad(k - sides - 2, k - sides - 1, k, k - 1);
                }
            }
        }
        const int count = coarse ? 15 : 52;
        const float phase = random() * 2 * pi;
        for (int i = 0; i < count; ++i)
        {
            const float z = std::lerp(shape.mBottom, shape.mTop, (i + random()) / count);
            const auto& row = shape.mSlices[std::min(39, static_cast<int>(z * 40))];
            const float reach = row.radius() * w;
            if (reach < std::max(root * 1.7f, crownWidth * 0.048f))
                continue;
            const float angle = phase + i * 2.399963f + (random() - 0.5f) * 0.6f;
            const auto dir = radial(angle);
            const auto across = radial(angle + pi / 2);
            const float rise = std::min(z * h - bottom, reach * (0.32f + 0.5f * random()));
            const auto start = trunk(z - rise / h);
            const auto joint = trunk(z) + dir * (reach * 0.42f) - osg::Vec3f(0, 0, rise * 0.32f);
            const float base = std::min(root * 0.53f, reach * 0.07f);
            limb(wood, start, joint, base, base * 0.54f, coarse ? 3 : 5);
            for (int fork = -1; fork <= 1; fork += 2)
            {
                const auto end = trunk(z) + dir * (reach * (0.65f + 0.2f * random())) + across * (fork * reach * 0.3f);
                limb(wood, joint, end, base * 0.54f, base * 0.07f, 3);
                if (!foliageState)
                    continue;
                const float spread = std::min(crownWidth * (coarse ? 0.26f : 0.18f), reach * 0.56f);
                for (int j = 0; j < (coarse ? 1 : 3); ++j)
                {
                    const auto pos = joint * (0.30f * j) + end * (1 - 0.30f * j);
                    const float yaw = angle + fork * 0.65f + j * 1.8f;
                    const auto side = radial(yaw + pi / 2);
                    const auto up = osg::Vec3f(0, 0, 0.64f + random() * 0.25f) + radial(yaw) * 0.52f;
                    osg::Vec3f normal = dir + osg::Vec3f(0, 0, 0.85f);
                    normal.normalize();
                    // A bare tree or a model without an ICON can still use the
                    // photographed twigs at this height in its own silhouette.
                    UvQuad tile;
                    if (tiles.empty())
                    {
                        const float u = 0.5f + fork * reach * 0.63f / w;
                        const float du = std::min(spread / w, 0.5f - std::abs(u - 0.5f));
                        const float dv = std::min({ spread * 0.7f / h, z, 1 - z });
                        tile = { osg::Vec2f(u - du, 1 - z + dv), { u + du, 1 - z + dv }, { u + du, 1 - z - dv },
                            { u - du, 1 - z - dv } };
                    }
                    else
                        tile = tiles[Misc::Rng::rollDice(tiles.size(), rng)];
                    const float aspect = (tile[3] - tile[0]).length() / std::max(0.01f, (tile[1] - tile[0]).length());
                    card(foliage, pos, side * spread, up * (spread * std::clamp(aspect, 0.35f, 1.5f)), normal, tile);
                }
            }
        }
        osg::ref_ptr<osg::Group> group = new osg::Group;
        group->addChild(wood.finish(bark));
        if (!foliage.mTriangles->empty())
            group->addChild(foliage.finish(foliageState));
        return group;
    }
}

osg::ref_ptr<osg::Node> Resource::loadTree(const TreeModel& model, const VFS::Manager& vfs, ImageManager& images)
{
    NormalizedView source(model.mSource);
    // Untyped model references can reach the scene manager through the meshes/ prefix.
    std::string_view relative = source.value();
    if (relative.starts_with("meshes/"))
        relative.remove_prefix(7);
    if (relative.starts_with("trees/"))
        relative.remove_prefix(6);
    const Normalized relativePath(relative);
    const auto spt = vfs.exists(source) ? Normalized(source) : NormalizedView("trees") / relativePath;
    const auto stream = vfs.find(spt);
    auto materials = stream ? readMaterials(*stream) : Materials{};
    auto sprite = textureImage(NormalizedView("textures/trees/billboards"), relativePath, vfs, images);
    auto bark = textureImage(NormalizedView("textures/trees/branches"), materials.mBark, vfs, images);
    auto leaves = textureImage(NormalizedView("textures/trees/leaves"), model.mLeaves, vfs, images);
    if (!sprite && !bark && !leaves)
        throw std::runtime_error("Missing vegetation assets for " + model.mSource.value());

    const float h = model.mHeight > 0 && std::isfinite(model.mHeight) ? model.mHeight : 512.f;
    const float w = model.mWidth > 0 && std::isfinite(model.mWidth) ? model.mWidth : h;
    auto shape = measure(sprite);
    if (shape.mBottom >= shape.mTop)
        shape = measure(nullptr);
    if (!leaves)
        materials.mLeafRects.clear();
    else if (materials.mLeafRects.empty())
        materials.mLeafRects.push_back({ osg::Vec2f(0, 1), { 1, 1 }, { 1, 0 }, { 0, 0 } });
    const auto barkState = material(bark, false);
    const auto spriteState = sprite ? material(sprite, true) : nullptr;
    const auto leafState = leaves ? material(leaves, true) : spriteState;
    std::uint32_t seed = 2166136261u;
    for (const unsigned char ch : relative)
        seed = (seed ^ ch) * 16777619u;
    const Misc::Rng::Generator rng(seed);
    const float nearRange = std::clamp(std::max(w, h) * 1.9f, 450.f, 3800.f);
    const float midRange = nearRange * 2.6f;
    osg::ref_ptr<osg::LOD> lod = new osg::LOD;
    lod->setCenter(osg::Vec3f(0, 0, shape.mTop * h / 2));
    lod->addChild(branches(shape, w, h, barkState, leafState, materials.mLeafRects, rng, false), 0, nearRange);
    if (sprite)
    {
        lod->addChild(impostor(shape, w, h, 3, spriteState), nearRange, midRange);
        lod->addChild(impostor(shape, w, h, 2, spriteState), midRange, std::numeric_limits<float>::max());
    }
    else
        lod->addChild(branches(shape, w, h, barkState, leafState, materials.mLeafRects, rng, true), nearRange,
            std::numeric_limits<float>::max());
    return lod;
}
