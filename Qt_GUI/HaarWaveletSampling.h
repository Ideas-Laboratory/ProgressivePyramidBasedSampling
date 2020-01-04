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
	Indices execute(const std::weak_ptr<FilteredPointSet> origin);

	Indices selectSeeds();

	uint getSideLength() { return side_length; }
	void setEndShift(int shift) { end_shift = shift; }

private:
	int getVal(const DensityMap &dm, const std::pair<int, int>& index) const { return dm[index.first][index.second]; }
	double getConvolvedVal(int i, int j) const;
	void transformHelper(DensityMap & dm, int i, int j, int interval, bool inverse = false);
	std::vector<std::pair<int, int>> lowDensityJudgementHelper(const std::vector<std::pair<int, int>>& indices) const;

	void convertToDensityMaps(const std::weak_ptr<FilteredPointSet> origin);
	void forwardTransform();
	void inverseTransfrom();
	
	std::vector<std::vector<Indices>> screen_grids;

	DensityMap actual_map;
	DensityMap visual_map;
	DensityMap assigned_map;

	uint horizontal_bin_num, // the actual number of horizontal bins
		 vertical_bin_num, // the actual number of vertical bins
		 side_length; // the length of the side of the bounding square

	int end_shift;

	std::mt19937 g{ std::random_device{}() };
};

inline int visual2grid(qreal pos, qreal margin)
{
	return static_cast<int>(pos - margin) / params.grid_width;
}

inline qreal grid2visual(qreal pos, qreal margin)
{
	return pos*params.grid_width + margin;
}