#include "networkable_systems.hpp"
#include "state.hpp"
#include "managers.hpp"

void projectile_base::on_cleanup(state& st)
{
    auto proj = st.projectile_manage.make_new<explosion_projectile>();

    proj->pos = pos;
    proj->init_collision_pos(pos);
}
