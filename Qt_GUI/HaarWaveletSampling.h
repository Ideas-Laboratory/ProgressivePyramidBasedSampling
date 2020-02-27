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
	std::pair<TempPointSet, TempPointSet> execute(const std::weak_ptr<FilteredPointSet> origin, bool is_first_frame);

	PointSet selectSeeds();
	std::pair<TempPointSet, TempPointSet> getSeedsDifference();
	std::pair<TempPointSet, TempPointSet> getSeedsWithDiff();

	int getFrameID() { return last_frame_id; }
	uint getSideLength() { return side_length; }
	void setEndShift(int shift) { end_shift = shift; }

private:
	int getVal(const DensityMap &dm, const std::pair<int, int>& index) const { return dm[index.first][index.second]; }
	void transformHelper(DensityMap & dm, int i, int j, int interval, bool inverse = false);
	std::vector<std::vector<std::pair<int, int>>> lowDensityJudgementHelper(const std::vector<std::pair<int, int>>& indices) const;
	bool hasDiscrepancy(const std::vector<std::pair<int, int>>& indices);

	void computeAssignMapsProgressively(const std::weak_ptr<FilteredPointSet> origin);
	void initializeGrids();
	void convertToDensityMap();
	void forwardTransform();
	void inverseTransfrom();
	
	std::vector<std::vector<std::unique_ptr<LabeledPoint>>> elected_points;

	DensityMap actual_map;
	DensityMap visual_map;
	DensityMap assigned_map;

	std::vector<DensityMap> previous_assigned_maps;
	std::vector<std::vector<bool>> update_needed_map;
	std::vector<std::pair<int, int>> added, removed;
	bool is_first_frame;
	int last_frame_id;

	uint horizontal_bin_num, // the actual number of horizontal bins
		 vertical_bin_num, // the actual number of vertical bins
		 side_length; // the length of the side of the bounding square

	int end_shift;

	std::mt19937 gen{ std::random_device{}() };
	std::uniform_real_distribution<> double_dist;
};

inline int visual2grid(qreal pos, qreal margin)
{
	return static_cast<int>(pos - margin) / params.grid_width;
}

inline qreal grid2visual(qreal pos, qreal margin)
{
	return pos*params.grid_width + margin;
}