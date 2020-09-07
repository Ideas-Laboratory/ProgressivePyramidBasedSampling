﻿#include "samplingworker.hpp"

void SamplingWorker::readAndSample()
{
	int frame_id = 1;
	qDebug() << "starting...";
	while (!data_source.eof()) {
		PointSet *new_data = readDataSource(data_source, class2label, params.batch);
		if (point_count == 0) {
			real_extent = getExtent(new_data); // use the extent of the first batch for the whole data
			//real_extent = { -0.278698, -0.494428,5.75817,3.9781}; // person activity
			//real_extent = { -2.02172e+06, -2.115e+06,2.51266e+06,730758}; // U.S. census
			//real_extent = { -74.238,40.51,-73.728,40.906 }; // taxis.csv
			//real_extent = { -7.516225,49.912941,1.762010,60.75754 }; // uk_accident.csv
		}
		if (new_data->empty()) {
			delete new_data;
			continue;
		}

		_filtered_new_data = filter(new_data, real_extent, point_count);
		linearScale(_filtered_new_data, real_extent, visual_extent);

		//clock_t start = std::clock();
		_result = hws.execute(_filtered_new_data, point_count == 0);
		//seeds = hws.selectSeeds();
		//_result = abs.executeWithoutCallback(_filtered_new_data, { QRect(MARGIN.left, MARGIN.top, CANVAS_WIDTH - MARGIN.left - MARGIN.right, CANVAS_HEIGHT - MARGIN.top - MARGIN.bottom) }, point_count == 0);
		//_result = rs.execute(_filtered_new_data, point_count == 0);
		//qDebug() << "exec time: " << (std::clock() - start) / (double)CLOCKS_PER_SEC;
		//qDebug() << _result->second.size();

		point_count += new_data->size(); 
		//std::ofstream output("./results/arxiv_articles_UMAP/" + std::to_string(params.grid_width) 
		//	+ "_" + std::to_string(params.end_level)
		//	+ "_" + "0.2" // std::to_string(params.low_density_weight)
		//	+ "_" + "0.25" //std::to_string(params.epsilon)
		//	+ "_" + std::to_string(frame_id) 
		//	+ ".csv", std::ios_base::trunc);
		//for (auto& idx : hws.selectSeeds()) {
		//	output << idx << std::endl;
		//}
		//output.close();
		emit readFinished(_filtered_new_data);
		emit sampleFinished(_result);
		emit writeFrame(frame_id);
		++frame_id;
		delete new_data;
	}
}

void SamplingWorker::setDataSource(std::string && data_path)
{
	data_source.close();
	point_count = 0;

	openDataSource(data_source, data_path);
}

void SamplingWorker::updateGrids()
{
	hws = HaarWaveletSampling{ QRect(MARGIN.left, MARGIN.top, CANVAS_WIDTH - MARGIN.left - MARGIN.right, CANVAS_HEIGHT - MARGIN.top - MARGIN.bottom) };
}