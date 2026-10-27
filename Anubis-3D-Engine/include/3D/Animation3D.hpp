#pragma once

#include <array>
#include <string>
#include <vector>

namespace Anubis {

    enum class AnimationPath {
        Translation,
        Rotation,
        Scale
    };

    enum class AnimationInterpolation {
        Linear,
        Step,
        CubicSpline
    };

    struct AnimationSampler3D {
        AnimationInterpolation _interpolation =
            AnimationInterpolation::Linear;

        // Keyframe times in seconds.
        std::vector<float> _times;

        // Translation and scale use XYZ.
        // Rotation uses glTF quaternion order: XYZW.
        // Cubic spline stores: in tangent, value, out tangent.
        std::vector<std::array<float, 4>> _values;
    };

    struct AnimationChannel3D {
        // Index into Animation3D::_samplers.
        int _samplerIndex = -1;

        // Index into Model3D::_nodes.
        int _nodeIndex = -1;

        AnimationPath _path = AnimationPath::Translation;
    };

    struct Animation3D {
        std::string _name;

        std::vector<AnimationSampler3D> _samplers;
        std::vector<AnimationChannel3D> _channels;

        float _duration = 0.f;
    };

}