#include "BinningTree.h"

using namespace std;

const static vector<uint> power_2 = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192 };

BinningTree::BinningTree(const shared_ptr<FilteredPointSet> origin, const QRect& bounding_rect)
	: dataset(origin), horizontal_bin_num(bounding_rect.width() / params.grid_width + 1), vertical_bin_num(bounding_rect.height() / params.grid_width + 1), margin_left(bounding_rect.left()), margin_top(bounding_rect.top())
{
	// create all min grids
	vector<weak_ptr<MinGrid>> vec;
	for (auto &pr : *origin) {
		auto &p = pr.second;
		int x = visual2grid(p->pos.x(), margin_left),
			y = visual2grid(p->pos.y(), margin_top);
		auto &pos = make_pair(x, y);
		if (min_grids.find(pos) == min_grids.end()) {
			min_grids[pos] = make_shared<MinGrid>(x, y);
			vec.push_back(min_grids[pos]);
		}
		min_grids[pos]->contents.push_back(pr.first);
	}
	vec.shrink_to_fit();
	StatisticalInfo info = countStatisticalInfo(vec);

	// create root of the tree
	Box root_bin(0, 0, horizontal_bin_num, vertical_bin_num);
	uint side = max(*lower_bound(power_2.begin(), power_2.end(), horizontal_bin_num), *lower_bound(power_2.begin(), power_2.end(), vertical_bin_num)) / 2;
	root = make_shared<BinningTreeNode>(move(root_bin), QPoint(horizontal_bin_num / 2, vertical_bin_num / 2), side, move(vec), move(info), weak_ptr<BinningTreeNode>());
}

bool BinningTree::split(std::shared_ptr<BinningTreeNode> node)
{
	if (node->min_grids_inside.size() < 2)
		return false;

	vector<vector<weak_ptr<MinGrid>>> children_vec(4);
	for (auto ptr : node->min_grids_inside) {
		auto g = ptr.lock();
		int i = (g->b.left < node->split_pos.x() ? 0 : 1) | (g->b.top < node->split_pos.y() ? 0 : 2);
		children_vec[i].push_back(g);
	}

	uint side = node->child_side / 2;
	for (int i = 0; i < node->child_num; ++i) {
		Box b;
		QPoint split_pos;
		if (i & 1) { // right nodes
			b.left = node->split_pos.x();
			b.right = max(b.left, node->b.right);
			split_pos.setX(node->split_pos.x() + side);
		}
		else { // left nodes
			b.right = node->split_pos.x();
			b.left = min(node->b.left, b.right);
			split_pos.setX(node->split_pos.x() - side);
		}
		if (i & 2) { // bottom nodes
			b.top = node->split_pos.y();
			b.bottom = max(b.top, node->b.bottom);
			split_pos.setY(node->split_pos.y() + side);
		}
		else { // top nodes
			b.bottom = node->split_pos.y();
			b.top = min(node->b.top, b.bottom);
			split_pos.setY(node->split_pos.y() - side);
		}
		node->children[i] = make_shared<BinningTreeNode>(move(b), move(split_pos), side, move(children_vec[i]), countStatisticalInfo(children_vec[i]), node);
	}
	return true;
}

uint BinningTree::selectSeedIndex(shared_ptr<BinningTreeNode> node)
{
	vector<uint> indices;
	for (auto &b : node->min_grids_inside) {
		for (uint i : b.lock()->contents) {
			indices.push_back(i);
		}
	}

	node->seed_index = indices[rand() % indices.size()];
	return node->seed_index;
}

uint BinningTree::selectSeedIndex(shared_ptr<BinningTreeNode> node, uint label)
{
	vector<uint> indices;
	for (auto &b : node->min_grids_inside) {
		for (uint i : b.lock()->contents) {
			if (dataset->at(i)->label == label) {
				indices.push_back(i);
			}
		}
	}

	node->seed_index = indices[rand() % indices.size()];
	return node->seed_index;
}

void BinningTree::updateLeafNum(shared_ptr<BinningTreeNode> node)
{
	node->leaf_num_inside = accumulate(node->children.begin(), node->children.end(), (uint)0, [](uint a, shared_ptr<BinningTreeNode> b) { return a + b->getLeafNum(); });
}

