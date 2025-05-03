#pragma once

#include "geom.h"

#include <algorithm>
#include <vector>

namespace collision_detector {

	struct CollectionResult {
		bool IsCollected(double collect_radius) const {
			return proj_ratio >= 0 && proj_ratio <= 1 && sq_distance <= collect_radius * collect_radius;
		}

		// квадрат расстояния до точки
		double sq_distance;

		// доля пройденного отрезка
		double proj_ratio;
	};

	// Движемся из точки a в точку b и пытаемся подобрать точку c.
	// Эта функция реализована в уроке.
	CollectionResult TryCollectPoint(geom::Point2D a, geom::Point2D b, geom::Point2D c);

	// Предмет
	struct Item {
		// позиция
		geom::Point2D position;
		// размер
		double width;
	};

	// Собиратель 
	struct Gatherer {
		// начало отрезка перемещения
		geom::Point2D start_pos;
		// конец отрезка перемещения
		geom::Point2D end_pos;
		// размер
		double width;
	};

	class ItemGathererProvider {
	protected:
		~ItemGathererProvider() = default;

	public:
		virtual size_t ItemsCount() const = 0;
		virtual Item GetItem(size_t idx) const = 0;
		virtual size_t GatherersCount() const = 0;
		virtual Gatherer GetGatherer(size_t idx) const = 0;
	};

	// Событие столкновения
	struct GatheringEvent {
		// индекс предмета
		size_t item_id;
		// индекс собирателя
		size_t gatherer_id;
		// квадратом расстояния до предмета
		double sq_distance;
		// относительное временя столкновения собирателя с предметом (число от 0 до 1).
		double time;
	};

	std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider);

}  // namespace collision_detector
