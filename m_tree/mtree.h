#ifndef M_TREE_H
#define M_TREE_H

#include <functional>
#include <type_traits>
#include <memory>
#include <vector>
#include <algorithm>
#include "boost\variant\variant.hpp"

namespace mt
{


	/*
		An M-Tree is a tree that partions elements in metric space so as to minimise the distance between them.

		This M-Tree works given an object to store and a distance function to minimise.

		The distance function must return an unsigned numeric type, can only be zero if the compared values are equal,
		is reflexive and obeys the triangle inequality.
		*/
	template<class T, class R=double>
	class M_Tree
	{
		struct Tree_Node;
		struct Routing_Object;
		struct Leaf_Object;
		struct Routing_Object
		{
			//object at the centre of the sphere, all children are <= cover_radius away.
			std::weak_ptr<T> obj;
			std::vector<std::shared_ptr<Tree_Node>> children;
			R cover_radius;
		};

		struct Leaf_Object
		{
			std::vector<std::weak_ptr<T>> values;
		};

		struct Tree_Node
		{
			std::weak_ptr<Tree_Node> parent;
			boost::variant<Routing_Object, Leaf_Object> data;
			R dist_parent;
		};

	public:
		M_Tree(std::function<R(const T&, const T&)> distanceFunction) :d(distanceFunction)
		{

		}
	/*	M_Tree()
		{
			static_assert(std::is_arithmetic<R>::value, "distance function must return arithmetic type");
		}*/

		~M_Tree();

		size_t size() const;
		bool empty() const;

		void insert(const T& t);

		void clear()
		{
			//Need to ensure that the children are deleted...
			root.reset();
		}

		void erase();

		// pre/in/post order
		//begin end

		//range and nearest neighbour searches
		std::vector<T> range_query(const T& ref, R range);
		std::vector<T> knn_query(const T& ref, int k);
	protected:

		void insert(const T& t, std::weak_ptr<Tree_Node> N);
		//split
		//partition
	private:
		std::function<R(const T&, const T&)> d;
		std::shared_ptr<Tree_Node> root;
		size_t leaf_capacity;
	};
	


	template<class T, class R>
	M_Tree<T, R>::~M_Tree()
	{

	}

	template<class T, class R>
	void M_Tree<T, R>::insert(const T& t)
	{
		insert(t, root);
	}

	template<class T, class R>
	void M_Tree<T, R>::insert(const T& t, std::weak_ptr<Tree_Node> N)
	{
		if (N)
		{
			if (N.get()->data.type() == typeid(Routing_Object))
			{
				std::vector<R> dists;
				Routing_Object<T, R>& r = N.get()->data;
				for (int i = 0; i < r.children.size(); i++)
				{
					std::weak_ptr<Tree_Node> nr = r[i].lock();
					if (nr)
					{
						dists.push_back(d(nr.get()->obj, t));
					}
				}
				auto minN=std::min_element(std::begin(dists), std::end(dists));

				if (minN > N.get()->cover_radius)
				{
					//
				}
				
				return insert(t, r.children[std::distance(std::begin(dists), minN)]);
			}
			else if (N.get()->data.type() == typeid(Leaf_Object))
			{
				
			}
			else 
			{
				//covers the edge case where a node exists but for some reason isn't leaf or internal. 
			}
		}
	}

}


#endif 