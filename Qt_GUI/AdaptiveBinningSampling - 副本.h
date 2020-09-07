#pragma once
#include <utility>
#include <set>

#include "global.h"
#include "utils.h"
#include "BinningTree.h"

class AdaptiveBinningSampling
{
public:
	struct Status {
		uint iteration;
		uint seed_num;
		uint split_num;
	};

	template<class T>
	using Report = std::function<void(const T&)>;

	std::vector<std::weak_ptr<BinningTreeNode>> getAllLeaves();

	/* main function that contains the framework */
	Indices execute(const std::shared_ptr<FilteredPointSet> origin, const QRect& bounding_rect);

	// determine class labels and select samples
	Indices KDTreeGuidedSampling();
	
	void setStatusCallback(Report<Status> cb) {	status_callback = cb; }

	const static int max_iteration;
private:
	// determine whether the subtree should be split
	void divideTree(std::shared_ptr<BinningTreeNode> root, bool should_split); 
	// Randomly select samples for leaves
	Indices selectNewSamples(const std::vector<std::weak_ptr<BinningTreeNode>>& leaves);
	// termination condition
	bool notFinish(); 

	//std::vector<NodeWithQuota> determineLabelOfLeaves();
	Indices leavesToSeeds();

	Status current_iteration_status;

	std::unique_ptr<BinningTree> tree;

	Report<Status> status_callback;
};

