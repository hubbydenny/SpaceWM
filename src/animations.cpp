#include "animations.hpp"
#include "wlr_compat.hpp"

#include <wlr/types/wlr_scene.h>

float easing(Easing e, float t) {
    switch (e) {
        case Easing::Linear:   return t;
        case Easing::EaseIn:   return t * t;
        case Easing::EaseOut:  return 1.0f - (1.0f - t) * (1.0f - t);
        case Easing::EaseInOut: return t < 0.5f ? 2.0f * t * t : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
    }
    return t;
}

uint32_t Animations::add(AnimState s) {
    s.start_time = s.start_time ? s.start_time : 0;
    uint32_t id = next_id_++;
    anims_[id] = s;
    return id;
}

void Animations::tick(uint32_t now_ms) {
    for (auto it = anims_.begin(); it != anims_.end(); ) {
        auto& s = it->second;
        uint32_t elapsed = now_ms - s.start_time;
        float t = (float)elapsed / s.duration_ms;
        if (t >= 1.0f) t = 1.0f;

        float k = easing(s.easing, t);

        if (s.node && (s.to_x != s.from_x || s.to_y != s.from_y)) {
            float x = s.from_x + (s.to_x - s.from_x) * k;
            float y = s.from_y + (s.to_y - s.from_y) * k;
            wlr_scene_node_set_position(s.node, (int)x, (int)y);
        }

        if (t >= 1.0f) {
            if (s.on_done) s.on_done();
            it = anims_.erase(it);
        } else {
            ++it;
        }
    }
}

bool Animations::empty() const { return anims_.empty(); }
void Animations::clear() { anims_.clear(); }

uint32_t anim_move(Animations& a, wlr_scene_node* node,
        float to_x, float to_y, uint32_t dur, Easing ease, std::function<void()> cb) {
    AnimState s;
    s.node = node;
    s.from_x = node ? node->x : 0;
    s.from_y = node ? node->y : 0;
    s.to_x = to_x;
    s.to_y = to_y;
    s.duration_ms = dur;
    s.easing = ease;
    s.on_done = cb;
    return a.add(s);
}

uint32_t anim_scale(Animations& a, wlr_scene_node* node,
        float to_sc, uint32_t dur, Easing ease, std::function<void()> cb) {
    AnimState s;
    s.node = node;
    s.from_opacity = 1.0f;
    s.to_opacity = to_sc;
    s.duration_ms = dur;
    s.easing = ease;
    s.on_done = cb;
    return a.add(s);
}
