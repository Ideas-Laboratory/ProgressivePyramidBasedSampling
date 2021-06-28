#include "HierarchicalSampling.h"

using namespace std;

const static vector<uint> power_2 = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192 };

chrono::time_point<chrono::steady_clock> start;

HierarchicalSampling::HierarchicalSampling(const QRect& bounding_rect)
{
	horizontal_bin_num = bounding_rect.width() / params.grid_width + 1;
	vertical_bin_num = bounding_rect.height() / params.grid_width + 1;

	elected_points.resize(horizontal_bin_num);
	index_map.resize(horizontal_bin_num);
	for (uint i = 0; i < horizontal_bin_num; ++i) {
		elected_points[i].resize(vertical_bin_num);
		index_map[i].resize(vertical_bin_num);
	}

	auto start = power_2.begin(), end = power_2.end();
	max_level = max(lower_bound(start, end, horizontal_bin_num) - start, lower_bound(start, end, vertical_bin_num) - start);

	py.density_map.resize(max_level+1);
	py.visibility_map.resize(py.density_map.size());
	py.assignment_map.resize(py.density_map.size());
	for (uint i = 0; i < py.density_map.size(); ++i) {
		py.density_map[i] = DensityMap(power_2[i], vector<int>(power_2[i]));
		py.visibility_map[i] = DensityMap(power_2[i], vector<int>(power_2[i]));
		py.assignment_map[i] = DensityMap(power_2[i], vector<int>(power_2[i]));
	}

	uint side_length = power_2[max_level];
	changed_map.resize(side_length);
	for (uint i = 0; i < side_length; ++i) {
		changed_map[i].resize(side_length);
	}
}

pair<PointSet, PointSet>* HierarchicalSampling::execute(const FilteredPointSet* origin, bool is_1st)
{
	_added.clear(), _removed.clear();
	if (is_1st) { // is a new dataset
		last_frame_id = -1;

		initializeGrids();
		previous_assigned_maps.clear();
	}
	is_first_frame = is_1st || params.ratio_threshold == 0.0;

	computeAssignMapsProgressively(origin);
	auto seeds = getSeedsDifference();
	qDebug() << "execution:" << (double)(chrono::high_resolution_clock::now() - start).count() / 1e9;
	return seeds;
}

void HierarchicalSampling::constructionHelper(vector<DensityMap>& map_list, int level, int i, int j)
{
	int i1 = 2 * i, i2 = 2 * i + 1, j1 = 2 * j, j2 = 2 * j + 1;
	auto &target = map_list[level], &source = map_list[level + 1];
	target[i][j] = source[i1][j1] + source[i1][j2] + source[i2][j1] + source[i2][j2];
}

vector<vector<pair<int, int>>> HierarchicalSampling::classifyRegions(int level, const vector<pair<int, int>>& indices)
{
	auto it = max_element(indices.begin(), indices.end(), [this, level](const pair<int, int>& a, const pair<int, int>& b) {
		return py.getVal(Pyramid::Density, level, a) < py.getVal(Pyramid::Density, level, b);
	});
	double threshold = params.density_threshold * py.getVal(Pyramid::Density, level, *it);
	size_t max_pos = distance(indices.begin(), it);
	vector<pair<int, int>> low, high;
	high.push_back(indices[max_pos]);
	for (size_t i = 0; i < indices.size(); ++i) {
		if (py.getVal(Pyramid::Density, level, indices[i]) < threshold)
			low.push_back(indices[i]);
		else if (i != max_pos)
			high.push_back(indices[i]);
	}
	return vector<vector<pair<int, int>>>{ low, high };
}

