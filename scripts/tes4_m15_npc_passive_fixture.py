"""Stage a pinned NPC passive-source fixture with an inspected script override.

The private overlay releases restraint on normal activation. This helper only
installs declared saved history, drawn state and that overlay's content identity;
it does not activate combat or inject hits, damage or resource values.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path

import tes4_runtime_state as runtime


def stage(source: Path, output: Path, recipe: dict, plugin: Path) -> dict:
    fields = {'schema_version', 'purpose', 'source', 'source_sha256', 'target_schema',
              'actor', 'actor_clock', 'actor_draw_state', 'melee_state',
              'physical_actions', 'process_action', 'content_override'}
    if (not isinstance(recipe, dict) or set(recipe) != fields
            or type(recipe['schema_version']) is not int or recipe['schema_version'] != 1
            or type(recipe['target_schema']) is not int or recipe['target_schema'] != runtime.CURRENT_VERSION
            or runtime.CURRENT_VERSION not in (59, 60, 61, 62, 63, 64, 65, 66, 67) or not isinstance(recipe['actor'], str)
            or not recipe['actor'].startswith('content:')
            or type(recipe['actor_draw_state']) is not int or recipe['actor_draw_state'] != 1
            or type(recipe['process_action']) is not int or recipe['process_action'] != -1
            or recipe['physical_actions'] != {'next': 2, 'pending': []}):
        raise ValueError('unsupported NPC passive fixture recipe')
    if output.exists():
        raise FileExistsError(output)
    override = recipe['content_override']
    if (not isinstance(override, dict)
            or set(override) != {'plugin', 'source_fingerprint', 'replacement_fingerprint'}
            or override['plugin'] != 'm15cachedblockopponent.esp'
            or override['replacement_fingerprint'] != 'sha256:' + hashlib.sha256(plugin.read_bytes()).hexdigest()):
        raise ValueError('NPC fixture plugin differs from inspected replacement')
    source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
    if source_hash != recipe['source_sha256']:
        raise ValueError('NPC passive source differs from pinned save')
    original = runtime.load_save(source)
    actor = recipe['actor']
    values = [v for v in original['native_actor_values'] if v['actor'] == actor]
    refs = [v for v in original['references'] if v['key'] == actor]
    clocks = [v for v in original['native_animation_clocks'] if v['actor'] == actor]
    previous = [v for v in original['native_melee_states'] if v['actor'] == actor]
    content = [v for v in original['content'] if v['plugin'].lower() == override['plugin']]
    if (original['schema_version'] != 59 or original['physical_actions'] != recipe['physical_actions']
            or len(values) != 1 or values[0]['owner'] != 1 or values[0]['process'] != 1
            or values[0]['process_action'] != -1 or len(refs) != 1 or refs[0]['actor_draw_state'] != 0
            or len(clocks) != 1 or clocks[0]['clock'] != recipe['actor_clock']
            or len(content) != 1 or content[0]['fingerprint'] != override['source_fingerprint']
            or len(previous) > 1 or any(v['strike'] or v['reaction'] or v['passive_sequence']
                or v['first_person'] or v['retiring_sequences'] or v['ai_intent']
                or v['input']['input_held'] or v['input']['held_seconds'] or v['input']['queued'] for v in previous)):
        raise ValueError('NPC passive source lost declared idle ownership or content identity')
    melee = recipe['melee_state']
    if (not isinstance(melee, dict) or melee.get('actor') != actor
            or any(melee.get(k) for k in ('strike', 'reaction', 'first_person', 'retiring_sequences', 'ai_intent'))
            or melee.get('input') != {'held_seconds': 0, 'input_held': False, 'prefer_left': True, 'queued': 0}
            or not melee.get('passive_sequence')):
        raise ValueError('NPC fixture requires recovered body-only passive history')
    state = copy.deepcopy(original)
    runtime._upgrade_held_blocks(state)
    runtime._upgrade_movement(state)
    runtime._upgrade_spoken_dialogue_history(state)
    runtime._upgrade_friendly_hit_flags(state)
    runtime._upgrade_disposition_modifiers(state)
    runtime._upgrade_friend_hit_counters(state)
    state['schema_version'] = recipe['target_schema']
    next(v for v in state['references'] if v['key'] == actor)['actor_draw_state'] = 1
    next(v for v in state['content'] if v['plugin'].lower() == override['plugin'])['fingerprint'] = override['replacement_fingerprint']
    if previous:
        index = next(i for i, v in enumerate(state['native_melee_states']) if v['actor'] == actor)
        state['native_melee_states'][index] = copy.deepcopy(melee)
    else:
        state['native_melee_states'].append(copy.deepcopy(melee))
    # Validate the complete wire state before creating a destination.
    runtime.encode_payload(state)
    runtime.write_save(source, output, state)
    if runtime.load_save(output) != state:
        raise ValueError('NPC staged save differs outside declared fixture fields')
    if hashlib.sha256(source.read_bytes()).hexdigest() != source_hash:
        raise ValueError('NPC fixture source changed during staging')
    report = dict(structural_write_success=True, semantic_reinspection_passed=True,
                  source_sha256=source_hash, output_sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
                  mutations=['NPC draw state0 -> 1', 'declared NPC passive history installed',
                             'declared inspected private plugin fingerprint replaced'],
                  schema_defaults={'melee.held_blocks': [], 'first_person.held_blocks': [], 'native_movement': None, 'spoken_dialogue_infos': [], 'ignore_friendly_hits': {}, 'disposition_modifiers': [], 'friend_hit_counters': {}},
                  all_other_decoded_state_preserved=True, compiler_world_validation=False,
                  runtime_acceptance=False, normal_input_contact=False)
    output.with_suffix('.staging.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--plugin', type=Path, required=True)
    parser.add_argument('--recipe', type=Path, default=Path(__file__).parent /
                        'data/oblivion_compat/m15_npc_passive_strike_fixture.json')
    args = parser.parse_args()
    print(json.dumps(stage(args.source, args.output, json.loads(args.recipe.read_text()), args.plugin), indent=2))


if __name__ == '__main__':
    main()
