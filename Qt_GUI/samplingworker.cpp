#include "samplingworker.hpp"

void SamplingWorker::readAndSample()
{
	int frame_id = 1;
	qDebug() << "starting...";
	while (!data_source.eof()) {
		PointSet *data_chunk = readDataSource(data_source, class2label, params.batch);
		if (point_count == 0) {
			real_extent = getExtent(data_chunk); // use the first chunk to approximate the extent of the whole data
			//real_extent = { -0.278698, -0.494428,5.75817,3.9781}; // person activity
			//real_extent = { -2.02172e+06, -2.115e+06,2.51266e+06,730758}; // U.S. census
			//real_extent = { -74.238,40.51,-73.728,40.906 }; // taxis.csv
			//real_extent = { -7.516225,49.912941,1.762010,60.75754 }; // uk_accident.csv
		}
		if (data_chunk->empty()) {
			delete data_chunk;
			continue;
		}

		_filtered_new_data = filter(data_chunk, real_extent, point_count);
		linearScale(_filtered_new_data, real_extent, visual_extent);

		_result = hws.execute(_filtered_new_data, point_count == 0);
		seeds = hws.selectSeeds();
		point_count += data_chunk->size(); 

		emit readFinished(_filtered_new_data);
		emit sampleFinished(_result);
		emit writeFrame(frame_id);
		++frame_id;
		delete data_chunk;
	}
	emit finished();
}



void SamplingWorker::setDataSource(std::string& data_path)
{
	data_source.close();
	point_count = 0;

	openDataSource(data_source, data_path);
}

void SamplingWorker::updateGrids()
{
	hws = HaarWaveletSampling{ QRect(MARGIN.left, MARGIN.top, CANVAS_WIDTH - MARGIN.left - MARGIN.right, CANVAS_HEIGHT - MARGIN.top - MARGIN.bottom) };
}