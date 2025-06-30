/*
 * Mini ECS.
 *
 * Copyright (C) 2025 Cannister of Sparrows <cannister_of_sparrows@proton.me>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef MINI_ECS_H
#define MINI_ECS_H

#include <stdbool.h>
#include <stddef.h>

#ifndef MAX_ENTITIES
#define MAX_ENTITIES 1024
#endif

typedef unsigned int Entity;

#define MECS_DEFINE_COMPONENT(CompType, Name) \
    CompType Name[MAX_ENTITIES]; \
    bool Name##_flag[MAX_ENTITIES]

#define MECS_HAS_COMPONENT(World, Name, e) ((World)->Name##_flag[(e)])

#define MECS_SET_COMPONENT(World, Name, e, Value) do { \
    (World)->Name[(e)] = (Value); \
    (World)->Name##_flag[(e)] = true; \
} while (0)

#define MECS_CLEAR_COMPONENT(World, Name, e) ((World)->Name##_flag[(e)] = false)

#define MECS_FOREACH_1(World, C1, e) \
    for (Entity e = 0; e < MAX_ENTITIES; ++e) \
        if ((World)->C1##_flag[e])

#define MECS_FOREACH_2(World, C1, C2, e) \
    for (Entity e = 0; e < MAX_ENTITIES; ++e) \
        if ((World)->C1##_flag[e] && \
            (World)->C2##_flag[e])

#define MECS_FOREACH_3(World, C1, C2, C3, e) \
    for (Entity e = 0; e < MAX_ENTITIES; ++e) \
        if ((World)->C1##_flag[e] && \
            (World)->C2##_flag[e] && \
            (World)->C3##_flag[e])

typedef struct {
    Entity next_entity;
    Entity free_list[MAX_ENTITIES];
    size_t free_count;
} EntityManager;

static inline Entity mecs_entity_create(EntityManager *em) {
    if (em->free_count > 0) {
        return em->free_list[--em->free_count];
    } else {
        return em->next_entity++;
    }
}

static inline void mecs_entity_destroy(EntityManager *em, Entity e) {
    if (em->free_count < MAX_ENTITIES) {
        em->free_list[em->free_count++] = e;
    }
}

#endif // MINI_ECS_H