void HierarchicalSampling::smoothingHelper(DensityMap& assignment_map, pair<int, int>&& pos_x, pair<int, int>&& pos_y, int level)
{
	pair<int, int> pos_low, pos_high;
	if (py.getVal(Pyramid::Density, level, pos_x) > py.getVal(Pyramid::Density, level, pos_y))
		pos_high = move(pos_x), pos_low = move(pos_y);
	else
		pos_high = move(pos_y), pos_low = move(pos_x);
	if (py.getVal(Pyramid::Density, level, pos_high) > 0) {
		int density_sum = py.getVal(Pyramid::Density, level, pos_high) + py.getVal(Pyramid::Density, level, pos_low),
			sample_sum = assignment_map[pos_high.first][pos_high.second] + assignment_map[pos_low.first][pos_low.second];
		if ((double)py.getVal(Pyramid::Density, level, pos_low) / py.getVal(Pyramid::Density, level, pos_high) > 
			(double)assignment_map[pos_low.first][pos_low.second] / assignment_map[pos_high.first][pos_high.second]) {
			assignment_map[pos_high.first][pos_high.second] = min(py.getVal(Pyramid::Visibility, level, pos_high),
				(int)round(static_cast<double>(sample_sum) * py.getVal(Pyramid::Density, level, pos_high) / density_sum));
			assignment_map[pos_low.first][pos_low.second] = sample_sum - assignment_map[pos_high.first][pos_high.second];
		}
		else if (assignment_map[pos_high.first][pos_high.second] < assignment_map[pos_low.first][pos_low.second]) {
			int visual_sum = py.getVal(Pyramid::Visibility, level, pos_high) + py.getVal(Pyramid::Visibility, level, pos_low);
			assignment_map[pos_high.first][pos_high.second] = min(py.getVal(Pyramid::Visibility, level, pos_high),
				(int)round(static_cast<double>(sample_sum) / ((1.0 - params.outlier_weight) * density_sum / py.getVal(Pyramid::Density, level, pos_high)
					+ params.outlier_weight * visual_sum / py.getVal(Pyramid::Visibility, level, pos_high))));
			assignment_map[pos_low.first][pos_low.second] = sample_sum - assignment_map[pos_high.first][pos_high.second];
		}
	}
}

void HierarchicalSampling::refinementHelper(DensityMap& assignment_map, int level, int i, int j, bool is_horizontal)
{
	int i1, i2, j1, j2;
	vector<int> group1, group2;
	if (is_horizontal) {
		i1 = 2 * i + 1, i2 = 2 * i + 2, j1 = 2 * j, j2 = 2 * j + 1;
		group1 = { 0,2 }, group2 = { 1,3 };
	}
	else{
		i1 = 2 * i, i2 = 2 * i + 1, j1 = 2 * j + 1, j2 = 2 * j + 2;
		group1 = { 0,1 }, group2 = { 2,3 };
	}
	vector<pair<int, int>> indices = { { i1,j1 },{ i2,j1 },{ i1,j2 },{ i2,j2 } };
	auto low_high_pair = classifyRegions(level, indices);
	vector<pair<int, int>> low_density_indices = low_high_pair[0], high_density_indices = low_high_pair[1];

	if (py.getVal(Pyramid::Density, level, high_density_indices[0]) > 0) {
		bool violate_relative_density = false;
		for (auto& a : group1) {
			for (auto& b : group2) {
				bool density_order = py.getVal(Pyramid::Density, level, indices[a]) > py.getVal(Pyramid::Density, level, indices[b]),
					assignment_order = assignment_map[indices[a].first][indices[a].second] > assignment_map[indices[b].first][indices[b].second];
				violate_relative_density |= (density_order ^ assignment_order);
			}
		}
		if (violate_relative_density) {
			int low_density_sum = 0, high_density_sum = 0, low_visual_sum = 0, high_visual_sum = 0, active_pixels = 0;
			for (auto& idx : low_density_indices) {
				low_density_sum += py.getVal(Pyramid::Density, level, idx);
				low_visual_sum += py.getVal(Pyramid::Visibility, level, idx);
				active_pixels += assignment_map[idx.first][idx.second];
			}
			if (low_visual_sum == 0) return; // the goal of our refinement is reallocated excessive active pixels in low density region
			for (auto& idx : high_density_indices) {
				high_density_sum += py.getVal(Pyramid::Density, level, idx);
				high_visual_sum += py.getVal(Pyramid::Visibility, level, idx);
				active_pixels += assignment_map[idx.first][idx.second];
			}
			int density_sum = high_density_sum + low_density_sum, visual_sum = high_visual_sum + low_visual_sum;

			int high_active_pixels = min(high_visual_sum,
				(int)round(static_cast<double>(active_pixels) / ((1.0 - params.outlier_weight) * density_sum / high_density_sum
					+ params.outlier_weight * visual_sum / high_visual_sum)));
			for (auto& idx : high_density_indices) {
				assignment_map[idx.first][idx.second] =
					(int)round((double)high_active_pixels * py.getVal(Pyramid::Density, level, idx) / high_density_sum);
			}

			int low_active_pixels = active_pixels - high_active_pixels;
			for (auto& idx : low_density_indices) {
				assignment_map[idx.first][idx.second] =
					(int)round((double)low_active_pixels * py.getVal(Pyramid::Visibility, level, idx) / low_visual_sum);
			}
		}
	}
}

