#include "ReservoirSampling.h"

const static int seeds_num = 7800;

using namespace std;

ReservoirSampling::ReservoirSampling()
{
	visited_num = 0;
	seeds.resize(seeds_num);
	elected_points = make_unique<FilteredPointSet>();
	removed_cache = make_unique<PointSet>();
	int_dis = uniform_int_distribution<>(0, seeds_num-1);
}

pair<TempPointSet, TempPointSet>* ReservoirSampling::execute(const FilteredPointSet* origin, bool is_first_frame)
{
	vector<bool> modified(seeds.size(), false);
	unordered_set<uint> _added;

	auto it = origin->cbegin();
	if (is_first_frame) {
		visited_num = 0;
		elected_points->clear();
		removed_cache->clear();
		W = exp(log(double_dist(gen) / seeds_num));
	}
	if (visited_num < seeds_num) {
		fill(modified.begin() + visited_num, modified.end(), true);

		for (; it != origin->cend() && visited_num < seeds_num; ++it) {
			seeds[visited_num] = it->first;
			elected_points->emplace(it->first, make_unique<LabeledPoint>(it->second));
			_added.emplace(it->first);
			++visited_num;
		}
		if(visited_num == seeds_num)
			next_target = visited_num + (int)floor(log(double_dist(gen)) / log(1 - W)) + 1;
	}
	
	TempPointSet removed;
	for (; it != origin->cend(); ++it) {
		++visited_num;
		if (visited_num > next_target) {
			int idx = int_dis(gen);
			if (modified[idx]) {
				_added.erase(seeds[idx]);
				_added.emplace(it->first);
			}
			else {
				removed.push_back(elected_points->at(seeds[idx]).get());
				removed_cache->push_back(move(elected_points->at(seeds[idx])));
				_added.emplace(it->first);
				modified[idx] = true;
			}
			elected_points->erase(seeds[idx]);
			elected_points->emplace(it->first, make_unique<LabeledPoint>(it->second));
			seeds[idx] = it->first;

			W *= exp(log(double_dist(gen)) / seeds_num);
			next_target += (int)floor(log(double_dist(gen)) / log(1 - W)) + 1;
		}
	}

	TempPointSet added;
	for (auto& idx : _added) {
		added.push_back(elected_points->at(idx).get());
	}

	qDebug() << ((int)added.size() - (int)removed.size());
	return new pair<TempPointSet, TempPointSet>(removed, added);
}
