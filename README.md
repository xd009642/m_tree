# m_tree

## Synopsis

Implementation of an metric-tree data structure. M-trees are a structure designed to group together objects in a generic metric space given a distance function that satisifies some constraints. They are designed to allow for fast range queries and nearest neighbour queries for objects such as images with a high number of features making R-trees or other alternatives infeasible.

This project is current on going currently implemented are:

* Insertion, including the common split and promote functions.
* Range queries
* Nearest Neighbour queries

On the todo list are:

* Bulk loading
* Statistics (e.g. fat factor)
* Optimisation/thinning of tree
* Deletion
* Tests

## Code Example

Here is a simple example showing insertion and querying of an m-tree.

```cpp
//template arguments are: object reference, node capacity, distance function return value and id type
//distance function is an std::function object and examples will be in a the tests
auto tree = mt::m_tree<double, 3, double, size_t>(distance_function);
std::vector<size_t> insertions;
//insertion
for(size_t i=0; i<10; i++)
{
    insertions.push_back(std::make_shared<double>(static_cast<double>(rand()%100)));
    tree.insert(i, insertions.back());
}

//queries - these return a list of object ids in this case the index

//k nearest neighbour query
std::vector<size_t> knn = tree.knn_query(60, 3);
for(size_t i: knn)
{
    std::cout<<insertions[i]<<"\t";
}
//range query
std::vector<size_t> range = tree.range_query(40, 10);
for(size_t i: range)
{
    std::cout<<insertions[i]<<"\t";
}
std::cout<<std::endl;
```

## Installation

This is a single header library with a dependency on boost::variant. 

## API Reference

To be continued.

## License

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.
