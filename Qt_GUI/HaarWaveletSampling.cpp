#include "HaarWaveletSampling.h"

using namespace std;

const static vector<uint> power_2 = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192 };

HaarWaveletSampling::HaarWaveletSampling(const QRect & bounding_rect)
{
	horizontal_bin_num = bounding_rect.width() / params.grid_width + 1;
	vertical_bin_num = bounding_rect.height() / params.grid_width + 1;

	elected_points.resize(horizontal_bin_num);
	for (uint i = 0; i < horizontal_bin_num; ++i) {
		elected_points[i].resize(vertical_bin_num);
	}

	side_length = max(*lower_bound(power_2.begin(), power_2.end(), horizontal_bin_num), *lower_bound(power_2.begin(), power_2.end(), vertical_bin_num));

	actual_map.resize(side_length);
	visual_map.resize(side_length);
	assigned_map.resize(side_length);
	update_needed_map.resize(side_length);
	for (uint i = 0; i < side_length; ++i) {
		actual_map[i].resize(side_length);
		visual_map[i].resize(side_length);
		assigned_map[i].resize(side_length);
		update_needed_map[i].resize(side_length);
	}
}

pair<TempPointSet, TempPointSet> HaarWaveletSampling::execute(const weak_ptr<FilteredPointSet> origin, bool is_1st)
{
	is_first_frame = is_1st;
	added.clear(), removed.clear();
	if (is_1st) {
		end_shift = pow(2, params.end_level);
		last_frame_id = -1;
		
		initializeGrids();
		previous_assigned_maps.clear();
	}

	computeAssignMapsProgressively(origin);
	return getSeedsDifference();
}

void HaarWaveletSampling::transformHelper(DensityMap & dm, int i, int j, int interval, bool inverse)
{
	auto &a = dm[i][j], &b = dm[i + interval][j], &c = dm[i][j + interval], &d = dm[i + interval][j + interval];
	int A = a + b + c + d,
		B = a - b + c - d,
		C = a + b - c - d,
		D = a - b - c + d;
	if (inverse) {
		A >>= 2, B >>= 2, C >>= 2, D >>= 2;
	}
	a = A, b = B, c = C, d = D;
}

vector<vector<pair<int, int>>> HaarWaveletSampling::lowDensityJudgementHelper(const vector<pair<int, int>>& indices) const
{
	double threshold = 0.8*getVal(actual_map, indices[0]); // A * tau(the nondense rate)
	set<int> low_density_indices;
	int B = getVal(actual_map, indices[1]), C = getVal(actual_map, indices[2]), D = getVal(actual_map, indices[3]);
	if (abs(B) > threshold) { // |B| > tau*A
		B > 0 ? low_density_indices.insert({ 1,3 }) : low_density_indices.insert({ 0,2 });
	}
	if (abs(C) > threshold) { // |C| > tau*A
		C > 0 ? low_density_indices.insert({ 2,3 }) : low_density_indices.insert({ 0,1 });
	}
	if (abs(D) > threshold) { // |D| > tau*A
		D > 0 ? low_density_indices.insert({ 1,2 }) : low_density_indices.insert({ 0,3 });
	}
    vector<int> high_density_indices, all_indices{0,1,2,3};
    set_difference(begin(all_indices), end(all_indices), begin(low_density_indices), end(low_density_indices), 
		inserter(high_density_indices, high_density_indices.begin()));

	vector<pair<int, int>> low, high;
	std::transform(low_density_indices.begin(), low_density_indices.end(), back_inserter(low), [&](int i) { return indices[i]; });
	std::transform(high_density_indices.begin(), high_density_indices.end(), back_inserter(high), [&](int i) { return indices[i]; });
	return vector<vector<pair<int, int>>>{ low, high };
}

bool HaarWaveletSampling::hasDiscrepancy(const std::vector<std::pair<int, int>>& indices)
{
	int Aa = getVal(previous_assigned_maps.back(), indices[0]), Ad = getVal(actual_map, indices[0]);
	if (Aa == 0) return true;
	double error = 0.0;
	for (size_t i = 1, sz = indices.size(); i < sz; ++i) {
		error += abs(static_cast<double>(getVal(actual_map, indices[i])) / Ad 
					 - static_cast<double>(getVal(previous_assigned_maps.back(), indices[i])) / Aa);
	}
	return error / 3.0 > params.epsilon;
}

