#ifndef SYSTEMS_HPP_INCLUDED
#define SYSTEMS_HPP_INCLUDED

#include <SFML/Graphics.hpp>
#include <vec/vec.hpp>
#include <imgui/imgui.h>

#define GRAVITY_STRENGTH 1600.f
#define FORCE_MULTIPLIER 1.f

struct state;

struct base_class
{
    bool should_cleanup = false;
    int16_t object_id = -1;
    int16_t ownership_class = -1;

    virtual void on_cleanup(state& st) {}
};

struct renderable
{
    sf::Image img;
    sf::Texture tex;
    vec3f col = {1, 1, 1};

    bool should_render = true;

    renderable()
    {
        img.create(20, 20, sf::Color(255, 255, 255));
        tex.loadFromImage(img);

        generate_colour();
    }

    virtual void render(sf::RenderWindow& win, vec2f pos)
    {
        if(!should_render)
            return;

        sf::Sprite spr(tex);
        spr.setOrigin(tex.getSize().x/2, tex.getSize().y/2);
        spr.setPosition({pos.x(), pos.y()});
        spr.setColor(sf::Color(255 * col.x(), 255 * col.y(),255 * col.z()));

        win.draw(spr);
    }

    void generate_colour()
    {
        float ffrac = 0.7f;

        col = randf<3, float>() * ffrac + (1.f - ffrac);
    }

    virtual void render(sf::RenderWindow& win) = 0;

    virtual ~renderable()
    {

    }
};

namespace collide
{
    enum type
    {
        NONE,
        RAD,
        PHYS_LINE,
    };
}

using collide_t = collide::type;

struct collideable : virtual base_class
{
    int team = 0;

    bool fully_init = false;
    uint32_t collision_state = 0;
    vec2f last_collision_pos;
    vec2f collision_pos;
    vec2f collision_dim = {2, 2};

    collide_t type;

    collideable(int t, collide_t _type)
    {
        team = t;
        type = _type;
    }

    virtual bool can_collide() {return true;}

    virtual void on_collide(state& st, collideable* other) {}

    virtual bool intersects(collideable* other)
    {
        //if (RectA.Left < RectB.Right && RectA.Right > RectB.Left &&
        //RectA.Top > RectB.Bottom && RectA.Bottom < RectB.Top )

        /*vec2f mhdim = collision_dim/2.f;
        vec2f thdim = other->collision_dim/2.f;

        vec2f Atl = collision_pos - mhdim;
        vec2f Abr = collision_pos + mhdim;

        vec2f Btl = other->collision_pos - thdim;
        vec2f Bbr = other->collision_pos + thdim;

        std::cout << collision_pos << " " << other->collision_pos << std::endl;
        std::cout << Atl << " " << Abr << " " << Btl << " " << Bbr << std::endl;

        if(Atl.x() < Bbr.x() && Abr.x() > Btl.x() && Atl.y() > Bbr.y() && Abr.y() < Btl.y())
        {
            std::cout << "hello\n";

            return true;
        }

        return false;*/

        if(type == collide::RAD && other->type == collide::RAD)
        {
            vec2f diff = collision_pos - other->collision_pos;

            if(diff.length() < collision_dim.length()/2.f || diff.length() < other->collision_dim.length()/2.f)
                return true;

            return false;
        }

        if(type == collide::RAD && other->type == collide::PHYS_LINE)
            return other->intersects(this);

        /*if(type == collide::PHYS_LINE && other->type == collide::RAD)
        {
            return
        }*/

        printf("unsupported collider type %i\n", type);

        return false;
    }

    void set_collision_pos(vec2f pos)
    {
        last_collision_pos = collision_pos;
        collision_pos = pos;
    }

    void init_collision_pos(vec2f pos)
    {
        last_collision_pos = pos;
        collision_pos = pos;
    }

    virtual ~collideable()
    {

    }
};

struct moveable : virtual base_class
{
    vec2f pos;

    bool has_default = false;
    bool on_default_side = false;
    float side_time = 0.f;
    float side_time_max = 0.100f;
};

struct jetpackable : virtual base_class
{
    float flight_time_max = 1.f;
    float flight_time_left = flight_time_max;

    float regen_flight_time = 3.f;

    float force = GRAVITY_STRENGTH * 1.5;

    bool should_jetpack = false;

    vec2f activate(float dt_s)
    {
        vec2f ret;

        flight_time_left -= dt_s;

        if(flight_time_left > 0)
        {
            ret.y() = -1;
        }

        flight_time_left = clamp(flight_time_left, 0.f, flight_time_max);

        return ret * force;
    }

    void idle(float dt_s)
    {
        flight_time_left += dt_s;

        flight_time_left = clamp(flight_time_left, 0.f, flight_time_max);
    }

    vec2f tick(float dt_s)
    {
        if(should_jetpack)
        {
            should_jetpack = false;

            return activate(dt_s);
        }
        else
        {
            idle(dt_s);

            return {0, 0};
        }
    }

    void render_ui()
    {
        ImGui::Begin("Jetpack UI");

        float flight_frac = flight_time_left / flight_time_max;

        constexpr int num_divisions = 10;

        float num_filled = floor(flight_frac * num_divisions);

        float extra = flight_frac * num_divisions - num_filled;

        float vals[num_divisions];

        for(int i=0; i<num_divisions; i++)
        {
            if(i < num_filled)
            {
                vals[i] = 1.f;
            }
            else if(i == num_filled)
            {
                vals[i] = extra;
            }
            else
            {
                vals[i] = 0;
            }
        }

        //ImGui::PlotHistogram("Jetpack", &flight_frac, 1, 0, nullptr, 0, 1);
        ImGui::PlotHistogram("", vals, num_divisions, 0, nullptr, 0, 1, ImVec2(100, 20));

        ImGui::End();
    }
};

///make this inherit from network stuff
struct grappling_hookable : virtual renderable
{
    bool hooking = false;
    vec2f destination;
    vec2f source;
    float cur_hook_dist = 0.f;

    float max_hook_dist = 300.f;

    bool can_hook(vec2f dest, vec2f src) const
    {
        return (dest - src).length() < max_hook_dist;
    }

    void hook(vec2f dest, vec2f src)
    {
        destination = dest;
        source = src;

        cur_hook_dist = (dest - src).length();

        //std::cout << "hook dist " << cur_hook_dist << std::endl;

        hooking = true;
    }

    void unhook()
    {
        hooking = false;
    }

    void update_current_pos(vec2f pos)
    {
        source = pos;
    }

    vec2f apply_constraint(vec2f p1, vec2f anchor) const
    {
        float len = (anchor - p1).length();

        if(len <= cur_hook_dist)
            return p1;

        float extra = len - cur_hook_dist;

        vec2f to_anchor = (anchor - p1).norm();

        vec2f constrained = p1 + to_anchor * extra;

        return constrained;
    }

    vec2f apply_constraint(vec2f p1)
    {
        return apply_constraint(p1, destination);
    }

    virtual void render(sf::RenderWindow& win)
    {
        if(!hooking)
            return;

        sf::RectangleShape shape;

        shape.setSize({(destination - source).length(), 2.f});
        shape.setOrigin(0, 1);

        shape.setPosition(source.x(), source.y());
        shape.setRotation(r2d((destination - source).angle()));

        win.draw(shape);

        sf::CircleShape circle;
        circle.setRadius(10.f);

        circle.setPosition(destination.x(), destination.y());
        circle.setPosition(source.x(), source.y());
        //win.draw(circle);
    }
};

#endif // SYSTEMS_HPP_INCLUDED
