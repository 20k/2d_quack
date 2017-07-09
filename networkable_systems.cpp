#include "networkable_systems.hpp"
#include "state.hpp"
#include "managers.hpp"

void projectile::on_cleanup(state& st)
{
    auto proj = st.projectile_manage.make_new<explosion_projectile_client>();

    proj->pos = pos;
    proj->init_collision_pos(pos);
}

void host_projectile::on_cleanup(state& st)
{
    auto proj = st.projectile_manage.make_new<explosion_projectile_host>();

    proj->pos = pos;
    proj->init_collision_pos(pos);
}
