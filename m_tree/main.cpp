#include <iostream>
#include "mtree.h"
#include <cmath>
#include <stdlib.h> //rand is good enough for tests

double l2(const double& a, const double& b)
{
	return std::sqrt(std::pow(a-b, 2));
}


int main()
{
    
	auto l2_dist = std::function<double(const double&, const double&)>(l2);

	mt::M_Tree<double, 3, double> tree = mt::M_Tree<double, 3, double>(l2_dist);
    for (size_t i = 0; i < 10; i++)
    {
        if (i > 5)
            i = i;
        double temp = (double)(rand() % 100);
        std::cout << "inserting " << temp << std::endl;
        tree.insert(i, std::make_shared<double>(temp));
        tree.print();
        std::cout << "_______________________________________" << std::endl;
    }



	return 0;
}