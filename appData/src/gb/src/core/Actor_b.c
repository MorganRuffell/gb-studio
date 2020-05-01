// clang-format off
#pragma bank=1
// clang-format on

#include "Actor.h"
#include "Sprite.h"
#include "Scroll.h"
#include "Math.h"
#include "GameTime.h"
#include "UI.h"
#include "ScriptRunner.h"

#define SCREENWIDTH_PLUS_64 224   // 160 + 64
#define SCREENHEIGHT_PLUS_64 208  // 144 + 64

UBYTE actors_active_delete[MAX_ACTIVE_ACTORS];

void MoveActors_b() {
  UBYTE i, a;
  for (i = 0; i != actors_active_size; i++) {
    a = actors_active[i];
    if (actors[a].moving) {
      if (actors[a].move_speed == 0) {
        // Half speed only move every other frame
        if (IS_FRAME_2) {
          actors[a].pos.x += actors[a].vel.x;
          actors[a].pos.y += actors[a].vel.y;
        }
      } else {
        actors[a].pos.x += actors[a].vel.x * actors[a].move_speed;
        actors[a].pos.y += actors[a].vel.y * actors[a].move_speed;
      }
    }
  }
}

void UpdateActors_b() {
  UBYTE i, k, a, flip, frame;
  UBYTE fo = 0;
  UINT16 screen_x;
  UINT16 screen_y;
  UBYTE del_count = 0;
  Actor *actor;

  k = 0;

  for (i = 0; i != actors_active_size; i++) {
    a = actors_active[i];
    actor = &actors[a];
    k = actors[a].sprite_index;
    flip = FALSE;
    fo = 0;

    if (!actor->enabled) {
      move_sprite(k, 0, 0);
      move_sprite(k + 1, 0, 0);
      continue;
    }

    screen_x = 8u + actor->pos.x - scroll_x;
    screen_y = 8u + actor->pos.y - scroll_y;

    // Update animation frames
    if (IS_FRAME_8 &&
        (((actors[a].moving) && actors[a].sprite_type != SPRITE_STATIC) || actors[a].animate)) {
      if (actors[a].anim_speed == 4 || (actors[a].anim_speed == 3 && IS_FRAME_16) ||
          (actors[a].anim_speed == 2 && IS_FRAME_32) ||
          (actors[a].anim_speed == 1 && IS_FRAME_64) ||
          (actors[a].anim_speed == 0 && IS_FRAME_128)) {
        actors[a].frame++;
      }
      if (actors[a].frame == actors[a].frames_len) {
        actors[a].frame = 0;
      }

      actor->rerender = TRUE;
    }

    // Rerender actors
    if (actor->rerender) {
      if (actor->sprite_type != SPRITE_STATIC) {
        // Increase frame based on facing direction
        if (IS_NEG(actor->dir.y)) {
          fo = 1 + (actor->sprite_type == SPRITE_ACTOR_ANIMATED);
        } else if (actor->dir.x != 0) {
          fo = 2 + MUL_2(actor->sprite_type == SPRITE_ACTOR_ANIMATED);
        }
      }

      LOG("RERENDER actor a=%u\n", a);
      // Facing left so flip sprite
      if (IS_NEG(actor->dir.x)) {
        LOG("AUR FLIP DIR X0\n");
        flip = TRUE;
      }

      frame = MUL_4(actor->sprite + actor->frame + fo);
      LOG("RERENDER actor a=%u with FRAME %u  [ %u + %u ] \n", a, frame, actor->sprite,
          actor->frame_offset);

      if (flip) {
        set_sprite_prop(k, S_FLIPX);
        set_sprite_prop(k + 1, S_FLIPX);
        set_sprite_tile(k, frame + 2);
        set_sprite_tile(k + 1, frame);
      } else {
        set_sprite_prop(k, 0);
        set_sprite_prop(k + 1, 0);
        set_sprite_tile(k, frame);
        set_sprite_tile(k + 1, frame + 2);
      }

      actor->rerender = FALSE;
    }

    /*
        if (IS_FRAME_8 && (((actors[a].vel.x != 0 || actors[a].vel.y != 0) &&
                            actors[a].sprite_type != SPRITE_STATIC) ||
                           actors[a].animate)) {
          actors[a].frame++;
          if (actors[a].frame == actors[a].frames_len) {
            actors[a].frame = 0;
          }

          sprites[actors[a].sprite_index].frame = actors[a].frame;
          sprites[actors[a].sprite_index].rerender = TRUE;
        }

        if (actor->sprite_type != SPRITE_STATIC) {
          // Increase frame based on facing direction
          if (IS_NEG(actor->dir.y)) {
            fo = 1 + (actor->sprite_type == SPRITE_ACTOR_ANIMATED);
            if (sprites[k].frame_offset != fo) {
              sprites[k].frame_offset = fo;
              sprites[k].flip = FALSE;
              sprites[k].rerender = TRUE;
            }
          } else if (actor->dir.x != 0) {
            fo = 2 + MUL_2(actor->sprite_type == SPRITE_ACTOR_ANIMATED);
            if (sprites[k].frame_offset != fo) {
              sprites[k].frame_offset = fo;
              sprites[k].rerender = TRUE;
            }
            // Facing left so flip sprite
            if (IS_NEG(actor->dir.x)) {
              flip = TRUE;
              if (!sprites[k].flip) {
                sprites[k].flip = TRUE;
                sprites[k].rerender = TRUE;
              }
            } else  // Facing right
            {
              if (sprites[k].flip) {
                sprites[k].flip = FALSE;
                sprites[k].rerender = TRUE;
              }
            }
          } else {
            fo = 0;
            if (sprites[k].frame_offset != fo) {
              sprites[k].frame_offset = fo;
              sprites[k].flip = FALSE;
              sprites[k].rerender = TRUE;
            }
          }
        }
    */

    // LOG("SPRITE MOVE %u %u\n", i, s);

    if (!hide_sprites_under_win && screen_x > WX_REG && screen_y - 8 > WY_REG) {
      move_sprite(k, 0, 0);
      move_sprite(k + 1, 0, 0);
    } else {
      move_sprite(k, screen_x, screen_y);
      move_sprite(k + 1, screen_x + 8, screen_y);
    }

    // Check if actor is off screen
    if (IS_FRAME_32 && (a != script_actor)) {
      if (((UINT16)(screen_x + 32u) >= SCREENWIDTH_PLUS_64) ||
          ((UINT16)(screen_y + 32u) >= SCREENHEIGHT_PLUS_64)) {
        // Mark off screen actor for removal
        actors_active_delete[del_count] = a;
        del_count++;
      }
    }
  }

  // Remove all offscreen actors
  for (i = 0; i != del_count; i++) {
    a = actors_active_delete[i];
    DeactivateActor(a);
  }
}

