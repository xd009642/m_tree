#include <iostream>
#include "mtree.h"


int dncr(const std::string& a, const std::string& b)
{
	return static_cast<int>(a == b);
}

std::string dupe(const std::string& a, const std::string& b)
{
	return a+b;
}


int main()
{
	auto distance = std::function<int(const std::string&, const std::string&)>(dncr);
	mt::M_Tree<std::string, int> tree = mt::M_Tree<std::string, int>(distance);

	//auto darn = std::function<std::string(const std::string&, const std::string&)>(dupe);
	//mt::M_Tree<std::string, std::string> badTree= mt::M_Tree<std::string, std::string>(darn);

	return 0;
}