#include "mtree.h"
#include <iostream>

int fn(const int& a, const int& b)
{
	return abs(a - b);
}

int dn(std::string a, std::string b)
{
	return static_cast<int>(a == b);
}

int main()
{

	mt::M_Tree<std::string, int > tree;
	tree.clear();
	return 0;
}