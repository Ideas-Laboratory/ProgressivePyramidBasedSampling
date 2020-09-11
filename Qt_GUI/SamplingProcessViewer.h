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
	void setDataName(std::string&& dn) { data_name = dn; }
	void setDataSource(std::string&& data_path);
	void gridWidthChanged(bool changed) { grid_width_changed = changed; }

	//void drawBorders();
	void redrawPoints();

	void sample();
	void showSpecificFrame();
	
	void saveImagePNG(const QString& path, bool is_virtual = false);
	void saveImageSVG(const QString& path, bool is_virtual = false);
	void saveImagePDF(const QString& path);
	void writeResult(const QString& path);

	uint getPointNum() { return sw.getPointCount(); }
	const std::vector<QBrush>& getColorBrushes() { return color_brushes; }

	const std::string ALGORITHM_NAME = "Progressive Sampling for Scatterplots";

public slots:
	void drawPointsProgressively(FilteredPointSet* points); // for virtual scene
	void drawSelectedPointsProgressively(std::pair<TempPointSet, TempPointSet>* removed_n_added); // for this scene
	void drawSelectedPointDiffsProgressively(std::pair<TempPointSet, TempPointSet>* removed_n_added);
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
	void drawPointByClass(TempPointSet& selected = TempPointSet());
	void drawPointRandomly(TempPointSet& selected = TempPointSet());

	void paletteToColors();
	void drawPoint(qreal x, qreal y, qreal radius, QBrush c, bool is_virtual = false);
	void drawLine(qreal x1, qreal y1, qreal x2, qreal y2, bool isRed)
	{
		QColor c = isRed ? Qt::red : Qt::black;
		c.setAlphaF(0.5);
		bound_pen.setColor(c);
		this->scene()->addLine(x1, y1, x2, y2, bound_pen);
	}

	QThread workerThread;
	SamplingWorker sw;
	bool grid_width_changed = false;

	std::string data_name;
	std::string data_path;
	std::unordered_map<uint, std::string>* class2label;
	size_t last_class_num = 0;
	clock_t last_time = 0l;

	std::unordered_map<int, QGraphicsEllipseItem*> pos2item;
	std::pair<std::vector<QGraphicsEllipseItem*>, std::vector<QGraphicsEllipseItem*>> _removed_and_added;
	QGraphicsScene *virtual_scene;

	std::vector<std::string> text_info;
	std::vector<QBrush> color_brushes;
	int color_index = 0;
	QPen bound_pen;
};

