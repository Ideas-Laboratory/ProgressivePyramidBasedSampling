#pragma once
#include "global.h"
#include "utils.h"

class RandomSampling
{
public:
	RandomSampling() {}
	Indices selectSeeds() { return seeds; }
	std::pair<PointSet, PointSet>* execute(const FilteredPointSet* origin);

	static uint seeds_num;
private:
	FilteredPointSet dataset;
	Indices existing_points;
	Indices seeds;
};

