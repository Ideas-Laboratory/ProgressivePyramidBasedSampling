#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>

#include "global.h"

extern Param params;
extern std::vector<int> selected_class_order;

void openDataSource(std::ifstream& input, std::string filename);
PointSet* readDataSource(std::ifstream& input, std::unordered_map<uint, std::string>* class2label, int chuck_size);
FilteredPointSet* filter(PointSet* points, const Extent& ext, uint pos);

inline double linearScale(double val, double oldLower, double oldUpper, double lower, double upper)
{
	return (val - oldLower)*(upper - lower) / (oldUpper - oldLower) + lower;
}
// will modify points
void linearScale(FilteredPointSet* points, const Extent& real_extent, double lower, double upper);
void linearScale(FilteredPointSet* points, const Extent& real_extent, int left, int right, int top, int bottom);
void linearScale(FilteredPointSet* points, const Extent& real_extent, const Extent& target_extent);
void linearScale(FilteredPointSet* points, const Extent& real_extent, std::function<double(double, double, double)> horizontalScale, std::function<double(double, double, double)> verticalScale);

inline double squaredEuclideanDistance(const LabeledPoint * a, const LabeledPoint * b)
{
	double dx = a->pos.x() - b->pos.x(), dy = a->pos.y() - b->pos.y();
	return dx*dx + dy*dy;
}

Extent getExtent(const PointSet* data);

inline int visual2grid(qreal pos, qreal margin)
{
	return static_cast<int>(pos - margin) / params.grid_width;
}

inline qreal grid2visual(qreal pos, qreal margin)
{
	return pos*params.grid_width + margin;
}