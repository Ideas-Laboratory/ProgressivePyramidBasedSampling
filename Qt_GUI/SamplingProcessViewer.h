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
	SamplingProcessViewer(const PointSet* points, QWidget* parent);
	void setDataset(const PointSet* points);
	void gridWidthChanged(bool changed) { grid_width_changed = changed; }

	//void drawBorders();
	void redrawPoints();

	void sample();
	void sampleWithoutTreeConstruction();
	
	void saveImagePNG(const QString& path);
	void saveImageSVG(const QString& path);
	void saveImagePDF(const QString& path);

	const Indices& getSeleted() { return seeds; }
	const std::vector<QBrush>& getColorBrushes() { return color_brushes; }

	const std::string ALGORITHM_NAME = "Dynamic Resampling for Animated Scatterplot";

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
	void drawPointByClass(const std::shared_ptr<FilteredPointSet>& points, const Indices& selected = Indices());
	void drawPointRandomly(const std::shared_ptr<FilteredPointSet>& points, const Indices& selected = Indices());
	void drawPointLowDensityUpper(const std::shared_ptr<FilteredPointSet>& points, const Indices& selected = Indices());

	void paletteToColors();
	void drawPoint(qreal x, qreal y, qreal radius, QBrush c);
	void drawLine(qreal x1, qreal y1, qreal x2, qreal y2, bool isRed)
	{
		QColor c = isRed ? Qt::red : Qt::black;
		c.setAlphaF(0.5);
		bound_pen.setColor(c);
		this->scene()->addLine(x1, y1, x2, y2, bound_pen);
	}

	bool grid_width_changed = false;

	HaarWaveletSampling hws{ QRect(MARGIN.left, MARGIN.top, CANVAS_WIDTH - MARGIN.left - MARGIN.right, CANVAS_HEIGHT - MARGIN.top - MARGIN.bottom) };
	std::shared_ptr<FilteredPointSet> points_in_visual_space = nullptr;
	Indices seeds;
	//std::vector<std::weak_ptr<RecontructedGrid>> nodes;

	std::vector<QBrush> color_brushes;

	QPen bound_pen;

	Extent real_extent;
};