void HaarWaveletSampling::initializeGrids()
{
	for (uint i = 0; i < horizontal_bin_num; ++i)
		for (uint j = 0; j < vertical_bin_num; ++j) {
			actual_map[i][j] = 0;
			elected_points[i][j] = nullptr;
		}
}

void HaarWaveletSampling::convertToDensityMap()
{
	for (uint i = 0; i < side_length; ++i) {
		for (uint j = 0; j < side_length; ++j) {
			if (i < horizontal_bin_num && j < vertical_bin_num) {
				visual_map[i][j] = actual_map[i][j] == 0 ? 0 : 1;
			}
			else {
				actual_map[i][j] = visual_map[i][j] = 0;
			}
			assigned_map[i][j] = 0;
			update_needed_map[i][j] = false;
		}
	}
}

void HaarWaveletSampling::computeAssignMapsProgressively(const weak_ptr<FilteredPointSet> origin)
{
	for (auto& pr : *(origin.lock())) {
		auto &p = pr.second;
		if (find(selected_class_order.begin(), selected_class_order.end(), p->label) == selected_class_order.end())
			continue;
		int x = visual2grid(p->pos.x(), MARGIN.left),
			y = visual2grid(p->pos.y(), MARGIN.top);
		if (actual_map[x][y] == 0 || double_dist(gen) > 0.1)
			elected_points[x][y] = move(pr.second);
		++actual_map[x][y];
	}
	convertToDensityMap();
	forwardTransform();
	inverseTransfrom();
}

void HaarWaveletSampling::forwardTransform()
{
//#include <fstream>
	int shift;
	for (int k = 1; k < side_length; k = shift) {
		shift = k << 1;
		//ofstream output(string("./results/test_csv/") + to_string(shift) + ".csv", ios_base::trunc);
		// only traverse the bins inside the screen
		for (int i = 0; i < horizontal_bin_num; i += shift) {
			for (int j = 0; j < vertical_bin_num; j += shift) {
				transformHelper(actual_map, i, j, k);
				transformHelper(visual_map, i, j, k);
				if (!is_first_frame)
					transformHelper(previous_assigned_maps.back(), i, j, k);
				//if(visual_map[i][j]!=0) output << visual_map[i][j];
				//output << ',';
			}
			//output << '\n';
		}
	}
}

