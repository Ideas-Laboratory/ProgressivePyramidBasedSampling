#include "utils.h"

using namespace std;

void setClassToLabelMapping(const unordered_map<string, uint>& label2class, unordered_map<uint, string>* class2label)
{
	for (auto &u : label2class)
		class2label->emplace(u.second, u.first);
}

PointSet * readDataset(string filename, unordered_map<uint, string>* class2label)
{
	ifstream input(filename);
	if(!input) //TODO opening error, should throw exception and show error message
		return nullptr;

	//skip invalid char
	while ((char)input.get() < 0);
	input.unget();

	PointSet* points = new PointSet();
	string value;
	double x = 0, y = 0;
	unordered_map<string, uint> label2class;
	while (getline(input, value, ',')) {
		x = atof(value.c_str());
		getline(input, value, ',');
		y = atof(value.c_str());

		getline(input, value);
		{ // deal with timestamp
			size_t pos = 0;
			if ((pos = value.find(',')) != std::string::npos)
				value = value.substr(pos + 1);
		}
		if (label2class.find(value) == label2class.end()) { // mapping label (string) to class (unsigned int)
			label2class[value] = label2class.size();
		}
		points->push_back(make_unique<LabeledPoint>(x, y, label2class[value]));
	}
	setClassToLabelMapping(label2class, class2label);

	return points;
}

void writeDataset(const string & filename, const PointSet& points, const unordered_map<uint, string>& class2label, const Indices& selected)
{
	ofstream output(filename, ios_base::trunc);
	for (auto &i : selected) {
		auto &p = points[i];
		output << p->pos.x() << ',' << p->pos.y() << ',' << class2label.at(p->label) << endl;
	}
}

shared_ptr<FilteredPointSet> filterByClass(const PointSet * points)
{
	auto copy = make_shared<FilteredPointSet>();
	for (uint i = 0, sz = points->size(); i < sz; ++i) {
		auto &p = points->at(i);
		if (std::find(selected_class_order.begin(), selected_class_order.end(), p->label) != selected_class_order.end()) {
			copy->insert(make_pair(i, make_unique<LabeledPoint>(p)));
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
	Extent e = { DBL_MAX,DBL_MAX,DBL_MIN,DBL_MIN };
	for (auto &p : *points) {
		e.x_min = min(e.x_min, p->pos.x());
		e.x_max = max(e.x_max, p->pos.x());
		e.y_min = min(e.y_min, p->pos.y());
		e.y_max = max(e.y_max, p->pos.y());
	}
	return e;
}