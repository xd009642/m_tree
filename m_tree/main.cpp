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
    for (size_t i = 0; i < 15; i++)
    {
        double temp = (double)(rand() % 100);
        if (61 == temp)
            i = i;
        std::cout  << temp << ", ";//std::endl;
        tree.insert(i, std::make_shared<double>(temp));
        std::cout << std::endl << "_______________________________________" << std::endl;
        tree.print(true);
        std::cout << "_______________________________________" << std::endl;     
    }

	return 0;
}