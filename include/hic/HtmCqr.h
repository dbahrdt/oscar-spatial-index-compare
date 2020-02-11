#pragma once
#include <hic/htm-index.h>

namespace hic {
namespace detail {
	

	
};

class HtmCqrDynamicTree {
public:
	using ItemIndexId = uint32_t;
	using TrixelId = uint64_t;
public:
	HtmCqrDynamicTree();
public:
	///add a full-match trixel
	void add(TrixelId trixelId);
	///add a partial-match trixel
	void add(TrixelId trixelId, ItemIndexId pmItemIndexId);
public:
	//merges full-match trixels
	void compactify();
private:
	struct Node {
		uint32_t fullMatch:1;
		uint32_t pmItemIndexId:31;
		std::array<std::unique_ptr<Node>, 4> children{{0,0,0,0}};
	};
	//root node has only 2 children
	std::unique_ptr<Node> m_root;
};

class HtmCqrTree {
public:
	struct NodeBase {
		enum Flags { F_NO_CHILD=std::numeric_limits<uint32_t>::max() };
	};
	struct RootNode: NodeBase {
		uint32_t children[4];
	};
	struct Node: NodeBase {
		uint32_t fullMatch:1;
		//present if fullMatch == 0
		uint32_t pmItemIndexId:31;
		//offsets to children relative to our own
		//and cummulative. 
		uint32_t children[4];
	};
private:
	std::vector<RootNode> m_roots;
	std::vector<Node> m_nodes;
};
	
} //end namespace hic
