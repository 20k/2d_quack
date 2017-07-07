#ifndef MANAGERS_HPP_INCLUDED
#define MANAGERS_HPP_INCLUDED

struct renderable;
struct projectile;

///shared state
///every object has a unique id globally
uint16_t o_id = 0;

template<typename T>
struct object_manager
{
    int16_t system_network_id = -1;

    std::vector<T*> objs;

    template<typename real_type, typename... U>
    T* make_new(U... u)
    {
        T* nt = new real_type(u...);

        nt->object_id = o_id++;

        objs.push_back(nt);

        return nt;
    }

    void destroy(T* t)
    {
        for(int i=0; i<objs.size(); i++)
        {
            if(objs[i] == t)
            {
                objs.erase(objs.begin() + i);
                i--;

                delete t;
                continue;
            }
        }
    }

    void add(T* t)
    {
        objs.push_back(t);
    }

    void rem(T* t)
    {
        for(int i=0; i<objs.size(); i++)
        {
            if(objs[i] == t)
            {
                objs.erase(objs.begin() + i);
                i--;
            }
        }
    }

    void erase_all()
    {
        /*for(auto& i : objs)
        {
            delete i;
        }*/

        objs.clear();
    }

    void cleanup()
    {
        for(int i=0; i<objs.size(); i++)
        {
            if(objs[i]->should_cleanup)
            {
                objs.erase(objs.begin() + i);
                i--;

                continue;
            }
        }
    }

    bool owns(int id, int32_t ownership_class)
    {
        for(auto& i : objs)
        {
            if(i->object_id == id && i->ownership_class == ownership_class)
                return true;
        }

        return false;
    }
};

template<typename T>
struct network_manager_base : virtual object_manager<T>
{
    ///real type is the type to create if we receive a new networked entity
    template<typename manager_type, typename real_type>
    void tick_create_networking(network_state& ns)
    {
        manager_type* type = dynamic_cast<manager_type*>(this);

        ///when reading this, ignore the template keyword
        ///its because this is a dependent type
        ns.template check_create_network_entity<manager_type, real_type>(*type);
    }

    void update_network_entities(network_state& ns)
    {
        for(T* obj : object_manager<T>::objs)
        {
            networkable_host* host_object = dynamic_cast<networkable_host*>(obj);

            if(host_object != nullptr)
            {
                host_object->update(ns, object_manager<T>::system_network_id);
                host_object->process_recv(ns);
            }

            networkable_client* client_object = dynamic_cast<networkable_client*>(obj);

            if(client_object != nullptr)
            {
                client_object->update(ns, object_manager<T>::system_network_id);
                client_object->process_recv(ns);
            }
        }
    }

    virtual ~network_manager_base(){}
};

template<typename T>
struct renderable_manager_base : virtual object_manager<T>
{
    void render(sf::RenderWindow& win)
    {
        for(renderable* r : object_manager<T>::objs)
        {
            r->render(win);
        }
    }
};

struct renderable_manager : virtual renderable_manager_base<renderable>
{

};

template<typename T>
struct collideable_manager_base : virtual object_manager<T>
{
    template<typename U>
    void check_collisions(collideable_manager_base<U>& other)
    {
        for(int i=0; i<object_manager<T>::objs.size(); i++)
        {
            T* my_t = object_manager<T>::objs[i];

            for(int j=0; j<other.objs.size(); j++)
            {
                U* their_t = other.objs[j];

                if(my_t->team != their_t->team && my_t->intersects(their_t))
                {
                    my_t->on_collide(their_t);
                    their_t->on_collide(my_t);
                }
            }
        }
    }
};

struct projectile_manager : virtual renderable_manager_base<projectile>, virtual collideable_manager_base<projectile>
{
    void tick(float dt_s, state& st)
    {
        for(projectile* p : objs)
        {
            p->tick(dt_s, st);
        }
    }
};


#endif // MANAGERS_HPP_INCLUDED