void HierarchicalSampling::adjacentChangedHelper(const DensityMap& assignment_map, pair<int, int>&& pos_x, pair<int, int>&& pos_y, int level)
{
	pair<int, int> pos_changed, pos_unchanged;
	bool exclusive_changed = false;
	if (isChangedRegion(level, pos_x.first, pos_x.second) && !isChangedRegion(level, pos_y.first, pos_y.second)) {
		pos_changed = pos_x, pos_unchanged = pos_y;
		exclusive_changed = true;
	}
	else if (!isChangedRegion(level, pos_x.first, pos_x.second) && isChangedRegion(level, pos_y.first, pos_y.second)) {
		pos_changed = pos_y, pos_unchanged = pos_x;
		exclusive_changed = true;
	}

	if (exclusive_changed) {
		int D_changed = py.getVal(Pyramid::Density, level, pos_changed), D_unchanged = py.getVal(Pyramid::Density, level, pos_unchanged);
		if (D_changed > D_unchanged) {
			if(assignment_map[pos_changed.first][pos_changed.second] > 0 && abs((double)D_unchanged/D_changed -
				(double)py.getVal(Pyramid::Assignment, level, pos_unchanged) / assignment_map[pos_changed.first][pos_changed.second]) > params.ratio_threshold)
				setChangedRegion(level, pos_unchanged.first, pos_unchanged.second);
		}
		else {
			if (assignment_map[pos_unchanged.first][pos_unchanged.second] > 0 && abs((double)D_changed / D_unchanged -
				(double)assignment_map[pos_changed.first][pos_changed.second] / py.getVal(Pyramid::Assignment, level, pos_unchanged)) > params.ratio_threshold)
				setChangedRegion(level, pos_unchanged.first, pos_unchanged.second);
		}
	}
}

bool HierarchicalSampling::detectChangedRegion(int level, const vector<pair<int, int>>& indices)
{
	int i = indices[0].first >> 1, j = indices[0].second >> 1, k = level - 1;
	int A_level_1 = py.assignment_map[k][i][j], D_level_1 = py.density_map[k][i][j];
	if (A_level_1 == 0) return true;
	double diff = 0.0;
	for (size_t i = 0, sz = indices.size(); i < sz; ++i) {
		diff += abs(static_cast<double>(py.getVal(Pyramid::Density, level, indices[i])) / D_level_1
			- static_cast<double>(py.getVal(Pyramid::Assignment, level, indices[i])) / A_level_1);
	}

	return diff / 4.0 > params.ratio_threshold;
}

bool HierarchicalSampling::isChangedRegion(int level, int i, int j)
{
	int side = power_2[max_level - level];
	return changed_map[side * i][side * j];
}

void HierarchicalSampling::setChangedRegion(int level, int i, int j)
{
	int side = power_2[max_level - level];
	for (int _i = side * i, i_e = _i + side; _i < i_e; ++_i) {
		for (int _j = side * j, j_e = _j + side; _j < j_e; ++_j) {
			changed_map[_i][_j] = true;
		}
	}
}

