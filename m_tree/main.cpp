#include <iostream>
#include "mtree.h"


int dncr(const std::string& a, const std::string& b)
{
	return static_cast<int>(a == b);
}


int main()
{
	auto distance = std::function<int(const std::string&, const std::string&)>(dncr);
	mt::M_Tree<std::string, int> tree = mt::M_Tree<std::string, int>(distance);
	tree.clear();
	return 0;
}