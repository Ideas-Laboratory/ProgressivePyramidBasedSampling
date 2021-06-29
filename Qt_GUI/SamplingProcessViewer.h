#pragma once

#include <QThread>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QInputDialog>

#include "global.h"
#include "utils.h"
#include "samplingworker.hpp"

#define CLASS_NUM 20

class SamplingProcessViewer : public QGraphicsView
{
	Q_OBJECT

public:
	SamplingProcessViewer(std::string&& data_name, std::unordered_map<uint, std::string>* class2label, QWidget* parent);
	~SamplingProcessViewer() { workerThread.quit(); workerThread.wait(); }
	void gridWidthChanged(bool changed) { grid_width_changed = changed; }
	void setDataName(std::string&& dn) { data_name = dn; }
	// set data path and invoke the sampling worker thread
	void setDataPath(std::string&& data_path);

	// invoke the sampling procedure in samplingworker
	void sample();
	// fetch the result of current params.displayed_frame_id parameter for HierarchicalSampling and display in the screen
	void showSpecificFrame();
	// redraw the current result in the screen
	void redrawPoints();

	void saveImagePNG(const QString& path, bool is_virtual = false);
	void saveImageSVG(const QString& path, bool is_virtual = false);
	void saveImagePDF(const QString& path);
	void writeResult(const QString& path);

	uint getPointNum() { return sw.getPointCount(); }
	const std::vector<QBrush>& getColorBrushes() { return color_brushes; }

	const std::string ALGORITHM_NAME = "Pyramid-based Scatterplots Sampling";

public slots:
	void drawPointsProgressively(FilteredPointSet* points); // for virtual scene
	void drawSelectedPointsProgressively(std::pair<PointSet, PointSet>* removed_n_added); // for this scene
	void generateFiles(int frame_id);
	void updateClassInfo();

signals:
	void finished();
	void redrawStart();
	void sampleStart();
	void inputImageChanged();
	void iterationStatus(int iteration, int numberPoints, int splits);
	void areaCounted(StatisticalInfo* total_info, StatisticalInfo* sample_info);
	void pointSelected(uint index, uint class_);
	void classChanged(const std::unordered_map<uint, std::string>* class2label);
	void frameChanged(int frame_id);

protected:
	void mousePressEvent(QMouseEvent *me);
	void mouseMoveEvent(QMouseEvent *me);
	void mouseReleaseEvent(QMouseEvent *me);

private:
	// clear both virtual scene and this scene 
	void reinitializeScreen();
	void drawPointRandomly(PointSet& selected);
	// draw result of two frames and highlight the differences between them
	void drawPointsByPair(std::pair<PointSet, PointSet>& selected);

	void paletteToColors();
	QGraphicsItem* drawPoint(qreal x, qreal y, qreal radius, QBrush c, bool is_virtual = false);

	QThread workerThread;
	SamplingWorker sw;
	bool grid_width_changed = false;

	// the file name of the dataset without suffix
	std::string data_name;
	std::string data_path = MY_DATASET_FILENAME;
	std::unordered_map<uint, std::string>* class2label;
	std::unordered_map<double, QGraphicsItem*> pos2item;
	std::unordered_map<qint64, std::vector<QGraphicsItem*>> date2item;
	size_t last_class_num = 0;

	// scene used to draw the original dataset
	QGraphicsScene *virtual_scene;

	std::vector<std::string> text_info;
	std::vector<QBrush> color_brushes;
	int color_index = 0;
	QPen bound_pen;
};

