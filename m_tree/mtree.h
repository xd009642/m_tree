#ifndef M_TREE_H
#define M_TREE_H

#include <functional>
#include <type_traits>
#include <memory>
#include <vector>
#include <algorithm>
#include "boost\variant\variant.hpp"
#include "boost\variant\get.hpp"

namespace mt
{
	/*
		Taken from "M-tree: An Efficient Access Method for Similarity Search in Metric Spaces"
		(P. Ciaccia, M. Patella, P. Zezula). Defines the method used when splitting a leaf node up 
		upon reaching capacity.

		Strategies are:

		MIN_RAD : Minimise the sum of covering radii of resulting nodes (most expensive operation)
		MIN_MAXRAD: Minimises the maximum of the two radii
		M_LB_DIST: "Maximum Lower Bound on Distance" only uses precomputed distance values unlike previous methods
		RANDOM: Selects reference objects randomly - fast but naive 
		SAMPLING: Like random but takes multiple random samples with the aim of choosing the best
	*/
	enum class split_policy
	{
		MIN_RAD, MIN_MAXRAD, M_LB_DIST, RANDOM, SAMPLING
	};
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
			std::unique_ptr<T> obj;
			std::vector<std::shared_ptr<Tree_Node>> children;
			R cover_radius;
		};

		struct Leaf_Object
		{
			std::vector<std::unique_ptr<T>> values;
		};

		struct Tree_Node
		{
			std::weak_ptr<Tree_Node> parent;
			boost::variant<std::vector<Routing_Object>, Leaf_Object> data;
			R dist_parent;
		};

	public:
		//Constructors and destructors 
		M_Tree(std::function<R(const T&, const T&)> distanceFunction = std::function<R(const T&, const T&)>(), size_t capacity = 3);
		~M_Tree();

		void setDistanceFunction(std::function<R(const T&, const T&)> distanceFunction);

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
		//insert functions, used to break up functionality or abstract away the implementation
		void insert(const T& t, std::weak_ptr<Tree_Node> N);
		void insert_in_routing_object(const T& t, std::vector<Routing_Object>& r);
		void insert_in_leaf_object(const T& t, std::weak_ptr<Tree_Node> l);

		void split(const T& t, std::weak_ptr<Tree_Node> N);
		//partition
	private:
		std::function<R(const T&, const T&)> d;
		std::shared_ptr<Tree_Node> root;
		size_t leaf_capacity;

	};
	


	template<class T, class R>
	M_Tree<T, R>::M_Tree(std::function<R(const T&, const T&)> distanceFunction, size_t capacity) : 
		d(distanceFunction),
		leaf_capacity(capacity)
	{
		static_assert(std::is_arithmetic<R>::value, "distance function must return arithmetic type");
	}




	template<class T, class R>
	void M_Tree<T, R>::setDistanceFunction(std::function<R(const T&, const T&)> distanceFunction)
	{
		d = distanceFunction;
	}

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
			if (N.get()->data.type() == typeid(std::vector<Routing_Object>))
			{
				insert_in_routing_object(t, boost::get<std::vector<Routing_Object>>(N.get()->data));
			}
			else if (N.get()->data.type() == typeid(Leaf_Object))
			{
				insert_in_leaf_object(t, boost::get<Leaf_Object>(N.get()->data));
			}
			else 
			{
				//covers the edge case where a node exists but for some reason isn't leaf or internal. 
			}
		}
	}

	template<class T, class R>
	void M_Tree<T, R>::insert_in_routing_object(const T& t, std::vector<Routing_Object>& r)
	{
		//Will numeric limits work with all arithmetic types?
		std::vector<R> dists(r.size(), std::numeric_limits<R>::max());
		for (int i = 0; i < r.size(); i++)
		{
			if (false == r[i].obj)
				continue;
			
			R object_distance = d(t, *r[i].obj);
			if (object_distance <= r[i].cover_radius)
				dists[i] = object_distance;
		}
		auto min_elem = std::min_element(std::begin(dists), std::end(dists));
		if (*min_elem == std::numeric_limits<R>::max())
		{
			for (int i = 0; i < r.size(); i++)
			{
				if (false == r[i].obj)
					continue;

				dists[i] = d(t, *r[i].obj) - r[i].cover_radius;
			}
			min_elem = std::min_element(std::begin(dists), std::end(dists));
			r[std::distance(std::begin(dists), min_elem)].cover_radius = *min_elem
		}
		insert(t, r.children[std::distance(std::begin(dists), minN)]);
	}

	template<class T, class R>
	void M_Tree<T, R>::insert_in_leaf_object(const T& t, std::weak_ptr<Tree_Node> n)
	{
		static_assert(l.lock().get()->data.type() == typeid(Leaf_Object), 
			"Routing object inputted into insert_in_leaf_object!");

		Leaf_Object& l = boost::get<Leaf_Object>(n.lock().get()->data);

		if (l.values.size() < leaf_capacity)
		{
			l.values.push_back(std::make_unique<T>(t));
		}
		else
		{
			// need to implement
			split(t, n);
		}
	}

	template<class T, class R>
	void M_Tree<T, R>::split(const T& t, std::weak_ptr<Tree_Node> N)
	{
		//create a new node
		//create two new objects
		//promote
		//partition

		if (root == N.lock())
		{
			//create a new node and set it as the new root and
			//store the new routing objects
		}
		else
		{
			/* Now use the parent routing object to store
			one of the new objects
			
			The second routing object is stored in the parent 
			only if it has free capacity else the level above needs 
			splitting!
			*/
		}
	}
}


#endif 