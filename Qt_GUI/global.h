#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <QPoint>
#include <QDate>
#include <QDebug>

//constants
#define MY_DATASET_FILENAME "./data/synthesis1.csv"

const static int CANVAS_WIDTH = 1600;//316;//1080; //720; //480; 
const static int CANVAS_HEIGHT = 900;//316;//810; //540; //360; 

//const static struct {
//	const int left = 0;
//	const int right = 0;
//	const int top = 0;
//	const int bottom = 0;
//} MARGIN;
const static struct {
	const int left = 20;
	const int right = 20;
	const int top = 20;
	const int bottom = 20;
} MARGIN;

//type definition
struct LabeledPoint
{
	QPointF pos;
	uint label;
	std::unique_ptr<QDate> date;
	LabeledPoint() {}
	LabeledPoint(double x, double y, uint l, std::unique_ptr<QDate> d) : pos(x, y), label(l), date(move(d)) {}
	LabeledPoint(const std::unique_ptr<LabeledPoint>& p) : pos(p->pos), label(p->label),
		date(p->date ? std::make_unique<QDate>(p->date->year(), p->date->month(), p->date->day()) : nullptr) {}
};
typedef std::vector<std::unique_ptr<LabeledPoint>> PointSet;

typedef std::unordered_map<uint, std::unique_ptr<LabeledPoint>> FilteredPointSet;

typedef std::vector<uint> Indices;

struct Extent {
	qreal x_min;
	qreal y_min;
	qreal x_max;
	qreal y_max;
};

struct Box {
	uint left;
	uint top;
	uint width;
	uint height;
	Box() {}
	Box(uint left, uint top, uint width, uint height) :left(left), top(top), width(width), height(height) {}
};

// total number and number of each class
struct StatisticalInfo {
	size_t total_num;
	std::unordered_map<uint, size_t> class_point_num;
	StatisticalInfo() { total_num = 0; }
	StatisticalInfo(const StatisticalInfo& info) : total_num(info.total_num), class_point_num(info.class_point_num) {}
	StatisticalInfo(StatisticalInfo&& info) : total_num(info.total_num), class_point_num(std::move(info.class_point_num)) {}
};

struct Param {
	uint chunk_size;
	uint displayed_frame_id;
	uint point_radius;
	uint grid_width;
	uint stop_level;
	double density_threshold;
	double outlier_weight;
	double ratio_threshold;
	bool is_streaming;
	uint time_step;
	uint time_window;
	bool use_alpha_channel;
};
