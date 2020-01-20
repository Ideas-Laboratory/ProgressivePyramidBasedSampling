#include "HaarWaveletSampling.h"

using namespace std;

const static vector<uint> power_2 = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192 };

HaarWaveletSampling::HaarWaveletSampling(const QRect & bounding_rect)
{
	horizontal_bin_num = bounding_rect.width() / params.grid_width + 1;
	vertical_bin_num = bounding_rect.height() / params.grid_width + 1;

	screen_grids.resize(horizontal_bin_num);
	for (uint i = 0; i < horizontal_bin_num; ++i) {
		screen_grids[i].resize(vertical_bin_num);
	}

	side_length = max(*lower_bound(power_2.begin(), power_2.end(), horizontal_bin_num), *lower_bound(power_2.begin(), power_2.end(), vertical_bin_num));

	actual_map.resize(side_length);
	visual_map.resize(side_length);
	assigned_map.resize(side_length);
	for (uint i = 0; i < side_length; ++i) {
		actual_map[i].resize(side_length);
		visual_map[i].resize(side_length);
		assigned_map[i].resize(side_length);
	}
}

Indices HaarWaveletSampling::execute(const weak_ptr<FilteredPointSet> origin)
{
	end_shift = pow(2, params.end_level);

	convertToDensityMaps(origin);
	forwardTransform();
	inverseTransfrom();
	return selectSeeds();
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

vector<pair<int, int>> HaarWaveletSampling::lowDensityJudgementHelper(const vector<pair<int, int>>& indices) const
{
	double threshold = params.low_density_threshold*getVal(actual_map, indices[0]); // A * tau(the nondense rate)
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

	vector<pair<int, int>> result;
	std::transform(low_density_indices.begin(), low_density_indices.end(), back_inserter(result), [&](int i) { return indices[i]; });
	return result;
}

void HaarWaveletSampling::convertToDensityMaps(const weak_ptr<FilteredPointSet> origin)
{
	// initialization
	for (uint i = 0; i < horizontal_bin_num; ++i)
		for (uint j = 0; j < vertical_bin_num; ++j)
			screen_grids[i][j].clear();

	for (const auto &pr : *(origin.lock())) {
		auto &p = pr.second;
		if (find(selected_class_order.begin(), selected_class_order.end(), p->label) == selected_class_order.end())
			continue;
		int x = visual2grid(p->pos.x(), MARGIN.left),
			y = visual2grid(p->pos.y(), MARGIN.top);
		auto &grid = screen_grids[x][y];
		grid.push_back(pr.first);
	}

	for (uint i = 0; i < side_length; ++i) {
		for (uint j = 0; j < side_length; ++j) {
			if (i < horizontal_bin_num && j < vertical_bin_num) {
				actual_map[i][j] = screen_grids[i][j].size();
				visual_map[i][j] = screen_grids[i][j].empty() ? 0 : 1;
				assigned_map[i][j] = 0;
			}
			else {
				actual_map[i][j] = visual_map[i][j] = assigned_map[i][j] = 0;
			}
		}
	}
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
		
		//ofstream output(string("./results/test_csv/") + to_string(shift) + ".csv", ios_base::trunc);
		for (int j = 0; j < vertical_bin_num; j += shift) {
			for (int i = 0; i < horizontal_bin_num; i += shift) {
				vector<pair<int, int>> indices = { { i,j },{ i + k,j },{ i,j + k },{ i + k,j + k } };
				vector<pair<int, int>> low_density_indices = lowDensityJudgementHelper(indices);
				//if(assigned_map[i][j]>0) output << assigned_map[i][j];
				//output << ',';

				int visual_point_num = visual_map[i][j], assigned_visual_point_num = assigned_map[i][j];
				transformHelper(actual_map, i, j, k, true);
				transformHelper(visual_map, i, j, k, true);
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
					sort(low_density_indices.begin(), low_density_indices.end(), [this](const pair<int, int> &a, const pair<int, int> &b) {
						return getVal(visual_map, a) < getVal(visual_map, b);
					});
					for (size_t _i = 1, sz = low_density_indices.size(); _i < sz; ++_i) {
						int x = getVal(assigned_map, low_density_indices[_i - 1]), y = getVal(assigned_map, low_density_indices[_i]);
						if (x == 0) continue;

						int assigned_val = ceil((static_cast<double>(getVal(visual_map, low_density_indices[_i])) / getVal(visual_map, low_density_indices[_i - 1]) + y) / (1.0 + 1.0 / x));
						assigned_val = min(assigned_val, getVal(visual_map, low_density_indices[_i]));
						assigned_map[low_density_indices[_i].first][low_density_indices[_i].second] = assigned_val;
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

			}
			//output << '\n';
		}
	}
}

Indices HaarWaveletSampling::selectSeeds()
{
	Indices result;

	for (uint i = 0; i < horizontal_bin_num; ++i) {
		for (uint j = 0; j < vertical_bin_num; ++j) {
			if (assigned_map[i][j] == 1) {
				Indices &local = screen_grids[i][j];
				//qDebug() << i << ' ' << j;
				std::uniform_int_distribution<> dis(0, local.size() - 1);
				result.push_back(local[dis(gen)]);
			}
		}
	}
	qDebug() << result.size();
	return result;
}
