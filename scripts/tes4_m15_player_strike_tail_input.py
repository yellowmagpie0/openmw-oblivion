"""Reproduce the pinned idle unarmed Player strike-tail runtime fixture.

Unequip one existing weapon and restrain three NPCs. Resource values, inventory
contents, conditions, action history and RNG are preserved and reinspected.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path

import tes4_runtime_state as runtime


def stage(source: Path, output: Path, recipe: dict) -> dict:
    fields = {'schema_version', 'purpose', 'source', 'source_sha256', 'source_schema',
              'target_schema', 'weapon_base', 'restrained_actors'}
    actors = ['content:oblivion.esm:01fc43', 'content:oblivion.esm:026be1', 'content:oblivion.esm:15985a']
    if (not isinstance(recipe, dict) or set(recipe) != fields
            or type(recipe['schema_version']) is not int or recipe['schema_version'] != 1
            or type(recipe['source_schema']) is not int or recipe['source_schema'] not in (57, 58)
            or type(recipe['target_schema']) is not int or recipe['target_schema'] != 58
            or runtime.CURRENT_VERSION != 58 or recipe['restrained_actors'] != actors
            or recipe['weapon_base'] != 'content:oblivion.esm:090615'):
        raise ValueError('unsupported Player strike-tail input recipe')
    if output.exists():
        raise FileExistsError(output)
    source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
    if source_hash != recipe['source_sha256']:
        raise ValueError('Player strike-tail source differs from pinned idle save')
    original = runtime.load_save(source)
    if (original['schema_version'] != recipe['source_schema'] or original['physical_actions'] != {'next': 2, 'pending': []}
            or any(a['process_action'] != -1 for a in original['native_actor_values'])
            or any(a['strike'] or a['reaction'] or a['retiring_sequences'] or a['first_person']
                   for a in original['native_melee_states'])):
        raise ValueError('Player strike-tail source must have known idle native state')
    state = copy.deepcopy(original)
    equipped = [i for i in state['player']['inventory'] if i['equipped_slots'] == 65536]
    if len(equipped) != 1 or equipped[0]['base'] != recipe['weapon_base']:
        raise ValueError('Player strike-tail equipped source weapon differs')
    equipped[0]['equipped_slots'] = 0
    changes = []
    for actor in actors:
        matches = [a for a in state['actor_ai'] if a['actor'] == actor]
        if len(matches) != 1:
            raise ValueError('Player strike-tail source NPC is missing or duplicated')
        changes.append(dict(actor=actor, restrained_before=matches[0]['restrained']))
        matches[0]['restrained'] = True
    output.parent.mkdir(parents=True, exist_ok=True)
    runtime.write_save(source, output, state)
    expected = copy.deepcopy(state)
    expected['schema_version'] = 58
    if runtime.load_save(output) != expected:
        raise ValueError('Player strike-tail output differs outside declared fixture fields')
    if hashlib.sha256(source.read_bytes()).hexdigest() != source_hash:
        raise ValueError('Player strike-tail source changed during staging')
    report = dict(structural_write_success=True, semantic_reinspection_passed=True,
                  source=str(source), source_sha256=source_hash,
                  output_sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
                  source_schema=recipe['source_schema'], target_schema=58,
                  mutations=dict(player_weapon_equipped_slots=[65536, 0], npc_restrained=changes),
                  all_other_decoded_state_preserved=True, compiler_world_validation=False,
                  runtime_acceptance=False)
    output.with_suffix('.staging.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--recipe', type=Path, default=Path(__file__).parent /
                        'data/oblivion_compat/m15_player_strike_tail_input.json')
    args = parser.parse_args()
    print(json.dumps(stage(args.source, args.output, json.loads(args.recipe.read_text())), indent=2))


if __name__ == '__main__':
    main()
