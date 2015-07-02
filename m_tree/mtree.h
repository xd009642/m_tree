#ifndef M_TREE_H
#define M_TREE_H

#include <functional>
#include <type_traits>
#include <memory>
#include <vector>


namespace mt
{

	template <T>
	class Tree_Node
	{
	public:
		Tree_Node();
		Tree_Node(Tree_Node& t);

		T data;
		Tree_Node* parent; //weak
		std::vector<Tree_Node*> children; //shared
		//siblings?
	};

	/*
		An M-Tree is a tree that partions elements in metric space so as to minimise the distance between them.

		This M-Tree works given an object to store and a distance function to minimise.

		The distance function must return a numeric type.
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

		void insert();
		void clear();
		void erase();

		// pre/in/post order

	protected:
		//split
	private:
		std::shared_ptr<Tree_Node> root;
	};
}
#endif 