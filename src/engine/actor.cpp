#include "engine/engine.h"

namespace sdk {

	std::uint64_t sdk::actor::userid() const {

		std::uint64_t userId = drive->read<std::uint64_t>(this->Address + offset::player::UserId);
		return userId;
	}

	std::uint64_t sdk::actor::teamid() const {

		return drive->read<uint64_t>(this->Address + offset::player::team);
	}

	sdk::actor sdk::actor::local() const {

		std::uint64_t local_address = drive->read<std::uint64_t>(this->Address + offset::player::LocalPlayer);
		return sdk::actor{ local_address };
	}

	sdk::instance sdk::instance::character() const {

		if (!this->Address)
		return sdk::instance();
		return drive->read<sdk::actor>(this->Address + offset::player::ModelInstance);
	}


	std::string actor::display() const
	{
		if (!this->Address)
		return "?";
		return drive->readstring(this->Address + offset::player::DisplayName);
	}
}
