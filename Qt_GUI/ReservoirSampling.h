#pragma once
#include <random>
#include <unordered_set>

#include "global.h"

class ReservoirSampling
{
public:
	ReservoirSampling();
	Indices selectSeeds() { return seeds; }
	std::pair<TempPointSet, TempPointSet>* execute(const FilteredPointSet* origin, bool is_first_frame);

private:
	Indices seeds;
	std::unique_ptr<FilteredPointSet> elected_points;
	std::unique_ptr<PointSet> removed_cache;

	int visited_num, next_target;
	double W;

	std::mt19937 gen{ std::random_device{}() };
	std::uniform_real_distribution<> double_dist;
	std::uniform_int_distribution<> int_dis;
};

