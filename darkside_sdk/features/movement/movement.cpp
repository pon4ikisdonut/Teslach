#include "movement.hpp"
#define _USE_MATH_DEFINES
#include <cmath>
constexpr float M_PI = 3.14159265358979323846f;

void c_movement::bunnyhop(c_user_cmd* user_cmd)
{
    if (!g_cfg->misc.m_bunny_hop)
        return;

    if (g_ctx->m_local_pawn->m_flags() & FL_ONGROUND)
        user_cmd->m_button_state.m_button_state &= ~IN_JUMP;
}

void c_movement::auto_strafe(c_user_cmd* user_cmd, float old_yaw) {
    if (!g_cfg->misc.m_auto_strafe)
        return;

    if (g_ctx->m_local_pawn->m_flags() & FL_ONGROUND)
        return;

    if (g_ctx->m_local_pawn->m_move_type() == movetype_ladder ||
        g_ctx->m_local_pawn->m_move_type() == movetype_noclip)
        return;

    vec3_t velocity = g_ctx->m_local_pawn->m_vec_abs_velocity();
    float flSpeed = velocity.length_2d();

    if (flSpeed < 5.0f)
        return;

    if (user_cmd->m_button_state.m_button_state & IN_SPEED)
        return;

    float current_yaw = user_cmd->pb.mutable_base()->viewangles().y();
    float flAirAccelerate = g_interfaces->m_var->get_by_name("sv_airaccelerate")->get_float();
    if (flAirAccelerate <= 0.0f)
        flAirAccelerate = 12.0f;

    float flMaxSpeed = 300.0f;
    float flDeltaAir = 0.0f;
    float term = 30.0f / flAirAccelerate / flMaxSpeed * 100.0f / flSpeed;

    if (term < 1.0f && term > -1.0f) {
        flDeltaAir = std::acos(term);
    }

    if (flDeltaAir == 0.0f)
        return;
    float flVelocityAngle = std::atan2(velocity.y, velocity.x) - (current_yaw * (M_PI / 180.0f));

    while (flVelocityAngle > M_PI) flVelocityAngle -= 2.0f * M_PI;
    while (flVelocityAngle < -M_PI) flVelocityAngle += 2.0f * M_PI;

    float flCurrentMoveAngle = std::atan2(user_cmd->pb.mutable_base()->leftmove(),
        user_cmd->pb.mutable_base()->forwardmove());

    auto delta_angle = [](float first, float second) -> float {
        float delta = first - second;
        float res = std::fmod(delta, 2.0f * M_PI);

        if (first > second) {
            if (res >= M_PI) {
                res = res - 2.0f * M_PI;
            }
        }
        else {
            if (res <= -M_PI) {
                res = res + 2.0f * M_PI;
            }
        }

        return res;
        };

    float flDeltaAngle = delta_angle(flVelocityAngle, flCurrentMoveAngle);
    float flFinalMove = flDeltaAngle < 0.0f ? flVelocityAngle + flDeltaAir : flVelocityAngle - flDeltaAir;

    bool bqm = g_interfaces->m_var->get_by_name("sv_quantize_movement_input")->get_bool();

    if (bqm) {
        float new_yaw = std::fmod(flFinalMove * 180.0f / M_PI, 45.0f) + current_yaw;
        user_cmd->pb.mutable_base()->mutable_viewangles()->set_y(new_yaw);

        if (g_cfg->anti_hit.m_enabled) {}
    }

    float flForwardMove = std::cos(flFinalMove);
    float flSideMove = std::sin(flFinalMove);

    user_cmd->pb.mutable_base()->set_forwardmove(std::clamp(flForwardMove, -1.0f, 1.0f));
    user_cmd->pb.mutable_base()->set_leftmove(std::clamp(flSideMove, -1.0f, 1.0f));
}

