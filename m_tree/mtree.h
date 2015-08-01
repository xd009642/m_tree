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
    class m_tree
    {
        ////////////////////////////////////////////////////////////////////////////
        ////            Typedefs and internal structure definitions             ////
        ////////////////////////////////////////////////////////////////////////////
        struct tree_node;
        struct routing_object;
        struct leaf_object;

        friend std::array<routing_object, C>;
        friend std::array<leaf_object, C>;

        typedef std::array<leaf_object, C> leaf_set;
        typedef std::array<routing_object, C> route_set;
        typedef boost::variant<leaf_set, route_set> data_variant;
        typedef std::vector<boost::variant<leaf_object, routing_object>> data_vector;
        typedef std::function<R(const T&, const T&)> distance_function;
        typedef std::function<void(const data_vector&, routing_object& o1, routing_object& o2)> partition_function;

        //Gets data entries in the form of an array of routing or leaf objects
        struct get_data_entries :public boost::static_visitor<>
        {
            data_vector& data;
            get_data_entries(data_vector& data) :data(data){}

            template<class S, class U>
            void operator()(S t, U& data)
            {
                BOOST_ASSERT_MSG(true, "EXPECTED DIFFERENT TYPES");
            }
            
            template<class S, size_t a=C>
            void operator()(std::array<S, a> t)
            {
                for (size_t i = 0; i < a; i++)
                {
                    if (t[i].value.use_count()>0 )
                        data.push_back(t[i]);
                }
            }
        };

        struct get_node_value :public boost::static_visitor < std::shared_ptr<T>>
        {
            template<class X>
            std::shared_ptr<T> operator()(X t)
            {
                return t.reference_value();
            }
        };

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

        struct update_parent :public boost::static_visitor<>
        {
            std::weak_ptr<tree_node> parent;
            distance_function d;
            update_parent(distance_function dist_func) :d(dist_func){}
            
            template<class X>
            void operator()(X x, route_set& routers)
            {
                for (routing_object& r : routers)
                {
                    if (r.covering_tree)
                    {
                        if (auto lock = r.value.lock())
                        {
                            r.distance = d(*lock, *x.reference_value());
                        }
                        r.covering_tree->parent = parent;
                    }
                }
            }
            template<class X, class Y>
            void operator()(X x, Y& y)
            {

            }
        };

        struct save_object_to_set :public boost::static_visitor<>
        {
            R distance;
            template<typename X>
            void operator ()(const X& x, std::array<X, C>& set)
            {
                for (size_t i = 0; i < C; i++)
                {
                    if (set[i].value.use_count() == 0)
                    {
                        set[i] = x;
                        set[i].distance = distance;
                        break;
                    }
                }
            }

            template<typename X, typename Y>
            void operator ()(const X& x, std::array<Y, C>& set)
            {
                BOOST_ASSERT_MSG(typeid(X) != typeid(Y), "Error state entered");
            }
        };

        /*
        * Node in the m-tree, is either a leaf or an internal node
        */
        struct tree_node
        {
            std::weak_ptr<tree_node> parent;
            data_variant data;

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

            std::shared_ptr<T> reference_value()
            {
                return value.lock();
            }
        };

        /*
        * Data contained in leaf node, owning pointer to the reference value and the object id
        * as well as the distance from the parent centre
        */
        struct leaf_object
        {
            std::shared_ptr<T> value;
            ID id;
            R distance;
            leaf_object() :distance(0)
            {}
            std::shared_ptr<T> reference_value()
            {
                return value;
            }
        };


    public:
        //Constructors and destructors 
        m_tree(distance_function dist_func = distance_function());
        ~m_tree();

        void set_distance_function(distance_function dist_func);
        void set_split_policy(split_policy policy);
        void set_partition_algorithm(partition_algorithm algorithm);

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
        std::vector<ID> range_query(const T& ref, R range);
        std::vector<ID> knn_query(const T& ref, int k);

        //Here for debugging purposes
        void print(bool print_distances = false, std::weak_ptr<tree_node> print_node = std::weak_ptr<tree_node>())
        {
            
            std::vector<std::weak_ptr<tree_node>> queue;
            if (print_node.lock())
                queue.push_back(print_node);
            else if (root)
                queue.push_back(root);
            while (false == queue.empty())
            {
                std::shared_ptr<tree_node> current = queue.front().lock();
                queue.erase(queue.begin());
                if (current->internal_node())
                {
                    route_set& ro_array = boost::get<route_set>(current->data);
                    std::cout << "| ";
                    for (size_t i = 0; i < ro_array.size(); i++)
                    {
                        if (i > 0)
                            std::cout << ", ";
                        if (auto lock = ro_array[i].value.lock())
                        {
                            std::cout << *lock;
                            if (print_distances)
                                std::cout<< ":" << ro_array[i].distance;
                            if (ro_array[i].covering_tree)
                                queue.push_back(ro_array[i].covering_tree);
                        }
                        else
                            std::cout << "_";
                    }
                    if (!current->parent.lock())
                        std::cout << " no parent";
                    std::cout << "| " << std::endl;
                }
                else if (current->leaf_node())
                {
                    leaf_set& lo_array = boost::get<leaf_set>(current->data);
                    std::cout << "| ";
                    for (size_t i = 0; i < lo_array.size(); i++)
                    {
                        if (i > 0)
                            std::cout << ", ";
                        if (lo_array[i].value)
                        {
                            std::cout << *lo_array[i].value;
                            if (print_distances)
                                std::cout << ":" << lo_array[i].distance;
                        }
                        else
                        {
                            std::cout << "_";
                        }
                    }
                    if (!current->parent.lock())
                        std::cout << " no parent";
                    std::cout << "| " << std::endl;

                }
            }
        }

    protected:
        //insert functions, used to break up functionality or abstract away the implementation
        void insert(ID id, std::weak_ptr<T> t, std::weak_ptr<tree_node> node);
        void internal_node_insert(ID id, std::weak_ptr<T> t, std::weak_ptr<tree_node> N);
        void leaf_node_insert(ID id, std::weak_ptr<T> t, std::weak_ptr<tree_node> lo);

        //split promote and partition functions
        void split(boost::variant<leaf_object, routing_object>& obj, std::weak_ptr<tree_node> n);
        void promote(const std::vector<boost::variant<leaf_object, routing_object>>& objects, routing_object& o1, routing_object& o2);
        void partition(const data_vector& o, routing_object& n1, routing_object& n2, std::vector<R> distances = std::vector<R>());

        //specific promotion strategies
        void maximise_distance_lower_bound(const data_vector& objects, routing_object& o1, routing_object& o2);
        void minimise_radius(const data_vector& objects, routing_object& o1, routing_object& o2);
        void minimise_max_radius(const data_vector& objects, routing_object& o1, routing_object& o2);
        void random(const data_vector& objects, routing_object& o1, routing_object& o2);
        void sampling(const data_vector& objects, routing_object& o1, routing_object& o2);

        //specific partitioning algorithms
        void balanced_partition(const data_vector& o, std::vector<R> distances, routing_object& n1, routing_object& n2);
        void generalised_partition(const data_vector& o, std::vector<R> distances, routing_object& n1, routing_object& n2);

        void calculate_distance_matrix(const data_vector& n, std::vector<R>& dst);

    private:
        std::function<R(const T&, const T&)> d;
        std::shared_ptr<tree_node> root;
        std::map<split_policy, partition_function> split_functions;
        split_policy policy;
        partition_algorithm partition_method;
    };




    template < class T, size_t C, typename R, typename ID>
    m_tree<T, C, R, ID>::m_tree(distance_function dist_func) :
        d(dist_func),
        policy(split_policy::M_LB_DIST),
        partition_method(partition_algorithm::BALANCED)
    {
        static_assert(std::is_arithmetic<R>::value, "distance function must return arithmetic type");
        static_assert(C > 1, "Node capacity must be >1");
        root = std::make_shared<tree_node>();
    }


    template < class T, size_t C, typename R, typename ID>
    m_tree<T, C, R, ID>::~m_tree()
    {

    }

    template < class T, size_t C, typename R, typename ID>
    void m_tree<T, C, R, ID>::set_distance_function(distance_function dist_func)
    {
        d = dist_func;
    }


    template < class T, size_t C, typename R, typename ID>
    void m_tree<T, C, R, ID>::set_split_policy(split_policy p)
    {
        policy = p;
    }

    template < class T, size_t C, typename R, typename ID>
    void m_tree<T, C, R, ID>::set_partition_algorithm(partition_algorithm algorithm)
    {
        partition_method = algorithm;
    }

    template < class T, size_t C, typename R, typename ID>
    void m_tree<T, C, R, ID>::insert(ID id, std::shared_ptr<T> t)
    {
        if (!root)
        {
            root = std::make_shared<tree_node>();
        }
        insert(id, t, root);
    }

    template < class T, size_t C, typename R, typename ID>
    void m_tree<T, C, R, ID>::insert(ID id, std::weak_ptr<T> t, std::weak_ptr<tree_node> node)
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
    void m_tree<T, C, R, ID>::internal_node_insert(ID id, std::weak_ptr<T> t, std::weak_ptr<tree_node> N)
    {
        if (auto lock = N.lock())
        {
            BOOST_ASSERT_MSG(lock->internal_node(), "leaf node input into internal_node_insert");

            route_set& rs = boost::get<route_set>(lock->data);
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
    void m_tree<T, C, R, ID>::leaf_node_insert(ID id, std::weak_ptr<T> t, std::weak_ptr<tree_node> lo)
    {
        if (auto lock = lo.lock())
        {
            BOOST_ASSERT_MSG(lock->leaf_node(), "internal node input into leaf_node_insert");

            leaf_set& ls = boost::get<leaf_set>(lock->data);
            for (size_t i = 0; i < ls.size(); i++)
            {
                if (false == ls[i].value)
                {
                    if (auto parent = lock->parent.lock())
                    {
                        std::vector<std::weak_ptr<T>> values;
                        get_object_values getter(values);
                        boost::apply_visitor(getter, parent->data);
                        for (size_t j = 0; j < C; j++)
                        {
                            for (size_t k = 0; k < C; k++)
                            {
                                if (values[j].lock() == ls[k].value && nullptr != ls[k].value)
                                {
                                    ls[i].distance = d(*ls[k].value, *t.lock());
                                    j = C; k = C; //quick break out of loop
                                }
                            }
                        }
                    }
                    ls[i].value = t.lock();
                    ls[i].id = id;
                    return;
                }
            }
            leaf_object leaf;
            leaf.id = id;
            leaf.value = t.lock();
            boost::variant<leaf_object, routing_object> temp = leaf;
            split(temp, lo);
        }
    }

    template < class T, size_t C, typename R, typename ID>
    void m_tree<T, C, R, ID>::split(boost::variant<leaf_object, routing_object>& obj, std::weak_ptr<tree_node> n)
    {
        if (auto locked = n.lock())
        {
            data_vector objects;
            objects.push_back(obj);
            get_data_entries getter(objects);
            boost::apply_visitor(getter, locked->data);
            routing_object o1, o2;
            promote(objects, o1, o2);
            if (locked == root)
            {
                std::shared_ptr<tree_node> new_root = std::make_shared<tree_node>();
                std::array<routing_object, C> temp_array;
                o1.covering_tree->parent = new_root;
                o2.covering_tree->parent = new_root;
                temp_array[0] = o1;
                temp_array[1] = o2;
                new_root->data = std::ref(temp_array);
                root = new_root;
            }
            else
            {
                if (auto p_lock = locked->parent.lock())
                {
                    bool split_again = true;
                    route_set& parent_ros = boost::get<route_set>(p_lock->data);
                    for (size_t i = 0; i < parent_ros.size(); i++)
                    {
                        if (parent_ros[i].covering_tree && parent_ros[i].distance == 0)
                        {                            
                            if (auto r_temp = parent_ros[i].value.lock())
                                if (auto l_temp = o2.value.lock())
                                    o2.distance = d(*r_temp, *l_temp);
                        }
                        if (parent_ros[i].covering_tree == locked)
                        {
                            o1.covering_tree->parent = p_lock;
                            parent_ros[i] = o1;
                        }
                        else if (false == parent_ros[i].covering_tree)
                        {

                            o2.covering_tree->parent = p_lock;
                            parent_ros[i] = o2;
                            split_again = false;
                        }
                    }
                    if (split_again)
                    {
                        boost::variant<leaf_object, routing_object> temp=o2;
                        split(temp, p_lock);
                    }
                }
            }
        }
    }


    template < class T, size_t C, typename R, typename ID>
    void m_tree<T, C, R, ID>::promote(const data_vector& objs, routing_object& o1, routing_object& o2)
    {
        switch (policy)
        {
        case split_policy::MIN_MAXRAD:
            minimise_max_radius(objs, o1, o2);
            break;
        case split_policy::MIN_RAD:
            minimise_radius(objs, o1, o2);
            break;
        case split_policy::M_LB_DIST:
            maximise_distance_lower_bound(objs, o1, o2);
            break;
        case split_policy::RANDOM:
            random(objs, o1, o2);
            break;
        case split_policy::SAMPLING:
            sampling(objs, o1, o2);
            break;
        }
    }


    template < class T, size_t C, typename R, typename ID>
    void m_tree<T, C, R, ID>::partition(const data_vector& o, routing_object& n1, routing_object& n2, std::vector<R> distances = std::vector<R>())
    {
        if (distances.empty())
        {
            calculate_distance_matrix(o, distances);
        }
        if (partition_method == partition_algorithm::BALANCED)
        {
            balanced_partition(o, distances, n1, n2);
        }
        else if (partition_method == partition_algorithm::GEN_HYPERPLANE)
        {

        }
    }


    template < class T, size_t C, typename R, typename ID>
    void m_tree<T, C, R, ID>::balanced_partition(const data_vector& o, std::vector<R> distances, routing_object& n1, routing_object& n2)
    {
        using namespace std::placeholders;
        BOOST_ASSERT_MSG(distances.size() == o.size()*o.size(), "NOT ENOUGH DISTANCES");
        auto if_routing = [](boost::variant<leaf_object, routing_object> x, std::weak_ptr<T> y){
            get_node_value getter;
            auto temp = boost::apply_visitor(getter, x);
            return temp == y.lock();
        };
        auto sort_pred = [](const std::pair<size_t, R>& a, const std::pair<size_t, R>& b){
            return a.second < b.second;
        };
        auto remove_id = [](const std::pair<size_t, R>& a, size_t id){
            return a.first == id;
        };

        std::vector<std::pair<size_t, R>> d1(o.size()), d2(o.size());

        size_t n1_index = std::distance(std::begin(o), std::find_if(std::begin(o), std::end(o), std::bind(if_routing, _1, n1.value)));
        size_t n2_index = std::distance(std::begin(o), std::find_if(std::begin(o), std::end(o), std::bind(if_routing, _1, n2.value)));

        BOOST_ASSERT_MSG(n1_index != n2_index, "PROMOTE FUNCTION CHOSE THE SAME OBJECTS");
        for (size_t i = 0; i < o.size(); i++)
        {
            d1[i] = std::make_pair(i, distances[o.size()*n1_index + i]);
            d2[i] = std::make_pair(i, distances[o.size()*n2_index + i]);
        }
        std::sort(std::begin(d1), std::end(d1), sort_pred);
        std::sort(std::begin(d2), std::end(d2), sort_pred);
        size_t x = 0, y = 0;
        data_variant data_1, data_2;
        if (o[0].type() == typeid(leaf_object))
        {
            data_1 = leaf_set();
            data_2 = leaf_set();
        }
        else
        {
            data_1 = route_set();
            data_2 = route_set();
        }
        save_object_to_set save_visitor;
        while ((false == d1.empty()) || (false == d2.empty()))
        {
            if (false == d1.empty() && x < C)
            {
                if (n1.covering_radius < d1[0].second)
                    n1.covering_radius = d1[0].second;
                save_visitor.distance = d1[0].second;
                boost::apply_visitor(save_visitor, o[d1[0].first], data_1);
                auto d2_end = std::remove_if(std::begin(d2), std::end(d2), std::bind(remove_id, _1, d1[0].first));
                auto d1_end = std::remove_if(std::begin(d1), std::end(d1), std::bind(remove_id, _1, d1[0].first));
                d1.erase(d1_end, std::end(d1));
                d2.erase(d2_end, std::end(d2));
                x++;
            }
            if(false == d2.empty() && y < C)
            {
                if (n2.covering_radius < d2[0].second)
                    n2.covering_radius = d2[0].second;
                save_visitor.distance = d2[0].second;
                boost::apply_visitor(save_visitor, o[d2[0].first], data_2);
                auto d1_end = std::remove_if(std::begin(d1), std::end(d1), std::bind(remove_id, _1, d2[0].first));
                auto d2_end = std::remove_if(std::begin(d2), std::end(d2), std::bind(remove_id, _1, d2[0].first));
                d1.erase(d1_end, std::end(d1));
                d2.erase(d2_end, std::end(d2));
                y++;
            }
        }
        update_parent parent_visitor(d);
        n1.covering_tree = std::make_shared<tree_node>();
        parent_visitor.parent = n1.covering_tree;
        boost::apply_visitor(parent_visitor, o[n1_index], data_1);
        n1.covering_tree->data = data_1;

        n2.covering_tree = std::make_shared<tree_node>();
        parent_visitor.parent = n2.covering_tree;
        boost::apply_visitor(parent_visitor, o[n2_index], data_2);
        n2.covering_tree->data = data_2;
    }


    template < class T, size_t C, typename R, typename ID>
    void m_tree<T, C, R, ID>::minimise_radius(const data_vector& objects, routing_object& o1, routing_object& o2)
    {
        get_node_value getter;
        R best_cover_radius = std::numeric_limits<R>::max();
        std::vector<R> distance_matrix;
        calculate_distance_matrix(objects, distance_matrix);

        std::pair<routing_object, routing_object> best;

        for (size_t i = 0; i < objects.size(); i++)
        {
            if (auto lock_1 = boost::apply_visitor(getter, objects[i]))
            {
                for (size_t j = i + 1; j < objects.size(); j++)
                {
                    if (auto lock_2 = boost::apply_visitor(getter, objects[j]))
                    {
                        routing_object temp_1, temp_2;
                        temp_1.value = lock_1; temp_2.value = lock_2;
                        partition(objects, o1, o2, distance_matrix); 

                        if (temp_1.covering_radius + temp_2.covering_radius < best_cover_radius)
                        {
                            best = std::make_pair(temp_1, temp_2);
                            best_cover_radius = temp_1.covering_radius + temp_2.covering_radius;
                        }
                    }
                }
            }
        }
        o1 = best.first;
        o2 = best.second;
    }

    template < class T, size_t C, typename R, typename ID>
    void m_tree<T, C, R, ID>::minimise_max_radius(const data_vector& objects, routing_object& o1, routing_object& o2)
    {
        std::pair<routing_object, routing_object> best;
        get_node_value getter;
        R best_cover_radius = std::numeric_limits<R>::max();
        std::vector<R> distance_matrix;
        calculate_distance_matrix(objects, distance_matrix);

        for (size_t i = 0; i < objects.size(); i++)
        {
            if (auto lock_1 = boost::apply_visitor(getter, objects[i]))
            {
                for (size_t j = i + 1; j < objects.size(); j++)
                {
                    if (auto lock_2 = boost::apply_visitor(getter, objects[j]))
                    {
                        routing_object temp_1, temp_2;
                        temp_1.value = lock_1; temp_2.value = lock_2;
                        partition(objects, o1, o2, distance_matrix); 

                        if (std::max(temp_1.covering_radius, temp_2.covering_radius) < best_cover_radius)
                        {
                            best = std::make_pair(temp_1, temp_2);
                            best_cover_radius = std::max(temp_1.covering_radius, temp_2.covering_radius);
                        }
                    }
                }
            }
        }
        o1 = best.first;
        o2 = best.second;
    }

    template < class T, size_t C, typename R, typename ID>
    void m_tree<T, C, R, ID>::random(const data_vector& objects, routing_object& o1, routing_object& o2)
    {
        get_node_value getter;
        std::default_random_engine generator;
        std::uniform_int_distribution<int> distribution(0, objects.size() - 1);
        int n1_index = distribution(generator);
        int n2_index = distribution(generator);
        while (n1_index == n2_index)
        {
            n2_index = distribution(generator);
        }

        o1.value = boost::apply_visitor(getter, objects[n1_index]);
        o2.value = boost::apply_visitor(getter, objects[n2_index]);
        partition(objects, o1, o2);
    }


    template < class T, size_t C, typename R, typename ID>
    void m_tree<T, C, R, ID>::sampling(const data_vector& objects, routing_object& o1, routing_object& o2)
    {
        //This sampling algorithm takes max(2, 0.1*C) samples and chooses the pair of objects that minimise 
        //the covering radius. This value was chosen as it is used in the reference literature and seems sensible
        size_t samples = static_cast<size_t>(std::max(2.0, 0.1*C));
        std::pair<routing_object, routing_object> best_sample;
        R radius_sum = std::numeric_limits<R>::max();
        for (size_t i = 0; i < samples; i++)
        {
            std::pair<routing_object, routing_object> current_sample;
            random(objects, current_sample.first, current_sample.second);
            if (radius_sum > (current_sample.first.covering_radius + current_sample.second.covering_radius))
            {
                radius_sum = (current_sample.first.covering_radius + current_sample.second.covering_radius);
                best_sample = current_sample;
            }
        }
        o1 = best_sample.first;
        o2 = best_sample.second;
    }


    template < class T, size_t C, typename R, typename ID>
    void m_tree<T, C, R, ID>::maximise_distance_lower_bound(const data_vector& objects, routing_object& o1, routing_object& o2)
    {
        get_node_value getter;
        double max_distance = 0.0;
        for (size_t i = 0; i < objects.size(); i++)
        {
            if (auto lock_1 = boost::apply_visitor(getter, objects[i]))
            {
                for (size_t j = 0; j < objects.size(); j++)
                {
                    if (auto lock_2 = boost::apply_visitor(getter, objects[j]))
                    {
                        double distance = d(*lock_1, *lock_2);
                        if (distance > max_distance)
                        {
                            max_distance = distance;
                            o1.value = lock_1;
                            o2.value = lock_2;
                        }
                    }
                }
            }
        }
        partition(objects, o1, o2);
    }

    template < class T, size_t C, typename R, typename ID>
    void m_tree<T, C, R, ID>::calculate_distance_matrix(const data_vector& n, std::vector<R>& dst)
    {
        get_node_value getter;
        dst.resize(n.size() * n.size());
        for (size_t i = 0; i < n.size(); i++)
        {
            if (auto lock_1 = boost::apply_visitor(getter, n[i]))
            {
                for (size_t j = 0; j < n.size(); j++)
                {
                    if (j == i)
                    {
                        dst[n.size()*i + j] = 0;
                    }
                    else if (auto lock_2 = boost::apply_visitor(getter, n[j]))
                    {
                        dst[n.size()*i + j] = d(*lock_1, *lock_2);
                    }
                }
            }
        }
    }
    
    template < class T, size_t C, typename R, typename ID>
    std::vector<ID> m_tree<T, C, R, ID>::range_query(const T& ref, R range)
    {
        get_node_value getter;
        std::vector<std::weak_ptr<tree_node>> queue;
        queue.push_back(root);
        while (false == queue.empty())
        {
            R dist_to_parent = 0;
            if (auto locked = queue[0].lock())
            {
                //implement a distance to parent function here
                if (current->internal_node())
                {
                    route_set& ros = boost::get<route_set>(locked->data);
                    for (size_t i = 0; i < C; i++)
                    {
                        if (auto temp_lock = ros[i].value.lock())
                        {
                            if ((std::abs(dist_to_parent - ros[i].distance) <=
                                range + ros[i].covering_radius) && auto v = ros[i].value.lock())
                            {
                                if (d(ref, *v) < range + ros[i].covering_radius)
                                    queue.push_back(ros[i].covering_tree);
                            }
                        }
                    }
                }
                else if (current->leaf_node())
                {
                    leaf_set& ros = boost::get<leaf_set>(locked->data);
                    for (size_t i = 0; i < C; i++)
                    {

                    }
                }
            }
            queue.erase(std::begin(queue));
        }
    }
}


#endif 