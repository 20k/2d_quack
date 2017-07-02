#include <iostream>
#include <vec/vec.hpp>

#include <SFML/Graphics.hpp>
#include "util.hpp"

struct renderable
{
    sf::Image img;
    sf::Texture tex;

    renderable()
    {
        img.create(20, 20, sf::Color(255, 128, 128));
        tex.loadFromImage(img);
    }

    virtual void render(sf::RenderWindow& win, vec2f pos)
    {
        sf::Sprite spr(tex);
        spr.setOrigin(tex.getSize().x/2, tex.getSize().y/2);
        spr.setPosition({pos.x(), pos.y()});

        win.draw(spr);
    }

    virtual void render(sf::RenderWindow& win) = 0;
};

struct renderable_manager
{
    std::vector<renderable*> renderables;

    void add(renderable* r)
    {
        renderables.push_back(r);
    }

    void draw(sf::RenderWindow& win)
    {
        for(renderable* r : renderables)
        {
            r->render(win);
        }
    }
};

struct physics_barrier : renderable
{
    vec2f p1;
    vec2f p2;

    virtual void render(sf::RenderWindow& win)
    {
        sf::RectangleShape rect;

        float width = (p2 - p1).length();
        float height = 5.f;

        rect.setSize({width, height});

        float angle = (p2 - p1).angle();

        rect.setRotation(r2d(angle));

        rect.setPosition(p1.x(), p1.y());

        win.draw(rect);
    }

    int side(vec2f pos)
    {
        vec2f line = (p2 - p1).norm();

        vec2f normal = line.rot(M_PI/2);

        float res = dot(normal.norm(), (pos - (p1 + p2)/2.f).norm());

        if(res > 0)
            return 1;

        return -1;
    }

    static int side(vec2f pos, vec2f pos_1, vec2f pos_2)
    {
        vec2f line = (pos_2 - pos_1).norm();

        vec2f normal = line.rot(M_PI/2);

        float res = dot(normal.norm(), (pos - (pos_1 + pos_2)/2.f).norm());

        if(res > 0)
            return 1;

        return -1;
    }

    bool crosses(vec2f pos, vec2f next_pos)
    {
        int s1 = side(pos);
        int s2 = side(next_pos);

        vec2f normal = get_normal();

        ///nominally
        if(s1 != s2)
        {
            vec2f n1 = p1 + normal;
            vec2f n2 = p2 + normal;

            int sn1 = physics_barrier::side(pos, p1, n1);
            int sn2 = physics_barrier::side(pos, p2, n2);

            if(sn1 != sn2)
            {
                return true;
            }
        }

        return false;
    }

    bool within(vec2f pos)
    {
        vec2f normal = get_normal();

        vec2f n1 = p1 + normal;
        vec2f n2 = p2 + normal;

        int sn1 = physics_barrier::side(pos, p1, n1);
        int sn2 = physics_barrier::side(pos, p2, n2);

        if(sn1 != sn2)
        {
            return true;
        }

        return false;
    }

    vec2f get_normal()
    {
        return (p2 - p1).rot(M_PI/2).norm();
    }

    vec2f get_normal_towards(vec2f pos)
    {
        vec2f rel = (pos - (p1 + p2)/2.f);

        vec2f n_1 = (p2 - p1).norm().rot(M_PI/2.f);
        vec2f n_2 = (p2 - p1).norm().rot(-M_PI/2.f);

        float a1 = angle_between_vectors(n_1, rel);
        float a2 = angle_between_vectors(n_2, rel);

        if(fabs(a1) < fabs(a2))
        {
            return n_1;
        }

        return n_2;
    }
};

struct physics_barrier_manager
{
    std::vector<physics_barrier*> phys;

    bool adding = false;
    vec2f adding_point;

