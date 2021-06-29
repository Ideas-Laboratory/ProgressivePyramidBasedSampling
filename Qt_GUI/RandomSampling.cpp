#include "RandomSampling.h"

using namespace std;

uint RandomSampling::seeds_num = 5500;

std::pair<PointSet, PointSet>* RandomSampling::execute(const FilteredPointSet * origin)
{
	Indices new_points;
	for (auto &pr : *origin) {
		new_points.push_back(pr.first);
		dataset.emplace(pr.first, make_unique<LabeledPoint>(pr.second));
	}
	existing_points.insert(existing_points.end(), new_points.begin(), new_points.end());
	
	chrono::time_point<chrono::steady_clock> start = chrono::high_resolution_clock::now();
	uint num = min(seeds_num, (uint)existing_points.size());
	std::random_shuffle(existing_points.begin(), existing_points.end());
	Indices result(num);
	std::copy_n(existing_points.begin(), num, result.begin());
	qDebug() << "random: " << (double)(chrono::high_resolution_clock::now() - start).count() / 1e9;
	
	Indices removed_idx, added_idx;
	sort(result.begin(), result.end());
	set_difference(seeds.begin(), seeds.end(), result.begin(), result.end(), back_inserter(removed_idx));
	set_difference(result.begin(), result.end(), seeds.begin(), seeds.end(), back_inserter(added_idx));
	qDebug() << "changed point num: " << (removed_idx.size() + added_idx.size());
	
	PointSet removed, added;
	for (auto idx : removed_idx)
		removed.push_back(make_unique<LabeledPoint>(dataset.at(idx)));
	for (auto idx : added_idx)
		added.push_back(make_unique<LabeledPoint>(dataset.at(idx)));
	seeds = move(result);
	return new pair<PointSet, PointSet>(move(removed), move(added));
}
