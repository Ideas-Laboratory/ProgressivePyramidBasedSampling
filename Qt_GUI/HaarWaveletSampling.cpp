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
	for (uint i = 0; i < side_length; ++i) {
		actual_map[i].resize(side_length);
		visual_map[i].resize(side_length);
		assigned_map[i].resize(side_length);
		update_needed_map[i].resize(side_length);
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

bool HaarWaveletSampling::isErrorOurOfBound(const std::vector<std::pair<int, int>>& indices)
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
	}

	convertToDensityMap();
	forwardTransform();
	customizedInverseTransform();
}

void HaarWaveletSampling::forwardTransform()
{
	int shift;
	for (int k = 1; k < side_length; k = shift) {
		shift = k << 1;
		// only traverse the bins inside the screen
		for (int i = 0; i < horizontal_bin_num; i += shift) {
			for (int j = 0; j < vertical_bin_num; j += shift) {
				transformHelper(actual_map, i, j, k);
				transformHelper(visual_map, i, j, k);
				if (!is_first_frame)
					transformHelper(previous_assigned_maps.back(), i, j, k);
			}
		}
	}
}

void HaarWaveletSampling::customizedInverseTransform()
{
	int k;
	assigned_map[0][0] = visual_map[0][0];
	for (int shift = side_length; shift > 1; shift = k) {
		k = shift >> 1;

		for (int j = 0; j < vertical_bin_num; j += shift) {
			for (int i = 0; i < horizontal_bin_num; i += shift) {
				// initial some common local variables
				int actual_point_num = actual_map[i][j], visual_point_num = visual_map[i][j], total_assigned_points = assigned_map[i][j];
				vector<pair<int, int>> indices = { { i,j },{ i + k,j },{ i,j + k },{ i + k,j + k } };
				// skip empty regions
				if (total_assigned_points == 0) {
					if (visual_point_num != 0) {
						transformHelper(actual_map, i, j, k, true);
						transformHelper(visual_map, i, j, k, true);
						if (!is_first_frame) transformHelper(previous_assigned_maps.back(), i, j, k, true);
					}
					continue;
				}

				if(shift > end_shift) {
					/* DetectLowDensityRegions */
					auto low_high_pair = lowDensityJudgementHelper(indices);
					vector<pair<int, int>> low_density_indices = low_high_pair[0], high_density_indices = low_high_pair[1];

					// detect changed regions
					bool update_needed = (!is_first_frame && !update_needed_map[i][j]) && isErrorOurOfBound(indices),
						have_identical_assigned_points = update_needed && (previous_assigned_maps.back()[i][j] == assigned_map[i][j]);

					/* InverseTransform */
					transformHelper(actual_map, i, j, k, true);
					transformHelper(visual_map, i, j, k, true);
					if (!is_first_frame) transformHelper(previous_assigned_maps.back(), i, j, k, true);
					assigned_map[i][j] = 0;
					
					/* AssignForOrdinaryRegions */
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
						if (actual_val == 0) break; // skip empty areas

						int assigned_val = round(sampling_ratio_zero * actual_val); // at least 1
						assigned_val = min({ assigned_val, getVal(visual_map, high_density_indices[_i]), remain_assigned_point_num });
						assigned_map[high_density_indices[_i].first][high_density_indices[_i].second] = assigned_val;

						remain_assigned_point_num -= assigned_val;
					}

					// calculate assigned number for low density regions intentionally
                    if(!low_density_indices.empty()) {
						/* AssignForSparseRegions */
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
                            int &ref2map = assigned_map[low_density_indices[_i].first][low_density_indices[_i].second];
                            ref2map = max(assigned_val, ref2map);
                        }
                    }

					// mark changed regions
					if (update_needed) {
						if (have_identical_assigned_points) {
							for (auto& idx : indices) {
								have_identical_assigned_points &= (getVal(previous_assigned_maps.back(), idx) == getVal(assigned_map, idx));
							}
						}
						if (!have_identical_assigned_points) {
							for (int _i = i, i_e = i + shift; _i < i_e; ++_i) {
								for (int _j = j, j_e = j + shift; _j < j_e; ++_j) {
									update_needed_map[_i][_j] = true;
								}
							}
						}
					}
				}
				else {
					/* DirectlyExpand */
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
		}

		/* RepairAcrossBoundary */
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
				}
			}
		}
		previous_assigned_maps.push_back(move(old));
	}
}

Indices HaarWaveletSampling::selectSeeds()
{
	Indices result;

	for (uint i = 0; i < horizontal_bin_num; ++i) {
		for (uint j = 0; j < vertical_bin_num; ++j) {
			if (previous_assigned_maps.back()[i][j] != 0) {
				result.push_back(index_map[i][j][elected_points[i][j]->label]);
			}
		}
	}

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
		added.push_back(elected_points[idx.first][idx.second].get());
	}
	int change = ((int)added.size() - (int)removed.size());
	qDebug() << (int)added.size() + (int)removed.size();
	current_point_num += change;
	qDebug() << "The number of points in current frame: " << current_point_num;

	last_frame_id = previous_assigned_maps.size() - 1;
	return new pair<TempPointSet, TempPointSet>(move(removed), move(added));
}

TempPointSet HaarWaveletSampling::getSeedPoints()
{
	if (params.displayed_frame_id > previous_assigned_maps.size() || params.displayed_frame_id < 1)
		return TempPointSet();
	DensityMap &assigned = previous_assigned_maps[params.displayed_frame_id - 1];
	TempPointSet results;
	for (uint i = 0; i < horizontal_bin_num; ++i) {
		for (uint j = 0; j < vertical_bin_num; ++j) {
			if (assigned[i][j] != 0) {
				results.push_back(elected_points[i][j].get());
			}
		}
	}
	return move(results);
}