void HierarchicalSampling::initializeGrids()
{
	for (uint i = 0; i < horizontal_bin_num; ++i)
		for (uint j = 0; j < vertical_bin_num; ++j) {
			py.density_map[max_level][i][j] = 0;
			index_map[i][j].clear();
			elected_points[i][j] = nullptr;
		}
}

void HierarchicalSampling::computeAssignMapsProgressively(const FilteredPointSet* origin)
{
	convertToDensityMap(origin);
	start = chrono::high_resolution_clock::now();
	constructPyramids();
	generateAssignmentMapsHierarchically();
}

void HierarchicalSampling::convertToDensityMap(const FilteredPointSet* origin)
{
	auto &D = py.density_map[max_level], &V = py.visibility_map[max_level], &A = py.assignment_map[max_level];
	QDate *last_date = nullptr;
	for (auto& pr : *origin) {
		auto& p = pr.second;
		int x = visual2grid(p->pos.x(), MARGIN.left),
			y = visual2grid(p->pos.y(), MARGIN.top);
		if (D[x][y] == 0) {
			elected_points[x][y] = make_unique<LabeledPoint>(p);
			index_map[x][y][p->label] = pr.first;
		}
		else if (double_dist(gen) < 0.1) {
			elected_points[x][y]->pos = p->pos;
			elected_points[x][y]->label = p->label;
			index_map[x][y][p->label] = pr.first;
		}
		++D[x][y];

		if (params.is_streaming) {
			if (sliding_window.find(*p->date) == sliding_window.end())
				sliding_window.emplace(*p->date, DensityMap(horizontal_bin_num, vector<int>(vertical_bin_num)));
			++sliding_window[*p->date][x][y];
			if (last_date == nullptr || *last_date < *p->date)
				last_date = p->date.get(); // find the last date
		}
	}
	if (params.is_streaming) {
		for (auto it = sliding_window.begin(); it != sliding_window.end();) {
			if (it->first.daysTo(*last_date) > params.time_window) {
				for (size_t i = 0; i < horizontal_bin_num; ++i)
					for (size_t j = 0; j < vertical_bin_num; ++j)
						D[i][j] -= it->second[i][j];
				it = sliding_window.erase(it);
			}
			else
				++it;
		}
	}
	
	for (uint i = 0; i < power_2[max_level]; ++i) {
		for (uint j = 0; j < power_2[max_level]; ++j) {
			if (i < horizontal_bin_num && j < vertical_bin_num) {
				V[i][j] = (D[i][j] == 0) ? 0 : 1;
			}
			else {
				D[i][j] = V[i][j] = 0;
			}
			A[i][j] = is_first_frame ? 0 : previous_assigned_maps.back()[i][j];
			changed_map[i][j] = is_first_frame;
		}
	}
}

void HierarchicalSampling::constructPyramids()
{
	//#include <fstream>
	for (int k = max_level; k > 0;) {
		//ofstream output(string("./results/test_csv/") + to_string(level) + ".csv", ios_base::trunc); // + "_" + to_string(last_frame_id) 
		--k;
		
		for (int i = 0; i < power_2[k]; ++i) {
			for (int j = 0; j < power_2[k]; ++j) {
				constructionHelper(py.density_map, k, i, j);
				constructionHelper(py.visibility_map, k, i, j);
				if (!is_first_frame)
					constructionHelper(py.assignment_map, k, i, j);
				//if(py.visibility_map[level][i][j]!=0)
				//	output << py.visibility_map[level][i][j];
				//output << ',';
			}
			//output << '\n';
		}

	}
}