void BinningTree::determineSplittingChildren(std::shared_ptr<BinningTreeNode> node, std::vector<bool>& ss, double threshold)
{
	double min_sr = DBL_MAX;
	for (int i = 0; i < node->child_num; ++i) {
		if (node->children[i]->leaf_num_inside == 0) {
			ss[i] = false;
		}
		else {
			double child_sampling_ratio = (double)node->children[i]->leaf_num_inside / node->children[i]->info.total_num, sampling_ratio = (double)node->leaf_num_inside / node->info.total_num;
			ss[i] = child_sampling_ratio - sampling_ratio < threshold;
		}
	}
}

/*
NodeWithQuota BinningTree::backtrack(shared_ptr<BinningTreeNode> leaf, uint max_depth)
{
	if (leaf->info.class_point_num.size() == 1) // only 1 class in the bin
		return make_pair(leaf, unordered_map<uint, size_t>{ {leaf->info.class_point_num.begin()->first, 1} });

	NodeWithQuota result;
	shared_ptr<BinningTreeNode> current = leaf;
	double min_diff = DBL_MAX;
	while ((current = current->parent.lock()) && max_depth--) {
		auto &&quota = determineQuota(current);
		auto &cpn = current->info.class_point_num;
		if (current->leaf_num_inside < cpn.size()) {
			result = make_pair(current, quota);
		}
		else {
			double diff = 0.0;
			vector<uint> class_;
			for (auto &pr : cpn) {
				class_.push_back(pr.first);
			}
			vector<bool> v(class_.size());
			fill(v.begin(), v.begin() + 2, true);
			do {
				vector<int> class_indices;
				for (int i = 0; i < v.size(); ++i) {
					if (v[i]) {
						class_indices.push_back(class_[i]);
					}
				}
				if(cpn[class_indices[0]] < cpn[class_indices[1]])
					diff += abs((double)quota[class_indices[0]] / quota[class_indices[1]] - (double)cpn[class_indices[0]] / cpn[class_indices[1]]);
				else
					diff += abs((double)quota[class_indices[1]] / quota[class_indices[0]] - (double)cpn[class_indices[1]] / cpn[class_indices[0]]);
			} while (std::prev_permutation(v.begin(), v.end()));

			if (diff < min_diff) {
				min_diff = diff;
				result = make_pair(current, quota);
			}
		}
	}
	return result;
}

void BinningTree::fillQuota(uint remaining_leaf_num, const unordered_map<uint, size_t> &label_point_map, unordered_map<uint, size_t>& quota, const function<size_t(uint)>& getAmount)
{
	vector<uint> labels;
	vector<size_t> class_point_num;
	for (auto &u : label_point_map) {
		labels.push_back(u.first);
		if (quota.find(u.first) != quota.end())
			class_point_num.push_back(getAmount(u.first));
		else
			class_point_num.push_back(u.second);
	}

	vector<uint> indices(class_point_num.size());
	iota(indices.begin(), indices.end(), 0);
	auto& getScore = [&class_point_num](uint i) { return static_cast<double>(class_point_num[i]); };

	while (remaining_leaf_num--) {
		uint i = rouletteSelection(indices, getScore);
		++quota[labels[i]];
		--class_point_num[i];
	}
}

unordered_map<uint, size_t> BinningTree::determineQuota(shared_ptr<BinningTreeNode> node)
{
	unordered_map<uint, size_t> quota;
	for (auto &u : node->info.class_point_num) { // initialization
		if (node->leaf_num_inside == quota.size())
			break;
		quota[u.first] = 1;
	}

	fillQuota(node->leaf_num_inside - quota.size(), node->info.class_point_num, quota, [node, &quota](uint key) {return node->info.class_point_num[key] - quota[key]; });
	return quota;
}

unordered_map<uint, size_t> computeQuota(vector<NodeWithQuota>* leaves)
{
	unordered_map<uint, size_t> quota;
	for (auto &l : *leaves) {
		auto &it = l.second.begin();
		quota[it->first]++;
	}
	return quota;
}

void BinningTree::splitQuota(const NodeWithQuota& node, vector<NodeWithQuota>* leaves)
{
	shared_ptr<BinningTreeNode> self = node.first.lock();
	if (!self->child1) { // is leaf
		leaves->push_back(node);
		return;
	}

	shared_ptr<BinningTreeNode> inadequate, another;
	if (self->child1->info.class_point_num.size() > self->child2->info.class_point_num.size()) {
		inadequate = self->child1;
		another = self->child2;
	}
	else {
		inadequate = self->child2;
		another = self->child1;
	}

	priority_queue<pair<uint, size_t>, vector<pair<uint, size_t>>, ClassPointComparator<size_t>> q;
	auto &map_in_inadequate = inadequate->info.class_point_num, &map_in_another = another->info.class_point_num;
	for (auto &u : node.second) {
		if (map_in_another.find(u.first) == map_in_another.end())
			q.push(*map_in_inadequate.find(u.first));
	}

	auto &pushKMaxToMap = [](uint k, decltype(q) &q, unordered_map<uint, size_t>& map)
	{
		while (k-- && !q.empty()) {
			map[q.top().first] = 1;
			q.pop();
		}
	};
	unordered_map<uint, size_t> inadequate_quato;
	if (q.size() > inadequate->leaf_num_inside) {
		pushKMaxToMap(inadequate->leaf_num_inside, q, inadequate_quato);
	}
	else {
		size_t remain = inadequate->leaf_num_inside - q.size();
		pushKMaxToMap(q.size(), q, inadequate_quato);
		fillQuota(remain, map_in_inadequate, inadequate_quato, [node, &map_in_inadequate, &inadequate_quato](uint key) {
			size_t amount = map_in_inadequate[key] - inadequate_quato[key];
			if (node.second.find(key) != node.second.end())
				return min(amount, node.second.at(key));
			else
				return (size_t) 0;
		});
	}
	splitQuota(make_pair(inadequate, inadequate_quato), leaves);

	unordered_map<uint, size_t> another_quato;
	uint remaining_leaf_num = another->leaf_num_inside;
	for (auto &u : node.second) {
		another_quato[u.first] = u.second;
		if (inadequate_quato.find(u.first) != inadequate_quato.end()) // exists in the inadequate bin
			another_quato[u.first] -= inadequate_quato[u.first];
		if (another_quato[u.first] == 0 || map_in_another.find(u.first) == map_in_another.end()) // not in the another bin
			another_quato.erase(u.first);
		else if (another_quato[u.first] > remaining_leaf_num) {
			another_quato[u.first] = remaining_leaf_num;
			remaining_leaf_num = 0;
			break;
		}
		else
			remaining_leaf_num -= another_quato[u.first];
	}
	if (remaining_leaf_num > 0) {
		fillQuota(remaining_leaf_num, map_in_another, another_quato, [&map_in_another, &another_quato](uint key) {return map_in_another[key] - another_quato[key]; });
	}

	splitQuota(make_pair(another, another_quato), leaves);
}
*/

StatisticalInfo BinningTree::countStatisticalInfo(const vector<weak_ptr<MinGrid>>& vec)
{
	StatisticalInfo result;
	for (auto ptr : vec) {
		auto &c = ptr.lock()->contents;
		result.total_num += c.size();
		for (auto i : c) {
			++result.class_point_num[dataset->at(i)->label];
		}
	}
	return result;
}

uint BinningTree::rouletteSelection(const vector<uint>& items, std::function<double(uint)> scoreFunc)
{
	vector<double> scores;
	double sum_score = 0.0;
	for (auto i : items) {
		scores.push_back(scoreFunc(i));
		sum_score += scores.back();
	}
	if (sum_score == 0.0) return 0;

	uniform_real_distribution<double> unif(0.0, sum_score);
	double val = unif(gen);
	size_t i = 0, sz = items.size();
	for (; i < sz; ++i) {
		val -= scores[i];
		if (val < 0.0)
			break;
	}
	return i;
}