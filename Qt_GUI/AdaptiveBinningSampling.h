#pragma once
#include <utility>
#include <set>

#include "global.h"
#include "utils.h"
#include "BinningTree.h"

struct TreeNode
{
	std::weak_ptr<BinningTreeNode> node;
	bool leftIsNew;
	bool topIsNew;
};

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
	std::pair<PointSet, PointSet>* execute(const FilteredPointSet* origin, const QRect& bounding_rect, bool is_1st);
	std::pair<PointSet, PointSet>* executeWithoutCallback(const FilteredPointSet* origin, const QRect& bounding_rect, bool is_1st);

	// determine class labels and select samples
	Indices KDTreeGuidedSampling();
	
	void setStatusCallback(Report<Status> cb) {	status_callback = cb; }
	void setCanvasCallback(Report<std::vector<TreeNode>> cb) { canvas_callback = cb; }

	const static int max_iteration;
private:
	// determine whether the subtree should be split
	void divideTree(std::shared_ptr<BinningTreeNode> root, std::vector<TreeNode>* leaves, bool should_split); 
	// Randomly select samples for leaves
	Indices selectNewSamples(const std::vector<TreeNode>& leaves);
	// termination condition
	bool notFinish(); 

	std::vector<NodeWithQuota> determineLabelOfLeaves();
	Indices leavesToSeeds();
	std::pair<PointSet, PointSet>* findRemovedAndAdded(const Indices& seeds);

	Status current_iteration_status;

	std::unique_ptr<BinningTree> tree;
	Indices last_seeds;

	Report<Status> status_callback;
	Report<std::vector<TreeNode>> canvas_callback;
};

