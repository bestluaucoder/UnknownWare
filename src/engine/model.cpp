#include "engine/engine.h"

namespace sdk {

	std::uint64_t sdk::model::place() const {

		return drive->read<uint64_t>(this->Address + offset::datamodel::PlaceId);
	}

	std::uint64_t sdk::model::game() const {

		return drive->read<uint64_t>(this->Address + offset::datamodel::GameId);
	}

	std::uint64_t sdk::model::creator() const {

		return drive->read<uint64_t>(this->Address + offset::datamodel::CreatorId);
	}

	std::uint64_t sdk::model::server() const {

		return drive->read<uint64_t>(this->Address + offset::datamodel::ServerIP);
	}
}
