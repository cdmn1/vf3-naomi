#!/usr/bin/env python3
"""Patch the pinned native pose source so descendants inherit the recovered row 0."""
from pathlib import Path

path = Path(__file__).with_name("motion_pose_kos.c")
source = path.read_text()

old_signature = """int vf3_native_motion_pose_fields(
    const int16_t pose_fields[VF3_NATIVE_POSE_FIELDS],
    vf3_native_motion_rig rig,
    float matrices[VF3_NATIVE_ATTACHMENT_COUNT][VF3_NATIVE_ATTACHMENT_MATRIX_FLOATS])
"""
new_signature = """static int motion_pose_fields_with_root(
    const int16_t pose_fields[VF3_NATIVE_POSE_FIELDS],
    vf3_native_motion_rig rig,
    const float root_override[16],
    float matrices[VF3_NATIVE_ATTACHMENT_COUNT][VF3_NATIVE_ATTACHMENT_MATRIX_FLOATS])
"""
if source.count(old_signature) != 1:
    raise SystemExit("Could not uniquely locate native pose-fields function signature")
source = source.replace(old_signature, new_signature)

old_loop = """        const motion_bone *bone = &bones[row];
        build_rest(bone->rest, bind_world[row]);
"""
new_loop = """        const motion_bone *bone = &bones[row];
        build_rest(bone->rest, bind_world[row]);
"""
if source.count(old_loop) != 1:
    raise SystemExit("Could not uniquely locate bind-pose loop")
# Keep bind-pose construction unchanged. Inject the override in the pose loop,
# after the empty-row check would otherwise be handled.
old_pose_loop = """        const motion_bone *bone = &bones[row];
        float anim[16], posed[16];
        if (bone->parent == -2) {
"""
new_pose_loop = """        const motion_bone *bone = &bones[row];
        float anim[16], posed[16];
        if (row == 0u && root_override) {
            /* Keep the renderer's authored torso/model basis. The recovered
             * row-0 sample supplies the actor origin, but its orientation is
             * not yet proven compatible with the POL attachment basis. */
            turns_for_row(bone, pose_fields, anim);
            vf3_native_matrix_multiply(bind_local[row], anim, world[row]);
            world[row][12] = root_override[12];
            world[row][13] = root_override[13];
            world[row][14] = root_override[14];
            memcpy(matrices[row], world[row], sizeof matrices[row]);
            continue;
        }
        if (bone->parent == -2) {
"""
if source.count(old_pose_loop) != 1:
    raise SystemExit("Could not uniquely locate attachment pose loop")
source = source.replace(old_pose_loop, new_pose_loop)

anchor = """    return 0;
}

/* ---- the recovered row 0 (0x8C09EAF2) ----
"""
wrapper = """    return 0;
}

int vf3_native_motion_pose_fields(
    const int16_t pose_fields[VF3_NATIVE_POSE_FIELDS],
    vf3_native_motion_rig rig,
    float matrices[VF3_NATIVE_ATTACHMENT_COUNT][VF3_NATIVE_ATTACHMENT_MATRIX_FLOATS])
{
    return motion_pose_fields_with_root(pose_fields, rig, NULL, matrices);
}

/* ---- the recovered row 0 (0x8C09EAF2) ----
"""
if source.count(anchor) != 1:
    raise SystemExit("Could not uniquely locate insertion point for public wrapper")
source = source.replace(anchor, wrapper)

old_actor = """    int16_t signed_fields[VF3_NATIVE_POSE_FIELDS];
    if (!stream || !actor || !matrices)
"""
new_actor = """    int16_t signed_fields[VF3_NATIVE_POSE_FIELDS];
    float native_root[16];
    if (!stream || !actor || !matrices)
"""
if source.count(old_actor) != 1:
    raise SystemExit("Could not uniquely locate actor-local declarations")
source = source.replace(old_actor, new_actor)

old_tail = """    if (vf3_native_motion_pose_fields(signed_fields, rig, matrices) != 0)
        return -3;
    if (vf3_native_motion_row0(signed_fields, actor, channels,
                              matrices[0]) != 0)
        return -4;
    return 0;
"""
new_tail = """    if (vf3_native_motion_row0(signed_fields, actor, channels,
                              native_root) != 0)
        return -4;
    if (motion_pose_fields_with_root(signed_fields, rig, native_root,
                                     matrices) != 0)
        return -3;
    return 0;
"""
if source.count(old_tail) != 1:
    raise SystemExit("Could not uniquely locate native actor pose tail")
source = source.replace(old_tail, new_tail)

path.write_text(source)
print("Applied native-root hierarchy correction: child rows now inherit native row 0")
