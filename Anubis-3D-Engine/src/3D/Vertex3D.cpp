#include "3D/Vertex3D.hpp"

namespace Anubis {
	Vertex3D::Vertex3D() {
		_x = _y = _z = 0.f;
		_nx = _ny = _nz = 0.f;
		_u = _v = 0.f;
	}

	Vertex3D::Vertex3D(float x, float y, float z, float nx, float ny, float nz, float u, float v) {
		_x = x;
		_y = y;
		_z = z;

		_nx = nx;
		_ny = ny;
		_nz = nz;

		_u = u;
		_v = v;
	}
}