#include "SamplingProcessViewer.h"

#include <QDir>
#include <QPrinter>
#include <QSvgGenerator>
#include <QAbstractGraphicsShapeItem>

//#define PALETTE {"#4478BD", "#9ecae9", "#f58518", "#59ba04", "#54a24b", "#88d27a", "#b79a20", "#f2cf5b", "#439894", "#83bcb6", "#e45756", "#ff9d98", "#79706e", "#bab0ac", "#d67195", "#fcbfd2", "#b279a2", "#d6a5c9", "#9e765f", "#d8b5a5"}
#define PALETTE {"#d98393", "#a84a90", "#87b72a", "#d62728", "#af7e65", "#b2d254", "#64acac", "#1f77b4", "#ad5a33", "#54e854", "#9bd5df", "#efe926", "#e377c2", "#f7b6d2", "#7f7f7f", "#c7c7c7", "#bcbd22", "#dbdb8d", "#17becf", "#9edae5"} // paperscape palette

//#define DRAW_ORIGIN

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
	connect(&sw, &SamplingWorker::readFinished, this, &SamplingProcessViewer::updateClassInfo);
	connect(&sw, &SamplingWorker::sampleFinished, this, &SamplingProcessViewer::drawSelectedPointsProgressively);
	connect(&sw, &SamplingWorker::writeFrame, [this](int frame_id) { emit frameChanged(frame_id); });
	//connect(&sw, &SamplingWorker::writeFrame, this, &SamplingProcessViewer::generateFiles);
	connect(&sw, &SamplingWorker::finished, [this]() {
		sw.setDataSource(data_path);
		emit finished();
	});

#ifdef DRAW_ORIGIN
	connect(&sw, &SamplingWorker::readFinished, this, &SamplingProcessViewer::drawPointsProgressively);
#else
	connect(&sw, &SamplingWorker::readFinished, this, [](auto ptr) { delete ptr; });
#endif

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

void SamplingProcessViewer::setDataSource(std::string&& dp)
{
	workerThread.quit();
	workerThread.wait();

	data_path = std::move(dp);
	sw.setDataSource(data_path);

	class2label->clear();
	updateClassInfo();

	this->scene()->clear();
	virtual_scene->clear();
	last_time = 0l;

	pos2item.clear();
	_removed_and_added.first.clear();
	_removed_and_added.second.clear();

	workerThread.start();
	emit inputImageChanged();
}

void SamplingProcessViewer::sample()
{
	if (grid_width_changed) {
		sw.updateGrids();
		grid_width_changed = false;
	}
	emit sampleStart();
	last_time = clock();
}

void SamplingProcessViewer::showSpecificFrame()
{
	auto &seeds = sw.getSeedPoints();
	drawPointRandomly(seeds);
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
	for (auto &pr : *points) {
		auto &p = pr.second;
		drawPoint(p->pos.x(), p->pos.y(), params.point_radius, color_brushes[p->label], true);
	}
	//params.use_alpha_channel = false;
	delete points;
}

void SamplingProcessViewer::drawSelectedPointsProgressively(std::pair<TempPointSet, TempPointSet>* removed_n_added)
{
	auto begin = std::chrono::high_resolution_clock::now();
	for (auto &p : removed_n_added->first) {
		double key = p->pos.x() * CANVAS_HEIGHT + p->pos.y();
		auto item = this->scene()->itemAt(p->pos, QGraphicsView::transform());
		if (item) { this->scene()->removeItem(item); }
	}
	for (auto &p : removed_n_added->second) {
		drawPoint(p->pos.x(), p->pos.y(), params.point_radius, color_brushes[p->label]);
	}
	auto end = std::chrono::high_resolution_clock::now();
	qDebug() << "render: " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() / 1e9;
	if (last_time > 0) {
		qDebug() << "duration: " << (clock() - last_time)/(double)CLOCKS_PER_SEC;
	}
	last_time = clock();
	delete removed_n_added;
}

void SamplingProcessViewer::drawSelectedPointDiffsProgressively(std::pair<TempPointSet, TempPointSet>* removed_n_added)
{
	static int r = params.point_radius;
	if (r != params.point_radius) r = params.point_radius;

	if (pos2item.empty()) {
		for (auto &p : removed_n_added->second) {
			int key = (int)p->pos.x() * CANVAS_HEIGHT + (int)p->pos.y();
			auto it = this->scene()->addEllipse(p->pos.x() - r, p->pos.y() - r, 2 * r, 2 * r, Qt::NoPen, color_brushes[0]); // blue
			pos2item.emplace(key, it);
		}
	}
	else {
		for (auto it = _removed_and_added.first.begin(); it != _removed_and_added.first.end(); ) {
			if (*it) {
				int pos = ((int)(*it)->boundingRect().x() + r) * CANVAS_HEIGHT + ((int)(*it)->boundingRect().y() + r);
				pos2item.erase(pos);
				this->scene()->removeItem(*it);
			}
			++it;
		}
		for (auto it = _removed_and_added.second.begin(); it != _removed_and_added.second.end();) {
			(*it)->setBrush(color_brushes[0]);
			++it;
		}
		_removed_and_added.first.clear();
		_removed_and_added.second.clear();
		for (auto &p : removed_n_added->first) {
			int key = (int)p->pos.x() * CANVAS_HEIGHT + (int)p->pos.y();
			if (pos2item.find(key) != pos2item.end()) {
				pos2item[key]->setBrush(color_brushes[5]); // green
				_removed_and_added.first.push_back(pos2item[key]);
			}
		}
		for (auto &p : removed_n_added->second) {
			int key = (int)p->pos.x() * CANVAS_HEIGHT + (int)p->pos.y();
			auto it = this->scene()->addEllipse(p->pos.x() - r, p->pos.y() - r, 2 * r, 2 * r, Qt::NoPen, color_brushes[2]); // orange
			
			pos2item.emplace(key, it);
			_removed_and_added.second.push_back(it);
		}
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
	fn_stream << params.grid_width << '_' << params.end_level << '_';
	fn_stream << params.data_density_weight << '_' << params.epsilon << '_' << frame_id;
	//fn_stream << params.grid_width << '_' << tree_sampling_params.threshold << '_'
	//	<< tree_sampling_params.occupied_space_ratio << '_' << tree_sampling_params.backtracking_depth << '_' << frame_id;
	auto filepath = fn_stream.str();
	saveImagePNG(QString(filepath.c_str()) + ".png"); // _diff
	//writeResult(QString(filepath.c_str()) + ".csv");
}

void SamplingProcessViewer::updateClassInfo()
{
	if (class2label->size() != last_class_num) {
		emit classChanged(class2label);
		last_class_num = class2label->size();
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

void SamplingProcessViewer::drawPoint(qreal x, qreal y, qreal radius, QBrush b, bool is_virtual)
{
	auto s = is_virtual ? virtual_scene : this->scene();
	if (params.use_alpha_channel) { // draw translucent circles
		QColor c = b.color(); // color_brushes[0].color(); 
		c.setAlpha(5);
		s->addEllipse(x - radius, y - radius, 2 * radius, 2 * radius, Qt::NoPen, c);
	}
	else {
		s->addEllipse(x - radius, y - radius, 2 * radius, 2 * radius, Qt::NoPen, b); //color_brushes[0]
	}
}
