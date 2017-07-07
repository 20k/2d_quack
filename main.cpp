#include <iostream>
#include <fstream>
#include <vec/vec.hpp>

#include <SFML/Graphics.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui-SFML.h>
#include "util.hpp"
#include <net/shared.hpp>
#include <set>
#include "camera.hpp"
#include "state.hpp"

struct base_class
{
    bool should_cleanup = false;
    int16_t object_id = -1;
    int16_t ownership_class = -1;
};

#include "networking.hpp"

bool suppress_mouse = false;

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

    virtual void on_collide(collideable* other) {}

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

struct projectile_base : virtual renderable, virtual collideable, virtual base_class, virtual network_serialisable
{
    vec2f pos;
    int type = 0;
    float rad = 2.f;

    virtual void tick(float dt_s, state& st) {}

    projectile_base(int team) : collideable(team, collide::RAD)
    {

    }

    projectile_base() : collideable(-1, collide::RAD) {}

    virtual void set_owner(int id)
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

        return vec;
    }

    virtual void deserialise_network(byte_fetch& fetch) override
    {
        vec2f fpos = fetch.get<vec2f>();

        int found_canary = fetch.get<decltype(canary_end)>();

        if(found_canary == canary_end)
            pos = fpos;
    }

    virtual ~projectile_base() {}
};

struct projectile : virtual projectile_base, virtual networkable_client
{
    projectile(int team) : projectile_base(team), collideable(team, collide::RAD) {}

    projectile() : collideable(-1, collide::RAD) {}

    virtual ~projectile(){}
};

struct host_projectile : virtual projectile_base, virtual networkable_host
{
    vec2f dir;
    float speed = 1.f;

    host_projectile(int team, network_state& ns) : projectile_base(team), collideable(team, collide::RAD), networkable_host(ns) {}

    virtual void on_collide(collideable* other) override {should_cleanup = true;}

    void tick(float dt_s, state& st) override
    {
        pos = pos + dir * dt_s;

        set_collision_pos(pos);
        collision_dim = {rad*2, rad*2};
    }

    virtual ~host_projectile() {}
};

#include "managers.hpp"

/*struct tickable
{
    static std::vector<tickable*> tickables;

    void add_tickable(tickable* t)
    {
        tickables.push_back(t);
    }

    virtual void tick(float dt_s, state& st);

    static void tick_all(float dt_s, state& st)
    {
        for(tickable* t : tickables)
        {
            t->tick(dt_s, st);
        }
    }
};*/

struct damageable
{
    float hp = 1.f;
    float max_hp = 1.f;

    void damage(float amount)
    {
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

    virtual ~damageable()
    {

    }
};

struct physics_barrier : virtual renderable, virtual collideable, virtual base_class
{
    vec2f p1;
    vec2f p2;

    physics_barrier() : collideable(-1, collide::PHYS_LINE) {}

    bool intersects(collideable* other)
    {
        if(other->type != collide::RAD)
            return false;

        if(crosses(other->collision_pos, other->last_collision_pos))
        {
            return true;
        }

        return false;
    }

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

    byte_vector serialise()
    {
        byte_vector vec;
        vec.push_back<vec2f>(p1);
        vec.push_back<vec2f>(p2);

        return vec;
    }

    void deserialise(byte_fetch& fetch)
    {
        p1 = fetch.get<vec2f>();
        p2 = fetch.get<vec2f>();
    }
};

struct physics_barrier_manager : virtual renderable_manager_base<physics_barrier>, virtual collideable_manager_base<physics_barrier>
{
    bool adding = false;
    vec2f adding_point;

    void add_point(vec2f pos, state& st)
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

            physics_barrier* bar = make_new<physics_barrier>();
            bar->p1 = adding_point;
            bar->p2 = p2;

            adding = false;

            return;
        }
    }

    byte_vector serialise()
    {
        byte_vector vec;

        for(physics_barrier* bar : objs)
        {
            vec.push_vector(bar->serialise());
        }

        return vec;
    }

    void deserialise(byte_fetch& fetch, int num_bytes)
    {
        objs.clear();

        for(int i=0; i<num_bytes / (sizeof(vec2f) * 2); i++)
        {
            physics_barrier* bar = new physics_barrier;

            bar->deserialise(fetch);

            objs.push_back(bar);
        }
    }
};

struct game_world_manager
{
    int16_t system_network_id = -1;

    int cur_spawn = 0;
    std::vector<vec2f> spawn_positions;

    bool should_render = false;

    void add(vec2f pos)
    {
        spawn_positions.push_back(pos);
    }

    void render(sf::RenderWindow& win)
    {
        if(!should_render)
            return;

        float rad = 8;

        sf::CircleShape circle;
        circle.setRadius(rad);
        circle.setOrigin(rad, rad);
        circle.setFillColor(sf::Color(255, 128, 255));

        for(vec2f& pos : spawn_positions)
        {
            circle.setPosition({pos.x(), pos.y()});
            win.draw(circle);
        }
    }

