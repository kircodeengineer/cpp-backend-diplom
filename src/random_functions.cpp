#include "random_functions.h"

#include <random>

namespace random_functions {
	std::string RandomHexString(std::size_t length) {
		const std::string CHARACTERS = "0123456789abcdef";
		int chars_count = static_cast<int>(CHARACTERS.size());
		std::random_device random_device;
		std::mt19937 generator(random_device());
		std::uniform_int_distribution<> distribution(0, chars_count - 1);

		std::string random_string;

		for (std::size_t i = 0; i < length; ++i) {
			random_string += CHARACTERS[distribution(generator)];
		}

		return random_string;
	}

	int RandomIntNumber(int from, int to) {
		std::random_device random_device;
		std::mt19937 generator(random_device());
		std::uniform_int_distribution<> distribution(from, to);
		return distribution(generator);
	}

	int RandomNumberFromZero(int max_num) {
		std::random_device random_device;
		std::mt19937 generator(random_device());
		std::uniform_int_distribution<> distribution(0, max_num);
		return distribution(generator);
	}

	double RandomDoubleNumber(int from, int to) {
		std::random_device random_device;
		std::mt19937 generator(random_device());
		std::uniform_real_distribution<> distribution(from, to);
		return distribution(generator);
	}
}  // namespace random_functions
