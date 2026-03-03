import random

# Animation mode configurations
ANIMATION_MODES = {
    "A": {
        "bp_path": "/Game/MetaHumans/ABP_Run.ABP_Run_C",
        "move_speed": "70.0",
        "prob": 0.4
    },
    "B": {
        "bp_path": "/Game/MetaHumans/ABP_RandomHeadMovement.ABP_RandomHeadMovement_C",
        "move_speed": "0.0",
        "prob": 0.2
    },
    "C": {
        "bp_path": "/Game/MetaHumans/ABP_RandomIdle.ABP_RandomIdle_C",
        "move_speed": "0.0",
        "prob": 0.4
    }
}


def select_animation_mode():
    """Select animation mode based on probability distribution."""
    r = random.random()
    if r < ANIMATION_MODES["A"]["prob"]:
        return "A"
    elif r < ANIMATION_MODES["A"]["prob"] + ANIMATION_MODES["B"]["prob"]:
        return "B"
    else:
        return "C"


def build_single_matting_scene():
    """
    Build a SINGLE Matting scene sequence (for concatenation).

    Returns a list of commands for ONE scene.
    Each call generates different random parameters.
    """
    if random.random() < 0.5:
        resolution = "1080x1920"
        fov_range = "50 60"
    else:
        resolution = "1920x1080"
        fov_range = "80 90"

    matting_trajectory_options = [
        "render_only",
        "render_left_rotate",
        "render_right_rotate",
        "render_rotate_left",
        "render_rotate_right"
    ]
    chosen_trajectory = random.choice(matting_trajectory_options)

    anim_mode = select_animation_mode()
    anim_config = ANIMATION_MODES[anim_mode]

    commands = [
        {"cmd": "vrun", "params": "vset /captureactor/time_dilation 0.85"},
        {"cmd": "load_scene_param_json"},
        {"cmd": "random_scene_param_camera_height", "params": "120 155"},
        {"cmd": "random_scene_param_camera_angle_offset", "params": "-90 90"},
        {"cmd": "random_scene_param_camera_distance", "params": "75 100"},
        {"cmd": "create_scene"},
        {"cmd": "set_animation_bp", "params": anim_config["bp_path"]},
        {"cmd": "prepare_groom"},
        {"cmd": "set_foreground_move_speed", "params": anim_config["move_speed"]},
        {"cmd": "set_foreground_move_angle_offset", "params": "90.0"},
        {"cmd": "sync_pawn_to_primary_camera"},
        {"cmd": "delay", "params": "2.0"},
        {"cmd": "random_resolution", "params": resolution},
        {"cmd": "random_fov", "params": fov_range},
        {"cmd": "aim_camera_at_foreground", "params": "125 175"},
        {"cmd": "add_camera_rotation_noise", "params": "12.0 4.0 6.0"},
        {"cmd": "prepare_record"},
        {"cmd": "delay", "params": "4.0"},
        {"cmd": "record_trajectory", "params": chosen_trajectory},
        {"cmd": "sync_all_cameras"},
        {"cmd": "delay", "params": "0.5"},
        {"cmd": "clear_scene"},
        {"cmd": "delay", "params": "0.5"},
        {"cmd": "increment_counter"},
    ]

    return commands, {
        "resolution": resolution,
        "fov": fov_range,
        "trajectory": chosen_trajectory,
        "animation_mode": anim_mode,
        "animation_bp": anim_config["bp_path"],
        "move_speed": anim_config["move_speed"]
    }


def build_concatenated_matting_sequence(num_scenes):
    """
    Build concatenated Matting sequence (Approach 2).

    Each scene has DIFFERENT random parameters:
    - Different resolution (50% portrait, 50% landscape)
    - Different FOV range
    - Different trajectory

    Args:
        num_scenes: Number of scenes to concatenate

    Returns:
        command_sequence: Dict with "task" and "commands" keys
        scene_configs: List of configs for each scene (for logging)
    """
    all_commands = []
    scene_configs = []

    for i in range(num_scenes):
        commands, config = build_single_matting_scene()
        all_commands.extend(commands)
        scene_configs.append({
            "scene": i + 1,
            **config
        })

    all_commands.append({"cmd": "check_completion"})

    command_sequence = {
        "task": "Matting",
        "commands": all_commands
    }

    return command_sequence, scene_configs

