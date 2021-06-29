#include "SamplingProcessViewer.h"

#include <QDir>
#include <QPrinter>
#include <QSvgGenerator>
#include <QAbstractGraphicsShapeItem>

#define PALETTE {"#4478BD", "#45b4c1", "#f58518", "#59ba04", "#54a24b", "#88d27a", "#b79a20", "#f2cf5b", "#439894", "#83bcb6", "#e45756", "#ff9d98", "#79706e", "#bab0ac", "#d67195", "#fcbfd2", "#b279a2", "#d6a5c9", "#9e765f", "#d8b5a5"}
//#define PALETTE {"#1f77b4", "#c5b0d5", "#ff9896", "#d62728", "#aec7e8", "#2ca02c", "#98df8a", "#ffbb78", "#9467bd", "#ff7f0e", "#8c564b", "#c49c94", "#e377c2", "#f7b6d2", "#7f7f7f", "#c7c7c7", "#bcbd22", "#dbdb8d", "#17becf", "#9edae5"} // Tableau 20
//#define DRAW_ORIGIN // whether or not draw the origin scatterplot

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

	sw.setClassMapping(class2label);
	bound_pen = QPen(Qt::black, 2, Qt::DashLine);

	paletteToColors();

	connect(this, &SamplingProcessViewer::sampleStart, &sw, &SamplingWorker::readAndSample);
#ifdef DRAW_ORIGIN
	connect(&sw, &SamplingWorker::readFinished, this, &SamplingProcessViewer::drawPointsProgressively);
#else
	connect(&sw, &SamplingWorker::readFinished, this, [](auto ptr) { delete ptr; });
#endif // DRAW_ORIGIN
	//connect(&sw, &SamplingWorker::readFinished, this, &SamplingProcessViewer::updateClassInfo);
	connect(&sw, &SamplingWorker::sampleFinished, this, &SamplingProcessViewer::drawSelectedPointsProgressively);
	connect(&sw, &SamplingWorker::writeFrame, [this](int frame_id) { emit frameChanged(frame_id); });
	//color_index = 1;
	sw.moveToThread(&workerThread);
	workerThread.start();
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

void SamplingProcessViewer::setDataPath(std::string&& dp)
{
	workerThread.quit();
	workerThread.wait();

	data_path = dp;

	reinitializeScreen();

	workerThread.start();
	emit inputImageChanged();
}

void SamplingProcessViewer::sample()
{
	if (grid_width_changed) {
		sw.updateGrids();
		grid_width_changed = false;
	}
	sw.setDataSource(data_path);
	reinitializeScreen();
	emit sampleStart();

	emit finished();
}

void SamplingProcessViewer::showSpecificFrame()
{
	auto& seeds = sw.getSeedsOfSpecificFrame();
	drawPointRandomly(seeds);
	emit finished();
}

void SamplingProcessViewer::saveImagePNG(const QString & path, bool is_virtual)
{
	auto s = is_virtual ? virtual_scene : this->scene();
	QPixmap pixmap(s->sceneRect().size().toSize());
	pixmap.fill(Qt::white);
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
	for (auto &idx : sw.getSelected()) {
		output << idx << endl;
	}
}

void SamplingProcessViewer::drawPointsProgressively(FilteredPointSet *points)
{
	//params.use_alpha_channel = true;
	QDate *last_date = nullptr;
	for (auto &pr : *points) {
		auto &p = pr.second;
		auto it = drawPoint(p->pos.x(), p->pos.y(), params.point_radius, color_brushes[0], true);
		if (params.is_streaming) {
			date2item[p->date->toJulianDay()].push_back(it);
			if (last_date == nullptr || *last_date < *p->date)
				last_date = p->date.get(); // find the last date
		}
	}
	if (params.is_streaming) {
		for (auto it = date2item.begin(); it != date2item.end();) {
			if (QDate::fromJulianDay(it->first).daysTo(*last_date) > params.time_window) {
				for (auto ptr : it->second)
					if (ptr) { virtual_scene->removeItem(ptr); }
				it = date2item.erase(it);
			}
			else
				++it;
		}
	}
	//saveImagePNG("./results/StockMarket/"+last_date->toString("yyyy-MM-dd")+".png", true);
	//params.use_alpha_channel = false;
	delete points;
}

