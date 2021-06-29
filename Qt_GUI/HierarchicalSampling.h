#pragma once

#include <QRect>
#include <set>
#include <random>

#include "global.h"
#include "utils.h"

using DensityMap = std::vector<std::vector<int>>;

extern Param params;
extern std::vector<int> selected_class_order;

class Pyramid
{
public:
	friend class HierarchicalSampling;
	enum MapType {Density, Visibility, Assignment};
	int getVal(MapType t, int level, const std::pair<int, int>& idx) {
		switch (t)
		{
		case Pyramid::Density:
			return density_map[level][idx.first][idx.second];
		case Pyramid::Visibility:
			return visibility_map[level][idx.first][idx.second];
		case Pyramid::Assignment:
			return assignment_map[level][idx.first][idx.second];
		default:
			exit(-1); // error
		}
	}
	
private:
	std::vector<DensityMap> density_map;
	std::vector<DensityMap> visibility_map;
	std::vector<DensityMap> assignment_map;
};

class HierarchicalSampling
{
public:
	HierarchicalSampling(const QRect& bounding_rect);

	// the main function that executes the sampling process and returns added and removed points in comparison to the previous frame
	std::pair<PointSet, PointSet>* execute(const FilteredPointSet* origin, bool is_first_frame);

	Indices getSeedIndices();
	// returns the index of added and removed points in comparison to the previous frame
	std::pair<PointSet, PointSet>* getSeedsDifference();
	// returns the unchanged and changed points between the result of current params.displayed_frame_id and the last result
	std::pair<PointSet, PointSet> getSeedsWithDiff();
	PointSet getSeeds();

	int getFrameID() { return last_frame_id; }

private:
	// sum the values of four children nodes at the given level
	void constructionHelper(std::vector<DensityMap>& map_list, int level, int i, int j);
	// classify regions to high- and low-density according to \lambda
	std::vector<std::vector<std::pair<int, int>>> classifyRegions(int level, const std::vector<std::pair<int, int>>& indices);
	// determine whether the adjacent regions violate the data density ratios and perform the sampling refinement at the given level
	void smoothingHelper(DensityMap& assignment_map, std::pair<int, int>&& pos_x, std::pair<int, int>&& pos_y, int level);
	// for the local region update stage
	bool detectChangedRegion(int level, const std::vector<std::pair<int, int>>& indices);
	// for the adjacent region refinement stage
	void adjacentChangedHelper(const DensityMap& assignment_map, std::pair<int, int>&& pos_x, std::pair<int, int>&& pos_y, int level);
	bool isChangedRegion(int level, int i, int j);
	// set all subregions to "changed" in the expanded map
	void setChangedRegion(int level, int i, int j);

	// initialize the predefined density maps
	void initializeGrids();
	// the framework of pyramid-based sampling
	void computeAssignMapsProgressively(const FilteredPointSet* origin);
	// map input points to screen
	void convertToDensityMap(const FilteredPointSet* origin);
	// construct density pyramid, visibility pyramid, and assignment pyramid if it is not the first frame
	void constructPyramids();
	// the main sampling procedure described by the Algorithm 1 in the paper
	void generateAssignmentMapsHierarchically();

	struct DateHash {
	public:
		std::size_t operator()(const QDate &d) const
		{
			return std::hash<int>{}(d.year()) * 372 + std::hash<int>{}(d.month()) * 31 + std::hash<int>{}(d.day()); // 12 * 31 = 372
		}
	};
	std::unordered_map<QDate, DensityMap, DateHash> sliding_window;

	Pyramid py;
	std::vector<std::vector<bool>> changed_map;
	std::vector<DensityMap> previous_assigned_maps;

	std::vector<std::vector<std::unique_ptr<LabeledPoint>>> elected_points;
	std::vector<std::vector<std::unordered_map<uint, uint>>> index_map;
	std::vector<std::pair<int, int>> _added, _removed;

	uint horizontal_bin_num, // the actual number of horizontal bins
		vertical_bin_num; // the actual number of vertical bins
	int max_level;
	
	bool is_first_frame;
	int last_frame_id;

	std::mt19937 gen{ std::random_device{}() };
	std::uniform_real_distribution<> double_dist;
};