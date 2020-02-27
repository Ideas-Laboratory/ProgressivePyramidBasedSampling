#pragma once

#include <queue>

#include <QGraphicsView>
#include <QMouseEvent>
#include <QInputDialog>

#include "global.h"
#include "utils.h"
#include "HaarWaveletSampling.h"

#define CLASS_NUM 30

class SamplingProcessViewer : public QGraphicsView
{
	Q_OBJECT

public:
	SamplingProcessViewer(std::string&& data_name, std::unordered_map<uint, std::string>* class2label, QWidget* parent);
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

	size_t getPointNum() { return point_count; }
	const std::vector<QBrush>& getColorBrushes() { return color_brushes; }

	const std::string ALGORITHM_NAME = "Progressive Sampling for Scatterplots";

signals:
	void finished();
	void redrawStart();
	void sampleStart();
	void adjustmentStart();
	void inputImageChanged();
	void iterationStatus(int iteration, int numberPoints, int splits);
	void areaCounted(StatisticalInfo* total_info, StatisticalInfo* sample_info);
	void pointSelected(uint index, uint class_);

protected:
	void mousePressEvent(QMouseEvent *me);
	void mouseMoveEvent(QMouseEvent *me);
	void mouseReleaseEvent(QMouseEvent *me);

private:
	void drawPointProgressively(const std::shared_ptr<FilteredPointSet>& points); // for virtual scene
	void drawPointProgressively(std::pair<TempPointSet, TempPointSet>& removed_n_added); // for this scene
	void drawPointByClass(TempPointSet& selected = TempPointSet());
	void drawPointRandomly(TempPointSet& selected = TempPointSet());
	void drawPointsByPair(std::pair<TempPointSet, TempPointSet>& selected);

	void paletteToColors();
	void drawPoint(qreal x, qreal y, qreal radius, QBrush c, bool is_virtual = false);
	void drawLine(qreal x1, qreal y1, qreal x2, qreal y2, bool isRed)
	{
		QColor c = isRed ? Qt::red : Qt::black;
		c.setAlphaF(0.5);
		bound_pen.setColor(c);
		this->scene()->addLine(x1, y1, x2, y2, bound_pen);
	}

	bool grid_width_changed = false;

	HaarWaveletSampling hws{ QRect(MARGIN.left, MARGIN.top, CANVAS_WIDTH - MARGIN.left - MARGIN.right, CANVAS_HEIGHT - MARGIN.top - MARGIN.bottom) };
	
	std::string data_name;
	std::ifstream data_source;
	std::unordered_map<uint, std::string>* class2label;
	uint point_count = 0;

	PointSet seeds;
	//std::vector<std::weak_ptr<RecontructedGrid>> nodes;
	QGraphicsScene *virtual_scene;

	std::vector<QBrush> color_brushes;
	int color_index = 0;
	QPen bound_pen;

	Extent real_extent, visual_extent = { (qreal)MARGIN.left, (qreal)MARGIN.top, (qreal)(CANVAS_WIDTH - MARGIN.right), (qreal)(CANVAS_HEIGHT - MARGIN.bottom) };
};

