#include "samplingworker.hpp"

void SamplingWorker::readAndSample()
{
	int frame_id = 1;
	qDebug() << "starting...";
	while (!data_source.eof()) {
		auto start = std::chrono::high_resolution_clock::now();
		PointSet* data_chunk = readDataSource(data_source, class2label);
		if (point_count == 0) {
			real_extent = getExtent(data_chunk); // use the extent of the first batch for the whole data
		}
		if (data_chunk->empty()) {
			delete data_chunk;
			continue;
		}

		_filtered_new_data = filter(data_chunk, real_extent, point_count);
		linearScale(_filtered_new_data, real_extent, visual_extent);

		// run sampling methods
		_result = hs.execute(_filtered_new_data, point_count == 0);
		seeds = hs.getSeedIndices();
		//_result = abs.executeWithoutCallback(_filtered_new_data, { QRect(MARGIN.left, MARGIN.top, CANVAS_WIDTH - MARGIN.left - MARGIN.right, CANVAS_HEIGHT - MARGIN.top - MARGIN.bottom) }, point_count == 0);
		//_result = rs.execute(_filtered_new_data, point_count == 0);
		//_result = rands.execute(_filtered_new_data);
		qDebug() << "total_execution: " << std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count() / 1e9;
		//qDebug() << _result->second.size();

		// post-processing
		point_count += data_chunk->size();
		emit readFinished(_filtered_new_data);
		emit sampleFinished(_result);
		emit writeFrame(frame_id);
		++frame_id;
		delete data_chunk;
	}
	emit finished();
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
