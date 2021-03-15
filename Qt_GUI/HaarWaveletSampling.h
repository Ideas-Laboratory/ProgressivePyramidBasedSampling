#pragma once

#include <QRect>
#include <set>
#include <random>

#include "global.h"
#include "utils.h"

using DensityMap = std::vector<std::vector<int>>;

extern Param params;
extern std::vector<int> selected_class_order;

class HaarWaveletSampling
{
public:
	HaarWaveletSampling(const QRect& bounding_rect);

	/* main function that contains the framework */
	std::pair<TempPointSet, TempPointSet>* execute(const FilteredPointSet* origin, bool is_first_frame);

	Indices selectSeeds();
	std::pair<TempPointSet, TempPointSet>* getSeedsDifference();
	std::pair<TempPointSet, TempPointSet> getSeedsWithDiff();
	TempPointSet getSeeds();

	int getFrameID() { return last_frame_id; }
	uint getSideLength() { return side_length; }
	void setEndShift(int shift) { end_shift = shift; }
	uint getIndexByPos(const QPointF& pos) {
		int i = visual2grid(pos.x(), MARGIN.left),
			j = visual2grid(pos.y(), MARGIN.top);
		return index_map[i][j][elected_points[i][j]->label];
	}

private:
	int getVal(const DensityMap &dm, const std::pair<int, int>& index) const { return dm[index.first][index.second]; }
	bool isBestMatched(const std::vector<std::vector<double>>& dm, int i, int j, int interval) {
		int i2 = i + interval, j2 = j + interval;
		bool result = dm[i][j] == 1.0;
		if (update_needed_map[i][j2]) result &= dm[i][j2] == 1.0;
		if (update_needed_map[i2][j]) result &= dm[i2][j] == 1.0;
		if (update_needed_map[i2][j2]) result &= dm[i2][j2] == 1.0;
		return result;
	}
	void transformHelper(DensityMap& dm, int i, int j, int interval, bool inverse = false);
	void transformHelper(std::vector<std::vector<std::unordered_map<uint, int>>>& dm, int i, int j, int interval, bool inverse = false);
	std::vector<std::vector<std::pair<int, int>>> lowDensityJudgementHelper(const std::vector<std::pair<int, int>>& indices) const;
	void smoothingHelper(std::pair<int, int> &&pos_x, std::pair<int, int> &&pos_y, int k);
	bool hasDiscrepancy(const std::vector<std::pair<int, int>>& indices);
	double fillQuota(int i, int j);
	void splitQuota(const std::unordered_map<uint, int>& upper_info, int i, int j, int interval);
	void degradeCoverMap(DensityMap& dm, int i, int j, int interval) {
		dm[i + interval][j] = dm[i][j + interval] = dm[i + interval][j + interval] = dm[i][j] >>= 1;
	}
	
	int backtracking(DensityMap& cover_map);
	void assignQuotaToSubregions(DensityMap& cover_map, int begin_shift);

	void computeAssignMapsProgressively(const FilteredPointSet* origin);
	void initializeGrids();
	void convertToDensityMap();
	void forwardTransform();
	void inverseTransform();
	void determineLabelOfSeeds();
	
	std::vector<std::vector<std::unique_ptr<LabeledPoint>>> elected_points;

	DensityMap actual_map;
	DensityMap visual_map;
	DensityMap assigned_map;

	std::vector<DensityMap> previous_assigned_maps;
	std::vector<std::vector<bool>> update_needed_map;
	std::vector<std::vector<std::unordered_map<uint, uint>>> index_map;
	std::vector<std::vector<std::unordered_map<uint, int>>> class_points_map;
	std::vector<std::vector<std::unordered_map<uint, int>>> quota_map;
	std::vector<std::pair<int, int>> _added, _removed;
	bool is_first_frame;
	int last_frame_id;

	uint horizontal_bin_num, // the actual number of horizontal bins
		 vertical_bin_num, // the actual number of vertical bins
		 side_length; // the length of the side of the bounding square

	int end_shift;

	std::mt19937 gen{ std::random_device{}() };
	std::uniform_real_distribution<> double_dist;
};