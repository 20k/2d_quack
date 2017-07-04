#ifndef MANAGERS_HPP_INCLUDED
#define MANAGERS_HPP_INCLUDED

struct renderable;
struct projectile;

template<typename T>
struct object_manager
{
    std::vector<T*> objs;

    template<typename real_type, typename... U>
    T* make_new(U... u)
    {
        T* nt = new real_type(u...);

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

struct projectile_manager : virtual renderable_manager_base<projectile>
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
