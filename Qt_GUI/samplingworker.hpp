#pragma once

#include "global.h"
#include "utils.h"
#include "HaarWaveletSampling.h"
#include "AdaptiveBinningSampling.h"
#include "ReservoirSampling.h"

class SamplingWorker : public QObject {
	Q_OBJECT

public:
	SamplingWorker() { openDataSource(data_source, MY_DATASET_FILENAME); }
	uint getPointCount() { return point_count; }
	const std::vector<uint>& getSelected() { return seeds; }
	uint getIndexByPos(const QPointF& pos) { return hws.getIndexByPos(pos); }

	void setDataSource(std::string&& data_path);
	void setClassMapping(std::unordered_map<uint, std::string>* class_mapping) { class2label = class_mapping; }
	void updateGrids();

public slots:
	void readAndSample();

signals:
	void readFinished(FilteredPointSet* filtered_points);
	void sampleFinished(std::pair<TempPointSet, TempPointSet>* removed_n_added);
	void writeFrame(int frame_id);

private:
	HaarWaveletSampling hws{ QRect(MARGIN.left, MARGIN.top, CANVAS_WIDTH - MARGIN.left - MARGIN.right, CANVAS_HEIGHT - MARGIN.top - MARGIN.bottom) };
	ReservoirSampling rs;
	AdaptiveBinningSampling abs;

	Indices seeds;
	uint point_count = 0;
	FilteredPointSet* _filtered_new_data = nullptr;
	std::pair<TempPointSet, TempPointSet>* _result = nullptr;

	std::unordered_map<uint, std::string>* class2label;
	std::ifstream data_source;
	Extent real_extent, visual_extent = { (qreal)MARGIN.left, (qreal)MARGIN.top, (qreal)(CANVAS_WIDTH - MARGIN.right), (qreal)(CANVAS_HEIGHT - MARGIN.bottom) };
};