void ActivateActor_b(UBYTE i) {
  UBYTE j;

  if (actors_active_size == MAX_ACTIVE_ACTORS) {
    return;
  }

  // Stop if actor already active
  for (j = 0; j != actors_active_size; j++) {
    if (actors_active[j] == i) {
      return;
    }
  }

  actors_active[actors_active_size++] = i;
  actors[i].sprite_index = SpritePoolNext();
  actors[i].frame_offset = 0;
  actors[i].rerender = TRUE;
}

void ActivateActorColumn_b(UBYTE tx_a, UBYTE ty_a) {
  UBYTE i;

  for (i = MAX_ACTORS - 1; i != 0; i--) {
    UBYTE tx_b, ty_b;

    tx_b = DIV_8(actors[i].pos.x);
    ty_b = DIV_8(actors[i].pos.y);

    if ((ty_a <= ty_b && ty_a + 20 >= ty_b) && (tx_a == tx_b || tx_a == tx_b + 1)) {
      ActivateActor_b(i);
    }
  }
}

void DeactivateActor_b(UBYTE i) {
  UBYTE j, a;

  a = 0;  // Required to fix GBDK bug

  for (j = 0; j != actors_active_size; j++) {
    if (actors_active[j] == i) {
      a = j;
      break;
    }
  }

  if (a) {
    SpritePoolReturn(actors[i].sprite_index);
    actors[i].sprite_index = 0;
    actors_active[a] = actors_active[--actors_active_size];
  }
}

void ActorsUnstick_b() {
  UBYTE i, a;
  // Fix stuck actors
  for (i = 0; i != actors_active_size; i++) {
    a = actors_active[i];
    if (!actors[a].moving && !ACTOR_ON_TILE(a)) {
      actors[a].moving = TRUE;
    }
  }
}