void HaarWaveletSampling::inverseTransfrom()
{
//#include <fstream>
	int k;
	assigned_map[0][0] = visual_map[0][0];
	for (int shift = side_length; shift > 1; shift = k) {
		k = shift >> 1;

		//ofstream output(string("./results/test_csv/") + to_string(shift) + "_" + to_string(params.end_level) + "_" + to_string(last_frame_id) + ".csv", ios_base::trunc);
		for (int j = 0; j < vertical_bin_num; j += shift) {
			for (int i = 0; i < horizontal_bin_num; i += shift) {
				vector<pair<int, int>> indices = { { i,j },{ i + k,j },{ i,j + k },{ i + k,j + k } };
				auto low_high_pair = lowDensityJudgementHelper(indices);
				vector<pair<int, int>> low_density_indices = low_high_pair[0], high_density_indices = low_high_pair[1];
				bool update_needed = (!is_first_frame && !update_needed_map[i][j]) && hasDiscrepancy(indices), // error exceeds the bound
					same_assigned_points = update_needed && (previous_assigned_maps.back()[i][j] == assigned_map[i][j]);
				//if(assigned_map[i][j]>0) output << assigned_map[i][j];
				//output << ',';

				int visual_point_num = visual_map[i][j], assigned_visual_point_num = assigned_map[i][j];
				transformHelper(actual_map, i, j, k, true);
				transformHelper(visual_map, i, j, k, true);
				if(!is_first_frame) transformHelper(previous_assigned_maps.back(), i, j, k, true);
				assigned_map[i][j] = 0;

				if (visual_point_num == 0) continue;

				if(shift > end_shift) {
					// calculate assigned number based on relative data density
					sort(indices.begin(), indices.end(), [this](const pair<int, int> &a, const pair<int, int> &b) {
						int am_a = getVal(actual_map, a), am_b = getVal(actual_map, b);
						return am_a > am_b ? true : am_a < am_b ? false :
							getVal(visual_map, a) > getVal(visual_map, b);
					});
					int &max_assigned_val = assigned_map[indices[0].first][indices[0].second];
					max_assigned_val = (int)ceil(static_cast<double>(getVal(visual_map, indices[0])) * assigned_visual_point_num / visual_point_num);

					double sampling_ratio_zero = static_cast<double>(getVal(visual_map, indices[0])) / getVal(actual_map, indices[0]);
					int remain_assigned_point_num = assigned_visual_point_num - max_assigned_val;
					for (size_t _i = 1, sz = indices.size(); _i < sz && remain_assigned_point_num > 0; ++_i) {
						int actual_val = getVal(actual_map, indices[_i]);
						if (actual_val == 0) break; // an empty area can only lead to useless calculation

						int assigned_val = round(sampling_ratio_zero * actual_val * assigned_visual_point_num / visual_point_num); // at least 1
						assigned_val = min({ assigned_val, getVal(visual_map, indices[_i]), remain_assigned_point_num });
						assigned_map[indices[_i].first][indices[_i].second] = assigned_val;

						remain_assigned_point_num -= assigned_val;
					}

					// calculate assigned number for low density regions intentionally
                    if(!low_density_indices.empty()) {
                        int low_density_sum = 0, high_density_sum = 0, low_visual_sum = 0, high_visual_sum = 0, high_assigned = 0;
                        for(auto &idx : low_density_indices) {
                            low_density_sum += getVal(actual_map, idx);
                            low_visual_sum += getVal(visual_map, idx);
                        }
                        if(low_density_sum == 0) continue;

                        for(auto &idx : high_density_indices) {
                            high_density_sum += getVal(actual_map, idx);
                            high_visual_sum += getVal(visual_map, idx);
                            high_assigned += getVal(assigned_map, idx);
                        }
                        int low_assigned = high_assigned*((1.0 - params.low_density_weight)*low_density_sum/high_density_sum + params.low_density_weight*low_visual_sum/high_visual_sum);
                        sort(low_density_indices.begin(), low_density_indices.end(), [this](const pair<int, int> &a, const pair<int, int> &b) {
                            return getVal(visual_map, a) < getVal(visual_map, b);
                        });
                        for (size_t _i = 0, sz = low_density_indices.size(); _i < sz; ++_i) {
                            int assigned_val = ceil(static_cast<double>(getVal(visual_map, low_density_indices[_i])) * low_assigned / low_density_sum);
                            int &ref2map = assigned_map[low_density_indices[_i].first][low_density_indices[_i].second];
                            ref2map = max(assigned_val, ref2map);
                        }
                    }
				}
				else {
					sort(indices.begin(), indices.end(), [this](const pair<int, int> &a, const pair<int, int> &b) {
						return getVal(actual_map, a) > getVal(actual_map, b);
					});
					int remain_assigned_point_num = assigned_visual_point_num;
					for (size_t _i = 0, sz = indices.size(); _i < sz && remain_assigned_point_num > 0; ++_i) {
						int assigned_val = ceil(static_cast<double>(getVal(visual_map, indices[_i])) * assigned_visual_point_num / visual_point_num);
						assigned_val = min({ assigned_val, getVal(visual_map, indices[_i]), remain_assigned_point_num });
						assigned_map[indices[_i].first][indices[_i].second] = assigned_val;

						remain_assigned_point_num -= assigned_val;
					}
				}

				if (update_needed) {
					if (same_assigned_points) {
						for (auto &idx : indices) {
							same_assigned_points &= (getVal(previous_assigned_maps.back(), idx) == getVal(assigned_map, idx));
						}
					}
					if (!same_assigned_points) {
						for (int _i = i, i_e = i + shift; _i < i_e; ++_i) {
							for (int _j = j, j_e = j + shift; _j < j_e; ++_j) {
								update_needed_map[_i][_j] = true;
							}
						}
					}
				}
			}
			//output << '\n';
		}

		if (k == end_shift) { // Gaussian smoothing
			vector<vector<double>> smooth_assign_map(horizontal_bin_num);
			for (int i = 0; i < horizontal_bin_num; ++i) {
				smooth_assign_map[i].resize(vertical_bin_num);
			}

			const vector<vector<pair<int, int>>> offsets{
				{ { -1,-1 },{ -1, 1 },{ 1,-1 },{ 1,1 } },
				{ { -1, 0 },{ 0,-1 },{ 0, 1 },{ 1,0 } } };
			const double share = 0.0416; // 1/24
			for (int j = 0; j < vertical_bin_num; j += k) {
				for (int i = 0; i < horizontal_bin_num; i += k) {
					if (assigned_map[i][j] == 0) continue;

					for (int g = 0, g_sz = offsets.size(); g < g_sz; ++g) {
						double i_share = (g + 1)*share*assigned_map[i][j];
						for (int o = 0, o_sz = offsets[g].size(); o < o_sz; ++o) {
							auto idx = make_pair(i + k*offsets[g][o].first, j + k*offsets[g][o].second);
							if (idx.first > -1 && idx.first < horizontal_bin_num && idx.second > -1 && idx.second < vertical_bin_num) {
								smooth_assign_map[idx.first][idx.second] += i_share;
							}
						}
					}
					smooth_assign_map[i][j] += assigned_map[i][j];
				}
			}

			//ofstream output("./results/test_csv/0.csv", ios_base::trunc);
			for (int j = 0; j < vertical_bin_num; j += k) {
				for (int i = 0; i < horizontal_bin_num; i += k) {
					assigned_map[i][j] = round(smooth_assign_map[i][j]);
					
					//if (assigned_map[i][j]>0) output << assigned_map[i][j];
					//output << ',';
				}
				//output << '\n';
			}
		}
	}

	if (is_first_frame) {
		previous_assigned_maps.push_back(assigned_map);
	}
	else {
		//ofstream output(string("./results/test_csv/updateNeeded_")+to_string(last_frame_id)+".csv", ios_base::trunc);
		DensityMap old = previous_assigned_maps.back();
		for (uint j = 0; j < vertical_bin_num; ++j) {
			for (uint i = 0; i < horizontal_bin_num; ++i) {
				if (update_needed_map[i][j]) {
					if (old[i][j] != 0 && assigned_map[i][j] == 0) {
						removed.push_back(make_pair(i, j));
						old[i][j] = assigned_map[i][j];
					}
					else if (old[i][j] == 0 && assigned_map[i][j] != 0) {
						added.push_back(make_pair(i, j));
						old[i][j] = assigned_map[i][j];
					}
					//output << update_needed_map[i][j];
				}
				//output << ',';
			}
			//output << '\n';
		}
		previous_assigned_maps.push_back(move(old));
	}
}

