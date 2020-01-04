#include "AdaptiveBinningSampling.h"

const int AdaptiveBinningSampling::max_iteration = 10000;

using namespace std;

std::vector<std::weak_ptr<BinningTreeNode>> AdaptiveBinningSampling::getAllLeaves()
{
	vector<weak_ptr<BinningTreeNode>> leaves;
	queue<weak_ptr<BinningTreeNode>> fifo;
	fifo.push(tree->getRoot());
	while (!fifo.empty()) {
		shared_ptr<BinningTreeNode> ptr = fifo.front().lock();
		fifo.pop();
		if (ptr->getLeafNum() > 1) {
			for (int i = 0; i < ptr->child_num; ++i) {
				fifo.push(ptr->getChildren(i));
			}
		}
		else {
			leaves.push_back(ptr);
		}
	}
	return leaves;
}


void AdaptiveBinningSampling::divideTree(shared_ptr<BinningTreeNode> root, bool should_split)
{
	if (tree->isNotLeaf(root)) { // if root has child nodes, then process tree recursion
		vector<bool> ss(root->child_num, should_split);
		if (should_split) {
			tree->determineSplittingChildren(root, ss, params.threshold);
		}
		for(int i=0; i<root->child_num; ++i) {
			divideTree(root->getChildren(i).lock(), ss[i]);
		}
		tree->updateLeafNum(root);
	}
	else { // no child nodes
		if ((should_split || root->tooManyFreeSpace()) // splitting condition
			&& tree->split(root)) {
			tree->updateLeafNum(root);
			++current_iteration_status.split_num;
		}
		else {
			// keep
		}
	}
}

Indices AdaptiveBinningSampling::selectNewSamples(const vector<weak_ptr<BinningTreeNode>>& leaves)
{
	Indices samples;
	for (auto &leaf : leaves) {
		uint index = tree->selectSeedIndex(leaf.lock());
		samples.push_back(index);
	}
	return samples;
}

Indices AdaptiveBinningSampling::KDTreeGuidedSampling()
{
	//auto &leaves = determineLabelOfLeaves();

	//Indices samples;
	//for (auto &leaf : leaves) {
	//	uint index = tree->selectSeedIndex(leaf.first.lock(), leaf.second.begin()->first);
	//	samples.push_back(index);
	//}
	auto &leaves = getAllLeaves();

	Indices samples;
	for (auto leaf : leaves) {
		auto ptr = leaf.lock();
		if (ptr->getLeafNum() == 1) {
			samples.push_back(tree->selectSeedIndex(ptr));
		}
	}
	return samples;
}

Indices AdaptiveBinningSampling::execute(const shared_ptr<FilteredPointSet> origin, const QRect& bounding_rect)
{
	if (origin->empty()) return Indices();

	tree = make_unique<BinningTree>(origin, bounding_rect);
	current_iteration_status = { 0,0 };
	//start iteration
	std::clock_t start = std::clock();
	do {
		current_iteration_status.split_num = 0;

		divideTree(tree->getRoot().lock(), true);
		++current_iteration_status.iteration;
	} while (notFinish());
	auto &samples = KDTreeGuidedSampling();
	qDebug() << (std::clock() - start) / (double)CLOCKS_PER_SEC;

	current_iteration_status.seed_num = samples.size();
	if(status_callback) status_callback(current_iteration_status);
	return samples;
}

inline bool AdaptiveBinningSampling::notFinish()
{
	return !(current_iteration_status.split_num == 0 || current_iteration_status.iteration == max_iteration);
}

//vector<NodeWithQuota> AdaptiveBinningSampling::determineLabelOfLeaves()
//{
//	vector<weak_ptr<BinningTreeNode>> leaves = getAllLeaves();
//
//	unordered_map<shared_ptr<BinningTreeNode>, unordered_map<uint, size_t>> forest;
//	{ // backtrack and form a forest
//		set<shared_ptr<BinningTreeNode>> visited_leaves;
//		for (auto &l : leaves) {
//			auto &leaf = l.lock();
//			if (visited_leaves.find(leaf) == visited_leaves.end()) { // not visited
//				NodeWithQuota root = tree->backtrack(leaf, params.backtracking_depth);
//				// update visited leaves
//				queue<weak_ptr<BinningTreeNode>> fifo;
//				fifo.push(root.first);
//				while (!fifo.empty()) {
//					shared_ptr<BinningTreeNode> ptr = fifo.front().lock();
//					fifo.pop();
//					forest.erase(ptr); // remove if ptr is the offspring of root 
//					auto &child1 = ptr->getChild1().lock();
//					if (child1) {
//						fifo.push(child1);
//						fifo.push(ptr->getChild2());
//					}
//					else
//						visited_leaves.insert(ptr);
//				}
//				forest.insert(root);
//			}
//		}
//	}
//
//	vector<NodeWithQuota> leavesWithLabel;
//	for (auto &root : forest) {
//		tree->splitQuota(root, &leavesWithLabel);
//	}
//	return leavesWithLabel;
//}

Indices AdaptiveBinningSampling::leavesToSeeds()
{
	auto &leaves = getAllLeaves();
	Indices seeds;
	for (auto &leaf : leaves) {
		uint index = tree->selectSeedIndex(leaf.lock());
		seeds.push_back(index);
	}
	return seeds;
}