void SamplingProcessViewer::drawSelectedPointsProgressively(std::pair<PointSet, PointSet>* removed_n_added)
{
	auto begin = std::chrono::high_resolution_clock::now();
	for (auto &p : removed_n_added->first) {
		double key = p->pos.x() * CANVAS_HEIGHT + p->pos.y();
		auto item = pos2item[key];
		if (item) { this->scene()->removeItem(item); }
		if (pos2item.find(key) != pos2item.end()) pos2item.erase(key);
	}
	QDate *last_date = nullptr;
	for (auto &p : removed_n_added->second) {
		auto it = drawPoint(p->pos.x(), p->pos.y(), params.point_radius, color_brushes[p->label]);
		pos2item.emplace(p->pos.x() * CANVAS_HEIGHT + p->pos.y(), it);
		if (params.is_streaming && (last_date == nullptr || *last_date < *p->date))
				last_date = p->date.get(); // find the last date
	}
	auto end = std::chrono::high_resolution_clock::now();
	qDebug() << "render: " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() / 1e9;
	const static QDate print_start(2001, 8, 10), print_end(2001, 10, 10);
	if (last_date) {
		qDebug() << *last_date;
		if (*last_date > print_start && *last_date < print_end)
			saveImagePNG("./results/StockMarket/PBS_" + last_date->toString("yyyy-MM-dd") + "_" + QString::number(params.ratio_threshold) + ".png");
	}
	delete removed_n_added;
}

void SamplingProcessViewer::generateFiles(int frame_id)
{
	std::stringstream fn_stream;
	fn_stream << "./results/" << data_name << "/";
	QDir().mkdir(fn_stream.str().c_str());
	//saveImagePNG(QString(fn_stream.str().c_str()) + "origin_" + QString::number(frame_id) + ".png", true); //_transparent

	fn_stream.precision(4);
	// file name of the result for the hierarchical sampling
	fn_stream << params.point_radius << '_' << params.grid_width << '_' << params.density_threshold << '_';
	fn_stream << params.outlier_weight << '_' << params.ratio_threshold << '_' << params.stop_level << '_' << frame_id;
	// file name of the result for the kd-tree based sampling
	//fn_stream << "kd_tree_" << params.point_radius << '_' << params.grid_width << '_' << tree_sampling_params.threshold << '_'
	//	<< tree_sampling_params.occupied_space_ratio << '_' << tree_sampling_params.backtracking_depth << '_' << frame_id;
	// file name of the result for the reservoir sampling
	//fn_stream << "reservoir_" << ReservoirSampling::seeds_num << '_' << frame_id;
	// file name of the result for the random sampling
	//fn_stream << "random_" << RandomSampling::seeds_num << '_' << frame_id;
	auto filepath = fn_stream.str();
	saveImagePNG(QString(filepath.c_str()) + ".png");
	//writeResult(QString(filepath.c_str()) + ".csv")
}

void SamplingProcessViewer::updateClassInfo()
{
	if (class2label->size() != last_class_num) {
		emit classChanged(class2label);
		last_class_num = class2label->size();
	}
}

void SamplingProcessViewer::reinitializeScreen() {
	pos2item.clear();
	class2label->clear();
	updateClassInfo();

	this->scene()->clear();
	virtual_scene->clear();
}

void SamplingProcessViewer::drawPointRandomly(PointSet& selected)
{
	this->scene()->clear();

	using namespace std;

	random_shuffle(selected.begin(), selected.end());
	for (auto &p : selected) {
		drawPoint(p->pos.x(), p->pos.y(), params.point_radius, color_brushes[p->label]);
	}
}

void SamplingProcessViewer::drawPointsByPair(std::pair<PointSet, PointSet>& selected)
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

void SamplingProcessViewer::redrawPoints()
{
	emit redrawStart();
	showSpecificFrame();
}

void SamplingProcessViewer::paletteToColors()
{
	std::vector<const char*> palette(PALETTE);
	for (const char* hex : palette) {
		color_brushes.push_back(QColor(hex));
	}
}

QGraphicsItem* SamplingProcessViewer::drawPoint(qreal x, qreal y, qreal radius, QBrush b, bool is_virtual)
{
	auto s = is_virtual ? virtual_scene : this->scene();
	if (params.use_alpha_channel) {
		QColor c = color_brushes[color_index].color();//b.color();
		c.setAlpha(5);
		return s->addEllipse(x - radius, y - radius, 2 * radius, 2 * radius, Qt::NoPen, c);
	}
	else {
		return s->addEllipse(x - radius, y - radius, 2 * radius, 2 * radius, Qt::NoPen, color_brushes[color_index]);//b.color();
	}
}