    void enable_rendering()
    {
        should_render = true;
    }

    void disable_rendering()
    {
        should_render = false;
    }

    vec2f get_next_spawn()
    {
        if(spawn_positions.size() == 0)
            return {0,0};

        vec2f pos = spawn_positions[cur_spawn];

        cur_spawn = (cur_spawn + 1) % spawn_positions.size();

        return pos;
    }

    byte_vector serialise()
    {
        byte_vector ret;

        for(vec2f& pos : spawn_positions)
        {
            ret.push_back<vec2f>(pos);
        }

        return ret;
    }

    void deserialise(byte_fetch& fetch, int num_bytes)
    {
        spawn_positions.clear();

        for(int i=0; i<num_bytes / sizeof(vec2f); i++)
        {
            spawn_positions.push_back(fetch.get<vec2f>());
        }
    }
};

#include "character.hpp"

struct debug_controls
{
    int controls_state = 0;

    int tools_state = 0;

    void line_draw_controls(vec2f mpos, state& st)
    {
        if(suppress_mouse)
            return;

        if(ONCE_MACRO(sf::Mouse::Left))
        {
            st.physics_barrier_manage.add_point(mpos, st);
        }
    }

    void spawn_controls(vec2f mpos, state& st)
    {
        if(suppress_mouse)
            return;

        if(ONCE_MACRO(sf::Mouse::Left))
        {
            st.game_world_manage.add(mpos);
        }
    }

