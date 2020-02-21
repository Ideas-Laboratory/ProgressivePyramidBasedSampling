#include "SamplingProcessViewer.h"

#include <QPrinter>
#include <QSvgGenerator>

//#define PALETTE {"#4c78a8", "#9ecae9", "#f58518", "#ffbf79", "#54a24b", "#88d27a", "#b79a20", "#f2cf5b", "#439894", "#83bcb6", "#e45756", "#ff9d98", "#79706e", "#bab0ac", "#d67195", "#fcbfd2", "#b279a2", "#d6a5c9", "#9e765f", "#d8b5a5"}
//#define PALETTE {"#215FAC", "#AD3944", "#F89F32", "#e15759", "#f6af5b", "#edc948", "#ee2f7f", "#4b62ad", "#9c755f", "#bab0ac", "#59a583"}
//#define PALETTE {"#9ecae9","#fb9a99","#00FF00","#000033","#FF00B6","#005300","#FFD300","#009FFF","#9A4D42","#00FFBE","#783FC1","#1F9698","#FFACFD","#B1CC71","#F1085C","#FE8F42","#DD00FF","#201A01","#720055","#766C95","#02AD24","#C8FF00","#886C00","#FFB79F","#858567","#A10300","#14F9FF","#00479E","#DC5E93","#93D4FF","#004CFF","#FFFFFF"}
//#define PALETTE {"#98df8a", "#42c66a", "#e15759", "#f28e2b", "#76b7b2", "#edc948", "#bca8c9", "#1f77b4", "#ff9da7", "#9c755f", "#ff7f0e", "#9f1990"} // teaser palette
//#define PALETTE {"#069ef2", "#42c66a", "#e15759", "#f28e2b", "#76b7b2", "#edc948", "#94d88b", "#f4a93d", "#ff9da7", "#9c755f", "#dd0505", "#9f1990"} // teaser new palette
//#define PALETTE {"#f9a435", "#1f7cea", "#59ba04" }
//#define PALETTE {"#2378cc", "#1cf1a2", "#3f862c", "#c05733", "#c5a674", "#f79add", "#f9e536", "#a1fa11", "#a24ddf", "#ea1240"}
#define PALETTE {"#4c78a8", "#9ecae9", "#f58518", "#59ba04", "#54a24b", "#88d27a", "#b79a20", "#f2cf5b", "#439894", "#83bcb6", "#e45756", "#ff9d98", "#79706e", "#bab0ac", "#d67195", "#fcbfd2", "#b279a2", "#d6a5c9", "#9e765f", "#d8b5a5"}

SamplingProcessViewer::SamplingProcessViewer(const PointSet* points, QWidget* parent) : QGraphicsView(parent)
{
	setInteractive(false);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setRenderHint(QPainter::Antialiasing, true);
	setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
	setOptimizationFlag(QGraphicsView::DontSavePainterState);
	setFrameStyle(0);
	setAttribute(Qt::WA_TranslucentBackground, false);
	setCacheMode(QGraphicsView::CacheBackground);

	setScene(new QGraphicsScene(this));
	this->scene()->setSceneRect(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT);
	this->scene()->setItemIndexMethod(QGraphicsScene::NoIndex);

	bound_pen = QPen(Qt::black, 2, Qt::DashLine);
	
	paletteToColors();
	setDataset(points);
}

void SamplingProcessViewer::mousePressEvent(QMouseEvent *me)
{
}

void SamplingProcessViewer::mouseMoveEvent(QMouseEvent *me)
{
}

void SamplingProcessViewer::mouseReleaseEvent(QMouseEvent *me)
{
}

void SamplingProcessViewer::setDataset(const PointSet * points)
{
	setAttribute(Qt::WA_TransparentForMouseEvents);
	
	seeds.clear();
	if (points_in_visual_space)
		points_in_visual_space = nullptr;

	real_extent = getExtent(points);
	points_in_visual_space = filterByClass(points);
	linearScale(points_in_visual_space, real_extent, MARGIN.left, CANVAS_WIDTH - MARGIN.right, MARGIN.top, CANVAS_HEIGHT - MARGIN.bottom);

	//drawPointByClass(points_in_visual_space);
	//drawPointRandomly(points_in_visual_space);
	//drawPointLowDensityUpper(points_in_visual_space);
	drawPointProgressively();

	emit inputImageChanged(points_in_visual_space->size());
}

