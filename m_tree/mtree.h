#ifndef M_TREE_H
#define M_TREE_H

#include <functional>
#include <type_traits>
#include <memory>
#include <vector>


namespace mt
{

	//leaf and internal nodes and parent child distances?
	template <T, R, std::enable_if<std::is_arithmetic<R>>>
	class Tree_Node
	{
	public:
		Tree_Node()
		{

		}

		Tree_Node(const T& t) :data(t), dist_parent(-1), cover_radius(-1)
		{

		}

		Tree_Node(R distance, R radius) : dist_parent(distance), cover_radius(radius)
		{

		}


		R dist_parent; //if no children check distance? Or maybe there is a better TMP method
		R cover_radius;
		T data;
		std::weak_ptr<Tree_Node> parent; //weak
		std::vector<std::shared_ptr<Tree_Node>> children; //shared
		//siblings?
	};

	/*
		An M-Tree is a tree that partions elements in metric space so as to minimise the distance between them.

		This M-Tree works given an object to store and a distance function to minimise.

		The distance function must return an unsigned numeric type, can only be zero if the compared values are equal,
		is reflexive and obeys the triangle inequality.
		*/
	template<T, std::function<R(const T&, const T&)> d, std::enable_if<std::is_arithmetic<R>>>
	class M_Tree
	{
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
	protected:
		//split
		//partition
	private:
		std::shared_ptr<Tree_Node> root;
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
		Tree_Node n(t);

	}

}
#endif 