    void editor_controls(vec2f mpos, state& st)
    {
        st.game_world_manage.enable_rendering();

        ImGui::Begin("Tools", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        std::vector<std::string> tools{"Line Draw", "Spawn Point"};

        for(int i=0; i<tools.size(); i++)
        {
            std::string str = tools[i];

            if(tools_state == i)
            {
                str += " <--";
            }

            if(ImGui::Button(str.c_str()))
            {
                tools_state = i;
            }
        }

        if(tools_state == 0)
        {
            line_draw_controls(mpos, st);
        }

        if(tools_state == 1)
        {
            spawn_controls(mpos, st);
        }

        if(ImGui::Button("Spawn Enemy"))
        {
            player_character* c = dynamic_cast<player_character*>(st.character_manage.make_new<player_character>(1, st.net_state));

            c->pos = st.game_world_manage.get_next_spawn();
            c->last_pos = c->pos;
            c->init_collision_pos(c->pos);
        }

        ImGui::End();
    }

    void player_controls(vec2f mpos, state& st, player_character* player)
    {
        if(!suppress_mouse && ONCE_MACRO(sf::Mouse::Left))
        {
            vec2f ppos = player->pos;

            vec2f to_mouse = mpos - ppos;

            host_projectile* p = dynamic_cast<host_projectile*>(st.projectile_manage.make_new<host_projectile>(player->team, st.net_state));
            p->pos = ppos;
            p->init_collision_pos(p->pos);
            p->dir = to_mouse.norm() * 1000.f;
        }
    }

    void tick(state& st, player_character* player)
    {
        vec2f mpos = st.cam.get_mouse_position_world();

        st.game_world_manage.disable_rendering();

        ImGui::Begin("Control menus");

        std::vector<std::string> modes{"Editor", "Player"};

        for(int i=0; i<modes.size(); i++)
        {
            std::string str = modes[i];

            if(controls_state == i)
            {
                str += " <--";
            }

            if(ImGui::Button(str.c_str()))
            {
                controls_state = i;
            }
        }

        if(controls_state == 0)
        {
            editor_controls(mpos, st);
        }

        if(controls_state == 1)
        {
            player_controls(mpos, st, player);
        }

        ImGui::End();
    }
};

void save(const std::string& file, physics_barrier_manager& physics_barrier_manage, game_world_manager& game_world_manage)
{
    byte_vector v1 = physics_barrier_manage.serialise();
    byte_vector v2 = game_world_manage.serialise();

    std::ofstream fout;
    fout.open(file, std::ios::binary | std::ios::out);

    int32_t v1_s = v1.ptr.size();
    int32_t v2_s = v2.ptr.size();

    fout.write((char*)&v1_s, sizeof(int32_t));
    fout.write((char*)&v2_s, sizeof(int32_t));

    if(v1.ptr.size() > 0)
        fout.write((char*)&v1.ptr[0], v1.ptr.size());

    if(v2.ptr.size() > 0)
        fout.write((char*)&v2.ptr[0], v2.ptr.size());
}

byte_fetch get_file(const std::string& fname)
{
    // open the file:
    std::ifstream file(fname, std::ios::binary);

    file.seekg(0, std::ios::end);
    auto file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    byte_fetch ret;

    if(file_size > 0)
    {
        ret.ptr.resize(file_size);
        file.read((char*)&ret.ptr[0], file_size);
    }

    return ret;
}

void load(const std::string& file, physics_barrier_manager& physics_barrier_manage, game_world_manager& game_world_manage, renderable_manager& renderable_manage)
{
    byte_fetch fetch = get_file(file);

    int32_t v1_s = fetch.get<int32_t>();
    int32_t v2_s = fetch.get<int32_t>();

    renderable_manage.erase_all();

    physics_barrier_manage.deserialise(fetch, v1_s);
    game_world_manage.deserialise(fetch, v2_s);
}

int main()
{
    networking_init();

    sf::ContextSettings context(0, 0, 8);

    sf::RenderWindow win;
    win.create(sf::VideoMode(1500, 1000), "fak u mark", sf::Style::Default, context);

    ImGui::SFML::Init(win);
    ImGui::NewFrame();

    renderable_manager renderable_manage;
    character_manager character_manage;

    physics_barrier_manager physics_barrier_manage;
    game_world_manager game_world_manage;

    projectile_manager projectile_manage;

    camera cam(win);

    network_state net_state;

    debug_controls controls;

    state st(character_manage, physics_barrier_manage, game_world_manage, renderable_manage, projectile_manage, cam, net_state);

    st.character_manage.system_network_id = 0;
    st.physics_barrier_manage.system_network_id = 1;
    st.game_world_manage.system_network_id = 2;
    st.renderable_manage.system_network_id = 3;
    st.projectile_manage.system_network_id = 4;

    player_character* test = dynamic_cast<player_character*>(character_manage.make_new<player_character>(0, st.net_state));

    sf::Clock clk;

    sf::Keyboard key;
    sf::Mouse mouse;

    sf::sleep(sf::milliseconds(1));

    uint32_t frame = 0;

    while(win.isOpen())
    {
        auto sfml_mpos = mouse.getPosition(win);

        vec2f mpos = {sfml_mpos.x, sfml_mpos.y};

        float dt_s = (clk.restart().asMicroseconds() / 1000.) / 1000.f;

        sf::Event event;

        float scrollwheel_delta;

        while(win.pollEvent(event))
        {
            ImGui::SFML::ProcessEvent(event);

            if(event.type == sf::Event::Closed)
            {
                win.close();
            }

            if(event.type == sf::Event::MouseWheelScrolled && event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel)
                scrollwheel_delta -= event.mouseWheelScroll.delta;
        }

        ImGui::SFML::Update(clk.restart());

        const ImGuiIO& io = ImGui::GetIO();

        suppress_mouse = false;

        if(io.WantCaptureMouse)
            suppress_mouse = true;

        vec2f move_dir;

        float mult = 1000.f;

        if(win.hasFocus())
        {
            move_dir.x() += (int)key.isKeyPressed(sf::Keyboard::D);
            move_dir.x() -= (int)key.isKeyPressed(sf::Keyboard::A);

            move_dir.y() += (int)key.isKeyPressed(sf::Keyboard::S);
            move_dir.y() -= (int)key.isKeyPressed(sf::Keyboard::W);

            if(controls.controls_state == 0)
            {
                float mult = 1.f;

                if(key.isKeyPressed(sf::Keyboard::LShift))
                    mult = 10.f;

                cam.move_pos(move_dir * mult);
            }
        }

        /*if(ONCE_MACRO(sf::Mouse::Left) && win.hasFocus() && controls.state == 0 && !suppress_mouse)
        {
            physics_barrier_manage.add_point(mpos, renderable_manage);
        }*/

        //test->velocity += move_dir * mult;

        if(win.hasFocus())
            controls.tick(st, test);

        if(controls.controls_state == 0)
        {
            ImGui::Begin("Save/Load");

            if(ImGui::Button("Save"))
            {
                save("file.mapfile", physics_barrier_manage, game_world_manage);
            }

            if(ImGui::Button("Load"))
            {
                load("file.mapfile", physics_barrier_manage, game_world_manage, renderable_manage);

                renderable_manage.add(test);
            }

            ImGui::End();
        }

        if(controls.controls_state == 1)
        {
            test->set_movement(move_dir * mult);

            if(frame > 1)
                character_manage.tick(dt_s, st);

            projectile_manage.tick(dt_s, st);

            if(ONCE_MACRO(sf::Keyboard::Space) && win.hasFocus())
            {
                test->jump();
            }
        }


        if(controls.controls_state == 1)
        {
            cam.set_pos(test->pos);
        }

        net_state.tick_cleanup();
        net_state.tick_join_game(dt_s);
        net_state.tick();

        projectile_manage.tick_all_networking<projectile_manager, projectile>(net_state);
        character_manage.tick_all_networking<character_manager, character>(net_state);

        cam.update_camera();

        win.clear();

        projectile_manage.cleanup();

        renderable_manage.render(win);
        physics_barrier_manage.render(win);
        game_world_manage.render(win);
        projectile_manage.render(win);
        character_manage.render(win);

        projectile_manage.check_collisions(character_manage);
        projectile_manage.check_collisions(physics_barrier_manage);

        ImGui::Render();
        win.display();

        sf::sleep(sf::milliseconds(1));

        frame++;
    }

    return 0;
}
