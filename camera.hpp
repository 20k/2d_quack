#ifndef CAMERA_HPP_INCLUDED
#define CAMERA_HPP_INCLUDED

struct camera
{
    vec2f pos;
    float zoom = 1.f;

    sf::View view;
    sf::RenderWindow& win;

    camera(sf::RenderWindow& in_win) : win(in_win) {view = win.getDefaultView(); view.setCenter(0, 0);}

    void set_pos(vec2f v)
    {
        pos = v;
    }

    void move_pos(vec2f v)
    {
        pos = pos + v;
    }

    vec2f get_mouse_position_world()
    {
        sf::Mouse mouse;

        auto p = mouse.getPosition(win);

        auto q = win.mapPixelToCoords(p);

        return {q.x, q.y};
    }

    void update_camera()
    {
        auto dim = win.getSize();

        view.setSize(dim.x * zoom, dim.y * zoom);

        view.setCenter(0,0);
        view.move(pos.x(), pos.y());

        win.setView(view);
    }

    void set_zoom(float pzoom)
    {
        zoom = pzoom;
    }
};

#endif // CAMERA_HPP_INCLUDED