void HierarchicalSampling::generateAssignmentMapsHierarchically()
{
	//#include <fstream>
	int k;
	DensityMap current_assignment_map = py.visibility_map[0];
	for (int level = 0; level < max_level; ) {
		k = level++;
		DensityMap A(power_2[level], vector<int>(power_2[level]));

		//ofstream output(string("./results/test_csv/") + to_string(level) + "_" + to_string(params.stop_level) + "_" + to_string(last_frame_id) + ".csv", ios_base::trunc);
		for (int j = 0; j < power_2[k]; ++j) {
			for (int i = 0; i < power_2[k]; ++i) {
				if (current_assignment_map[i][j] == 0) {
					continue;
				}

				int actual_density = py.density_map[k][i][j],
					visual_pixels = py.visibility_map[k][i][j],
					point_samples = current_assignment_map[i][j];
				//if(current_assignment_map[i][j]>0) output << current_assignment_map[i][j];
				//output << ',';
				int i1 = 2 * i, i2 = 2 * i + 1, j1 = 2 * j, j2 = 2 * j + 1;
				vector<pair<int, int>> indices = { { i1,j1 },{ i2,j1 },{ i1,j2 },{ i2,j2 } };
				bool changed = (!is_first_frame && !isChangedRegion(k, i, j)) && detectChangedRegion(level, indices); // density ratio diff exceeds ¦Å

				if (level < params.stop_level) {
					auto low_high_pair = classifyRegions(level, indices);
					vector<pair<int, int>> low_density_indices = low_high_pair[0], high_density_indices = low_high_pair[1];

					// calculate active pixels based on density ratio
					int& max_assigned_val = A[high_density_indices[0].first][high_density_indices[0].second];
					max_assigned_val = (int)ceil((double)py.getVal(Pyramid::Visibility, level, high_density_indices[0]) * point_samples / visual_pixels);

					double sampling_ratio = static_cast<double>(max_assigned_val) / py.getVal(Pyramid::Density, level, high_density_indices[0]);
					int remain_pixels = point_samples - max_assigned_val;
					for (size_t _i = 1, sz = high_density_indices.size(); _i < sz && remain_pixels > 0; ++_i) {
						int density_val = py.getVal(Pyramid::Density, level, high_density_indices[_i]);
						if (density_val == 0) break; // an empty area can only lead to useless calculation

						int assigned_val = round(sampling_ratio * density_val);
						assigned_val = min({ assigned_val, py.getVal(Pyramid::Visibility, level, high_density_indices[_i]), remain_pixels });
						A[high_density_indices[_i].first][high_density_indices[_i].second] = assigned_val;

						remain_pixels -= assigned_val;
					}

					// allocate additional active pixels for low density regions
					if (!low_density_indices.empty()) {
						int low_density_sum = 0, high_density_sum = 0, low_visual_sum = 0, high_visual_sum = 0, high_assigned = 0;
						for (auto& idx : low_density_indices) {
							low_density_sum += py.getVal(Pyramid::Density, level, idx);
							low_visual_sum += py.getVal(Pyramid::Visibility, level, idx);
						}
						if (low_density_sum != 0) {
							high_density_sum = actual_density - low_density_sum;
							high_visual_sum = visual_pixels - low_visual_sum;

							for (auto& idx : high_density_indices) {
								high_assigned += A[idx.first][idx.second];
							}
							int low_assigned = round(high_assigned * ((1.0 - params.outlier_weight) * low_density_sum / high_density_sum + params.outlier_weight * low_visual_sum / high_visual_sum));
							for (size_t _i = 0, sz = low_density_indices.size(); _i < sz; ++_i) {
								int assigned_val = ceil(static_cast<double>(py.getVal(Pyramid::Visibility, level, low_density_indices[_i])) * low_assigned / low_visual_sum);
								int& ref2map = A[low_density_indices[_i].first][low_density_indices[_i].second];
								ref2map = max(assigned_val, ref2map); // ensure low density region has more points
							}
						}
					}
				}
				else {
					sort(indices.begin(), indices.end(), [this, level](const pair<int, int>& a, const pair<int, int>& b) {
						return py.getVal(Pyramid::Density, level, a) > py.getVal(Pyramid::Density, level, b);
					});
					int remain_assigned_point_num = point_samples;
					for (size_t _i = 0, sz = indices.size(); _i < sz && remain_assigned_point_num > 0; ++_i) {
						int assigned_val = ceil((double)point_samples * py.getVal(Pyramid::Visibility, level, indices[_i]) / visual_pixels);
						assigned_val = min({ assigned_val, py.getVal(Pyramid::Visibility, level, indices[_i]), remain_assigned_point_num });
						A[indices[_i].first][indices[_i].second] = assigned_val;

						remain_assigned_point_num -= assigned_val;
					}

				}

				if (changed)
					setChangedRegion(k, i, j);
			}

			//output << '\n';
		}

		if (level > 1) {
			int end = power_2[k] - 1;
			//for (int j = 0; j < power_2[k]; ++j) {
			//	for (int i = 0; i < power_2[k]; ++i) {
			//		if (i < end) refinementHelper(A, level, i, j, true);
			//		if (j < end) refinementHelper(A, level, i, j, false);
			//	}
			//}
			
			for (int j = 0; j < power_2[k]; ++j) {
				for (int i = 0; i < end; ++i) {
					int i1 = 2 * i + 1, i2 = 2 * i + 2, j1 = 2 * j, j2 = 2 * j + 1;
					smoothingHelper(A, make_pair(i1, j1), make_pair(i2, j1), level);
					smoothingHelper(A, make_pair(i1, j2), make_pair(i2, j2), level);
				}
			}
			for (int j = 0; j < end; ++j) {
				for (int i = 0; i < power_2[k]; ++i) {
					int i1 = 2 * i, i2 = 2 * i + 1, j1 = 2 * j + 1, j2 = 2 * j + 2;
					smoothingHelper(A, make_pair(i1, j1), make_pair(i1, j2), level);
					smoothingHelper(A, make_pair(i2, j1), make_pair(i2, j2), level);
				}
			}

			// Adjacent Region Examination
			if (!is_first_frame) {
				for (int j = 0; j < power_2[k]; ++j) {
					for (int i = 0; i < end; ++i) {
						int i1 = 2 * i + 1, i2 = 2 * i + 2, j1 = 2 * j, j2 = 2 * j + 1;
						adjacentChangedHelper(A, make_pair(i1, j1), make_pair(i2, j1), level);
						adjacentChangedHelper(A, make_pair(i1, j2), make_pair(i2, j2), level);
					}
				}
				for (int j = 0; j < end; ++j) {
					for (int i = 0; i < power_2[k]; ++i) {
						int i1 = 2 * i, i2 = 2 * i + 1, j1 = 2 * j + 1, j2 = 2 * j + 2;
						adjacentChangedHelper(A, make_pair(i1, j1), make_pair(i1, j2), level);
						adjacentChangedHelper(A, make_pair(i2, j1), make_pair(i2, j2), level);
					}
				}
			}
		}
		current_assignment_map = move(A);
	}

	int point_num = 0;
	if (previous_assigned_maps.empty()) {
		for (uint j = 0; j < vertical_bin_num; ++j) {
			for (uint i = 0; i < horizontal_bin_num; ++i) {
				if (current_assignment_map[i][j] != 0) {
					_added.push_back(make_pair(i, j));
					++point_num;
				}
			}
		}
		previous_assigned_maps.push_back(current_assignment_map);
	}
	else if (params.ratio_threshold == 0.0) {
		DensityMap old = previous_assigned_maps.back();
		for (uint j = 0; j < vertical_bin_num; ++j) {
			for (uint i = 0; i < horizontal_bin_num; ++i) {
				if (old[i][j] != 0 && current_assignment_map[i][j] == 0)
					_removed.push_back(make_pair(i, j));
				else if (old[i][j] == 0 && current_assignment_map[i][j] != 0)
					_added.push_back(make_pair(i, j));
				old[i][j] = current_assignment_map[i][j];
				//output << changed_map[i][j];
				if (old[i][j] == 1) ++point_num;
				//output << ',';
			}
		}
		previous_assigned_maps.push_back(move(old));
	}
	else {
		//ofstream output(string("./results/test_csv/updateNeeded_")+to_string(last_frame_id)+".csv", ios_base::trunc);
		DensityMap old = previous_assigned_maps.back();
		for (uint j = 0; j < vertical_bin_num; ++j) {
			for (uint i = 0; i < horizontal_bin_num; ++i) {
				if (changed_map[i][j]) {
					if (old[i][j] != 0 && current_assignment_map[i][j] == 0)
						_removed.push_back(make_pair(i, j));
					else if (old[i][j] == 0 && current_assignment_map[i][j] != 0)
						_added.push_back(make_pair(i, j));
					old[i][j] = current_assignment_map[i][j];
					//output << changed_map[i][j];
				}
				// forcely remove points out of the sliding window
				if (params.is_streaming && py.visibility_map[max_level][i][j] == 0 && old[i][j] != 0) {
					_removed.push_back(make_pair(i, j));
					old[i][j] = 0;
				}
				if (old[i][j] == 1) ++point_num;
				//output << ',';
			}
			//output << '\n';
		}
		previous_assigned_maps.push_back(move(old));
	}
	qDebug() << "point number: " << point_num;
}

