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

	/* main function that contains the framework */
	std::pair<PointSet, PointSet>* execute(const FilteredPointSet* origin, bool is_first_frame);

	Indices selectSeeds();
	std::pair<PointSet, PointSet>* getSeedsDifference();
	std::pair<PointSet, PointSet> getSeedsWithDiff();
	PointSet getSeeds();

	int getFrameID() { return last_frame_id; }
	uint getIndexByPos(const QPointF& pos) {
		int i = visual2grid(pos.x(), MARGIN.left),
			j = visual2grid(pos.y(), MARGIN.top);
		return index_map[i][j][elected_points[i][j]->label];
	}

private:
	void constructionHelper(std::vector<DensityMap>& map_list, int level, int i, int j);
	std::vector<std::vector<std::pair<int, int>>> classifyRegions(int level, const std::vector<std::pair<int, int>>& indices);
	void refinementHelper(DensityMap& assignment_map, int level, int i, int j, bool is_horizontal);
	void smoothingHelper(DensityMap& assignment_map, std::pair<int, int>&& pos_x, std::pair<int, int>&& pos_y, int level);
	void adjacentChangedHelper(const DensityMap& assignment_map, std::pair<int, int>&& pos_x, std::pair<int, int>&& pos_y, int level);
	bool detectChangedRegion(int level, const std::vector<std::pair<int, int>>& indices);
	bool isChangedRegion(int level, int i, int j);
	void setChangedRegion(int level, int i, int j);

	void initializeGrids();
	void computeAssignMapsProgressively(const FilteredPointSet* origin);
	void convertToDensityMap(const FilteredPointSet* origin);
	void constructPyramids();
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