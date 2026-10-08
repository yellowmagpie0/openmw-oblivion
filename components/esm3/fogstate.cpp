#include "fogstate.hpp"

#include "esmreader.hpp"
#include "esmwriter.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

#include <osgDB/ReadFile>

#include <components/debug/debuglog.hpp>
#include <components/files/memorystream.hpp>
#include <components/misc/constants.hpp>

namespace ESM
{
    bool isUsableFogImage(const osg::Image& image)
    {
        return image.s() == FogTexture::Resolution && image.t() == FogTexture::Resolution && image.r() == 1
            && image.getPixelFormat() == GL_RGBA && image.getDataType() == GL_UNSIGNED_BYTE
            && image.isDataContiguous() && image.getTotalSizeInBytes() == FogTexture::Resolution * FogTexture::Resolution * 4;
    }

    FogState prepareFogState(const FogState& state, bool interior)
    {
        if (interior)
        {
            for (float value : {state.mNorthMarkerAngle, state.mBounds.mMinX, state.mBounds.mMinY,
                     state.mBounds.mMaxX, state.mBounds.mMaxY, state.mCenterX, state.mCenterY})
                if (!std::isfinite(value))
                    throw std::runtime_error("Saved local-map fog has a nonfinite interior value");
            // Local-map coordinates and segment counts are represented by int.
            // Compute in double so finite float subtraction cannot overflow first.
            const auto fitsGrid = [](double value) {
                const double index = std::ceil(value / Constants::CellSizeInUnits);
                return index >= std::numeric_limits<int>::min() && index <= std::numeric_limits<int>::max();
            };
            const double width = double(state.mBounds.mMaxX) - state.mBounds.mMinX;
            const double height = double(state.mBounds.mMaxY) - state.mBounds.mMinY;
            if (width < 0 || height < 0 || !fitsGrid(width) || !fitsGrid(height))
                throw std::runtime_error("Saved local-map fog has unrepresentable interior bounds");
            for (float value : {state.mBounds.mMinX, state.mBounds.mMinY, state.mBounds.mMaxX,
                     state.mBounds.mMaxY, state.mCenterX, state.mCenterY})
                if (!fitsGrid(value))
                    throw std::runtime_error("Saved local-map fog has unrepresentable interior bounds");
        }
        FogState result = state;
        for (auto& texture : result.mFogTextures)
        {
            texture.mPrepared = true;
            texture.mPreparedImage = nullptr;
            if (texture.mImageData.empty()) continue;
            // Bound recognized PNG dimensions before the codec allocates pixels.
            static constexpr unsigned char signature[]{137, 80, 78, 71, 13, 10, 26, 10};
            const auto& data = texture.mImageData;
            if (data.size() >= 24 && std::memcmp(data.data(), signature, 8) == 0
                && std::memcmp(data.data() + 12, "IHDR", 4) == 0)
            {
                const auto dimension = [&](std::size_t offset) {
                    std::uint32_t value = 0;
                    for (std::size_t i = offset; i != offset + 4; ++i)
                        value = (value << 8) | static_cast<unsigned char>(data[i]);
                    return value;
                };
                if (dimension(16) != FogTexture::Resolution || dimension(20) != FogTexture::Resolution)
                    throw std::runtime_error("Saved local-map fog image must be 32x32 RGBA unsigned bytes");
            }
            auto* codec = osgDB::Registry::instance()->getReaderWriterForExtension("png");
            if (!codec)
                throw std::runtime_error("Local-map fog PNG decoder is unavailable");
            Files::IMemStream stream(texture.mImageData.data(), texture.mImageData.size());
            auto decoded = codec->readImage(stream);
            if (!decoded.success() || !decoded.getImage())
            {
                // Existing local-map restoration skips unreadable optional fog images.
                Log(Debug::Warning) << "Skipping unreadable saved local-map fog: " << decoded.message();
                continue;
            }
            if (!isUsableFogImage(*decoded.getImage()))
                throw std::runtime_error("Saved local-map fog image must be 32x32 RGBA unsigned bytes");
            texture.mPreparedImage = decoded.getImage();
            texture.mPreparedImage->flipVertical();
        }
        return result;
    }

    namespace
    {
        void convertFogOfWar(std::vector<char>& imageData)
        {
            if (imageData.empty())
            {
                return;
            }

            osgDB::ReaderWriter* tgaReader = osgDB::Registry::instance()->getReaderWriterForExtension("tga");
            if (!tgaReader)
            {
                Log(Debug::Error) << "Error: Unable to load fog, can't find a tga ReaderWriter";
                return;
            }

            Files::IMemStream in(imageData.data(), imageData.size());

            osgDB::ReaderWriter::ReadResult result = tgaReader->readImage(in);
            if (!result.success())
            {
                Log(Debug::Error) << "Error: Failed to read fog: " << result.message() << " code " << result.status();
                return;
            }

            osgDB::ReaderWriter* pngWriter = osgDB::Registry::instance()->getReaderWriterForExtension("png");
            if (!pngWriter)
            {
                Log(Debug::Error) << "Error: Unable to write fog, can't find a png ReaderWriter";
                return;
            }

            std::ostringstream ostream;
            osgDB::ReaderWriter::WriteResult png = pngWriter->writeImage(*result.getImage(), ostream);
            if (!png.success())
            {
                Log(Debug::Error) << "Error: Unable to write fog: " << png.message() << " code " << png.status();
                return;
            }

            std::string str = ostream.str();
            imageData = std::vector<char>(str.begin(), str.end());
        }

    }

    void FogState::load(ESMReader& esm)
    {
        if (esm.isNextSub("BOUN"))
            esm.getHT(mBounds.mMinX, mBounds.mMinY, mBounds.mMaxX, mBounds.mMaxY);
        esm.getHNOT(mNorthMarkerAngle, "ANGL");
        if (!esm.getHNOT("CNTR", mCenterX, mCenterY))
        {
            mCenterX = (mBounds.mMinX + mBounds.mMaxX) / 2;
            mCenterY = (mBounds.mMinY + mBounds.mMaxY) / 2;
        }
        const FormatVersion dataFormat = esm.getFormatVersion();
        while (esm.isNextSub("FTEX"))
        {
            esm.getSubHeader();
            FogTexture tex;

            esm.getT(tex.mX);
            esm.getT(tex.mY);

            const std::size_t imageSize = esm.getSubSize() - sizeof(int32_t) * 2;
            tex.mImageData.resize(imageSize);
            esm.getExact(tex.mImageData.data(), imageSize);

            if (dataFormat <= MaxOldFogOfWarFormatVersion)
                convertFogOfWar(tex.mImageData);

            mFogTextures.push_back(std::move(tex));
        }
    }

    void FogState::save(ESMWriter& esm, bool interiorCell) const
    {
        if (interiorCell)
        {
            esm.writeHNT("BOUN", mBounds);
            esm.writeHNT("ANGL", mNorthMarkerAngle);
            esm.startSubRecord("CNTR");
            esm.writeT(mCenterX);
            esm.writeT(mCenterY);
            esm.endRecord("CNTR");
        }
        for (const FogTexture& texture : mFogTextures)
        {
            esm.startSubRecord("FTEX");
            esm.writeT(texture.mX);
            esm.writeT(texture.mY);
            esm.write(texture.mImageData.data(), texture.mImageData.size());
            esm.endRecord("FTEX");
        }
    }

}
