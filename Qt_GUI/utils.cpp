#include "utils.h"

using namespace std;

void openDataSource(ifstream& input, string filename)
{
	input.open(filename);
	if (!input) //TODO opening error, should throw exception and show error message
		throw exception();
	
	//skip invalid char
	while ((char)input.get() < 0);
	input.unget();
}

PointSet * readDataSource(ifstream& input, unordered_map<uint, string>* class2label, int chuck_size)
{
	unordered_map<string, uint> label2class;
	for (auto &u : *class2label)
		label2class.emplace(u.second, u.first);

	PointSet* points = new PointSet();
	string value;
	double x = 0, y = 0;
	int count = 0;
	while (count < chuck_size && getline(input, value, ',')) {
		x = atof(value.c_str());
		getline(input, value, ',');
		y = atof(value.c_str());

		getline(input, value);

		if (label2class.find(value) == label2class.end()) { // mapping label (string) to class (unsigned int)
			label2class[value] = label2class.size();
			class2label->emplace(label2class[value], value);
		}
		points->push_back(make_unique<LabeledPoint>(x, y, label2class[value]));
		++count;
	}

	return points;
}

shared_ptr<FilteredPointSet> filter(const PointSet * points, const Extent& ext, uint pos)
{
	auto copy = make_shared<FilteredPointSet>();
	for (uint i = 0, sz = points->size(); i < sz; ++i) {
		auto &p = points->at(i);
		if (p->pos.x() > ext.x_min && p->pos.x() < ext.x_max && p->pos.y() > ext.y_min && p->pos.y() < ext.y_max &&
			std::find(selected_class_order.begin(), selected_class_order.end(), p->label) != selected_class_order.end()) {
			copy->insert(make_pair(pos + i, make_unique<LabeledPoint>(p)));
		}
	}
	return copy;
}

void linearScale(shared_ptr<FilteredPointSet> points, const Extent& real_extent, double lower, double upper)
{
	auto scale = [=](double val, double oldLower, double oldUpper) { return linearScale(val, oldLower, oldUpper, lower, upper); };

	linearScale(points, real_extent, scale, scale);
}

void linearScale(shared_ptr<FilteredPointSet> points, const Extent& real_extent, int left, int right, int top, int bottom)
{
	auto horizontalScale = [=](double val, double oldLower, double oldUpper) { return linearScale(val, oldLower, oldUpper, left, right); };
	auto verticalScale = [=](double val, double oldLower, double oldUpper) { return linearScale(val, oldLower, oldUpper, bottom, top); };

	linearScale(points, real_extent, horizontalScale, verticalScale);
}

void linearScale(shared_ptr<FilteredPointSet> points, const Extent & real_extent, const Extent & target_extent)
{
	auto horizontalScale = [=](double val, double oldLower, double oldUpper) { return linearScale(val, oldLower, oldUpper, target_extent.x_min, target_extent.x_max); };
	auto verticalScale = [=](double val, double oldLower, double oldUpper) { return linearScale(val, oldLower, oldUpper, target_extent.y_max, target_extent.y_min); };

	linearScale(points, real_extent, horizontalScale, verticalScale);
}

void linearScale(shared_ptr<FilteredPointSet> points, const Extent& real_extent, std::function<double(double, double, double)> horizontalScale, std::function<double(double, double, double)> verticalScale)
{
	auto xScale = [=](double val) { return horizontalScale(val, real_extent.x_min, real_extent.x_max); };
	auto yScale = [=](double val) { return verticalScale(val, real_extent.y_min, real_extent.y_max); };

	for (auto &p : *points) {
		p.second->pos.setX(xScale(p.second->pos.x()));
		p.second->pos.setY(yScale(p.second->pos.y()));
	}
}

Extent getExtent(const PointSet * points)
{
	Extent e = { DBL_MAX,DBL_MAX,-DBL_MAX,-DBL_MAX };
	for (auto &p : *points) {
		e.x_min = min(e.x_min, p->pos.x());
		e.x_max = max(e.x_max, p->pos.x());
		e.y_min = min(e.y_min, p->pos.y());
		e.y_max = max(e.y_max, p->pos.y());
	}
	return e;
}