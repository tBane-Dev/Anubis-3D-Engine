#pragma once

#include <array>
#include <string>
#include <vector>

#include "3D/Quaternion.hpp"

namespace Anubis {

    using Matrix4 = std::array<float, 16>;

    inline Matrix4 identityMatrix4() {
        // Column-major layout.
        return {
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f
        };
    }

    struct Node3D {
        std::string _name;

        int _parent = -1;
        std::vector<int> _children;

        int _mesh = -1;
        int _skin = -1;

        // Default local transform loaded from the file.
        std::array<float, 3> _translation{0.f, 0.f, 0.f};
        Quaternion _rotation{};
        std::array<float, 3> _scale{1.f, 1.f, 1.f};

        // Some nodes provide a matrix instead of TRS.
        bool _hasMatrix = false;
        Matrix4 _matrix = identityMatrix4();

        // Current transforms evaluated during animation.
        Matrix4 _localMatrix = identityMatrix4();
        Matrix4 _globalMatrix = identityMatrix4();

        // Current local pose, separate from the default transform.
        std::array<float, 3> _poseTranslation{ 0.f, 0.f, 0.f };
        Quaternion _poseRotation{};
        std::array<float, 3> _poseScale{ 1.f, 1.f, 1.f };
    };

    struct Skin3D {
        std::string _name;

        // Optional root node of the skeleton.
        int _skeletonRoot = -1;

        // Indices into Model3D::_nodes.
        std::vector<int> _joints;

        // One matrix per entry in _joints.
        std::vector<Matrix4> _inverseBindMatrices;
    };

}