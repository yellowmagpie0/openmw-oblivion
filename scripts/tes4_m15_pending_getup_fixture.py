"""Stage declared pending GetUp ownership for a save-format persistence course.

This injects synthetic request state. It does not select an IDLE, queue IO or
establish recovery gameplay acceptance. Use an actual current-schema seed.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path

import tes4_runtime_state as runtime


def stage(source: Path, output: Path, recipe: dict) -> dict:
    fields = {'schema_version', 'purpose', 'source', 'source_sha256',
              'target_schema', 'actor', 'process_knocked_state', 'process_action', 'playback'}
    if (not isinstance(recipe, dict) or set(recipe) != fields
            or type(recipe['schema_version']) is not int or recipe['schema_version'] != 1
            or type(recipe['target_schema']) is not int
            or recipe['target_schema'] != runtime.CURRENT_VERSION
            or recipe['target_schema'] != 74
            or type(recipe['process_knocked_state']) is not int or recipe['process_knocked_state'] != 5
            or type(recipe['process_action']) is not int or recipe['process_action'] != -1
            or not isinstance(recipe['playback'], dict)
            or recipe['playback'].get('actor') != recipe['actor']):
        raise ValueError('unsupported pending GetUp persistence recipe')
    if output.exists():
        raise FileExistsError(output)
    digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    source_hash = digest(source)
    if source_hash != recipe['source_sha256']:
        raise ValueError('pending GetUp seed differs from pinned save')
    original = runtime.load_save(source)
    actor = recipe['actor']
    values = [v for v in original['native_actor_values'] if v['actor'] == actor]
    if (original['schema_version'] != recipe['target_schema'] or len(values) != 1
            or values[0]['process'] == 0
            or any(v['actor'] == actor for v in original['native_getup_playback'])
            or any(v['actor'] == actor for v in original['native_getup_idles'])):
        raise ValueError('pending GetUp seed requires current active authority without prior GetUp state')
    state = copy.deepcopy(original)
    selected = next(v for v in state['native_actor_values'] if v['actor'] == actor)
    selected['process_knocked_state'] = recipe['process_knocked_state']
    selected['process_action'] = recipe['process_action']
    state['native_getup_playback'].append(copy.deepcopy(recipe['playback']))
    state['native_getup_playback'].sort(key=lambda graph: graph['actor'])
    # Validate the complete graph, including source/base ownership, before writing.
    runtime.encode_payload(state)
    output.parent.mkdir(parents=True, exist_ok=True)
    runtime.write_save(source, output, state)
    if runtime.load_save(output) != state:
        raise ValueError('pending GetUp output differs outside declared fields')
    if digest(source) != source_hash:
        raise ValueError('pending GetUp source changed during staging')
    report = dict(structural_write_success=True, semantic_reinspection_passed=True,
                  source=str(source), source_sha256=source_hash, output_sha256=digest(output),
                  source_schema=original['schema_version'], target_schema=recipe['target_schema'],
                  mutations=['declared actor knocked byte and action', 'declared pending request graph'],
                  all_other_decoded_state_preserved=True, compiler_world_validation=False,
                  runtime_acceptance=False, normal_input_recovery=False, io_queued=False)
    output.with_suffix('.staging.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--recipe', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(stage(args.source, args.output, json.loads(args.recipe.read_text())), indent=2))


if __name__ == '__main__':
    main()
