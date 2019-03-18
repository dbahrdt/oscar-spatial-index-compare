#include "HcqrOpTree.h"

namespace hic {

HcqrOpTree::HCQRPtr
HcqrOpTree::Calc::Calc::calc(const Node * node) {
    if (!node) {
        return HCQRPtr();
    }
	switch (node->baseType) {
	case Node::LEAF:
		switch (node->subType) {
		case Node::STRING:
		case Node::STRING_REGION:
		case Node::STRING_ITEM:
		{
			if (!node->value.size()) {
				return HCQR();
			}
			const std::string & str = node->value;
			std::string qstr(str);
			sserialize::StringCompleter::QuerryType qt = sserialize::StringCompleter::QT_NONE;
			qt = sserialize::StringCompleter::normalize(qstr);
			if (node->subType == Node::STRING_ITEM) {
				return m_d->items(qstr, qt);
			}
			else if (node->subType == Node::STRING_REGION) {
				return m_d->regions(qstr, qt);
			}
			else {
				return m_d->complete(qstr, qt);
			}
		}
		case Node::REGION:
			throw sserialize::UnsupportedFeatureException("HcqrOpTree: region");
		case Node::REGION_EXCLUSIVE_CELLS:
			throw sserialize::UnsupportedFeatureException("HcqrOpTree: region exclusive cells");
		case Node::CELL:
			throw sserialize::UnsupportedFeatureException("HcqrOpTree: cell");
		case Node::CELLS:
			throw sserialize::UnsupportedFeatureException("HcqrOpTree: cells");
		case Node::RECT:
			throw sserialize::UnsupportedFeatureException("HcqrOpTree: rectangle");
		case Node::POLYGON:
			throw sserialize::UnsupportedFeatureException("HcqrOpTree: polygon");
		case Node::PATH:
			throw sserialize::UnsupportedFeatureException("HcqrOpTree: path");
		case Node::POINT:
			throw sserialize::UnsupportedFeatureException("HcqrOpTree: point");
		case Node::ITEM:
			throw sserialize::UnsupportedFeatureException("HcqrOpTree: item");
		default:
			break;
		};
		break;
	case Node::UNARY_OP:
		switch(node->subType) {
		case Node::FM_CONVERSION_OP:
		    {
                auto child = calc(node->children.at(0));
                if (child) {
                    return child->allToFull();
                }
                else {
                    return child;
                }
            }
		case Node::CELL_DILATION_OP:
			throw sserialize::UnsupportedFeatureException("HcqrOpTree: cell dilation");
		case Node::REGION_DILATION_OP:
			throw sserialize::UnsupportedFeatureException("HcqrOpTree: region dilation");
		case Node::COMPASS_OP:
			throw sserialize::UnsupportedFeatureException("HcqrOpTree: compass");
		case Node::IN_OP:
			throw sserialize::UnsupportedFeatureException("HcqrOpTree: in");
		case Node::NEAR_OP:
			throw sserialize::UnsupportedFeatureException("HcqrOpTree: near");
		case Node::RELEVANT_ELEMENT_OP:
			throw sserialize::UnsupportedFeatureException("HcqrOpTree: relevant item");
		case Node::QUERY_EXCLUSIVE_CELLS:
			throw sserialize::UnsupportedFeatureException("HcqrOpTree: query exclusive cells");
		default:
			break;
		};
		break;
	case Node::BINARY_OP:
		switch(node->subType) {
		case Node::SET_OP:
        {
            auto firstOperand = calc(node->children.front());
            auto secondOperand = calc(node->children.back());
			switch (node->value.at(0)) {
			case '+':
            {
				if (firstOperand && secondOperand) {
                    return *firstOperand + *secondOperand;
                }
                else if (firstOperand) {
                    return firstOperand;
                }
                else if (secondOperand) {
                    return secondOperand;
                }
                else {
                    return HCQRPtr();
                }
            }
            case '/':
			case ' ':
				if (firstOperand && secondOperand) {
                    return *firstOperand / secondOperand;
                }
                else {
                    return HCQRPtr();
                }
			case '-':
                if (firstOperand && secondOperand) {
                    return *firstOperand - *secondOperand;
                }
                else {
                    return firstOperand;
                }
			case '^':
				throw sserialize::UnsupportedFeatureException("HcqrOpTree: symmetric difference");
			default:
				return HCQRPtr();
			};
        }
			break;
		case Node::BETWEEN_OP:
			throw sserialize::UnsupportedFeatureException("HcqrOpTree: between query");
		default:
			break;
		};
		break;
	default:
		break;
	};
	return HCQRPtr();
}

}//end namespace hic