#include "SamplingProcessViewer.h"

#include <QDir>
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

SamplingProcessViewer::SamplingProcessViewer(std::string&& data_name, std::unordered_map<uint, std::string>* class2label, QWidget* parent)
	: QGraphicsView(parent), data_name(data_name), class2label(class2label)
{
	setInteractive(false);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setRenderHint(QPainter::Antialiasing, true);
	setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
	setOptimizationFlag(QGraphicsView::DontSavePainterState);
	setFrameStyle(0);
	setAttribute(Qt::WA_TranslucentBackground, false);
	setCacheMode(QGraphicsView::CacheBackground);
	setAttribute(Qt::WA_TransparentForMouseEvents);

	setScene(new QGraphicsScene(this));
	this->scene()->setSceneRect(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT);
	this->scene()->setItemIndexMethod(QGraphicsScene::BspTreeIndex);
	virtual_scene = new QGraphicsScene(this);
	virtual_scene->setSceneRect(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT);
	virtual_scene->setItemIndexMethod(QGraphicsScene::NoIndex);

	bound_pen = QPen(Qt::black, 2, Qt::DashLine);
	openDataSource(data_source, MY_DATASET_FILENAME);

	paletteToColors();
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

void SamplingProcessViewer::setDataSource(std::string&& dp)
{
	data_source.close();
	point_count = 0;

	openDataSource(data_source, dp);

	emit inputImageChanged();
}

void SamplingProcessViewer::sample()
{
	emit sampleStart();
	if (grid_width_changed) {
		hws = HaarWaveletSampling{ QRect(MARGIN.left, MARGIN.top, CANVAS_WIDTH - MARGIN.left - MARGIN.right, CANVAS_HEIGHT - MARGIN.top - MARGIN.bottom) };
		grid_width_changed = false;
	}
	while (!data_source.eof()) {
		PointSet *new_data = readDataSource(data_source, class2label, params.batch);
		if (point_count == 0) {
			real_extent = getExtent(new_data); // use the extent of the first batch for the whole data
			//real_extent = { -125.22,23.85,-66.05,49.00 }; // U.S. census
			//real_extent = { -74.238,40.51,-73.728,40.906 }; // taxis.csv
		}

		auto filtered_new_data = filter(new_data, real_extent, point_count);
		//linearScale(filtered_new_data, real_extent, visual_extent);
		linearScale(filtered_new_data, real_extent, MARGIN.left, CANVAS_WIDTH - MARGIN.right, MARGIN.top, CANVAS_HEIGHT - MARGIN.bottom);
		auto &result = hws.execute(filtered_new_data, point_count == 0);
		point_count += filtered_new_data->size();
		
		//params.use_alpha_channel = true;
		//drawPointProgressively(filtered_new_data); // draw points in virtual scene for output png & svg
		//params.use_alpha_channel = false;
		//PointSet selected_in_visual_space = hws.selectSeeds(); // select seeds for output csv
		//linearScale(seeds, visual_extent, real_extent);

		delete new_data;

		drawPointProgressively(result);

		std::stringstream fn_stream;
		fn_stream << "./results/" << data_name << "/";
		QDir().mkdir(fn_stream.str().c_str());
		//saveImagePNG(QString(fn_stream.str().c_str()) + "origin_" + QString::number(hws.getFrameID()) + "_transparent.png", true);

		fn_stream << params.grid_width << '_' << params.end_level << '_';
		fn_stream.precision(4);
		fn_stream << params.low_density_weight << '_' << params.epsilon << '_' << hws.getFrameID();
		auto filepath = fn_stream.str();
		saveImagePNG(QString(filepath.c_str()) + ".png");
		//writeResult(QString(filepath.c_str()) + ".csv");
	}
	emit finished();
}

void SamplingProcessViewer::showSpecificFrame()
{
	emit adjustmentStart();
	auto &pr = hws.getSeedsWithDiff();
	drawPointsByPair(pr);
	emit finished();
}

void SamplingProcessViewer::saveImagePNG(const QString & path, bool is_virtual)
{
	auto s = is_virtual ? virtual_scene : this->scene();
	QPixmap pixmap(s->sceneRect().size().toSize());
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing, true);
	s->render(&painter);
	pixmap.save(path);
}

