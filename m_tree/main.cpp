#include <iostream>
#include "mtree.h"
#include <cmath>

double l2(const double& a, const double& b)
{
	return std::sqrt(a*a + b*b);
}


int main()
{
	auto l2_dist = std::function<double(const double&, const double&)>(l2);

	mt::M_Tree<double, 3, double> tree = mt::M_Tree<double, 3, double>(l2_dist);
	tree.insert(std::make_shared<double>(5.0));
	tree.insert(std::make_shared<double>(25.0));
	tree.insert(std::make_shared<double>(3.0));
	tree.insert(std::make_shared<double>(7.0));
	tree.insert(std::make_shared<double>(30.0));

	//auto darn = std::function<std::string(const std::string&, const std::string&)>(dupe);
	//mt::M_Tree<std::string, std::string> badTree= mt::M_Tree<std::string, std::string>(darn);

	return 0;
}