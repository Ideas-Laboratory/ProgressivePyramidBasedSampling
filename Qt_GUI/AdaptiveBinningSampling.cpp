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
		auto &child1 = ptr->getChild1().lock();
		if (child1) {
			fifo.push(child1);
			fifo.push(ptr->getChild2());
		}
		else
			leaves.push_back(ptr);
	}
	return leaves;
}

std::pair<PointSet, PointSet>* AdaptiveBinningSampling::execute(const FilteredPointSet * origin, const QRect& bounding_rect, bool is_1st)
{
	if (origin->empty()) return new std::pair<PointSet, PointSet>();;

	if (is_1st)
		tree = make_unique<BinningTree>(origin, bounding_rect);
	else
		tree->updateMinGrids(origin);
	//initialize seed List and status
	Indices samples(selectNewSamples(vector<TreeNode>{ {tree->getRoot(), true, true}}));
	current_iteration_status = { 0 };
	//start iteration
	do {
		current_iteration_status.split_num = 0;

		vector<TreeNode> leaves;
		divideTree(tree->getRoot().lock(), &leaves, true);

		samples = selectNewSamples(leaves);

		current_iteration_status.seed_num = samples.size();
		++current_iteration_status.iteration;

		canvas_callback(leaves);
		status_callback(current_iteration_status);
	} while (notFinish());
	return findRemovedAndAdded(samples);
}

void AdaptiveBinningSampling::divideTree(shared_ptr<BinningTreeNode> root, vector<TreeNode>* leaves, bool should_split)
{
	auto child1 = root->getChild1().lock(), child2 = root->getChild2().lock();
	
	if (child1) { // if root has child nodes, then process tree recursion
		bool c1_ss = should_split, c2_ss = should_split;
		if (should_split) {
			c1_ss = root->childShouldSplit(child1, tree_sampling_params.threshold);
			c2_ss = root->childShouldSplit(child2, tree_sampling_params.threshold);
		}
		divideTree(child1, leaves, c1_ss);
		divideTree(child2, leaves, c2_ss);
		tree->updateLeafNum(root);
	}
	else { // no child nodes
		if ((should_split || root->tooManyFreeSpace()) // splitting condition
			&& tree->split(root)) {
			tree->updateLeafNum(root);
			if (leaves) {
				bool leftIsNew = root->getChild1().lock()->getBoundBox().top == root->getChild2().lock()->getBoundBox().top;
				leaves->push_back({ root->getChild1(), false, false });
				leaves->push_back({ root->getChild2(), leftIsNew, !leftIsNew });
			}
			++current_iteration_status.split_num;
		}
		else {
			// keep
			if (leaves) {
				leaves->push_back({ root, false, false });
			}
		}
	}
}

Indices AdaptiveBinningSampling::selectNewSamples(const vector<TreeNode>& leaves)
{
	Indices samples;
	for (auto &leaf : leaves) {
		uint index = tree->selectSeedIndex(leaf.node.lock());
		samples.push_back(index);
	}
	return samples;
}

Indices AdaptiveBinningSampling::KDTreeGuidedSampling()
{
	auto &leaves = determineLabelOfLeaves();

	Indices samples;
	for (auto &leaf : leaves) {
		uint index = tree->selectSeedIndex(leaf.first.lock(), leaf.second.begin()->first);
		samples.push_back(index);
	}
	return samples;
}

std::pair<PointSet, PointSet>* AdaptiveBinningSampling::executeWithoutCallback(const FilteredPointSet * origin, const QRect& bounding_rect, bool is_1st)
{
	if (origin->empty()) return new std::pair<PointSet, PointSet>();

	if (is_1st) {
		tree = make_unique<BinningTree>(origin, bounding_rect);
		last_seeds.clear();
	}
	else
		tree->updateMinGrids(origin);
	current_iteration_status = { 0,0 };
	//start iteration
	//std::clock_t start = std::clock();
	do {
		current_iteration_status.split_num = 0;

		divideTree(tree->getRoot().lock(), nullptr, true);
		++current_iteration_status.iteration;
	} while (notFinish());
	auto &samples = leavesToSeeds(); //KDTreeGuidedSampling();
	//qDebug() << (std::clock() - start) / (double)CLOCKS_PER_SEC;
	if (is_1st) qDebug() << "The number of points in first frame: " << (int)samples.size();
	else qDebug() << "The number of points in current frame: " << (int)samples.size();

	current_iteration_status.seed_num = samples.size();
	if(status_callback) status_callback(current_iteration_status);
	return findRemovedAndAdded(samples);
}

inline bool AdaptiveBinningSampling::notFinish()
{
	return !(current_iteration_status.split_num == 0 || current_iteration_status.iteration == max_iteration);
}

vector<NodeWithQuota> AdaptiveBinningSampling::determineLabelOfLeaves()
{
	vector<weak_ptr<BinningTreeNode>> leaves = getAllLeaves();

	unordered_map<shared_ptr<BinningTreeNode>, unordered_map<uint, size_t>> forest;
	{ // backtrack and form a forest
		set<shared_ptr<BinningTreeNode>> visited_leaves;
		for (auto &l : leaves) {
			auto &leaf = l.lock();
			if (visited_leaves.find(leaf) == visited_leaves.end()) { // not visited
				NodeWithQuota root = tree->backtrack(leaf, tree_sampling_params.backtracking_depth);
				// update visited leaves
				queue<weak_ptr<BinningTreeNode>> fifo;
				fifo.push(root.first);
				while (!fifo.empty()) {
					shared_ptr<BinningTreeNode> ptr = fifo.front().lock();
					fifo.pop();
					forest.erase(ptr); // remove if ptr is the offspring of root 
					auto &child1 = ptr->getChild1().lock();
					if (child1) {
						fifo.push(child1);
						fifo.push(ptr->getChild2());
					}
					else
						visited_leaves.insert(ptr);
				}
				forest.insert(root);
			}
		}
	}

	vector<NodeWithQuota> leavesWithLabel;
	for (auto &root : forest) {
		tree->splitQuota(root, &leavesWithLabel);
	}
	return leavesWithLabel;
}

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

pair<PointSet, PointSet>* AdaptiveBinningSampling::findRemovedAndAdded(const Indices & seeds)
{
	Indices current = seeds, removed_idx, added_idx;
	sort(current.begin(), current.end());
	set_difference(last_seeds.begin(), last_seeds.end(), current.begin(), current.end(), back_inserter(removed_idx));
	set_difference(current.begin(), current.end(), last_seeds.begin(), last_seeds.end(), back_inserter(added_idx));
	PointSet removed, added;
	for (auto idx : removed_idx)
		removed.push_back(make_unique<LabeledPoint>(tree->getDataset()->at(idx)));
	for (auto idx : added_idx)
		added.push_back(make_unique<LabeledPoint>(tree->getDataset()->at(idx)));
	last_seeds = move(current);
	return new pair<PointSet, PointSet>(move(removed), move(added));
}
