#ifndef M_TREE_H
#define M_TREE_H

#include <functional>
#include <type_traits>
#include <memory>
#include <vector>
#include <array>
#include <algorithm>
#include <random>
#include <map>
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
    
    enum class partition_algorithm
    {
        BALANCED, GEN_HYPERPLANE
    };
    /*
        An M-Tree is a tree that partions elements in metric space so as to minimise the distance between them.

        This M-Tree works given an object to store and a distance function to minimise.

        The distance function must return an unsigned numeric type, can only be zero if the compared values are equal,
        is reflexive and obeys the triangle inequality.

        The template arguments for this class are:
            T: class of reference value o
            C: capacity of a node: how many values it can hold
            R: what the distance function returns
            ID: type for the reference values ID
    */
    template < class T, size_t C = 3, typename R = double, typename ID = int >
    class M_Tree
    {
        ////////////////////////////////////////////////////////////////////////////
        ////			Typedefs and internal structure definitions				////
        ////////////////////////////////////////////////////////////////////////////
        struct tree_node;
        struct routing_object;
        struct leaf_object;

        friend std::array<routing_object, C>;
        friend std::array<leaf_object, C>;

        typedef std::array<leaf_object, C> leaf_set;
        typedef std::array<routing_object, C> route_set;

        /*
        * gets the list of reference objects in the node for routing or leaf objects
        */
        struct get_object_values :public boost::static_visitor<>
        {
            std::vector<std::weak_ptr<T>>& values;
            get_object_values(std::vector<std::weak_ptr<T>>& v) :values(v)
            {	}
			
            template<typename X>
            void operator ()(X x)
            {
                for (size_t i = 0; i < x.size(); i++)
                {
                    if (x[i].value.use_count() > 0)
                    {
                        values.push_back(x[i].value);
                    }
                }
            }
        };

        /*
        * Node in the m-tree, is either a leaf or an internal node
        */
        struct tree_node
        {
            std::weak_ptr<tree_node> parent;
            boost::variant<leaf_set, route_set> data;

            bool leaf_node() const
            {
                return data.type() == typeid(leaf_set);
            }
            bool internal_node() const
            {
                return data.type() == typeid(route_set);
            }
        };

        /*
        * Data for internal node.
        *	
        *	value: reference value for object
        *	covering_tree: tree that this object points to (with value as the roots reference value
        *	covering_radius: radius of sphere centred on reference value, all values in the covering 
        *	tree are within this sphere
        *	distance: distance from parent object
        */
        struct routing_object
        {
            std::weak_ptr<T> value;
            std::shared_ptr<tree_node> covering_tree;
            double covering_radius;
            double distance;

            routing_object() :covering_radius(0), distance(0)
            {}
        };

        /*
        * Data contained in leaf node, owning pointer to the reference value and the object id
        * as well as the distance from the parent centre
        */
        struct leaf_object
        {
            std::shared_ptr<T> value;
            ID id;
            double distance;
        };


    public:
        //Constructors and destructors 
        M_Tree(std::function<R(const T&, const T&)> distanceFunction = std::function<R(const T&, const T&)>());
        ~M_Tree();

        void setDistanceFunction(std::function<R(const T&, const T&)> distanceFunction);

        size_t size() const;
        bool empty() const;

        void insert(ID id, std::shared_ptr<T> t);

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
        void insert(ID id, std::weak_ptr<T> t, std::weak_ptr<tree_node> node);
        void internal_node_insert(ID id, std::weak_ptr<T> t, std::weak_ptr<tree_node> N);
        void leaf_node_insert(ID id, std::weak_ptr<T> t, std::weak_ptr<tree_node> lo);

        void split(ID id, std::weak_ptr<T> t, std::weak_ptr<tree_node> n);
        void promote(std::vector<std::weak_ptr<T>> n, routing_object& o1, routing_object& o2);
        void partition(std::vector<std::weak_ptr<T>> o, routing_object& n1, routing_object& n2);

        //split policy functions
        void minimise_radius(std::vector<std::weak_ptr<T>> t, std::weak_ptr<T> o1, std::weak_ptr<T> o2);
        void minimise_max_radius(std::vector<std::weak_ptr<T>> t, std::weak_ptr<T> o1, std::weak_ptr<T> o2);
        void maximise_distance_lower_bound(std::vector<std::weak_ptr<T>> t, std::weak_ptr<T> o1, std::weak_ptr<T> o2);
        void random(std::vector<std::weak_ptr<T>> t, std::weak_ptr<T> o1, std::weak_ptr<T> o2);
        void sampling(std::vector<std::weak_ptr<T>> t, std::weak_ptr<T> o1, std::weak_ptr<T> o2);
    private:
        std::function<R(const T&, const T&)> d;
        std::shared_ptr<tree_node> root;
        std::map<split_policy, std::function<void(std::vector<std::weak_ptr<T>>, std::weak_ptr<T>, std::weak_ptr<T>)>> split_functions;
        // std::map<split_policy, std::function<(vector then two routing objects, bit of a hassle)> 
        //and really use two of these for promote and partition!?
        split_policy policy;
        partition_algorithm partition_method;
    };
	



    template < class T, size_t C, typename R, typename ID>
    M_Tree<T, C, R, ID>::M_Tree(std::function<R(const T&, const T&)> distanceFunction):
        d(distanceFunction),
        policy(split_policy::M_LB_DIST),
        partition_method(partition_algorithm::BALANCED)
    {
        static_assert(std::is_arithmetic<R>::value, "distance function must return arithmetic type");
		static_assert(C > 1, "Node capacity must be >1");
		root = std::make_shared<tree_node>();

        using namespace std::placeholders;
        split_functions[split_policy::MIN_MAXRAD] = std::bind(&M_Tree<T, C, R, ID>::random, this, _1, _2, _3);
        split_functions[split_policy::M_LB_DIST] = std::bind(&M_Tree<T, C, R, ID>::maximise_distance_lower_bound, this, _1, _2, _3);
	}


    template < class T, size_t C, typename R, typename ID>
    M_Tree<T, C, R, ID>::~M_Tree()
    {

    }

    template < class T, size_t C, typename R, typename ID>
    void M_Tree<T, C, R, ID>::setDistanceFunction(std::function<R(const T&, const T&)> distanceFunction)
    {
        d = distanceFunction;
    }

    template < class T, size_t C, typename R, typename ID>
    void M_Tree<T, C, R, ID>::insert(ID id, std::shared_ptr<T> t)
    {
        if (!root)
        {
            root = std::make_shared<tree_node>();
        }
        insert(id, t, root);
    }

    template < class T, size_t C, typename R, typename ID>
    void M_Tree<T, C, R, ID>::insert(ID id, std::weak_ptr<T> t, std::weak_ptr<tree_node> node)
    {
        if (auto lock = node.lock())
        {
            if (lock->internal_node())
            {
                internal_node_insert(id, t, node);
            }
            else if (lock->leaf_node())
            {
                leaf_node_insert(id, t, node);
            }
        }
    }

    template < class T, size_t C, typename R, typename ID>
    void M_Tree<T, C, R, ID>::internal_node_insert(ID id, std::weak_ptr<T> t, std::weak_ptr<tree_node> N)
    {
        if (auto lock = N.lock())
        {
            BOOST_ASSERT_MSG(lock->internal_node(), "leaf node input into internal_node_insert");

            route_set rs = boost::get<route_set>(lock->data);
            std::array<R, C> distances;
            std::fill(std::begin(distances), std::end(distances), std::numeric_limits<R>::max());
            if (auto t_locked = t.lock())
            {
                for (size_t i = 0; i < C; i++)
                {
                    if (auto temp = rs[i].value.lock())
                    {
                        distances[i] = d(*t_locked, *temp);
                    }
                }
            }
            auto min_router = std::min_element(std::begin(distances), std::end(distances));
            if (*min_router > rs[std::distance(std::begin(distances), min_router)].covering_radius)
            {
                for (size_t i = 0; i < rs.size(); i++)
                {
                    distances[i] -= rs[i].covering_radius;
                }
                min_router = std::min_element(std::begin(distances), std::end(distances));
                rs[std::distance(std::begin(distances), min_router)].covering_radius = *min_router;
            }
            insert(id, t, rs[std::distance(std::begin(distances), min_router)].covering_tree);
        }
    }


    template < class T, size_t C, typename R, typename ID>
    void M_Tree<T, C, R, ID>::leaf_node_insert(ID id, std::weak_ptr<T> t, std::weak_ptr<tree_node> lo)
    {
        if (auto lock = lo.lock())
        {
            BOOST_ASSERT_MSG(lock->leaf_node(), "internal node input into leaf_node_insert");

            leaf_set ls = boost::get<leaf_set>(lock->data);
            for (size_t i = 0; i < ls.size(); i++)
            {
                if (false == ls[i].value)
                {
                    ls[i].value = t.lock();
                    ls[i].id = id;
                    //TODO set distance...?
                    return;
                }
            }
            split(id, t, lo);
        }
    }

    template < class T, size_t C, typename R, typename ID>
    void M_Tree<T, C, R, ID>::split(ID id, std::weak_ptr<T> t, std::weak_ptr<tree_node> n)
    {
        std::vector<std::weak_ptr<T>> n_set;
        n_set.push_back(t);
        if (auto locked = n.lock())
        {
            get_object_values getter(n_set);
            boost::apply_visitor(getter, locked->data);
        }
        routing_object o1, o2;
        promote(n_set, o1, o2);
        partition(n_set, o1, o2);
		
        if (auto nl = n.lock())
        {
            if (nl == root)
            {
                std::shared_ptr<tree_node> new_root = std::make_shared<tree_node>();
                std::array<routing_object, C> temp_array;
                temp_array[0] = o1;
                temp_array[1] = o2;
                new_root->data = std::ref(temp_array);
                root = new_root;
            }
            else
            {
                if (auto parent = nl->parent.lock())
                {
                    bool split_again = true;
                    route_set parent_ros = boost::get<route_set>(parent->data);
                    for (size_t i = 0; i < parent_ros.size(); i++)
                    {
                        if (parent_ros[i].covering_tree == nl)
                        {
                            parent_ros[i] = o1;
                        }
                        else if (false == parent_ros[i].covering_tree)
                        {
                            parent_ros[i] = o2;
                            split_again = false;
                        }
                    }
                    if (split_again)
                    {
                        split(id, t, parent);
                    }
                }
            }
        }
    }

    /*
        Currently just implements random distribution. 
    */
    template < class T, size_t C, typename R, typename ID>
    void M_Tree<T, C, R, ID>::promote(std::vector<std::weak_ptr<T>> n, routing_object& o1, routing_object& o2)
    {
        std::default_random_engine generator;
        std::uniform_int_distribution<int> distribution(0, n.size() - 1);
        int index_1 = distribution(generator);
        o1.value = n[index_1];
        int index_2 = distribution(generator);
        while (index_2 != index_1)
        {
            index_2 = distribution(generator);
        }
        o2.value = n[index_2];
    }

    template < class T, size_t C, typename R, typename ID>
    void M_Tree<T, C, R, ID>::partition(std::vector<std::weak_ptr<T>> o, routing_object& n1, routing_object& n2)
    {
        if (partition_method == partition_algorithm::BALANCED)
        {

        }
        else
        {

        }
    }

    template < class T, size_t C, typename R, typename ID>
    void M_Tree<T, C, R, ID>::minimise_radius(std::vector<std::weak_ptr<T>> t, std::weak_ptr<T> o1, std::weak_ptr<T> o2)
    {

    }

    template < class T, size_t C, typename R, typename ID>
    void M_Tree<T, C, R, ID>::minimise_max_radius(std::vector<std::weak_ptr<T>> t, std::weak_ptr<T> o1, std::weak_ptr<T> o2)
    {

    }

    template < class T, size_t C, typename R, typename ID>
    void M_Tree<T, C, R, ID>::maximise_distance_lower_bound(std::vector<std::weak_ptr<T>> t, std::weak_ptr<T> o1, std::weak_ptr<T> o2)
    {
        double max_distance = 0.0;
        for (int i = 0; i < t.size(); i++)
        {
            if (auto lock_1 = t[i].lock())
            {
                for (int j = 0; j < t.size(); j++)
                {
                    if (auto lock_2 = t[j].lock())
                    {
                        double distance = d(*lock_1, *lock_2);
                        if (distance > max_distance)
                        {
                            max_distance = distance;
                            o1 = lock_1;
                            o2 = lock_2;
                        }
                    }
                }
            }
        }
    }


    template < class T, size_t C, typename R, typename ID>
    void M_Tree<T, C, R, ID>::random(std::vector<std::weak_ptr<T>> t, std::weak_ptr<T> o1, std::weak_ptr<T> o2)
    {
        std::default_random_engine generator;
        std::uniform_int_distribution<int> distribution(0, t.size() - 1);
        int n1_index = distribution(generator);
        int n2_index = distribution(generator);
        while (n1_index == n2_index)
        {
            n2_index = distribution(generator);
        }

        o1 = t[n1_index];
        o2 = t[n2_index];
    }


    template < class T, size_t C, typename R, typename ID>
    void M_Tree<T, C, R, ID>::sampling(std::vector<std::weak_ptr<T>> t, std::weak_ptr<T> o1, std::weak_ptr<T> o2)
    {

    }
}


#endif 