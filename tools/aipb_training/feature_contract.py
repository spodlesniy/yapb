#!/usr/bin/env python3
"""Ordered semantic names for the AiPB model feature vector."""

from __future__ import annotations

from .model_contract import MODEL_FEATURE_COUNT


CORE_FEATURE_NAMES = (
    "round_time_remaining",
    "health",
    "armor",
    "max_speed",
    "velocity_x",
    "velocity_y",
    "velocity_z",
    "desired_velocity_x",
    "desired_velocity_y",
    "desired_velocity_z",
    "destination_relative_x",
    "destination_relative_y",
    "destination_relative_z",
    "alive",
    "moving_to_goal",
    "stuck",
    "difficulty",
    "skill",
    "aggression",
    "risk",
    "teamwork",
    "objective_focus",
    "camping",
    "exploration",
    "ammo_in_clip",
    "blind",
    "blind_time_remaining",
    "fire_pause_remaining",
    "enemy_distance",
    "last_enemy_distance",
    "weapon.unknown",
    "weapon.none",
    "weapon.melee",
    "weapon.pistol",
    "weapon.shotgun",
    "weapon.zoom_rifle",
    "weapon.rifle",
    "weapon.smg",
    "weapon.sniper",
    "weapon.heavy",
    "reload.none",
    "reload.primary",
    "reload.secondary",
    "objective.bomb_planted",
    "objective.bomb_carrier",
    "objective.has_hostage",
    "objective.in_bomb_zone",
    "objective.in_rescue_zone",
    "objective.in_escape_zone",
    "objective.in_vip_zone",
    "navigation.jump",
    "navigation.ladder",
    "navigation.crouch",
    "navigation.falling",
    "perception.seeing_enemy",
    "perception.hearing_enemy",
    "perception.suspected_enemy",
    "perception.enemy_reachable",
    "task.unknown",
    "task.normal",
    "task.pause",
    "task.move_to_position",
    "task.follow_user",
    "task.pickup_item",
    "task.camp",
    "task.plant_bomb",
    "task.defuse_bomb",
    "task.attack",
    "task.hunt",
    "task.seek_cover",
    "task.throw_explosive",
    "task.throw_flashbang",
    "task.throw_smoke",
    "task.double_jump",
    "task.escape_from_bomb",
    "task.shoot_breakable",
    "task.hide",
    "task.blind",
    "task.spraypaint",
)

PLAYER_FEATURE_NAMES = (
    "valid",
    "alive",
    "enemy",
    "visible",
    "heard",
    "relative_x",
    "relative_y",
    "relative_z",
    "distance",
    "health",
    "armor",
)

WAYPOINT_FEATURE_NAMES = (
    "present",
    "visible",
    "is_current",
    "is_goal",
    "relative_x",
    "relative_y",
    "relative_z",
    "distance",
)

MODEL_FEATURE_NAMES = (
    CORE_FEATURE_NAMES
    + tuple(
        f"player.{slot}.{name}"
        for slot in range(8)
        for name in PLAYER_FEATURE_NAMES
    )
    + tuple(
        f"waypoint.{slot}.{name}"
        for slot in range(8)
        for name in WAYPOINT_FEATURE_NAMES
    )
)

if len(MODEL_FEATURE_NAMES) != MODEL_FEATURE_COUNT:
    raise RuntimeError(
        f"feature schema has {len(MODEL_FEATURE_NAMES)} names; expected {MODEL_FEATURE_COUNT}"
    )

MODEL_FEATURE_INDEX = {name: index for index, name in enumerate(MODEL_FEATURE_NAMES)}

if len(MODEL_FEATURE_INDEX) != MODEL_FEATURE_COUNT:
    raise RuntimeError("feature schema contains duplicate names")
