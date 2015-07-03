#include "mtree.h"


int fn(const int& a, const int& b)
{
	return abs(a - b);
}

int main()
{
	
	mt::M_Tree<int, int> tree(std::function<int(const int&, const int&)>(fn));
	return 0;
}