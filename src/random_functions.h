#pragma once
#include <string>

namespace random_functions {
/// @brief 16ричное число в строке заданной длины
/// @param length
/// @return
std::string random_hex_string(std::size_t length);

/// @brief рандомное целое число в заданном диапазоне
/// @param from
/// @param to
/// @return
int random_int_number(int from, int to);

/// @brief  рандомное число от 0 до заданного значения
/// @param max_num
/// @return
int random_number_from_zero(int max_num);

/// @brief рандомное число с плавающей точкой в заданном диапазоне
/// @param from
/// @param to
/// @return
double random_double_number(int from, int to);
}  // namespace random_functions
