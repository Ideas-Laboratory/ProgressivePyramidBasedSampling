#pragma once

#include <memory>
#include <functional>
#include <ctime>
#include <random>
#include <numeric>
#include <queue>
#include <algorithm>

#include <QRect>

#include "global.h"

extern Param params;
extern std::vector<int> selected_class_order;

class BinningTreeNode
{
public:
	friend class BinningTree;
	BinningTreeNode(Box&& b, QPoint&& split_pos, uint side_length, std::vector<std::weak_ptr<MinGrid>>&& vec, StatisticalInfo&& info, std::weak_ptr<BinningTreeNode> parent) 
		: b(std::move(b)), split_pos(std::move(split_pos)), child_side(side_length), min_grids_inside(std::move(vec)), info(std::move(info)), parent(parent)
	{
		children.resize(child_num);
		seed_index = -1;
		leaf_num_inside = min_grids_inside.empty() ? 0 : 1; // empty bin isn't a valid leaf
	}

	bool tooManyClasses() { return info.class_point_num.size() > leaf_num_inside; }
	bool tooManyFreeSpace() { return min_grids_inside.size() < params.occupied_space_ratio*(b.right-b.left)*(b.bottom-b.top); }
	std::weak_ptr<BinningTreeNode> getParent() { return parent; }
	std::weak_ptr<BinningTreeNode> getChildren(int i) { return i < child_num ? children[i] : nullptr; }
	size_t getPointNum() { return info.total_num; }
	const Box& getBoundBox()  { return b; }
	uint getSeedIndex() { return seed_index; }
	uint getLeafNum() { return leaf_num_inside; }

	const static int child_num = 4;
private:
	const Box b;
	const QPoint split_pos; // computed by its parent
	const uint child_side; // the number of bins of the side of child nodes

	StatisticalInfo info;
	std::vector<std::weak_ptr<MinGrid>> min_grids_inside;
	
	std::weak_ptr<BinningTreeNode> parent;
	std::vector<std::shared_ptr<BinningTreeNode>> children;
	
	uint seed_index; // the seed in this bin
	uint leaf_num_inside;
};

typedef std::pair<std::weak_ptr<BinningTreeNode>, std::unordered_map<uint, size_t>> NodeWithQuota;

class BinningTree
{
public:
	BinningTree(const std::shared_ptr<FilteredPointSet> origin, const QRect& bounding_rect);

	std::weak_ptr<BinningTreeNode> getRoot() { return root; }
	bool isNotLeaf(std::shared_ptr<BinningTreeNode> node) { return node->children[0].get(); }

	bool split(std::shared_ptr<BinningTreeNode> node);
	
	uint selectSeedIndex(std::shared_ptr<BinningTreeNode> node);
	uint selectSeedIndex(std::shared_ptr<BinningTreeNode> node, uint label);

	void updateLeafNum(std::shared_ptr<BinningTreeNode> node);
	void determineSplittingChildren(std::shared_ptr<BinningTreeNode> node, std::vector<bool> &ss, double threshold);

	NodeWithQuota backtrack(std::shared_ptr<BinningTreeNode> leaf, uint max_depth);

	std::unordered_map<uint, size_t> determineQuota(std::shared_ptr<BinningTreeNode> node);
	void splitQuota(const NodeWithQuota& node, std::vector<NodeWithQuota>* leaves);
private:
	void fillQuota(uint remaining_leaf_num, const std::unordered_map<uint, size_t> &label_point_map, std::unordered_map<uint, size_t>& quato, const std::function<size_t(uint)>& getAmount);
	StatisticalInfo countStatisticalInfo(const std::vector<std::weak_ptr<MinGrid>>& vec);
	
	uint rouletteSelection(const std::vector<uint>& items, std::function<double(uint)> scoreFunc);
	
	std::shared_ptr<BinningTreeNode> root;

	const std::shared_ptr<FilteredPointSet> dataset; // original dataset

	struct pairhash {
	public:
		template <typename T, typename U>
		std::size_t operator()(const std::pair<T, U> &x) const
		{
			return std::hash<T>()(x.first)*CANVAS_HEIGHT + std::hash<U>()(x.second);
		}
	};
	std::unordered_map<std::pair<int,int>, std::shared_ptr<MinGrid>, pairhash> min_grids;

	const uint horizontal_bin_num;
	const uint vertical_bin_num;
	const uint margin_left;
	const uint margin_top;

	std::mt19937 gen{ time(NULL) };
};

inline int visual2grid(qreal pos, qreal margin)
{
	return static_cast<int>(pos - margin) / params.grid_width;
}

inline qreal grid2visual(qreal pos, qreal margin)
{
	return pos*params.grid_width + margin;
}

template<class T>
struct ClassPointComparator {
	bool operator()(const std::pair<uint, T>& a, const std::pair<uint, T>& b)	{
		return a.second < b.second;
	}
};