    void add_point(vec2f pos, renderable_manager& renderable_manage)
    {
        if(!adding)
        {
            adding_point = pos;

            adding = true;

            return;
        }

        if(adding)
        {
            vec2f p2 = pos;

            physics_barrier* bar = new physics_barrier;
            bar->p1 = adding_point;
            bar->p2 = p2;

            phys.push_back(bar);
            renderable_manage.add(bar);

            adding = false;

            return;
        }
    }
};

struct character : renderable
{
    vec2f last_pos;
    vec2f pos;
    //vec2f velocity;
    vec2f acceleration;
    vec2f impulse;

    //bool jumped = false;
    float jump_stick_cooldown_cur = 0.f;
    float jump_stick_cooldown_time = 0.05f;

    bool can_jump = false;
    vec2f jump_dir;

    float last_dt = 1.f;

    float jump_cooldown_cur = 0.f;
    float jump_cooldown_time = 0.15f;

    void render(sf::RenderWindow& win) override
    {
        renderable::render(win, pos);
    }

    vec2f reflect_physics(vec2f next_pos, physics_barrier* bar)
    {
        vec2f cdir = (next_pos - pos).norm();
        float clen = (next_pos - pos).length();

        //vec2f ndir = (bar->p2 - bar->p1).norm();

        vec2f ndir = reflect((next_pos - pos).norm(), bar->get_normal());

        vec2f to_line = point2line_shortest(bar->p1, (bar->p2 - bar->p1).norm(), pos);

        pos += to_line - to_line.norm();
        next_pos = ndir * clen + pos;// + to_line - to_line.norm() * 0.25f;

        return next_pos;
    }

    ///instead of line normal use vertical
    vec2f stick_physics(vec2f next_pos, physics_barrier* bar)
    {
        float approx_ish_velocity = (pos - last_pos).length();

        vec2f cdir = (next_pos - pos).norm();
        float clen = (next_pos - pos).length();

        vec2f to_line = point2line_shortest(bar->p1, (bar->p2 - bar->p1).norm(), pos);

        /*float a1 = angle_between_vectors((bar->p2 - bar->p1).norm(), cdir);
        float a2 = angle_between_vectors((bar->p1 - bar->p2).norm(), cdir);*/

        float a1 = angle_between_vectors((bar->p2 - bar->p1).norm(), cdir);
        float a2 = angle_between_vectors((bar->p1 - bar->p2).norm(), cdir);

        //float angle_from_horizontal = (bar->p2 - bar->p1).angle();

        vec2f dir;

        if(fabs(a1) < fabs(a2))
        {
            dir = (bar->p2 - bar->p1).norm();
        }
        else
        {
            dir = (bar->p1 - bar->p2).norm();
        }

        vec2f projected = projection(next_pos - pos, dir);

        pos += to_line - to_line.norm();
        next_pos = projected + pos;
        //next_pos = dir * clen + pos;

        return next_pos;
    }

    vec2f adjust_next_pos_for_physics(vec2f next_pos, physics_barrier_manager& physics_barrier_manage)
    {
        for(physics_barrier* bar : physics_barrier_manage.phys)
        {
            bool did_phys = false;

            if(bar->crosses(pos, next_pos))
            {
                next_pos = stick_physics(next_pos, bar);

                did_phys = true;
            }

            float line_jump_dist = 2;

            vec2f dist = point2line_shortest(bar->p1, (bar->p2 - bar->p1).norm(), next_pos);

            if(dist.length() < line_jump_dist && bar->within(next_pos))
            {
                /*if(!did_phys && jump_stick_cooldown_cur >= jump_stick_cooldown_time)
                {
                    next_pos = stick_physics(next_pos, bar);
                }*/

                can_jump = true;
                jump_dir += bar->get_normal_towards(next_pos);
            }
        }

        return next_pos;
    }

