#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <QPoint>
#include <QDebug>

//constants
#define MY_DATASET_FILENAME "./data/synthesis1.csv"

const static int CANVAS_WIDTH = 1600;//316;//1080; //720; //480; 
const static int CANVAS_HEIGHT = 900;//316;//810; //540; //360; 

const static struct {
	const int left = 20;
	const int right = 40;
	const int top = 20;
	const int bottom = 40;
} MARGIN;

//type definition
struct LabeledPoint
{
	QPointF pos;
	uint label;
	LabeledPoint() {}
	LabeledPoint(double x, double y, uint l) : pos(x, y), label(l) {}
	LabeledPoint(const std::unique_ptr<LabeledPoint>& p) : pos(p->pos), label(p->label) {}
};
typedef std::vector<std::unique_ptr<LabeledPoint>> PointSet;
typedef std::vector<LabeledPoint*> TempPointSet;

typedef std::unordered_map<uint, std::unique_ptr<LabeledPoint>> FilteredPointSet;

struct Extent {
	qreal x_min;
	qreal y_min;
	qreal x_max;
	qreal y_max;
};

struct Box {
	int left;
	int top;
	int right;
	int bottom;
	Box() {}
	Box(int left, int top, int right, int bottom) :left(left), top(top), right(right), bottom(bottom) {}
};

// total number and number of each class
struct StatisticalInfo {
	size_t total_num;
	std::unordered_map<uint, size_t> class_point_num;
	StatisticalInfo() { total_num = 0; }
	StatisticalInfo(const StatisticalInfo& info) : total_num(info.total_num), class_point_num(info.class_point_num){}
	StatisticalInfo(StatisticalInfo&& info) : total_num(info.total_num), class_point_num(std::move(info.class_point_num)) {}
};

struct Param {
	uint batch;
	uint displayed_frame_id;
	double epsilon;
	uint point_radius;
	uint grid_width;
	uint end_level;
	double low_density_weight;
	//bool show_border;
	bool use_alpha_channel;
};