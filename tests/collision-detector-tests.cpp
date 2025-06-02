#define _USE_MATH_DEFINES


#include "../src/collision_detector.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_templated.hpp>
#include <catch2/matchers/catch_matchers_predicate.hpp>
#include <catch2/matchers/catch_matchers_contains.hpp>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <ranges>
#include <vector>

using namespace std::literals;

namespace Catch {
	template<>
	struct StringMaker<collision_detector::GatheringEvent> {
	  static std::string convert(collision_detector::GatheringEvent const& value) {
	      std::ostringstream tmp;
	      tmp << "(" << value.gatherer_id << "," << value.item_id << "," << value.sq_distance << "," << value.time << ")";

	      return tmp.str();
	  }
	};
}  // namespace Catch 

class TestProvider: public collision_detector::ItemGathererProvider{
public:
	using Items = std::vector<collision_detector::Item>;
    using Gatherers = std::vector <collision_detector::Gatherer>;
	size_t ItemsCount() const{
		return items_.size();
	}

	collision_detector::Item GetItem(size_t idx) const{
		return items_[idx];
	}

	size_t GatherersCount() const{
		return gatherers_.size();
	}

	collision_detector::Gatherer GetGatherer(size_t idx) const{
		return gatherers_[idx];
	}

	void AddGatherer(collision_detector::Gatherer& new_gath){
		gatherers_.emplace_back(new_gath);
	}

	void AddItem(collision_detector::Item& new_item){
		items_.emplace_back(new_item);
	}
	
	const Gatherers& GetGatherers(){
		return gatherers_;
	}

private:
	Items items_;
	Gatherers gatherers_;
};


template <typename Range, typename Predicate>
struct EqualsRangeMatcher : Catch::Matchers::MatcherGenericBase {
    EqualsRangeMatcher(Range const& range, Predicate predicate)
        : range_{range}
        , predicate_{predicate} {
    }

    template <typename OtherRange>
    bool match(const OtherRange& other) const {
        return std::equal(std::begin(range_), std::end(range_), std::begin(other), std::end(other), predicate_);
    }

    std::string describe() const override {
        return "Equals: " + Catch::rangeToString(range_);
    }

private:
    const Range& range_;
    Predicate predicate_;
};

template <typename Range, typename Predicate>
auto EqualsRange(const Range& range, Predicate prediate) {
    return EqualsRangeMatcher<Range, Predicate>{range, prediate};
}

class CompareGatheringEvents {
public:
    bool operator()(const collision_detector::GatheringEvent& l,
                    const collision_detector::GatheringEvent& r) {
        if (l.gatherer_id != r.gatherer_id || l.item_id != r.item_id)
            return false;

        static const double EPS = 1e-10;

        if (std::abs(l.sq_distance - r.sq_distance) > EPS) {
            return false;
        }

        if (std::abs(l.time - r.time) > EPS) {
            return false;
        }
        return true;
    }
};

bool operator==(const collision_detector::Item& lh, const collision_detector::Item &rh) {
        return lh.position == rh.position && lh.width == rh.width;
}
bool operator==(const collision_detector::Gatherer& lh, const collision_detector::Gatherer &rh) {
        return lh.start_pos == rh.start_pos && lh.end_pos == rh.end_pos && lh.width == rh.width;
}

using Catch::Matchers::Predicate;
using Catch::Matchers::Contains; 

SCENARIO("Test TestProvider") {
	collision_detector::Item item = {.position = {1,1}, .width = 0.2};
	collision_detector::Gatherer gatherer{.start_pos = {0,0}, .end_pos = {0,0}, .width = 0.1};
	collision_detector::Gatherer gatherer2{.start_pos = {2,2}, .end_pos = {2,2}, .width = 0.1};
	TestProvider test_provider;
	WHEN("Conteiner emtpy") {
		CHECK(test_provider.GatherersCount() == 0);
		CHECK(test_provider.ItemsCount() == 0);
	}
	THEN("Add one item") {
		test_provider.AddItem(item);
		THEN("one item") {
			CHECK(test_provider.ItemsCount() == 1);
		}
		AND_THEN("check item") {
			CHECK(test_provider.GetItem(0) == item);
		}
	}
	AND_THEN("Add gatherer") {
		test_provider.AddGatherer(gatherer);
		THEN("one getharer") {
			CHECK(test_provider.GatherersCount() == 1);
		}
		AND_THEN("check gatherer") {
			CHECK(test_provider.GetGatherer(0) == gatherer);
		}
	}
	AND_THEN("Not contains") {
		auto GathersIsEqual = [&](const collision_detector::Gatherer& r){
			return gatherer2 == r;
		};
		CHECK_THAT(test_provider.GetGatherers(), !Contains(Predicate<const collision_detector::Gatherer&>(GathersIsEqual))); 
	}
}