void SamplingProcessViewer::sample()
{
	emit sampleStart();
	if (grid_width_changed) {
		hws = HaarWaveletSampling{ QRect(MARGIN.left, MARGIN.top, CANVAS_WIDTH - MARGIN.left - MARGIN.right, CANVAS_HEIGHT - MARGIN.top - MARGIN.bottom) };
		grid_width_changed = false;
	}
	seeds = hws.execute(points_in_visual_space);
	//drawPointByClass(indicesToPointSet(points_in_visual_space, seeds));
	drawPointRandomly(points_in_visual_space, seeds);
	//drawPointLowDensityUpper(indicesToPointSet(points_in_visual_space, seeds));
	emit finished();
}

void SamplingProcessViewer::showSpecificFrame()
{
	emit adjustmentStart();
	seeds = hws.selectSeeds();
	drawPointRandomly(points_in_visual_space, seeds);
	//auto &pr = hws.getSeedsWithDiff();
	//drawPointsByPair(points_in_visual_space, pr);
	//pr.first.insert(pr.first.end(), pr.second.begin(), pr.second.end());
	//seeds = pr.first;
	emit finished();
}

void SamplingProcessViewer::saveImagePNG(const QString & path)
{
	QPixmap pixmap(this->scene()->sceneRect().size().toSize());
	pixmap.fill(Qt::white);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing, true);
	this->scene()->render(&painter);
	pixmap.save(path);
}

void SamplingProcessViewer::saveImageSVG(const QString & path)
{
	QSvgGenerator generator;
	generator.setFileName(path);
	generator.setSize(QSize(this->scene()->width(), this->scene()->height()));
	generator.setViewBox(this->scene()->sceneRect());
	generator.setTitle("Resampling Result");
	generator.setDescription(("SVG File created by "+ALGORITHM_NAME).c_str());
	QPainter painter;
	painter.begin(&generator);
	this->scene()->render(&painter);
	painter.end();
}

void SamplingProcessViewer::saveImagePDF(const QString & path)
{
	adjustSize();

	QPrinter printer(QPrinter::HighResolution);
	printer.setOutputFormat(QPrinter::PdfFormat);
	printer.setCreator(ALGORITHM_NAME.c_str());
	printer.setOutputFileName(path);
	printer.setPaperSize(this->size(), QPrinter::Point);
	printer.setFullPage(true);

	QPainter painter(&printer);
	this->render(&painter);
}

void SamplingProcessViewer::drawPointProgressively()
{
	this->scene()->clear();

	int count = 0, target = (params.displayed_frame_id + 1) * params.batch;
	for (auto &pr : *points_in_visual_space) {
		auto &p = pr.second;
		drawPoint(p->pos.x(), p->pos.y(), params.point_radius, color_brushes[p->label]);
		if (++count == target) break;
	}
}

void SamplingProcessViewer::drawPointByClass(const std::shared_ptr<FilteredPointSet>& points, const Indices& selected)
{
	this->scene()->clear();

	std::vector<Indices> _class(CLASS_NUM);
	if (selected.empty()) {
		for (auto &pr : *points)
			_class[pr.second->label].push_back(pr.first);
	}
	else {
		for (auto &i : selected) {
			auto &p = points->at(i);
			_class[p->label].push_back(i);
		}
	}
	for (size_t i = 0; i < selected_class_order.size(); i++) {
		for (auto &i : _class[selected_class_order[i]]) {
			auto &p = points->at(i);
			drawPoint(p->pos.x(), p->pos.y(), params.point_radius, color_brushes[p->label]);
		}
	}
}

void SamplingProcessViewer::drawPointRandomly(const std::shared_ptr<FilteredPointSet> &points, const Indices& selected)
{
	this->scene()->clear();

	using namespace std;
	Indices shuffled_points = selected;
	if (shuffled_points.empty()) {
		for (auto &pr : *points) {
			shuffled_points.push_back(pr.first);
		}
	}
	random_shuffle(shuffled_points.begin(), shuffled_points.end());
	for (auto &i : shuffled_points) {
		auto &p = points->at(i);
		drawPoint(p->pos.x(), p->pos.y(), params.point_radius, color_brushes[p->label]);
	}
}

