#ifndef NETWORKABLE_SYSTEMS_HPP_INCLUDED
#define NETWORKABLE_SYSTEMS_HPP_INCLUDED

#include "networking.hpp"

struct damageable_base : virtual network_serialisable
{
    float hp = 1.f;
    float max_hp = 1.f;

    float pending_network_damage = 0.f;

    virtual void damage(float amount)
    {
        pending_network_damage += amount;

        hp -= amount;

        hp = std::max(hp, 0.f);
    }

    float get_hp()
    {
        return hp;
    }

    void reset_hp()
    {
        hp = max_hp;
    }

    bool dead()
    {
        return hp <= 0.f;
    }

    virtual ~damageable_base()
    {

    }

    virtual byte_vector serialise_network() override
    {
        byte_vector vec;
        vec.push_back<float>(pending_network_damage);
        vec.push_back<int32_t>(0);

        pending_network_damage = 0.f;

        return vec;
    }

    virtual void deserialise_network(byte_fetch& fetch) override
    {
        //float pending_damage = fetch.get<float>();

        float found_data = fetch.get<float>();

        int32_t type = fetch.get<int32_t>();

        if(type == 0)
            hp -= found_data;
        if(type == 1)
            hp = found_data;
    }
};

struct damageable_client : virtual damageable_base, virtual networkable_client
{
    virtual void damage(float amount) override
    {
        damageable_base::damage(amount);

        should_update = true;
    }
};

struct damageable_host : virtual damageable_base, virtual networkable_host
{
    damageable_host(network_state& ns) : networkable_host(ns) {}

    virtual byte_vector serialise_network() override
    {
        byte_vector ret;
        ret.push_back<float>(hp);
        ret.push_back<int32_t>(1);

        return ret;
    }

    /*virtual void deserialise_network(byte_fetch& fetch) override
    {
        ///received from a client
        float pending_damage = fetch.get<float>();

        damage(pending_damage);
    }*/
};


///Ok. On any projectile collision, client or host, we need to spawn the explosion graphic
///Only host wants to do collision detection
struct projectile_base : virtual renderable, virtual collideable, virtual base_class, virtual network_serialisable
{
    vec2f pos;
    int type = 0;
    float rad = 2.f;

    virtual void tick(float dt_s, state& st) {}

    projectile_base(int team) : collideable(team, collide::RAD)
    {
        collision_dim = {rad*2, rad*2};
    }

    projectile_base() : collideable(-1, collide::RAD) {}

    virtual void set_owner(int id) override
    {
        network_serialisable::set_owner(id);

        team = id;
    }

    void render(sf::RenderWindow& win) override
    {
        sf::CircleShape shape;
        shape.setRadius(rad);

        shape.setOrigin(rad, rad);

        shape.setPosition(pos.x(), pos.y());

        win.draw(shape);
    }

    virtual byte_vector serialise_network() override
    {
        byte_vector vec;

        vec.push_back<vec2f>(pos);
        vec.push_back<int32_t>(should_cleanup);

        return vec;
    }

    virtual void on_collide(state& st, collideable* other)
    {

    }

    virtual ~projectile_base() {}
};

struct explosion_projectile_base : virtual projectile_base, virtual networkable_none
{
    std::map<collideable*, bool> hit;
    float alive_time = 0.f;
    float alive_time_max = 0.15f;

    explosion_projectile_base() : projectile_base(-3), collideable(-3, collide::RAD)
    {
        rad = 10.f;

        collision_dim = {rad * 2.f, rad * 2.f};
    }

    virtual void tick(float dt_s, state& st) override
    {
        alive_time += dt_s;

        if(alive_time > alive_time_max)
        {
            should_cleanup = true;
        }

        set_collision_pos(pos);
    }

    virtual byte_vector serialise_network() override
    {
        return byte_vector();
    }


    virtual void on_cleanup(state& st) {}

    virtual void process_recv(network_state& st)
    {

    }

    virtual ~explosion_projectile_base(){}
};

struct explosion_projectile_client : virtual explosion_projectile_base
{
    explosion_projectile_client() : explosion_projectile_base(), collideable(-3, collide::RAD) {}

    virtual ~explosion_projectile_client(){}
};

struct explosion_projectile_host : virtual explosion_projectile_base
{
    explosion_projectile_host() : explosion_projectile_base(),collideable(-3, collide::RAD) {}

    virtual void on_collide(state& st, collideable* other)
    {
        if(hit[other])
            return;

        hit[other] = true;

        if(dynamic_cast<damageable_base*>(other) != nullptr)
        {
            dynamic_cast<damageable_base*>(other)->damage(0.35);
        }
    }

    virtual ~explosion_projectile_host(){}
};

struct projectile : virtual projectile_base, virtual networkable_client
{
    bool have_pos = false;

    projectile(int team) : projectile_base(team), collideable(team, collide::RAD) {}

    projectile() : collideable(-1, collide::RAD) {}

    virtual void deserialise_network(byte_fetch& fetch) override
    {
        vec2f fpos = fetch.get<vec2f>();
        should_cleanup = fetch.get<int32_t>();

        pos = fpos;

        if(!have_pos)
        {
            collideable::init_collision_pos(pos);

            have_pos = true;
        }

        set_collision_pos(pos);
    }

    virtual void on_cleanup(state& st) override;

    virtual ~projectile(){}
};

struct host_projectile : virtual projectile_base, virtual networkable_host
{
    vec2f dir;
    float speed = 1.f;

    host_projectile(int team, network_state& ns) : projectile_base(team), collideable(team, collide::RAD), networkable_host(ns) {}

    virtual void on_collide(state& st, collideable* other) override
    {
        projectile_base::on_collide(st, other);

        should_cleanup = true;

        if(dynamic_cast<damageable_base*>(other) != nullptr)
        {
            dynamic_cast<damageable_base*>(other)->damage(0.6);
        }
    }

    void tick(float dt_s, state& st) override
    {
        #ifdef PROJECTILE_GRAVITY
        dir += (vec2f){0, 1} * GRAVITY_STRENGTH * dt_s;
        #endif

        pos = pos + dir * dt_s * speed;

        set_collision_pos(pos);
    }

    virtual void deserialise_network(byte_fetch& fetch) override
    {
        vec2f fpos = fetch.get<vec2f>();
        should_cleanup = fetch.get<int32_t>();

        //pos = fpos;
    }

    virtual void on_cleanup(state& st) override;

    virtual ~host_projectile() {}
};


#endif // NETWORKABLE_SYSTEMS_HPP_INCLUDED
