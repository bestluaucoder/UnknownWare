#include "engine/engine.h"

namespace sdk {

	sdk::vector2 sdk::render::size() const {

		return drive->read<sdk::vector2>(this->Address + offset::render::Dimensions);
	}

	sdk::matrix4 sdk::render::matrix() const {

		return drive->read<sdk::matrix4>(this->Address + offset::render::ViewMatrix);
	}

	sdk::vector2 sdk::render::screen(const sdk::vector3& world)  const {

		sdk::matrix4 ViewMatrix = matrix();
		sdk::vector2 Dimensions = size();
		sdk::vector4 Q;
		Q.x = (world.x * ViewMatrix.data[0]) + (world.y * ViewMatrix.data[1]) + (world.z * ViewMatrix.data[2]) + ViewMatrix.data[3];
		Q.y = (world.x * ViewMatrix.data[4]) + (world.y * ViewMatrix.data[5]) + (world.z * ViewMatrix.data[6]) + ViewMatrix.data[7];
		Q.z = (world.x * ViewMatrix.data[8]) + (world.y * ViewMatrix.data[9]) + (world.z * ViewMatrix.data[10]) + ViewMatrix.data[11];
		Q.w = (world.x * ViewMatrix.data[12]) + (world.y * ViewMatrix.data[13]) + (world.z * ViewMatrix.data[14]) + ViewMatrix.data[15];
		if (Q.w < 0.1f)
			return { -1, -1 };
		sdk::vector3 NDC;
		NDC.x = Q.x / Q.w;
		NDC.y = Q.y / Q.w;
		NDC.z = Q.z / Q.w;
		sdk::vector2 ScreenPosition =
		{
			(Dimensions.x / 2 * NDC.x) + (Dimensions.x / 2),
			-(Dimensions.y / 2 * NDC.y) + (Dimensions.y / 2),
		};
		if (ScreenPosition.x < 0 || ScreenPosition.x > Dimensions.x || ScreenPosition.y < 0 || ScreenPosition.y > Dimensions.y)
			return { -1, -1 };
		return ScreenPosition;
	}
}
