#ifndef STATE_HPP_INCLUDED
#define STATE_HPP_INCLUDED

struct character_manager;
struct physics_barrier_manager;
struct game_world_manager;
struct renderable_manager;

struct state
{
    character_manager& character_manage;
    physics_barrier_manager& physics_barrier_manage;
    game_world_manager& game_world_manage;
    renderable_manager& renderable_manage;

    state(character_manager& pcharacter_manage,
          physics_barrier_manager& pphysics_barrier_manage,
          game_world_manager& pgame_world_manage,
          renderable_manager& prenderable_manage) :


             character_manage(pcharacter_manage),
             physics_barrier_manage(pphysics_barrier_manage),
             game_world_manage(pgame_world_manage),
             renderable_manage(prenderable_manage)
     {}
};


#endif // STATE_HPP_INCLUDED
