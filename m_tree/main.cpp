#include <iostream>
#include "mtree.h"
#include <cmath>
#include <stdlib.h> //rand is good enough for tests

#include "sandbox.h"


double l2(const double& a, const double& b)
{
	return std::sqrt(std::pow(a-b, 2));
}


int main()
{
    
	auto l2_dist = std::function<double(const double&, const double&)>(l2);

    mt::m_tree<double, 3> tree = mt::m_tree<double, 3>(std::function<double(const double&, const double&)>(l2));
    std::vector<double> entries;
    for (size_t i = 0; i < 15; i++)
    {
        double temp = (double)(rand() % 100);
        entries.push_back(temp);
        tree.insert(i, std::make_shared<double>(temp));
    }
    auto res=tree.range_query(60, 10);
    auto res2 = tree.knn_query(60, 3);
    for (int i : res)
        std::cout << entries[i] << ", ";
    std::cout << std::endl;
    for (auto i : res2)
        std::cout << entries[i.first] << ", ";
    std::cout << std::endl;

    std::sort(std::begin(entries), std::end(entries));
    for (auto i : entries)
        std::cout << i << ", ";
	return 0;
}