void SamplingProcessViewer::drawPointsByPair(const std::shared_ptr<FilteredPointSet>& points, const std::pair<Indices, Indices>& selected)
{
	this->scene()->clear();
	
	for (auto &i : selected.first) {
		auto &p = points->at(i);
		drawPoint(p->pos.x(), p->pos.y(), params.point_radius, color_brushes[p->label]);
	}
	color_index = 2;
	for (auto &i : selected.second) {
		auto &p = points->at(i);
		drawPoint(p->pos.x(), p->pos.y(), params.point_radius, color_brushes[p->label]);
	}
	color_index = 0;
}

void SamplingProcessViewer::drawPointLowDensityUpper(const std::shared_ptr<FilteredPointSet> &points, const Indices& selected)
{
	this->scene()->clear();

	using namespace std;
	int area_size = 100;
	// assign points to area
	unordered_map<uint, unordered_map<uint, Indices>> small_areas;
	if (selected.empty()) {
		for (auto &pr : *points) {
			auto &p = pr.second;
			int x = (int)p->pos.x() / area_size, y = (int)p->pos.y() / area_size, pos = x*CANVAS_HEIGHT + y;
			small_areas[pos][p->label].push_back(pr.first);
		}
	}
	else {
		for (auto &i : selected) {
			auto &p = points->at(i);
			int x = (int)p->pos.x() / area_size, y = (int)p->pos.y() / area_size, pos = x*CANVAS_HEIGHT + y;
			small_areas[pos][p->label].push_back(i);
		}
	}

	for (auto &u : small_areas) {
		priority_queue<pair<size_t, uint>> q;
		for (auto &u2 : u.second) {
			q.push(make_pair(u2.second.size(), u2.first));
		}
		while (!q.empty()) {
			uint label = q.top().second;
			q.pop();
			for (auto &i : u.second[label]) {
				auto &p = points->at(i);
				drawPoint(p->pos.x(), p->pos.y(), params.point_radius, color_brushes[p->label]);
			}
		}
	}
}

//void SamplingProcessViewer::drawBorders()
//{
//	for (auto &n : nodes) {
//		auto &box = n.lock()->getBoundBox();
//		if (box.left != 0) // left bound
//			drawLine(grid2visual(box.left, MARGIN.left), grid2visual(box.top, MARGIN.top), grid2visual(box.left, MARGIN.left), grid2visual(box.bottom, MARGIN.top), false);
//		if (box.top != 0) // top bound
//			drawLine(grid2visual(box.left, MARGIN.left), grid2visual(box.top, MARGIN.top), grid2visual(box.right, MARGIN.left), grid2visual(box.top, MARGIN.top), false);
//	}
//}

void SamplingProcessViewer::redrawPoints()
{
	emit redrawStart();
	if (seeds.empty())
		drawPointProgressively();
	else
		showSpecificFrame();
	//if (params.show_border) drawBorders();
}

void SamplingProcessViewer::paletteToColors()
{
	std::vector<const char*> palette(PALETTE);
	for (const char* hex : palette) {
		color_brushes.push_back(QColor(hex));
	}
}

void SamplingProcessViewer::drawPoint(qreal x, qreal y, qreal radius, QBrush b)
{
	//x += params.grid_width*(2.0*rand()/RAND_MAX - 1.0);
	//y += params.grid_width*(2.0*rand()/RAND_MAX - 1.0);
	if (params.use_alpha_channel) { // draw gaussian shape
		//QColor c2 = b.color();
		//c2.setAlpha(60);
		//this->scene()->addEllipse(x - 1.0, y - 1.0, 2.0, 2.0, QPen(c2, 1), b);
		//c2.setAlpha(30);
		//QPen p(c2, 3);
		//this->scene()->addEllipse(x - 2.0, y - 2.0, 4.0, 4.0, p, Qt::NoBrush);
		QColor c = color_brushes[color_index].color();//b.color();
		c.setAlpha(15);
		this->scene()->addEllipse(x - radius, y - radius, 2 * radius, 2 * radius, Qt::NoPen, c);
	}
	else {
		//this->scene()->addEllipse(x, y, 1.0, 1.0, Qt::NoPen, b);
		this->scene()->addEllipse(x - radius, y - radius, 2 * radius, 2 * radius, Qt::NoPen, color_brushes[color_index]);
	}
}