"""Stage the idle S4 save for the cached-block overlay with a typed migration.

Only the one declared idle object script's winning origin changes. The codec
adds enumerated empty schema fields. All other decoded state must match exactly.
This fixture preparation does not grant general save/script rebasing support.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path

import tes4_runtime_state as runtime
from tes4_m15_stationary_fixture import fields


def stage(source: Path, output: Path, recipe: dict) -> dict:
    fields(recipe, 'version purpose source source_sha256 source_schema target_schema old_unit new_unit context locals')
    if (type(recipe['version']) is not int or recipe['version'] != 1
            or type(recipe['source_schema']) is not int or recipe['source_schema'] != 46
            or type(recipe['target_schema']) is not int or recipe['target_schema'] != runtime.CURRENT_VERSION
            or runtime.CURRENT_VERSION not in (59, 60, 61, 62, 63, 64)):
        raise ValueError('unsupported cached-block input recipe/schema')
    digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    source_hash = digest(source)
    if source_hash != recipe['source_sha256']:
        raise ValueError('cached-block input differs from pinned source')
    if output.exists():
        raise FileExistsError(output)
    old_unit = 'content:m15stationaryopponent.esp:000881@m15stationaryopponent.esp/object/unit=0'
    new_unit = old_unit.replace('@m15stationaryopponent.esp/', '@m15cachedblockopponent.esp/')
    if (recipe['old_unit'] != old_unit or recipe['new_unit'] != new_unit
            or recipe['context'] != 'content:oblivion.esm:01fc43'
            or recipe['locals'] != [{'type': 'number', 'value': 0}]):
        raise ValueError('unsupported cached-block idle script identity/layout')
    original = runtime.load_save(source)
    if original['schema_version'] != recipe['source_schema']:
        raise ValueError('cached-block input source schema differs')
    state = copy.deepcopy(original)
    instances = [s for s in state['script_instances'] if s['unit'] == old_unit]
    if len(instances) != 1 or instances[0]['context'] != recipe['context'] or instances[0]['locals'] != recipe['locals']:
        raise ValueError('cached-block source must have exactly the declared idle script instance')
    before = copy.deepcopy(instances[0])
    instances[0]['unit'] = new_unit
    output.parent.mkdir(parents=True, exist_ok=True)
    runtime.write_save(source, output, state)
    expected = copy.deepcopy(state)
    expected['schema_version'] = recipe['target_schema']
    expected.setdefault('native_loose_weapons', [])
    expected.setdefault('native_movement', None)
    expected.setdefault('spoken_dialogue_infos', [])
    for reference in expected['references']:
        reference.setdefault('dropper', None)
        reference.setdefault('dropped_items', None)
    for items in [expected['player']['inventory']] + [r['inventory'] for r in expected['references']]:
        for item in items:
            item.setdefault('extra_scale', None)
    for melee in expected['native_melee_states']:
        melee.setdefault('reaction', None)
        melee.setdefault('retiring_sequences', [])
        melee.setdefault('first_person', None)
        melee.setdefault('passive_sequence', None)
        melee.setdefault('held_blocks', [])
    actual = runtime.load_save(output)
    canonical = lambda value: json.dumps(value, sort_keys=True, separators=(',', ':'))
    if canonical(actual) != canonical(expected):
        raise ValueError('cached-block typed migration changed state outside its declared fixture fields')
    if digest(source) != source_hash:
        raise ValueError('cached-block input source changed during staging')
    report = dict(structural_write_success=True, semantic_reinspection_passed=True,
                  compiler_world_validation='not performed by this save staging helper', runtime_acceptance=False,
                  source=str(source), source_sha256=source_hash, staged_sha256=digest(output),
                  source_schema=recipe['source_schema'], target_schema=recipe['target_schema'],
                  script_before=before, script_after=instances[0],
                  original_reference_inventories_checked=len(original['references']),
                  state_other_than_declared_script_and_empty_schema_defaults_preserved=True,
                  schema_defaults={'reference.dropper': None, 'reference.dropped_items': None,
                                   'inventory.extra_scale': None, 'melee.reaction': None,
                                   'melee.retiring_sequences': [], 'melee.first_person': None,
                                   'melee.passive_sequence': None, 'melee.held_blocks': [],
                                   'native_loose_weapons': [], 'native_movement': None, 'spoken_dialogue_infos': []})
    output.with_suffix('.staging.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--recipe', type=Path,
                        default=Path(__file__).parent / 'data/oblivion_compat/m15_cached_block_input.json')
    args = parser.parse_args()
    print(json.dumps(stage(args.source, args.output, json.loads(args.recipe.read_text())), indent=2))


if __name__ == '__main__':
    main()