void c_movement::limit_speed(c_user_cmd* user_cmd, c_cs_player_pawn* local_player, c_base_player_weapon* active_weapon, float max_speed) {
    c_player_movement_service* movement_services = local_player->m_movement_services();

    if (!movement_services)
        return;

    vec3_t velocity = g_ctx->m_local_pawn->m_vec_abs_velocity();

    float cmd_speed = std::sqrt(
        (user_cmd->pb.mutable_base()->leftmove() * user_cmd->pb.mutable_base()->leftmove())
        + (user_cmd->pb.mutable_base()->forwardmove() * user_cmd->pb.mutable_base()->forwardmove())
    );

    float speed_2d = velocity.length_2d();

    if (cmd_speed <= 0.1f
        && speed_2d <= 50.f)
        return;

    float accelerate = g_interfaces->m_var->get_by_name(xorstr_("sv_accelerate"))->get_float();

    vec3_t view_angles = {
        g_ctx->m_user_cmd->pb.mutable_base()->viewangles().x(),
        g_ctx->m_user_cmd->pb.mutable_base()->viewangles().y(),
        g_ctx->m_user_cmd->pb.mutable_base()->viewangles().z()
    };

    vec3_t forward{ }, right{ }, up{ };
    g_math->angle_vectors(view_angles, forward, right, up);

    float diff = speed_2d - max_speed;
    float wish_speed = max_speed;

    vec3_t direction = { forward.x * user_cmd->pb.mutable_base()->forwardmove() + right.x * user_cmd->pb.mutable_base()->leftmove(),
        forward.y * user_cmd->pb.mutable_base()->forwardmove() + right.y * user_cmd->pb.mutable_base()->leftmove(), 0.f };

    const float max_accelerate = accelerate * INTERVAL_PER_TICK * max(250.f, movement_services->m_max_speed() * movement_services->m_surface_friction());

    if (diff - max_accelerate <= 0.f
        || speed_2d - max_accelerate - 3.f <= 0.f)
        wish_speed = max_speed;
    else {
        direction = velocity;
        wish_speed = -1.f;
    }

    if (user_cmd->pb.mutable_base()->forwardmove() > 0)
        user_cmd->pb.mutable_base()->set_forwardmove(std::clamp(wish_speed, -1.0f, 1.0f));
    else if (user_cmd->pb.mutable_base()->forwardmove() < 0)
        user_cmd->pb.mutable_base()->set_forwardmove(std::clamp(-wish_speed, -1.0f, 1.0f));

    if (user_cmd->pb.mutable_base()->leftmove() > 0)
        user_cmd->pb.mutable_base()->set_leftmove(std::clamp(wish_speed, -1.0f, 1.0f));
    else if (user_cmd->pb.mutable_base()->leftmove() < 0)
        user_cmd->pb.mutable_base()->set_leftmove(std::clamp(-wish_speed, -1.0f, 1.0f));
}

void c_movement::auto_stop(c_user_cmd* user_cmd, c_cs_player_pawn* local_player, c_base_player_weapon* active_weapon, bool no_spread) {
    if (!g_cfg->rage_bot.m_auto_stop)
        return;

    if (no_spread)
        return;

    if (!(local_player->m_flags() & FL_ONGROUND))
        return;

    auto remove_button = [&](int button) {
        user_cmd->m_button_state.m_button_state &= ~button;
        user_cmd->m_button_state.m_button_state2 &= ~button;
        user_cmd->m_button_state.m_button_state3 &= ~button;
        };


    

    remove_button(IN_SPEED);
    
    float wish_speed = active_weapon->get_max_speed() * 0.25f;

    limit_speed(user_cmd, local_player, active_weapon, wish_speed);
}

void c_movement::movement_fix(c_user_cmd* user_cmd, vec3_t angle)
{
    vec3_t wish_angle{ user_cmd->pb.mutable_base()->viewangles().x(), user_cmd->pb.mutable_base()->viewangles().y(), user_cmd->pb.mutable_base()->viewangles().z() };
    int sign = wish_angle.x > 89.f ? -1.f : 1.f;
    wish_angle.clamp();

    vec3_t forward, right, up, old_forward, old_right, old_up;
    vec3_t view_angles = angle;

    g_math->angle_vectors(wish_angle, forward, right, up);

    forward.z = right.z = up.x = up.y = 0.f;

    forward.normalize_in_place();
    right.normalize_in_place();
    up.normalize_in_place();

    g_math->angle_vectors(view_angles, old_forward, old_right, old_up);

    old_forward.z = old_right.z = old_up.x = old_up.y = 0.f;

    old_forward.normalize_in_place();
    old_right.normalize_in_place();
    old_up.normalize_in_place();

    forward *= user_cmd->pb.mutable_base()->forwardmove();
    right *= user_cmd->pb.mutable_base()->leftmove();
    up *= user_cmd->pb.mutable_base()->upmove();

    float fixed_forward_move = old_forward.dot(right) + old_forward.dot(forward) + old_forward.dot(up, true);

    float fixed_side_move = old_right.dot(right) + old_right.dot(forward) + old_right.dot(up, true);

    float fixed_up_move = old_up.dot(right, true) + old_up.dot(forward, true) + old_up.dot(up);

    user_cmd->pb.mutable_base()->set_forwardmove(std::clamp(fixed_forward_move, -1.0f, 1.0f));
    user_cmd->pb.mutable_base()->set_leftmove(std::clamp(fixed_side_move, -1.0f, 1.0f));
    user_cmd->pb.mutable_base()->set_upmove(std::clamp(fixed_up_move, -1.0f, 1.0f));
}

void c_movement::on_create_move(c_user_cmd* user_cmd, float old_yaw) {
    if (g_ctx->m_local_pawn->m_move_type() == movetype_ladder || g_ctx->m_local_pawn->m_move_type() == movetype_noclip)
        return;

    bunnyhop(user_cmd);
    auto_strafe(user_cmd, old_yaw);
}