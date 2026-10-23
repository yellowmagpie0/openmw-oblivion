"""Stage a pinned synthetic passive-slot3/Stagger saved-state render fixture.

This does not select a resource, generate a contact or establish native blend
durations. The editable recipe declares the complete injected Player state.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct

import tes4_runtime_state as runtime


def stage(source: Path, output: Path, recipe: dict) -> dict:
    fields = {'schema_version', 'purpose', 'source', 'source_sha256', 'target_schema',
              'actor', 'actor_clock', 'physical_actions', 'process_action', 'melee_state',
              'shared_player_draw_state', 'camera_first_person'}
    actor = 'dynamic:player:0000000000000001'
    if (not isinstance(recipe, dict) or set(recipe) != fields
            or type(recipe['schema_version']) is not int or recipe['schema_version'] != 1
            or type(recipe['target_schema']) is not int
            or recipe['target_schema'] != runtime.CURRENT_VERSION or runtime.CURRENT_VERSION not in (59, 60, 61, 62, 63, 64)
            or recipe['actor'] != actor or type(recipe['process_action']) is not int or recipe['process_action'] != 8
            or type(recipe['shared_player_draw_state']) is not int or recipe['shared_player_draw_state'] != 1
            or type(recipe['camera_first_person']) is not bool
            or recipe['physical_actions'] != {'next': 2, 'pending': []}
            or not isinstance(recipe['melee_state'], dict) or recipe['melee_state'].get('actor') != actor):
        raise ValueError('unsupported passive slot3 render recipe')
    if output.exists():
        raise FileExistsError(output)
    source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
    if source_hash != recipe['source_sha256']:
        raise ValueError('passive slot3 source differs from pinned idle save')
    blob = source.read_bytes()
    draw = [struct.unpack('<i', blob[start + 8:end])[0]
            for _, _, name, subs in runtime._save_records(blob) if name == b'PLAY'
            for tag, start, end in subs if tag == b'DRAW' and end - start == 12]
    if draw != [1]:
        raise ValueError('passive slot3 source requires a drawn Player from the declared normal-input seed')
    camera = [blob[start + 8:end] for _, _, name, subs in runtime._save_records(blob) if name == b'CAM_'
              for tag, start, end in subs if tag == b'FIRS']
    if camera != [b'\1']:
        raise ValueError('passive slot3 source requires its declared first-person seed camera')
    original = runtime.load_save(source)
    clocks = [v for v in original['native_animation_clocks'] if v['actor'] == actor]
    values = [v for v in original['native_actor_values'] if v['actor'] == actor]
    previous = [v for v in original['native_melee_states'] if v['actor'] == actor]
    if (original['schema_version'] != 59 or original['physical_actions'] != recipe['physical_actions']
            or len(clocks) != 1 or clocks[0]['clock'] != recipe['actor_clock']
            or len(values) != 1 or values[0]['process_action'] != -1 or len(previous) > 1
            or any(v['strike'] or v['reaction'] or v['passive_sequence'] or v['first_person']
                   or v['retiring_sequences'] or v['input']['input_held'] or v['input']['queued']
                   or v['input']['held_seconds'] for v in previous)):
        raise ValueError('passive slot3 source must have the declared idle Player clock and action')
    state = copy.deepcopy(original)
    runtime._upgrade_held_blocks(state)
    runtime._upgrade_movement(state)
    runtime._upgrade_spoken_dialogue_history(state)
    state['schema_version'] = recipe['target_schema']
    next(v for v in state['native_actor_values'] if v['actor'] == actor)['process_action'] = 8
    state['native_melee_states'] = [v for v in state['native_melee_states'] if v['actor'] != actor]
    state['native_melee_states'].append(copy.deepcopy(recipe['melee_state']))
    output.parent.mkdir(parents=True, exist_ok=True)
    runtime.write_save(source, output, state)
    # Shared World::write emits CAM_/FIRS as one bool byte. Keep this explicit
    # fixture mutation separate from the decoded native actor state.
    rendered = bytearray(output.read_bytes())
    camera_offsets = [start + 8 for _, _, name, subs in runtime._save_records(rendered) if name == b'CAM_'
                      for tag, start, end in subs if tag == b'FIRS' and end - start == 9]
    if len(camera_offsets) != 1:
        raise ValueError('passive slot3 output lost its shared camera flag')
    rendered[camera_offsets[0]] = int(recipe['camera_first_person'])
    output.write_bytes(rendered)
    if runtime.load_save(output) != state:
        raise ValueError('passive slot3 output differs outside declared fixture fields')
    if hashlib.sha256(source.read_bytes()).hexdigest() != source_hash:
        raise ValueError('passive slot3 source changed during staging')
    report = dict(structural_write_success=True, semantic_reinspection_passed=True,
                  source=str(source), source_sha256=source_hash,
                  output_sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
                  source_schema=59, target_schema=runtime.CURRENT_VERSION,
                  mutations=['Player process action -1 -> 8', 'declared Player melee state installed'],
                  shared_camera=dict(source_first_person=True, output_first_person=recipe['camera_first_person']),
                  schema_defaults={'melee.held_blocks': [], 'first_person.held_blocks': [], 'native_movement': None, 'spoken_dialogue_infos': []},
                  all_other_decoded_state_preserved=True, compiler_world_validation=False,
                  runtime_acceptance=False, normal_input_contact=False)
    output.with_suffix('.staging.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--recipe', type=Path, default=Path(__file__).parent /
                        'data/oblivion_compat/m15_passive_slot3_controller_fixture.json')
    args = parser.parse_args()
    print(json.dumps(stage(args.source, args.output, json.loads(args.recipe.read_text())), indent=2))


if __name__ == '__main__':
    main()
