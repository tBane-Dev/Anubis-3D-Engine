#pragma once
#include <array>

namespace Anubis {
    struct Vertex3D
    {
        float _x, _y, _z;      // position
        float _nx, _ny, _nz;   // normal
        float _u, _v;          // texcoord

        // bones
        std::array<uint16_t, 4> _joints{};
        std::array<float, 4> _weights{};

        Vertex3D();
        Vertex3D(float x, float y, float z, float nx, float ny, float nz, float u, float v);

    };


}