Indices HierarchicalSampling::selectSeeds()
{
	Indices result;

	for (uint i = 0; i < horizontal_bin_num; ++i) {
		for (uint j = 0; j < vertical_bin_num; ++j) {
			if (previous_assigned_maps.back()[i][j] != 0) {
				result.push_back(index_map[i][j][elected_points[i][j]->label]);
			}
		}
	}
	//qDebug() << result.size();
	last_frame_id = previous_assigned_maps.size() - 1;
	return result;
}

pair<PointSet, PointSet>* HierarchicalSampling::getSeedsDifference()
{
	PointSet removed, added;
	static int current_point_num;
	if (is_first_frame) current_point_num = 0;

	for (auto& idx : this->_removed) {
		removed.push_back(make_unique<LabeledPoint>(elected_points[idx.first][idx.second]));
	}
	for (auto& idx : this->_added) {
		added.push_back(make_unique<LabeledPoint>(elected_points[idx.first][idx.second]));
	}
	int change = ((int)added.size() - (int)removed.size());
	qDebug() << "modified points:" << (int)added.size() + (int)removed.size();
	current_point_num += change;
	//qDebug() << "add: " << added.size() << "remove:" << removed.size() << ", frame_id=" << last_frame_id + 1;
	//qDebug() << "The number of points in current frame: " << current_point_num;

	last_frame_id = previous_assigned_maps.size() - 1;
	return new pair<PointSet, PointSet>(move(removed), move(added));
}