void SamplingProcessViewer::saveImageSVG(const QString & path, bool is_virtual)
{
	auto s = is_virtual ? virtual_scene : this->scene();
	QSvgGenerator generator;
	generator.setFileName(path);
	generator.setSize(QSize(s->width(), s->height()));
	generator.setViewBox(s->sceneRect());
	generator.setTitle("Resampling Result");
	generator.setDescription(("SVG File created by "+ALGORITHM_NAME).c_str());
	QPainter painter;
	painter.begin(&generator);
	s->render(&painter);
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

void SamplingProcessViewer::writeResult(const QString & path)
{
	using namespace std;
	ofstream output(path.toStdString(), ios_base::trunc);
	for (auto &p : seeds) {
		output << p->pos.x() << ',' << p->pos.y() << ',' << class2label->at(p->label) << endl;
	}
}

void SamplingProcessViewer::drawPointProgressively(const std::shared_ptr<FilteredPointSet>& points)
{
	for (auto &pr : *points) {
		auto &p = pr.second;
		drawPoint(p->pos.x(), p->pos.y(), params.point_radius, color_brushes[p->label], true);
	}
}

void SamplingProcessViewer::drawPointProgressively(std::pair<TempPointSet, TempPointSet>& removed_n_added)
{
	for (auto &p : removed_n_added.first) {
		auto item = this->scene()->itemAt(p->pos, this->transform());
		if(item) this->scene()->removeItem(item);
	}
	for (auto &p : removed_n_added.second) {
		drawPoint(p->pos.x(), p->pos.y(), params.point_radius, color_brushes[p->label]);
	}
}

void SamplingProcessViewer::drawPointByClass(TempPointSet& selected)
{
	this->scene()->clear();

	std::vector<TempPointSet> _class(CLASS_NUM);

	for (auto &p : selected) {
		_class[p->label].push_back(p);
	}
	for (size_t i = 0; i < selected_class_order.size(); i++) {
		for (auto &p : _class[selected_class_order[i]]) {
			drawPoint(p->pos.x(), p->pos.y(), params.point_radius, color_brushes[p->label]);
		}
	}
}

void SamplingProcessViewer::drawPointRandomly(TempPointSet& selected)
{
	this->scene()->clear();

	using namespace std;

	random_shuffle(selected.begin(), selected.end());
	for (auto &p : selected) {
		drawPoint(p->pos.x(), p->pos.y(), params.point_radius, color_brushes[p->label]);
	}
}

void SamplingProcessViewer::drawPointsByPair(std::pair<TempPointSet, TempPointSet>& selected)
{
	this->scene()->clear();
	
	for (auto &p : selected.first) {
		drawPoint(p->pos.x(), p->pos.y(), params.point_radius, color_brushes[p->label]);
	}
	color_index = 2;
	for (auto &p : selected.second) {
		drawPoint(p->pos.x(), p->pos.y(), params.point_radius, color_brushes[p->label]);
	}
	color_index = 0;
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
	if (!data_source.eof()) {
		showSpecificFrame();
	}
	//if (params.show_border) drawBorders();
}

void SamplingProcessViewer::paletteToColors()
{
	std::vector<const char*> palette(PALETTE);
	for (const char* hex : palette) {
		color_brushes.push_back(QColor(hex));
	}
}

void SamplingProcessViewer::drawPoint(qreal x, qreal y, qreal radius, QBrush b, bool is_virtual)
{
	//x += params.grid_width*(2.0*rand()/RAND_MAX - 1.0);
	//y += params.grid_width*(2.0*rand()/RAND_MAX - 1.0);
	auto s = is_virtual ? virtual_scene : this->scene();
	if (params.use_alpha_channel) { // draw gaussian shape
		//QColor c2 = b.color();
		//c2.setAlpha(60);
		//this->scene()->addEllipse(x - 1.0, y - 1.0, 2.0, 2.0, QPen(c2, 1), b);
		//c2.setAlpha(30);
		//QPen p(c2, 3);
		//this->scene()->addEllipse(x - 2.0, y - 2.0, 4.0, 4.0, p, Qt::NoBrush);
		QColor c = color_brushes[color_index].color();//b.color();
		c.setAlpha(10);
		s->addEllipse(x - radius, y - radius, 2 * radius, 2 * radius, Qt::NoPen, c);
	}
	else {
		//this->scene()->addEllipse(x, y, 1.0, 1.0, Qt::NoPen, b);
		s->addEllipse(x - radius, y - radius, 2 * radius, 2 * radius, Qt::NoPen, color_brushes[color_index]);
	}
}
