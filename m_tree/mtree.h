#ifndef M_TREE_H
#define M_TREE_H

#include <functional>
#include <type_traits>
#include <memory>
#include <vector>
#include "boost\variant\variant.hpp"

namespace mt
{


	



	/*
		So internal nodes are represented by:

		{feature value of rooting object, parent point, covering radius, distance to parent}

		and leaf nodes are represented by:
		
		{feature value of DB object, object id, distance to parent}
	
	*/

	/*
		An M-Tree is a tree that partions elements in metric space so as to minimise the distance between them.

		This M-Tree works given an object to store and a distance function to minimise.

		The distance function must return an unsigned numeric type, can only be zero if the compared values are equal,
		is reflexive and obeys the triangle inequality.
		*/
	template<T, std::function<R(const T&, const T&)> d, std::enable_if<std::is_arithmetic<R>>>
	class M_Tree
	{
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
			boost::variant<mt::Routing_Object, mt::Leaf_Node> data;
			R dist_parent;
		};

	public:
		M_Tree();
		M_Tree(M_Tree& tree);
		~M_Tree();

		size_t size() const;
		bool empty() const;

		void insert(const T& t);

		void clear();
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
		std::shared_ptr<Tree_Node> root;
		size_t leaf_capacity;
	};

	template<T, std::function<R(const T&, const T&)> d, std::enable_if<std::is_arithmetic<R>>>
	M_Tree::M_Tree()
	{
		
	}

	template<T, std::function<R(const T&, const T&)> d, std::enable_if<std::is_arithmetic<R>>>
	M_Tree::M_Tree(M_Tree& tree)
	{

	}

	template<T, std::function<R(const T&, const T&)> d, std::enable_if<std::is_arithmetic<R>>>
	M_Tree::~M_Tree()
	{

	}

	template<T, std::function<R(const T&, const T&)> d, std::enable_if<std::is_arithmetic<R>>>
	void M_Tree::insert(const T& t)
	{
		insert(t, root);
	}

	template<T, std::function<R(const T&, const T&)> d, std::enable_if<std::is_arithmetic<R>>>
	void M_Tree::insert(const T& t, std::weak_ptr<Tree_Node> N)
	{
		if (N)
		{
			if (N.get()->data.type() == typeid(Routing_Object))
			{
				std::vector<R> dists;
				for (Routing_Object<T, R>& r : N.get()->data)
				{
					if (r.obj)
					{
						dists.push_back(d(r.obj, t));
					}
				}
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