pair<PointSet, PointSet> HierarchicalSampling::getSeedsWithDiff()
{
	PointSet result, diff;

	for (uint i = 0; i < horizontal_bin_num; ++i) {
		for (uint j = 0; j < vertical_bin_num; ++j) {
			if (previous_assigned_maps[params.displayed_frame_id][i][j] != 0) {
				if (last_frame_id != -1 && previous_assigned_maps[last_frame_id][i][j] != 0) {
					result.push_back(make_unique<LabeledPoint>(elected_points[i][j]));
				}
				else {
					diff.push_back(make_unique<LabeledPoint>(elected_points[i][j]));
				}
			}
		}
	}
	last_frame_id = params.displayed_frame_id;
	return make_pair(move(result), move(diff));
}

PointSet HierarchicalSampling::getSeeds()
{
	if (previous_assigned_maps.empty()) return PointSet();

	int frame_id = params.displayed_frame_id > previous_assigned_maps.size() ? previous_assigned_maps.size() - 1 : params.displayed_frame_id - 1;

	PointSet result;
	for (uint i = 0; i < horizontal_bin_num; ++i) {
		for (uint j = 0; j < vertical_bin_num; ++j) {
			if (previous_assigned_maps[frame_id][i][j] != 0) {
				result.push_back(make_unique<LabeledPoint>(elected_points[i][j]));
			}
		}
	}
	last_frame_id = params.displayed_frame_id;
	return result;
}