PointSet HaarWaveletSampling::selectSeeds()
{
	PointSet result;

	for (uint i = 0; i < horizontal_bin_num; ++i) {
		for (uint j = 0; j < vertical_bin_num; ++j) {
			if (assigned_map[i][j] != 0) {
				result.push_back(make_unique<LabeledPoint>(elected_points[i][j]));
			}
		}
	}
	qDebug() << result.size();
	last_frame_id = previous_assigned_maps.size() - 1;
	return result;
}

pair<TempPointSet, TempPointSet> HaarWaveletSampling::getSeedsDifference()
{
	TempPointSet removed, added;

	if (is_first_frame) {
        for (uint i = 0; i < horizontal_bin_num; ++i)
            for (uint j = 0; j < vertical_bin_num; ++j)
                if (assigned_map[i][j] != 0)
                    added.push_back(elected_points[i][j].get());
	}
	else {
		for (auto &idx : this->removed) {
			removed.push_back(elected_points[idx.first][idx.second].get());
		}
		for (auto &idx : this->added) {
			added.push_back(elected_points[idx.first][idx.second].get());
		}

		qDebug() << ((int)added.size() - (int)removed.size());
	}

	last_frame_id = previous_assigned_maps.size() - 1;
	return make_pair(removed, added);
}

pair<TempPointSet, TempPointSet> HaarWaveletSampling::getSeedsWithDiff()
{
	TempPointSet result, diff;

	for (uint i = 0; i < horizontal_bin_num; ++i) {
		for (uint j = 0; j < vertical_bin_num; ++j) {
			if (previous_assigned_maps[params.displayed_frame_id][i][j] != 0) {
				if (last_frame_id != -1 && previous_assigned_maps[last_frame_id][i][j] != 0) {
					result.push_back(elected_points[i][j].get());
				}
				else {
					diff.push_back(elected_points[i][j].get());
				}
			}
		}
	}
	last_frame_id = params.displayed_frame_id;
	return make_pair(result, diff);
}
