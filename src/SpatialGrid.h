#pragma once
#include <sserialize/utility/refcounting.h>
#include <sserialize/containers/AbstractArray.h>

namespace hic::interface {
	
class SpatialGrid: public sserialize::RefCountObject {
public:
	using PixelId = uint64_t;
	using Level = uint32_t;
	using Size = uint32_t;
	class TreeNode {
		PixelId cellId;
		std::vector< std::unique_ptr<TreeNode> > children; 
	};
	using CellIterator = sserialize::AbstractArrayIterator<PixelId>;
public:
	virtual uint32_t maxLevel() const = 0;
	virtual uint32_t defaultLevel() const = 0;
public:
	virtual PixelId index(double lat, double lon, Level level) const = 0;
	virtual PixelId index(double lat, double lon) const = 0;
	virtual PixelId index(PixelId parent, uint32_t childNumber) const = 0;
	virtual Size childrenCount(PixelId pixel) const = 0;
	virtual std::unique_ptr<TreeNode> tree(CellIterator begin, CellIterator end) const = 0;
protected:
	SpatialGrid() {}
	virtual ~SpatialGrid() {}
};
	
}//end namespace hic::interface
