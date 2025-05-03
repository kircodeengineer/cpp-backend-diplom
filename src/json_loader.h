#pragma once

#include <filesystem>

#include "model.h"

namespace json_loader {

	void LoadGame(model::Game& game, const std::filesystem::path& json_path);

}  // namespace json_loader
