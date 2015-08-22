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
    tree.set_partition_algorithm(mt::partition_algorithm::GEN_HYPERPLANE);
    std::vector<double> entries;

    for (size_t i = 0; i < 10; i++)
    {
        double temp = (double)(rand() % 100);
        if (temp == 45)
            temp = 45;
        entries.push_back(temp);
        tree.insert(i + 1, std::make_shared<double>(temp));


    }
    tree.print(mt::RADIUS);
    auto ff= tree.fat_factor();
    std::cout << std::endl;
    auto res=tree.range_query(60, 10);
    auto res2 = tree.knn_query(60, 3);
    for (int i : res)
        std::cout << entries[i-1] << ", ";
    std::cout << std::endl;
    for (auto i : res2)
    {
        if (i.first > 0)
            std::cout << entries[i.first-1] << ", ";
        else
            std::cout << "_, ";
    }
    std::cout << std::endl;

    std::sort(std::begin(entries), std::end(entries));
    for (auto i : entries)
        std::cout << i << ", ";
	return 0;
}