    void tick(float dt, physics_barrier_manager& physics_barrier_manage)
    {
        can_jump = false;
        jump_dir = {0,0};

        //clamp_velocity();

        //if(dt <= 0.000001f)
        //    return;

        do_gravity({0, 1});

        float dt_f = dt / last_dt;

        dt_f = 1.f;

        //float friction = 0.99f;

        vec2f friction = {1.f, 1.f};

        vec2f next_pos = pos + (pos - last_pos) * dt_f * friction + acceleration * dt * dt + impulse;

        float max_speed = 0.85f;

        /*if((next_pos - pos).length() > max_speed)
        {
            next_pos = (next_pos - pos).norm() * max_speed + pos;
        }*/

        //next_pos = adjust_next_pos_for_physics(next_pos, physics_barrier_manage);

        if(fabs((next_pos - pos).x()) > max_speed)
        {
            float diff = (next_pos - pos).x();

            if(diff < 0)
                next_pos.x() = -max_speed + pos.x();
            else
                next_pos.x() = max_speed + pos.x();
        }

        next_pos = adjust_next_pos_for_physics(next_pos, physics_barrier_manage);

        //pos = pos + velocity * dt;

        last_dt = dt;

        last_pos = pos;
        pos = next_pos;

        //std::cout << pos << " " << acceleration << std::endl;

        //std::cout << (pos - last_pos).length() << std::endl;

        acceleration = {0,0};
        impulse = {0,0};

        jump_cooldown_cur += dt;
        jump_stick_cooldown_cur += dt;
    }

    void do_gravity(vec2f dir)
    {
        acceleration += dir * 800.f;
    }

    void set_movement(vec2f dir)
    {
        acceleration += dir * 8.f;
    }

    void jump()
    {
        if(!can_jump)
            return;

        if(jump_cooldown_cur < jump_cooldown_time)
            return;

        impulse += jump_dir.norm() * 0.85f;
        //acceleration += jump_dir.norm() * 200000.f;
        jump_cooldown_cur = 0;
        jump_stick_cooldown_cur = 0.f;

        std::cout << jump_dir << std::endl;
    }
};

struct character_manager
{
    std::vector<character*> characters;

    void add(character* c)
    {
        characters.push_back(c);
    }

    void tick(float dt_s, physics_barrier_manager& physics_barrier_manage)
    {
        for(character* c : characters)
        {
            c->tick(dt_s, physics_barrier_manage);
        }
    }
};


int main()
{
    sf::RenderWindow win;
    win.create(sf::VideoMode(1500, 1000), "fak u mark");

    renderable_manager renderable_manage;
    character_manager character_manage;

    character* test = new character;

    renderable_manage.add(test);
    character_manage.add(test);

    physics_barrier_manager physics_barrier_manage;

    sf::Clock clk;

    sf::Keyboard key;
    sf::Mouse mouse;

    sf::sleep(sf::milliseconds(1));

    while(win.isOpen())
    {
        auto sfml_mpos = mouse.getPosition(win);

        vec2f mpos = {sfml_mpos.x, sfml_mpos.y};

        float dt_s = (clk.restart().asMicroseconds() / 1000.) / 1000.f;

        sf::Event event;

        while(win.pollEvent(event))
        {
            if(event.type == sf::Event::Closed)
            {
                win.close();
            }
        }

        vec2f move_dir;

        float mult = 1000.f;

        if(win.hasFocus())
        {
            move_dir.x() += (int)key.isKeyPressed(sf::Keyboard::D);
            move_dir.x() -= (int)key.isKeyPressed(sf::Keyboard::A);

            move_dir.y() += (int)key.isKeyPressed(sf::Keyboard::S);
            move_dir.y() -= (int)key.isKeyPressed(sf::Keyboard::W);
        }

        if(ONCE_MACRO(sf::Mouse::Left) && win.hasFocus())
        {
            physics_barrier_manage.add_point(mpos, renderable_manage);
        }

        //test->velocity += move_dir * mult;

        test->set_movement(move_dir * mult);

        win.clear();

        character_manage.tick(dt_s, physics_barrier_manage);

        if(ONCE_MACRO(sf::Keyboard::Space) && win.hasFocus())
        {
            test->jump();
        }

        renderable_manage.draw(win);

        win.display();

        sf::sleep(sf::milliseconds(1));
    }

    return 0;
}
