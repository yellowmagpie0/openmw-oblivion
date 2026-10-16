#ifndef OPENMW_COMPONENTS_NIFOSG_MATRIXTRANSFORM_H
#define OPENMW_COMPONENTS_NIFOSG_MATRIXTRANSFORM_H

#include <components/nif/niftypes.hpp>

#include <osg/MatrixTransform>

#include <memory>

namespace NifOsg
{

    class MatrixTransform : public osg::MatrixTransform
    {
    public:
        MatrixTransform() = default;
        MatrixTransform(const Nif::NiTransform& transform);
        MatrixTransform(const MatrixTransform& copy, const osg::CopyOp& copyop);

        META_Node(NifOsg, MatrixTransform)

        // Optional local-to-parent traversal pose, independent of getMatrix()
        // and the NIF components used by controllers. Prepare immutable handles
        // before publication; copying/restoring a handle does not allocate.
        class TraversalMatrix
        {
        public:
            TraversalMatrix() = default;
            explicit TraversalMatrix(const osg::Matrixd& matrix);
            const osg::Matrixd* getMatrix() const noexcept;
            bool operator==(const TraversalMatrix& other) const noexcept;

        private:
            struct Data;
            std::shared_ptr<const Data> mData;
            friend class MatrixTransform;
        };

        const TraversalMatrix& getTraversalMatrix() const noexcept { return mTraversalMatrix; }
        void setTraversalMatrix(const TraversalMatrix& matrix) noexcept;
        void setTraversalMatrix(const osg::Matrixd& matrix) { setTraversalMatrix(TraversalMatrix(matrix)); }
        void clearTraversalMatrix() noexcept { setTraversalMatrix(TraversalMatrix{}); }

        bool computeLocalToWorldMatrix(osg::Matrix& matrix, osg::NodeVisitor* visitor) const override;
        bool computeWorldToLocalMatrix(osg::Matrix& matrix, osg::NodeVisitor* visitor) const override;

        // Hack: account for Transform differences between OSG and NIFs.
        // OSG uses a 4x4 matrix, NIF's use a 3x3 rotationScale, float scale, and vec3 position.
        // Decomposing the original components from the 4x4 matrix isn't possible, which causes
        // problems when a KeyframeController wants to change only one of these components. So
        // we store the scale and rotation components separately here.
        float mScale{ 0.f };
        Nif::Matrix3 mRotationScale;

        // Utility methods to transform the node and keep these components up-to-date.
        // The matrix's components should not be overridden manually or using preMult/postMult
        // unless you're sure you know what you are doing.
        virtual void setScale(float scale);
        virtual void setRotation(const osg::Quat& rotation);
        virtual void setRotation(const Nif::Matrix3& rotation);
        virtual void setTranslation(const osg::Vec3f& translation);

    private:
        TraversalMatrix mTraversalMatrix;
    };

}

#endif
