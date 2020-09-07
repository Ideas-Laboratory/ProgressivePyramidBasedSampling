#include "HaarWaveletSampling.h"

using namespace std;

const static vector<uint> power_2 = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192 };

HaarWaveletSampling::HaarWaveletSampling(const QRect & bounding_rect)
{
	horizontal_bin_num = bounding_rect.width() / params.grid_width + 1;
	vertical_bin_num = bounding_rect.height() / params.grid_width + 1;

	elected_points.resize(horizontal_bin_num);
	index_map.resize(horizontal_bin_num);
	for (uint i = 0; i < horizontal_bin_num; ++i) {
		elected_points[i].resize(vertical_bin_num);
		index_map[i].resize(vertical_bin_num);
	}

	side_length = max(*lower_bound(power_2.begin(), power_2.end(), horizontal_bin_num), *lower_bound(power_2.begin(), power_2.end(), vertical_bin_num));

	actual_map.resize(side_length);
	visual_map.resize(side_length);
	assigned_map.resize(side_length);
	update_needed_map.resize(side_length);
	class_points_map.resize(side_length);
	quota_map.resize(side_length);
	for (uint i = 0; i < side_length; ++i) {
		actual_map[i].resize(side_length);
		visual_map[i].resize(side_length);
		assigned_map[i].resize(side_length);
		update_needed_map[i].resize(side_length);
		class_points_map[i].resize(side_length);
		quota_map[i].resize(side_length);
	}
}

