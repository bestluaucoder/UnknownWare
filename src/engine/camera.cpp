#include "engine/engine.h"

namespace sdk {

	sdk::vector3 sdk::camera::position() const {

		return drive->read<sdk::vector3>(this->Address + offset::camera::Position);
	}

	sdk::matrix3 sdk::camera::rotation() const {

		return drive->read<matrix3>(this->Address + offset::camera::Rotation);
	}

	void sdk::camera::position(const sdk::vector3& pos) const {

		drive->write<sdk::vector3>(this->Address + offset::camera::Position, pos);
	}

	void sdk::camera::rotation(const sdk::matrix3& rot) const {

		drive->write<sdk::matrix3>(this->Address + offset::camera::Rotation, rot);
	}

	vector2 sdk::camera::viewportat(vector2 target_screen_pos, vector2 screen_size) {

		vector2 result;
		result.x = (int16_t)(2 * (screen_size.x - target_screen_pos.x));
		result.y = (int16_t)(2 * (screen_size.y - target_screen_pos.y));
		return result;
	}

	void sdk::camera::viewport(sdk::viewport Vp) const {

		drive->write<sdk::viewport>(this->Address + offset::camera::Viewport, Vp);
	}
}
