#include "matrixtransform.hpp"

#include <components/nif/nativetransform.hpp>

#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace NifOsg
{
    struct MatrixTransform::TraversalMatrix::Data
    {
        osg::Matrixd mMatrix, mInverse;
    };

    MatrixTransform::TraversalMatrix::TraversalMatrix(const osg::Matrixd& matrix)
    {
        for (unsigned i = 0; i < 16; ++i)
            if (!std::isfinite(matrix.ptr()[i]))
                throw std::invalid_argument("nonfinite NIF traversal matrix");
        osg::Matrixd inverse;
        if (!inverse.invert(matrix))
            throw std::invalid_argument("singular NIF traversal matrix");
        for (unsigned i = 0; i < 16; ++i)
            if (!std::isfinite(inverse.ptr()[i]))
                throw std::invalid_argument("nonfinite NIF traversal inverse");
        mData = std::make_shared<const Data>(Data{matrix, inverse});
    }

    const osg::Matrixd* MatrixTransform::TraversalMatrix::getMatrix() const noexcept
    {
        return mData ? &mData->mMatrix : nullptr;
    }

    bool MatrixTransform::TraversalMatrix::operator==(const TraversalMatrix& other) const noexcept
    {
        if (mData == other.mData) return true;
        if (!mData || !other.mData) return false;
        for (unsigned i = 0; i < 16; ++i)
            if (std::bit_cast<std::uint64_t>(mData->mMatrix.ptr()[i])
                != std::bit_cast<std::uint64_t>(other.mData->mMatrix.ptr()[i])) return false;
        return true;
    }

    void MatrixTransform::setTraversalMatrix(const TraversalMatrix& matrix) noexcept
    {
        mTraversalMatrix = matrix;
        dirtyBound();
    }

    bool MatrixTransform::computeLocalToWorldMatrix(osg::Matrix& matrix, osg::NodeVisitor* visitor) const
    {
        if (!mTraversalMatrix.mData)
            return osg::MatrixTransform::computeLocalToWorldMatrix(matrix, visitor);
        if (_referenceFrame == RELATIVE_RF) matrix.preMult(mTraversalMatrix.mData->mMatrix);
        else matrix = mTraversalMatrix.mData->mMatrix;
        return true;
    }

    bool MatrixTransform::computeWorldToLocalMatrix(osg::Matrix& matrix, osg::NodeVisitor* visitor) const
    {
        if (!mTraversalMatrix.mData)
            return osg::MatrixTransform::computeWorldToLocalMatrix(matrix, visitor);
        if (_referenceFrame == RELATIVE_RF) matrix.postMult(mTraversalMatrix.mData->mInverse);
        else matrix = mTraversalMatrix.mData->mInverse;
        return true;
    }

    MatrixTransform::MatrixTransform(const Nif::NiTransform& transform)
        : osg::MatrixTransform(transform.toMatrix())
        , mScale(transform.mScale)
        , mRotationScale(transform.mRotation)
    {
    }

    MatrixTransform::MatrixTransform(const MatrixTransform& copy, const osg::CopyOp& copyop)
        : osg::MatrixTransform(copy, copyop)
        , mScale(copy.mScale)
        , mRotationScale(copy.mRotationScale)
        , mTraversalMatrix(copy.mTraversalMatrix)
    {
    }

    Nif::NiTransform MatrixTransform::getControllerTransform() const
    {
        const auto& node = *this;
        const auto require = [](bool condition, const char* message) {
            if (!condition) throw std::invalid_argument(message);
        };
        require(std::isfinite(node.mScale) && node.mScale != 0, "singular NIF controller scale");
        const auto& matrix = node.getMatrix();
        require(matrix(3, 3) == 1, "nonaffine NIF controller matrix");
        auto local = Nif::NiTransform::getIdentity();
        local.mScale = node.mScale;
        bool cacheMatchesDrawing = true;
        for (unsigned row = 0; row < 3; ++row)
        {
            require(matrix(row, 3) == 0, "projective NIF controller matrix");
            require(Nif::tryStoreNativeTransformFloat(matrix(3, row), local.mTranslation[row]),
                "invalid NIF controller translation");
            for (unsigned column = 0; column < 3; ++column)
            {
                const float cached = node.mRotationScale.mValues[row][column];
                require(std::isfinite(cached), "invalid NIF controller rotation cache");
                float drawing;
                require(Nif::tryStoreNativeTransformFloat(matrix(column, row), drawing),
                    "invalid NIF controller drawing rotation");
                const float scaled = cached * node.mScale;
                cacheMatchesDrawing &= std::bit_cast<std::uint32_t>(drawing)
                    == std::bit_cast<std::uint32_t>(scaled);
            }
        }
        if (cacheMatchesDrawing)
            local.mRotation = node.mRotationScale;
        else
            // Raw procedural controllers can deliberately leave the NIF
            // rotation cache unchanged. Only that path needs reconstruction.
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned column = 0; column < 3; ++column)
                    require(Nif::tryStoreNativeTransformFloat(matrix(column, row) / double(node.mScale),
                        local.mRotation.mValues[row][column]), "invalid NIF controller rotation");
        return local;
    }

    void MatrixTransform::setScale(float scale)
    {
        // Update the decomposed scale.
        mScale = scale;

        // Rescale the node using the known components.
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                _matrix(i, j) = mRotationScale.mValues[j][i] * mScale; // NB: column/row major difference

        _inverseDirty = true;
        dirtyBound();
    }

    void MatrixTransform::setRotation(const osg::Quat& rotation)
    {
        // First override the rotation ignoring the scale.
        _matrix.setRotate(rotation);

        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                // Update the current decomposed rotation and restore the known scale.
                mRotationScale.mValues[j][i] = static_cast<float>(_matrix(i, j)); // NB: column/row major difference
                _matrix(i, j) *= mScale;
            }
        }

        _inverseDirty = true;
        dirtyBound();
    }

    void MatrixTransform::setRotation(const Nif::Matrix3& rotation)
    {
        // Update the decomposed rotation.
        mRotationScale = rotation;

        // Reorient the node using the known components.
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                _matrix(i, j) = mRotationScale.mValues[j][i] * mScale; // NB: column/row major difference

        _inverseDirty = true;
        dirtyBound();
    }

    void MatrixTransform::setTranslation(const osg::Vec3f& translation)
    {
        // The translation is independent from the rotation and scale so we can apply it directly.
        _matrix.setTrans(translation);

        _inverseDirty = true;
        dirtyBound();
    }
}