pair<TempPointSet, TempPointSet>* HaarWaveletSampling::execute(const FilteredPointSet* origin, bool is_1st)
{
	auto start = chrono::high_resolution_clock::now();
	is_first_frame = is_1st;// true;
	_added.clear(), _removed.clear();
	if (is_1st) {
		end_shift = pow(2, params.end_level);
		last_frame_id = -1;
		
		initializeGrids();
		previous_assigned_maps.clear();
	}

	computeAssignMapsProgressively(origin);
	auto seeds = getSeedsDifference();
	qDebug() << "execution:" << (double) (chrono::high_resolution_clock::now() - start).count() / 1e9;
	return seeds;
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

void HaarWaveletSampling::transformHelper(std::vector<std::vector<std::unordered_map<uint, int>>>& dm, int i, int j, int interval, bool inverse)
{
	auto& a = dm[i][j], & b = dm[i + interval][j], & c = dm[i][j + interval], & d = dm[i + interval][j + interval];
	unordered_map<uint, int> A, B, C, D;
	for (auto co : selected_class_order) {
		if (inverse || (a[co] || b[co] || c[co] || d[co])) {
			A[co] = a[co] + b[co] + c[co] + d[co];
			B[co] = a[co] - b[co] + c[co] - d[co];
			C[co] = a[co] + b[co] - c[co] - d[co];
			D[co] = a[co] - b[co] - c[co] + d[co];
		}
	}
	if (inverse) {
		for (auto co : selected_class_order) {
			A[co] ? A[co] >>= 2 : A.erase(co);
			B[co] ? B[co] >>= 2 : B.erase(co);
			C[co] ? C[co] >>= 2 : C.erase(co);
			D[co] ? D[co] >>= 2 : D.erase(co);
		}
	}
	a = move(A), b = move(B), c = move(C), d = move(D);
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

void HaarWaveletSampling::smoothingHelper(pair<int, int>&& pos_x, pair<int, int>&& pos_y, int area)
{
	pair<int, int> pos_low, pos_high;
	if (actual_map[pos_x.first][pos_x.second] > actual_map[pos_y.first][pos_y.second])
		pos_high = move(pos_x), pos_low = move(pos_y);
	else
		pos_high = move(pos_y), pos_low = move(pos_x);
	if (actual_map[pos_high.first][pos_high.second] > 0) {
		if (assigned_map[pos_high.first][pos_high.second] < assigned_map[pos_low.first][pos_low.second]) {
			assigned_map[pos_high.first][pos_high.second] = min(visual_map[pos_high.first][pos_high.second], assigned_map[pos_low.first][pos_low.second]);
		}
		else if (assigned_map[pos_high.first][pos_high.second] == area) {
			assigned_map[pos_low.first][pos_low.second] = max(assigned_map[pos_low.first][pos_low.second],
				(int)round((double)assigned_map[pos_high.first][pos_high.second] * actual_map[pos_low.first][pos_low.second] / actual_map[pos_high.first][pos_high.second]));
			assigned_map[pos_low.first][pos_low.second] = min(visual_map[pos_low.first][pos_low.second], assigned_map[pos_low.first][pos_low.second]);
		}
	}
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

double HaarWaveletSampling::fillQuota(int i, int j)
{
	int total_assigned_points = previous_assigned_maps.back()[i][j], actual_point_num = actual_map[i][j], filled_result = 0;
	vector<uint> class_labels;
	for (auto& pr : class_points_map[i][j]) {
		class_labels.push_back(pr.first);

		int cls_assigned = ceil(total_assigned_points * static_cast<double>(pr.second) / actual_point_num);
		quota_map[i][j][pr.first] = cls_assigned;
		filled_result += cls_assigned;
	}
	
	int idx = 0, sz = class_labels.size(), current = filled_result;
	while (current > total_assigned_points) {
		if (quota_map[i][j][class_labels[idx]] > 1) { // may cause bug when the total_assigned_points is less than the number of classes
			--quota_map[i][j][class_labels[idx]];
			--current;
		}
		idx = (++idx) % sz;
	}

	if (sz == 1) return 1.0;
	double ratio_sum = 0.0, cond_ratio_sum = 0.0, ratio;
	auto sign = [](int a, int b) { return a > b ? 1 : a == b ? 0 : -1; };
	for (int _i = 1; _i < sz; ++_i) {
		for (int _j = 0; _j < _i; ++_j) {
			ratio = static_cast<double>(class_points_map[i][j][class_labels[_i]]) / class_points_map[i][j][class_labels[_j]];
			ratio_sum += ratio;
			if (sign(class_points_map[i][j][class_labels[_i]], class_points_map[i][j][class_labels[_j]]) ==
				sign(quota_map[i][j][class_labels[_i]], quota_map[i][j][class_labels[_j]]))
				cond_ratio_sum += ratio;
		}
	}
	return cond_ratio_sum / ratio_sum;
}

void HaarWaveletSampling::splitQuota(const unordered_map<uint, int>& upper_info, int i, int j, int interval)
{
	if (quota_map[i][j].empty()) return;
	vector<pair<uint, uint>> indices = { { i,j },{ i + interval,j },{ i,j + interval },{ i + interval,j + interval } };
	unordered_map<uint, int> upper_quota = quota_map[i][j], filled_cls_result, filled_pos_result;
	for (auto& idx : indices) {
		auto& cpm = class_points_map[idx.first][idx.second];
		uint pos = idx.first * side_length + idx.second;
		for (auto& pr : upper_info) {
			if (cpm.find(pr.first) != cpm.end()) {
				int cls_assigned = ceil(cpm[pr.first] * static_cast<double>(upper_quota[pr.first]) / pr.second);
				quota_map[idx.first][idx.second][pr.first] = cls_assigned;
				filled_cls_result[pr.first] += cls_assigned;
				filled_pos_result[pos] += cls_assigned;
			}
			else {
				quota_map[idx.first][idx.second].erase(pr.first);
			}
		}
	}

	for (auto& pr : filled_cls_result) {
		vector<pair<uint, uint>*> cls_idxs;
		if (pr.second > upper_quota[pr.first]) {
			for (auto& idx : indices) {
				auto& qm = quota_map[idx.first][idx.second];
				if (qm.find(pr.first) != qm.end())
					cls_idxs.push_back(&idx);
			}
			random_shuffle(cls_idxs.begin(), cls_idxs.end());
		}
		while (pr.second > upper_quota[pr.first]) {
			int current = pr.second;
			for (auto idx : cls_idxs) {
				if (filled_pos_result[idx->first * side_length + idx->second] > getVal(previous_assigned_maps.back(), *idx)
					&& quota_map[idx->first][idx->second][pr.first] > 0) {
					--filled_pos_result[idx->first * side_length + idx->second];
					--pr.second;
					--quota_map[idx->first][idx->second][pr.first];
				}
				if (pr.second == upper_quota[pr.first])
					break;
			}
			if (current == pr.second) { // can't remove points of this class
				break;
			}
		}
	}

	vector<pair<uint, uint>*> surplus_idxs, deficit_idxs;
	for (auto& idx : indices) {
		uint pos = idx.first * side_length + idx.second;
		if (filled_pos_result[pos] > getVal(previous_assigned_maps.back(), idx))
			surplus_idxs.push_back(&idx);
		if (filled_pos_result[pos] < getVal(previous_assigned_maps.back(), idx))
			deficit_idxs.push_back(&idx);
		for (auto it = quota_map[idx.first][idx.second].begin(); it != quota_map[idx.first][idx.second].end();) {
			if (it->second == 0)
				it = quota_map[idx.first][idx.second].erase(it);
			else
				++it;
		}
	}
	unordered_map<uint, int> cls_balance_sheet;
	for (auto idx : surplus_idxs) {
		for (auto it = quota_map[idx->first][idx->second].begin(); it != quota_map[idx->first][idx->second].end();) {
			--it->second;
			--filled_pos_result[idx->first * side_length + idx->second];
			++cls_balance_sheet[it->second];
			if (it->second == 0)
				it = quota_map[idx->first][idx->second].erase(it);
			else
				++it;
			if (filled_pos_result[idx->first * side_length + idx->second] == getVal(previous_assigned_maps.back(), *idx))
				break;
		}
	}
	for (auto idx : deficit_idxs) {
		for (auto& pr : cls_balance_sheet) {
			if (pr.second > 0) {
				--pr.second;
				++filled_pos_result[idx->first * side_length + idx->second];
				++quota_map[idx->first][idx->second][pr.first];
				if (filled_pos_result[idx->first * side_length + idx->second] == getVal(previous_assigned_maps.back(), *idx))
					break;
			}
		}
	}
}

int HaarWaveletSampling::backtracking(DensityMap& cover_map)
{
	auto& last_map = previous_assigned_maps.back();
	vector<vector<double>> match_rates(side_length, vector<double>(side_length));
	int shift;
	
	for (int k = 1; k < side_length; k = shift) {
		shift = k << 1;
		bool changed = false, enough = true;
		for (int i = 0; i < horizontal_bin_num; i += shift) {
			for (int j = 0; j < vertical_bin_num; j += shift) {
				if (!update_needed_map[i][j]) continue;

				transformHelper(actual_map, i, j, k);
				transformHelper(last_map, i, j, k);
				transformHelper(class_points_map, i, j, k);
				if (last_map[i][j] > params.max_sample_number ||
					isBestMatched(match_rates, i, j, k))
					continue; 
				else if (last_map[i][j] == 0) {
					match_rates[i][j] = 1.0;
					continue;
				}

				if (last_map[i][j] >= class_points_map[i][j].size()) { // can show all classes
					match_rates[i][j] = fillQuota(i, j);
					cover_map[i][j] = shift;
					changed = true;
				}
				else {
					enough = false;
				}
			}
		}
		if (enough && !changed)
			break;
	}
	return shift;
}

void HaarWaveletSampling::assignQuotaToSubregions(DensityMap& cover_map, int begin_shift)
{
	auto& last_map = previous_assigned_maps.back();
	int k;

	for (int shift = begin_shift; shift > 1; shift = k) {
		k = shift >> 1;

		for (int j = 0; j < vertical_bin_num; j += shift) {
			for (int i = 0; i < horizontal_bin_num; i += shift) {
				if (!update_needed_map[i][j]) continue;
				
				transformHelper(actual_map, i, j, k, true);
				transformHelper(last_map, i, j, k, true);
				if (shift == cover_map[i][j]) {
					const unordered_map<uint, int> upper_class_info = class_points_map[i][j];
					transformHelper(class_points_map, i, j, k, true);
					splitQuota(upper_class_info, i, j, k);
					degradeCoverMap(cover_map, i, j, k);
				}
				else {
					transformHelper(class_points_map, i, j, k, true);
				}
			}
		}
	}
}

void HaarWaveletSampling::initializeGrids()
{
	for (uint i = 0; i < horizontal_bin_num; ++i)
		for (uint j = 0; j < vertical_bin_num; ++j) {
			actual_map[i][j] = 0;
			index_map[i][j].clear();
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
			update_needed_map[i][j] = is_first_frame;
		}
	}
}

void HaarWaveletSampling::computeAssignMapsProgressively(const FilteredPointSet* origin)
{
	for (auto& pr : *origin) {
		auto &p = pr.second;
		int x = visual2grid(p->pos.x(), MARGIN.left),
			y = visual2grid(p->pos.y(), MARGIN.top);
		if (index_map[x][y][p->label] == 0) {
			index_map[x][y][p->label] = pr.first;
		}
		if (actual_map[x][y] == 0) {
			elected_points[x][y] = make_unique<LabeledPoint>(p);
		}
		else if (double_dist(gen) < 0.1) {
			elected_points[x][y]->pos = p->pos;
			elected_points[x][y]->label = p->label;
			index_map[x][y][p->label] = pr.first;
		}
		++actual_map[x][y];
		++class_points_map[x][y][p->label];
	}

	convertToDensityMap();
	forwardTransform();
	inverseTransform();
	//determineLabelOfSeeds();
}

void HaarWaveletSampling::forwardTransform()
{
//#include <fstream>
	int shift;
	for (int k = 1; k < side_length; k = shift) {
		shift = k << 1;
		//ofstream output(string("./results/test_csv/") + to_string(shift) + "_" + to_string(last_frame_id) + ".csv", ios_base::trunc);
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

void HaarWaveletSampling::inverseTransform()
{
//#include <fstream>
	int k;
	assigned_map[0][0] = visual_map[0][0];
	for (int shift = side_length; shift > 1; shift = k) {
		k = shift >> 1;

		//ofstream output(string("./results/test_csv/") + to_string(shift) + "_" + to_string(params.end_level) + "_" + to_string(last_frame_id) + ".csv", ios_base::trunc);
		for (int j = 0; j < vertical_bin_num; j += shift) {
			for (int i = 0; i < horizontal_bin_num; i += shift) {
				int actual_point_num = actual_map[i][j], visual_point_num = visual_map[i][j], total_assigned_points = assigned_map[i][j];
				//if(assigned_map[i][j]>0) output << assigned_map[i][j];
				//output << ',';
				if (total_assigned_points == 0) {
					if (visual_point_num != 0) {
						transformHelper(actual_map, i, j, k, true);
						transformHelper(visual_map, i, j, k, true);
						if (!is_first_frame) transformHelper(previous_assigned_maps.back(), i, j, k, true);
					}
					continue;
				}

				vector<pair<int, int>> indices = { { i,j },{ i + k,j },{ i,j + k },{ i + k,j + k } };

				if(shift > end_shift) {
					auto low_high_pair = lowDensityJudgementHelper(indices);
					vector<pair<int, int>> low_density_indices = low_high_pair[0], high_density_indices = low_high_pair[1];
					bool update_needed = (!is_first_frame && !update_needed_map[i][j]) && hasDiscrepancy(indices), // error exceeds the bound
						has_same_assigned_points = update_needed && (previous_assigned_maps.back()[i][j] == assigned_map[i][j]);

					transformHelper(actual_map, i, j, k, true);
					transformHelper(visual_map, i, j, k, true);
					if (!is_first_frame) transformHelper(previous_assigned_maps.back(), i, j, k, true);
					assigned_map[i][j] = 0;
					
					// calculate assigned number based on relative data density
					sort(high_density_indices.begin(), high_density_indices.end(), [this](const pair<int, int> &a, const pair<int, int> &b) {
						int am_a = getVal(actual_map, a), am_b = getVal(actual_map, b);
						return am_a > am_b ? true : am_a < am_b ? false :
							getVal(visual_map, a) > getVal(visual_map, b);
					});
					int &max_assigned_val = assigned_map[high_density_indices[0].first][high_density_indices[0].second];
					max_assigned_val = (int)ceil(static_cast<double>(getVal(visual_map, high_density_indices[0])) * total_assigned_points / visual_point_num);

					double sampling_ratio_zero = static_cast<double>(max_assigned_val) / getVal(actual_map, high_density_indices[0]);
					int remain_assigned_point_num = total_assigned_points - max_assigned_val;
					for (size_t _i = 1, sz = high_density_indices.size(); _i < sz && remain_assigned_point_num > 0; ++_i) {
						int actual_val = getVal(actual_map, high_density_indices[_i]);
						if (actual_val == 0) break; // an empty area can only lead to useless calculation

						int assigned_val = round(sampling_ratio_zero * actual_val); // at least 1
						assigned_val = min({ assigned_val, getVal(visual_map, high_density_indices[_i]), remain_assigned_point_num });
						assigned_map[high_density_indices[_i].first][high_density_indices[_i].second] = assigned_val;

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
						high_density_sum = actual_point_num - low_density_sum;
						high_visual_sum = visual_point_num - low_visual_sum;

                        for(auto &idx : high_density_indices) {
                            high_assigned += getVal(assigned_map, idx);
                        }
						int low_assigned = round(high_assigned * ((1.0 - params.low_density_weight)*low_density_sum / high_density_sum + params.low_density_weight * low_visual_sum / high_visual_sum));
                        sort(low_density_indices.begin(), low_density_indices.end(), [this](const pair<int, int> &a, const pair<int, int> &b) {
                            return getVal(visual_map, a) < getVal(visual_map, b);
                        });
                        for (size_t _i = 0, sz = low_density_indices.size(); _i < sz; ++_i) {
                            int assigned_val = ceil(static_cast<double>(getVal(visual_map, low_density_indices[_i])) * low_assigned / low_visual_sum);
							//int assigned_val = ceil(total_assigned_points *
							//	((1.0 - params.low_density_weight) * getVal(actual_map, indices[_i]) / actual_point_num + params.low_density_weight * getVal(visual_map, indices[_i]) / visual_point_num));
                            int &ref2map = assigned_map[low_density_indices[_i].first][low_density_indices[_i].second];
                            ref2map = max(assigned_val, ref2map);
                        }
                    }

					if (update_needed) {
						if (has_same_assigned_points) {
							for (auto& idx : indices) {
								has_same_assigned_points &= (getVal(previous_assigned_maps.back(), idx) == getVal(assigned_map, idx));
							}
						}
						if (!has_same_assigned_points) {
							for (int _i = i, i_e = i + shift; _i < i_e; ++_i) {
								for (int _j = j, j_e = j + shift; _j < j_e; ++_j) {
									update_needed_map[_i][_j] = true;
								}
							}
						}
					}
				}
				else {
					transformHelper(actual_map, i, j, k, true);
					transformHelper(visual_map, i, j, k, true);
					if (!is_first_frame) transformHelper(previous_assigned_maps.back(), i, j, k, true);
					assigned_map[i][j] = 0;

					sort(indices.begin(), indices.end(), [this](const pair<int, int> &a, const pair<int, int> &b) {
						return getVal(actual_map, a) > getVal(actual_map, b);
					});
					int remain_assigned_point_num = total_assigned_points;
					for (size_t _i = 0, sz = indices.size(); _i < sz && remain_assigned_point_num > 0; ++_i) {
						int assigned_val = ceil((double)total_assigned_points * getVal(visual_map, indices[_i]) / visual_point_num);
						assigned_val = min({ assigned_val, getVal(visual_map, indices[_i]), remain_assigned_point_num });
						assigned_map[indices[_i].first][indices[_i].second] = assigned_val;

						remain_assigned_point_num -= assigned_val;
					}
				}

			}
			//output << '\n';
		}

		if (shift < side_length && k > 0) {
			int area = k*k;
			for (int j = 0; j < vertical_bin_num; j += k) {
				for (int i = k; i + k < horizontal_bin_num; i += shift) {
					smoothingHelper(make_pair(i, j), make_pair(i + k, j), area);
				}
			}
			for (int j = k; j + k < vertical_bin_num; j += shift) {
				for (int i = 0; i < horizontal_bin_num; i += k) {
					smoothingHelper(make_pair(i, j), make_pair(i, j + k), area);
				}
			}
		}
	}

	if (is_first_frame) {
		static DensityMap empty_map(side_length, vector<int>(side_length));
		if (empty_map.size() != side_length) {
			empty_map = DensityMap(side_length, vector<int>(side_length));
		}
		
		for (uint j = 0; j < vertical_bin_num; ++j) {
			for (uint i = 0; i < horizontal_bin_num; ++i) {
				if (empty_map[i][j] != 0 && assigned_map[i][j] == 0) {
					_removed.push_back(make_pair(i, j));
				}
				else if (empty_map[i][j] == 0 && assigned_map[i][j] != 0) {
					_added.push_back(make_pair(i, j));
				}
			}
		}
		previous_assigned_maps.push_back(assigned_map);
	}
	else {
		//ofstream output(string("./results/test_csv/updateNeeded_")+to_string(last_frame_id)+".csv", ios_base::trunc);
		DensityMap old = previous_assigned_maps.back();
		for (uint j = 0; j < vertical_bin_num; ++j) {
			for (uint i = 0; i < horizontal_bin_num; ++i) {
				if (update_needed_map[i][j]) {
					if (old[i][j] != 0 && assigned_map[i][j] == 0) {
						_removed.push_back(make_pair(i, j));
						old[i][j] = assigned_map[i][j];
					}
					else if (old[i][j] == 0 && assigned_map[i][j] != 0) {
						_added.push_back(make_pair(i, j));
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

void HaarWaveletSampling::determineLabelOfSeeds()
{
	DensityMap cover_map(side_length, vector<int>(side_length, 1));
	int shift = backtracking(cover_map);
	assignQuotaToSubregions(cover_map, shift);
}

Indices HaarWaveletSampling::selectSeeds()
{
	Indices result;

	for (uint i = 0; i < horizontal_bin_num; ++i) {
		for (uint j = 0; j < vertical_bin_num; ++j) {
			if (previous_assigned_maps.back()[i][j] != 0) {
				if (quota_map[i][j].empty())
					result.push_back(index_map[i][j][elected_points[i][j]->label]);
				else
					result.push_back(index_map[i][j][quota_map[i][j].begin()->first]);
			}
		}
	}
	//qDebug() << result.size();
	last_frame_id = previous_assigned_maps.size() - 1;
	return result;
}

pair<TempPointSet, TempPointSet>* HaarWaveletSampling::getSeedsDifference()
{
	TempPointSet removed, added;
	static int current_point_num;
	if (is_first_frame) current_point_num = 0;

	for (auto& idx : this->_removed) {
		removed.push_back(elected_points[idx.first][idx.second].get());
	}
	for (auto& idx : this->_added) {
		if (!quota_map[idx.first][idx.second].empty())
			elected_points[idx.first][idx.second]->label = quota_map[idx.first][idx.second].begin()->first;
		added.push_back(elected_points[idx.first][idx.second].get());
	}
	int change = ((int)added.size() - (int)removed.size());
	qDebug() << (int)added.size() + (int)removed.size();
	current_point_num += change;
	//qDebug() << "add: " << added.size() << "remove:" << removed.size() << ", frame_id=" << last_frame_id + 1;
	//qDebug() << "The number of points in current frame: " << current_point_num;

	last_frame_id = previous_assigned_maps.size() - 1;
	return new pair<TempPointSet, TempPointSet>(removed, added);
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
