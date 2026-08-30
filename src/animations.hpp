#pragma once

#include <functional>
#include <map>
#include <cstdint>

struct wlr_scene_node;
struct wlr_scene_rect;

enum class Easing {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
};

float easing(Easing, float t);

struct AnimState {
    wlr_scene_node* node = nullptr;
    float from_x = 0, from_y = 0;
    float to_x = 0, to_y = 0;
    float from_scale = 1.0f;
    float to_scale = 1.0f;
    float from_opacity = 1.0f;
    float to_opacity = 1.0f;
    uint32_t duration_ms = 0;
    uint32_t start_time = 0;
    Easing easing = Easing::EaseOut;
    std::function<void()> on_done;
};

class Animations {
public:
    uint32_t add(AnimState);
    void tick(uint32_t now_ms);
    bool empty() const;
    void clear();

private:
    std::map<uint32_t, AnimState> anims_;
    uint32_t next_id_ = 1;
};

uint32_t anim_move(Animations&, wlr_scene_node*,
    float to_x, float to_y, uint32_t duration_ms,
    Easing = Easing::EaseOut, std::function<void()> on_done = nullptr);

uint32_t anim_scale(Animations&, wlr_scene_node*,
    float to_scale, uint32_t duration_ms,
    Easing = Easing::EaseOut, std::function<void()> on_done = nullptr);
