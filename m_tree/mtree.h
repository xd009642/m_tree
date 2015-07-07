#ifndef M_TREE_H
#define M_TREE_H

#include <functional>
#include <type_traits>
#include <memory>
#include <vector>
#include <array>
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
	template<class T, size_t capacity=3, class R=double>
	class M_Tree
	{
		struct Tree_Node;
		struct Routing_Object;
		struct Leaf_Object;
		friend std::array<Routing_Object, capacity>;

		struct Routing_Object
		{
			Routing_Object(){}
			//object at the centre of the sphere, all children are <= cover_radius away.
			std::weak_ptr<T> obj;
			//std::vector<std::shared_ptr<Tree_Node>> children;
			std::array<std::shared_ptr<Tree_Node>, capacity-1> children;
			R cover_radius;
		};

		struct Leaf_Object
		{
			Leaf_Object(){}
			//shared on leaf should be safe as it's build bottom up
			std::array<std::shared_ptr<T>, capacity> values;
			//std::vector<std::unique_ptr<T>> values;
		};

		struct Tree_Node
		{
			Tree_Node() :dist_parent(0),data(Leaf_Object()){}
			Tree_Node(R distance) :dist_parent(distance), data(std::array<Routing_Object, capacity>()){}
			std::weak_ptr<Tree_Node> parent;
			boost::variant<std::array<Routing_Object, capacity>, Leaf_Object> data;
			//	boost::variant<std::vector<Routing_Object>, Leaf_Object> data;
			R dist_parent;

			const bool leaf_node()
			{
				return data.type() == typeid(Leaf_Object);
			}
			const bool internal_node()
			{
				return data.type() == typeid(std::array<Routing_Object, capacity>);
			}
		};
	public:
		//Constructors and destructors 
		M_Tree(std::function<R(const T&, const T&)> distanceFunction = std::function<R(const T&, const T&)>());
		~M_Tree();

		void setDistanceFunction(std::function<R(const T&, const T&)> distanceFunction);

		size_t size() const;
		bool empty() const;

		void insert(std::shared_ptr<T> t);

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
		void insert(std::shared_ptr<T>t, std::weak_ptr<Tree_Node> N);
		void insert_in_routing_object(std::shared_ptr<T> t, std::array<Routing_Object, capacity>& r);
		void insert_in_leaf_object(std::shared_ptr<T> t, std::shared_ptr<Tree_Node> l);

		void split(std::shared_ptr<T> t, std::weak_ptr<Tree_Node> N);
		void promote(std::vector<std::weak_ptr<T>> n, Routing_Object& o1, Routing_Object& o2);
		void partition(std::vector<std::weak_ptr<T>> o, Routing_Object& n1, Routing_Object& n2);
	private:
		std::function<R(const T&, const T&)> d;
		std::shared_ptr<Tree_Node> root;
		split_policy policy;
	//	size_t capacity;

	};
	

	template<class T, size_t capacity, class R>
	M_Tree<T, capacity, R>::M_Tree(std::function<R(const T&, const T&)> distanceFunction):
		d(distanceFunction),
		policy(split_policy::M_LB_DIST)
	{
		static_assert(std::is_arithmetic<R>::value, "distance function must return arithmetic type");
		static_assert(capacity > 1, "Node capacity must be >1");
		root = std::make_shared<Tree_Node>();
	}


	template<class T, size_t capacity, class R>
	void M_Tree<T, capacity, R>::setDistanceFunction(std::function<R(const T&, const T&)> distanceFunction)
	{
		d = distanceFunction;
	}

	template<class T, size_t capacity, class R>
	M_Tree<T, capacity, R>::~M_Tree()
	{

	}

	template<class T, size_t capacity, class R>
	void M_Tree<T, capacity, R>::insert(std::shared_ptr<T> t)
	{
		if (!root)
			root = std::make_shared<Tree_Node>();
		insert(t, root);
	}

	template<class T, size_t capacity, class R>
	void M_Tree<T, capacity, R>::insert(std::shared_ptr<T> t, std::weak_ptr<Tree_Node> N)
	{
		if (auto locked = N.lock())
		{
			if (locked->internal_node())
			{
				insert_in_routing_object(t, boost::get<std::array<Routing_Object, capacity>>(locked->data));
			}
			else if (locked->leaf_node())
			{
				insert_in_leaf_object(t, locked);
			}
		}
	}

	template<class T, size_t capacity, class R>
	void M_Tree<T, capacity, R>::insert_in_routing_object(std::shared_ptr<T> t, std::array<Routing_Object, capacity>& r)
	{
		//Will numeric limits work with all arithmetic types?
		std::array<R, capacity> dists;
		std::fill(std::begin(dists), std::end(dists), std::numeric_limits<R>::max());
		for (size_t i = 0; i < r.size(); i++)
		{
			if (auto locked =r[i].obj.lock())
			{
				R object_distance = d(*t, *locked);
				if (object_distance <= r[i].cover_radius)
					dists[i] = object_distance;
			}
		}
		auto min_elem = std::min_element(std::begin(dists), std::end(dists));
		if (*min_elem == std::numeric_limits<R>::max())
		{
			for (size_t i = 0; i < r.size(); i++)
			{
				if (auto locked = r[i].obj.lock())
				{

					dists[i] = d(*t, *locked) - r[i].cover_radius;
				}
			}
			min_elem = std::min_element(std::begin(dists), std::end(dists));
			r[std::distance(std::begin(dists), min_elem)].cover_radius = *min_elem;
		}
//		insert(t, r[std::distance(std::begin(dists), min_elem)]);
	}



	template<class T, size_t capacity, class R>
	void M_Tree<T, capacity, R>::insert_in_leaf_object(std::shared_ptr<T> t, std::shared_ptr<Tree_Node> n)
	{
		assert(n->leaf_node());

		Leaf_Object& l = boost::get<Leaf_Object>(n->data);
		for (size_t i = 0; i < l.values.size(); i++)
		{
			if (false == l.values[i])
			{
				l.values[i] = t;
				return;
			}
		}
		split(t, n);	
	}

	template<class T, size_t capacity, class R>
	void M_Tree<T, capacity, R>::split(std::shared_ptr<T>t, std::weak_ptr<Tree_Node> N)
	{
		auto locked = N.lock();

		if (false == locked)
			return;

		bool is_root = (root == locked);
		std::weak_ptr<Tree_Node> split_node = N;
		if (false == is_root)
		{
			split_node = locked->parent;
		}
		std::vector<std::weak_ptr<T>> n_set = { t };

		if (locked->internal_node())
		{
			auto temp = boost::get<std::array<Routing_Object, capacity>>(locked->data);
			for (int i = 0; i < capacity; i++)
			{
				auto temp_shared = temp[i].obj.lock();
				if (temp_shared)
					n_set.push_back(temp_shared);
			}
		}
		else
		{
			auto ns = boost::get<Leaf_Object>(locked->data);
			for (int i = 0; i < capacity; i++)
			{
				std::weak_ptr<T> temp;
				if (ns.values[i])
				{
					temp = ns.values[i];
					n_set.push_back(temp);
				}
			}
		}
		Tree_Node new_node(0);
		Routing_Object ro1, ro2;
		promote(n_set, ro1, ro2);
		partition(n_set, ro1, ro2);
		if (is_root)
		{
			std::shared_ptr<Tree_Node> new_parent = std::make_shared<Tree_Node>();
			std::array<Routing_Object, capacity> temp_array;
			temp_array[0] = ro1;
			temp_array[1] = ro2;
			new_parent->data = std::ref(temp_array);
			root = new_parent;
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

	template<class T, size_t capacity, class R>
	void M_Tree<T, capacity, R>::promote(std::vector<std::weak_ptr<T>> n, Routing_Object& o1, Routing_Object& o2)
	{

	}

	template<class T, size_t capacity, class R>
	void M_Tree<T, capacity, R>::partition(std::vector<std::weak_ptr<T>> o, Routing_Object& n1, Routing_Object& n2)
	{

	}
}


#endif 