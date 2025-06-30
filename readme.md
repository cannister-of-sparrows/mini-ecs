# 🧠 Mini ECS

*A minimal, header-only Entity Component System for C — simple, flexible, and tiny.*

![License](https://img.shields.io/badge/license-GPLv3-blue)  
![Language](https://img.shields.io/badge/language-C-informational)

---

## ✨ Overview

**Mini ECS** is a lightweight, header-only **Entity Component System (ECS)** framework written in pure C. It’s designed for simplicity, clarity, and educational value — with zero dependencies and a focus on being embedded directly into small projects or prototypes.

The system avoids complexity in favor of readability, using just macros and flat arrays to manage components and entities.

---

## 📦 Features

- 🧱 **Component-based**: Data is stored in tightly packed arrays.
- 🔍 **Query macros**: Use `MECS_FOREACH` macros to filter entities with specific components.
- 🎯 **No dynamic memory allocation required**.
- 📁 **Single-header**: Drop `mini_ecs.h` into your project — done.
- 💡 **Includes a Snake game demo** to show the ECS system in action.

---

## 🚀 Quick Example

```c
#include "mini_ecs.h"

typedef struct { float x, y; } Position;
typedef struct { float dx, dy; } Velocity;

typedef struct {
    EntityManager em;
    MECS_DEFINE_COMPONENT(Position, position);
    MECS_DEFINE_COMPONENT(Velocity, velocity);
} World;

void update(World* world) {
    MECS_FOREACH_2(world, position, velocity, e) {
        world->position[e].x += world->velocity[e].dx;
        world->position[e].y += world->velocity[e].dy;
    }
}
```

---

## 🕹 Snake Demo

Included in the repository is `mecs_snake.c` — a full implementation of the classic Snake game using only ECS principles:

- No special casing for “snakes” — just components like `Position`, `Direction`, `Follower`, `Edible`.
- Extensible: Add multiple snakes, AI-controlled hazards, decay timers, or portals with minimal code changes.

📄 Check the file header for suggested ECS-based gameplay extensions!

---

## 📚 API Summary

| Macro / Function              | Description                                      |
|-------------------------------|--------------------------------------------------|
| `MECS_DEFINE_COMPONENT(T, n)` | Declare a component type                         |
| `MECS_SET_COMPONENT(...)`     | Set a component on an entity                     |
| `MECS_HAS_COMPONENT(...)`     | Check if an entity has a given component         |
| `MECS_CLEAR_COMPONENT(...)`   | Remove a component from an entity                |
| `MECS_FOREACH_{1,2,3}(...)`   | Iterate entities with 1–3 required components    |
| `mecs_entity_create(...)`     | Create a new entity                              |
| `mecs_entity_destroy(...)`    | Recycle an entity back into the free list        |

---

## ⚖️ License

This project is licensed under the **GNU GPLv3**.  
See [LICENSE](https://www.gnu.org/licenses/gpl-3.0.en.html) for details.
