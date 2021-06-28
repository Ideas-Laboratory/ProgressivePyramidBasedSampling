#include "samplingworker.hpp"

void SamplingWorker::readAndSample()
{
	int frame_id = 1;
	qDebug() << "starting...";
	while (!data_source.eof()) {
		auto start = std::chrono::high_resolution_clock::now();
		PointSet* new_data = readDataSource(data_source, class2label);
		if (point_count == 0) {
			//real_extent = getExtent(new_data); // use the extent of the first batch for the whole data
			//real_extent = { -13.225, -22.217,23.85400,14.88304 }; // 3 clusters
			//real_extent = { -0.278698, -0.494428,5.75817,3.9781}; // person activity
			real_extent = { -1.0,-0.3,1e7,0.3 }; // stock market
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

		_result = hs.execute(_filtered_new_data, point_count == 0);
		//seeds = hs.selectSeeds();
		//_result = abs.executeWithoutCallback(_filtered_new_data, { QRect(MARGIN.left, MARGIN.top, CANVAS_WIDTH - MARGIN.left - MARGIN.right, CANVAS_HEIGHT - MARGIN.top - MARGIN.bottom) }, point_count == 0);
		//_result = rs.execute(_filtered_new_data, point_count == 0);
		//_result = rands.execute(_filtered_new_data);
		qDebug() << "total_execution: " << std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count() / 1e9;
		//qDebug() << _result->second.size();

		point_count += new_data->size();
		//char filename[160];
		//sprintf(filename, "./results/census_sampled/%d_%d_%.2f_%.2f_%.2f_%d_%d.csv", params.point_radius, params.grid_width,
		//	params.density_threshold, params.outlier_weight, params.ratio_threshold, params.stop_level, frame_id);
		//std::ofstream output(filename, std::ios_base::trunc);
		//for (auto& idx : hs.selectSeeds()) {
		//std::ofstream output("./results/PersonActivity_165k/random_" + std::to_string(ReservoirSampling::seeds_num)
		//	+ "_" + std::to_string(frame_id)
		//	+ ".csv", std::ios_base::trunc);
		//for (auto& idx : rs.selectSeeds()) {
		//std::ofstream output("./results/ConfLongDemo_JSI/random_" + std::to_string(RandomSampling::seeds_num)
		//	+ "_" + std::to_string(frame_id)
		//	+ ".csv", std::ios_base::trunc);
		//for (auto& idx : rands.selectSeeds()) {
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

void SamplingWorker::setDataSource(const std::string& data_path)
{
	data_source.close();
	point_count = 0;

	openDataSource(data_source, data_path);
}

void SamplingWorker::updateGrids()
{
	hs = HierarchicalSampling{ QRect(MARGIN.left, MARGIN.top, CANVAS_WIDTH - MARGIN.left - MARGIN.right, CANVAS_HEIGHT - MARGIN.top - MARGIN.bottom) };
}
