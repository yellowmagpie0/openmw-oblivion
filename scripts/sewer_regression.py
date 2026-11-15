#!/usr/bin/env python3
"""Small sewer suite: existing assembler/scenario runner plus retained-save audits."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import time
import traceback

from tes4_runtime_state import load_save
from verify_oblivion_sewer_escape import audit as audit_integration, audit_course, require

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / 'scripts/data/oblivion_compat/sewer_regression_checkpoints.json'
CONTROLLER = '030006c95e0400008e02000010010000,OpenMW Sewer Test Controller,a:b0,b:b1,x:b2,y:b3,back:b6,guide:b8,start:b7,leftstick:b9,rightstick:b10,leftshoulder:b4,rightshoulder:b5,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a3,righty:a4,lefttrigger:a2,righttrigger:a5,platform:Linux,'


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + '\n')


def checkpoint(name, catalog, directory):
    spec = catalog['checks'][name]
    local = directory / (name + '.omwsave')
    binding = directory / (name + '.json')
    candidates = [local, ROOT / spec['path']]
    for path in candidates:
        if not path.is_file():
            continue
        digest = sha(path)
        if digest == spec['sha256']:
            return path.resolve(), {'kind': 'retained canonical checkpoint', **spec, 'resolved': str(path.resolve())}
        if path == local and binding.is_file():
            meta = json.loads(binding.read_text())
            report = Path(meta['source_report'])
            require(meta['sha256'] == digest and meta['checkpoint'] == name, 'Regenerated checkpoint hash/name mismatch')
            source = json.loads(report.read_text())
            require(source['status'] == 'passed' and source['mode'] == 'regenerate', 'Unverified checkpoint regeneration')
            require(source['checkpoint_sha256'] == digest and source['checkpoint'] == name, 'Regeneration provenance differs')
            return path.resolve(), meta
        raise ValueError(f'Checkpoint bytes differ from provenance: {path}')
    raise FileNotFoundError(f'Missing {name} checkpoint; see docs/oblivion/SEWER-REGRESSION.md or run regenerate --checkpoint {name}')


def classify(scenario, native, death, infrastructure=None):
    if infrastructure:
        return 'infrastructure', str(infrastructure)
    if scenario.get('exit_code') in (-11, -6) or any('AddressSanitizer' in str(x) or 'runtime error:' in str(x)
                                                  for x in scenario.get('forbidden_log_findings', [])):
        return 'engine', 'Crash or sanitizer diagnostic'
    if native and (native.get('error_count', 0) or native.get('pending_count', 0)):
        return 'engine', 'Native error or unfinished native work'
    if death:
        return 'automated_player', death
    if not scenario.get('passed'):
        return 'unresolved_course_failure', 'Input/route/observation gate failed; inspect diagnostics before blaming the engine or player'
    return 'engine_or_persistence', 'Native/save/restore audit failed; inspect audit-error.txt'


def process_identity(pid, course):
    proc = Path('/proc') / str(pid)
    raw = (proc / 'stat').read_text()
    fields = raw[raw.rfind(')') + 2:].split()
    require(fields[0] != 'Z', 'Game already ended before identity capture')
    require(str(course / 'config') in (proc / 'cmdline').read_bytes().decode(), 'Native PID is not this course')
    exe = (proc / 'exe').resolve()
    return {'game_pid': pid, 'process_start_ticks': fields[19], 'exe': str(exe), 'exe_sha256': sha(exe)}


def game_live(identity):
    try:
        raw = Path('/proc', str(identity['game_pid']), 'stat').read_text()
        f = raw[raw.rfind(')') + 2:].split()
        return f[0] != 'Z' and f[19] == identity['process_start_ticks']
    except FileNotFoundError:
        return False


def assert_no_game():
    for p in Path('/proc').iterdir():
        if p.name.isdigit():
            try:
                require(not ((p / 'comm').read_text().strip() == 'openmw'
                            and (p / 'stat').read_text().split()[2] != 'Z'), f'Another game is live: {p.name}')
            except (FileNotFoundError, ProcessLookupError, PermissionError):
                pass


def prepare(args, out, segments=None, input_save=None, reload_manifest=None):
    folder = out / 'assembly'
    folder.mkdir(parents=True)
    manifest = folder / 'course.json'
    if reload_manifest:
        data = json.loads((ROOT / reload_manifest).read_text())
    else:
        cmd = [sys.executable, str(ROOT / 'scripts/assemble_oblivion_sewer_course.py'), '--output', str(manifest)]
        if segments is not None:
            selection = json.loads((ROOT / 'scripts/data/oblivion_compat/oblivion_sewer_continuous_selection.json').read_text())
            selection['segments'] = [x for x in selection['segments'] if x['segment'] in segments]
            write(folder / 'selection.json', selection)
            cmd += ['--selection', str(folder / 'selection.json')]
        with (folder / 'assembly.log').open('w') as assembly_log:
            subprocess.run(cmd, cwd=ROOT, check=True, stdout=assembly_log)
        data = json.loads(manifest.read_text())
    data['command'] = [str(args.openmw), '--replace=config', '--config', '{output}/config', '--no-sound=1', '--no-grab']
    if input_save:
        data['command'].append('--load-savegame=' + str(input_save))
    for f in data['files']:
        if f['path'] == 'config/openmw.cfg':
            import re
            f['content'] = re.sub(r'new-game=\d', 'new-game=' + ('0' if input_save else '1'), f['content'])
    data['xvfb'] = True
    data['sdl_xvfb_input'] = True
    data.pop('sdl_offscreen_input', None)
    data['sdl_input_timeout_seconds'] = 90
    data['environment'] = {'OPENMW_OBSCRIPT_EVENTS': '{output}/test-data/events.txt',
                           'OPENMW_M15_EVENTS': '{output}/combat-events.jsonl', 'OPENMW_M15_RUN_ID': 'sewer-regression',
                           'OPENMW_M15_EPOCH': '3', 'OPENMW_OBLIVION_AI_EVENTS': '{output}/ai-events.jsonl',
                           'OPENMW_OBLIVION_AI_SEED': '1414', 'SDL_GAMECONTROLLERCONFIG': CONTROLLER}
    if args.library_path:
        data['environment']['LD_LIBRARY_PATH'] = str(args.library_path)
    write(manifest, data)
    return manifest


def execute(args, course, manifest, input_save=None, new_game=False):
    assert_no_game()
    cmd = [sys.executable, str(ROOT / 'scripts/oblivion_compat.py'), 'scenario', str(manifest), '--output', str(course)]
    for k, v in [('source', ROOT), ('openmw', args.openmw), ('resources', args.resources), ('oblivion_data', args.data)]:
        cmd += ['--variable', k + '=' + str(v)]
    with (course.parent / (course.name + '-driver.log')).open('w') as log:
        environment = {k: v for k, v in os.environ.items() if not k.startswith(('OPENMW_', 'SDL_', 'OSG_')) and k != 'LD_PRELOAD'}
        driver = subprocess.Popen(cmd, cwd=ROOT, env=environment, stdout=log, stderr=subprocess.STDOUT)
        identity = None
        archiver = None
        death = None
        death_stopped = False
        offset = 0
        last_notice = 0
        while driver.poll() is None:
            eventfile = course / 'combat-events.jsonl'
            if identity is None and eventfile.exists():
                lines = eventfile.read_text().splitlines()
                if lines:
                    try:
                        event = json.loads(lines[0])
                        identity = process_identity(event['pid'], course)
                        write(course / 'runtime-identity.json', identity)
                        with (course / 'archive.log').open('w') as archive_log:
                            archiver = subprocess.Popen([sys.executable, str(ROOT / 'scripts/archive_oblivion_sewer_saves.py'),
                                                         str(course), '--game-pid', str(event['pid'])], cwd=ROOT,
                                                        stdout=archive_log, stderr=subprocess.STDOUT)
                    except (json.JSONDecodeError, FileNotFoundError):
                        pass
            processlog = course / 'process.log'
            if processlog.exists():
                with processlog.open('rb') as stream:
                    stream.seek(offset)
                    chunk = stream.read()
                    complete = chunk.rfind(b'\n') + 1
                    offset += complete
                for line in chunk[:complete].decode(errors='replace').splitlines():
                    if 'M15 native death callback:' in line and 'actor=dynamic:player:' in line:
                        death = line
            if death and not death_stopped and identity and game_live(identity):
                # End only a verified dead-Player case, never a healthy observation wait.
                os.kill(identity['game_pid'], signal.SIGTERM)
                death_stopped = True
            if time.monotonic() - last_notice > 30:
                print(f'[{course.name}] runner live; game {identity["game_pid"] if identity else "starting"} ' + ('live' if identity and game_live(identity) else 'starting/ended'), flush=True)
                last_notice = time.monotonic()
            time.sleep(.2)
        if archiver:
            try:
                archiver.wait(timeout=10)
            except subprocess.TimeoutExpired:
                if identity and game_live(identity):
                    raise RuntimeError(f'Controller ended but game remains live: {identity["game_pid"]}; preserve it for investigation')
                archiver.terminate()
                archiver.wait(timeout=5)
    require(not identity or not game_live(identity), 'Game remains live after scenario end')
    scenario_path = course / 'scenario.json'
    if not scenario_path.exists():
        return {'status': 'failed', 'classification': 'infrastructure', 'reason': 'No terminal scenario report', 'diagnostics': str(course)}
    scenario = json.loads(scenario_path.read_text())
    native = None
    events = course / 'combat-events.jsonl'
    if events.exists():
        ends = [json.loads(x) for x in events.read_text().splitlines() if json.loads(x).get('event') == 'run-end']
        native = ends[-1] if ends else None
    if not scenario['passed']:
        kind, reason = classify(scenario, native, death)
        return {'status': 'failed', 'classification': kind, 'reason': reason,
                'diagnostics': str(course), 'last_action': {k: scenario['actions'][-1].get(k) for k in ('type', 'pattern', 'reason')} if scenario['actions'] else None}
    try:
        audit, state = audit_course(course, input_save, allow_startup_deferrals=new_game)
        return {'status': 'passed', 'classification': 'native_input_save_checks', 'diagnostics': str(course),
                'audit': str(course / 'regression-audit.json'), 'game_pid': audit['game_pid'],
                'save': audit['save'], 'save_sha256': audit['save_sha256'], 'state': state}
    except (ValueError, OSError, KeyError, AssertionError, RuntimeError, StopIteration) as error:
        (course / 'audit-error.txt').write_text(str(error) + '\n')
        kind, reason = classify(scenario, native, death)
        return {'status': 'failed', 'classification': kind, 'reason': reason + ': ' + str(error), 'diagnostics': str(course)}


def semantics(name, state):
    life = {x['actor']: x for x in state['native_actor_life']}
    ref = lambda x: 'content:oblivion.esm:' + x
    if name in ('floor', 'combat', 'exit'):
        targets = {'floor': ['0491fc'], 'combat': ['03b543', '03b541'], 'exit': ['0666a7']}[name]
        require(all(life[ref(x)]['phase'] == 1 and life[ref(x)]['killer'] == state['player']['reference'] for x in targets), 'Missing native Player victories')
    if name in ('floor', 'combat'):
        require(state['player']['actor_values']['health.current'] >= 79.99, 'Recovery gate not retained')
    if name == 'combat':
        require(life[ref('03b52f')]['phase'] == 1, 'Eastern goblin survived; original death gate retained')
        require(state['player']['actor_values']['magicka.current'] >= 79.99, 'Full magicka recovery missing')
        require(life[ref('03b52e')]['phase'] == 0, 'Shaman killed before its later encounter')
    if name == 'story':
        require(80 in next(x for x in state['quests'] if x['quest'] == ref('01e723'))['completed_stages'], 'Original story stage80 missing')
        require(88 in next(x for x in state['quests'] if x['quest'] == ref('02466e'))['completed_stages'], 'Original class completion missing')
        base = next(x for x in state['native_actor_bases'] if x['base'] == ref('01e6ff'))
        require(base['factions'] == [[ref('014a20'), 1], [ref('0227bc'), 0]], 'Native faction replacement missing')
        require(next(x for x in state['references'] if x['key'] == ref('022812'))['owner'] == ref('0150bc'), 'Door ownership lost')
    if name == 'exit':
        require(state['player']['cell'] == ref('005e3d'), 'Physical outdoor exit missing')
        require(88 in next(x for x in state['quests'] if x['quest'] == ref('01e723'))['completed_stages'], 'Original outdoor story result missing')
        require(not state['player']['character_generation']['tracking'], 'Character generation still active')

    return {'health': state['player']['actor_values']['health.current'], 'cell': state['player']['cell'],
            'relevant_deaths': [v for k, v in life.items() if k in {ref(x) for x in ['0491fc', '03b543', '03b541', '03b52f', '0666a7']}]}


def summarize(args, results, mode, complete=True, **extra):
    clean = [{k: v for k, v in r.items() if k != 'state'} for r in results]
    if complete:
        names = {r['name'] for r in clean}
        for parent, child in [('story', 'story-own-reload'), ('exit', 'exit-own-reload'),
                              ('continuous-new-game', 'own-outdoor-reload')]:
            if parent in names and child not in names:
                clean.append({'name': child, 'status': 'could_not_run', 'classification': 'upstream_failure',
                              'reason': 'Parent course did not produce an accepted save; no substitute checkpoint used'})
    status = 'failed' if any(r['status'] == 'failed' for r in clean) else 'could_not_run' if any(r['status'] == 'could_not_run' for r in clean) else 'passed'
    if not complete:
        status = 'running'
    report = {'mode': mode, 'status': status, 'complete': complete, 'scope': 'Reusable sewer regression/closure; focused checks do not establish full escape acceptance',
              'full_escape_acceptance': False, 'rendered_review': 'separate manual gate',
              'runtime': {'openmw': str(args.openmw), 'sha256': sha(args.openmw) if args.openmw.is_file() else None},
              'checks': clean, 'reused_evidence': json.loads(CATALOG.read_text())['reused_evidence'], **extra}
    write(args.output / 'results.json', report)
    rows = ['# Sewer regression result', '', f'**{status}** — {mode}. Full sewer acceptance is separate.', '',
            '| Check | Status | Classification | Diagnostics |', '| --- | --- | --- | --- |']
    for r in clean:
        path = Path(r.get('diagnostics', args.output))
        target = os.path.relpath(path, args.output)
        rows.append(f'| {r["name"]} | {r["status"]} | {r.get("classification", "")} | [retained files]({target}/) |')
    rows += ['', 'Native/input/save checks and rendered review are distinct. No failed course is promoted to acceptance.', '', '[Machine-readable details](results.json)']
    (args.output / 'RESULTS.md').write_text('\n'.join(rows) + '\n')
    print(f'{status}: {args.output / "RESULTS.md"}', flush=True)
    return 1 if status == 'failed' else 2 if status == 'could_not_run' else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=['focused', 'integration', 'regenerate', 'list', 'checkpoint-path'])
    parser.add_argument('--output', type=Path)
    parser.add_argument('--only', action='append', choices=['floor', 'combat', 'story', 'exit'])
    parser.add_argument('--checkpoint', choices=['floor', 'combat', 'story', 'exit'])
    parser.add_argument('--checkpoint-dir', type=Path, default=ROOT / 'build/oblivion-compat/sewer-checkpoints')
    parser.add_argument('--openmw', type=Path, default=ROOT / 'build/openmw')
    parser.add_argument('--resources', type=Path, default=ROOT / 'build/resources')
    parser.add_argument('--library-path', type=Path)
    parser.add_argument('--data', type=Path, default=Path(os.environ.get('OBLIVION_DATA', str(Path.home() / '.local/share/Steam/steamapps/common/Oblivion/Data'))))
    args = parser.parse_args()
    catalog = json.loads(CATALOG.read_text())
    if args.mode == 'list':
        for name, spec in catalog['checks'].items():
            print(name + ': ' + spec['purpose'])
        return 0
    if args.mode == 'checkpoint-path':
        if not args.checkpoint:
            parser.error('--checkpoint required')
        try:
            print(checkpoint(args.checkpoint, catalog, args.checkpoint_dir)[0])
            return 0
        except (OSError, ValueError) as error:
            print(error, file=sys.stderr)
            return 2
    if not args.output:
        parser.error('--output required; old results are never overwritten')
    args.output = args.output.resolve()
    if args.output.exists():
        parser.error('output already exists; retain old evidence and choose a new directory')
    args.output.mkdir(parents=True)
    args.openmw, args.resources, args.data = args.openmw.resolve(), args.resources.resolve(), args.data.resolve()
    if args.library_path:
        args.library_path = args.library_path.resolve()
    results = []
    missing = [str(p) for p in [args.openmw, args.resources, args.data / 'Oblivion.esm'] if not p.exists()]
    if missing:
        names = args.only or list(catalog['checks']) if args.mode == 'focused' else [args.mode]
        return summarize(args, [{'name': name, 'status': 'could_not_run', 'classification': 'unavailable', 'reason': 'Missing runtime/assets: ' + ', '.join(missing)} for name in names], args.mode)
    if args.mode == 'focused':
        for name in dict.fromkeys(args.only or list(catalog['checks'])):
            phase = 'checkpoint_provenance'
            spec = catalog['checks'][name]
            try:
                save, provenance = checkpoint(name, catalog, args.checkpoint_dir)
                phase = 'infrastructure'
                out = args.output / name
                manifest = prepare(args, out, range(spec['segments'][0], spec['segments'][1] + 1), save)
                result = execute(args, out / 'course', manifest, save)
                result.update(name=name, checkpoint_provenance=provenance)
                if result['status'] == 'passed':
                    phase = 'engine_or_persistence'
                    result['semantic_checks'] = semantics(name, result['state'])
                    if spec.get('reload'):
                        own_save = Path(result['save'])
                        reload_out = out / 'reload'
                        reload_manifest = prepare(args, reload_out, input_save=own_save, reload_manifest=spec['reload'])
                        follow = execute(args, reload_out / 'course', reload_manifest, own_save)
                        follow['name'] = name + '-own-reload'
                        if follow['status'] == 'passed':
                            follow['semantic_checks'] = semantics(name, follow['state'])
                        results.append(result)
                        result = follow
                results.append(result)
            except (OSError, ValueError, RuntimeError, KeyError, StopIteration, AssertionError, subprocess.SubprocessError) as error:
                failure_dir = args.output / name
                failure_dir.mkdir(exist_ok=True)
                (failure_dir / 'failure-traceback.txt').write_text(traceback.format_exc())
                results.append({'name': name, 'status': 'could_not_run' if isinstance(error, FileNotFoundError) else 'failed',
                                'classification': 'unavailable' if isinstance(error, FileNotFoundError) else phase, 'reason': str(error), 'diagnostics': str(args.output / name)})
            summarize(args, results, 'focused', complete=False)
        return summarize(args, results, 'focused')
    if args.mode == 'regenerate':
        if not args.checkpoint:
            parser.error('--checkpoint required')
        if (args.checkpoint_dir / (args.checkpoint + '.omwsave')).exists():
            return summarize(args, [{'name': args.checkpoint, 'status': 'could_not_run', 'classification': 'checkpoint_provenance', 'reason': 'Local checkpoint already exists; choose a new --checkpoint-dir'}], 'regenerate')
        stop = catalog['checks'][args.checkpoint]['regenerate_stop_after']
        manifest = prepare(args, args.output / 'prefix', range(1, stop + 1))
        result = execute(args, args.output / 'prefix/course', manifest, new_game=True)
        result['name'] = args.checkpoint
        extra = {'checkpoint': args.checkpoint}
        if result['status'] == 'passed':
            args.checkpoint_dir.mkdir(parents=True, exist_ok=True)
            dest = args.checkpoint_dir / (args.checkpoint + '.omwsave')
            require(not dest.exists(), 'Refusing to overwrite a local checkpoint')
            shutil.copyfile(result['save'], dest)
            extra['checkpoint_sha256'] = sha(dest)
            write(args.checkpoint_dir / (args.checkpoint + '.json'), {'checkpoint': args.checkpoint, 'sha256': sha(dest),
                  'source_report': str(args.output / 'results.json'), 'source_course': result['diagnostics'], 'generated': True})
        return summarize(args, [result], 'regenerate', **extra)
    # Exactly one new-game attempt per invocation. No retry/tactics loop.
    try:
        manifest = prepare(args, args.output / 'full')
        result = execute(args, args.output / 'full/course', manifest, new_game=True)
        result['name'] = 'continuous-new-game'
        results.append(result)
        if result['status'] == 'passed':
            save = Path(result['save'])
            semantics('exit', result['state'])
            reload_manifest = prepare(args, args.output / 'outdoor-reload', input_save=save,
                                      reload_manifest='scripts/data/oblivion_compat/oblivion_sewer_joined_outdoor_reload.json')
            follow = execute(args, args.output / 'outdoor-reload/course', reload_manifest, save)
            follow['name'] = 'own-outdoor-reload'
            results.append(follow)
            if follow['status'] == 'passed':
                native = audit_integration(args.output / 'full/course', args.output / 'outdoor-reload/course')
                write(args.output / 'continuous-verification.json', native)
    except (OSError, ValueError, RuntimeError, KeyError, StopIteration, AssertionError, subprocess.SubprocessError) as error:
        (args.output / 'failure-traceback.txt').write_text(traceback.format_exc())
        results.append({'name': 'integration-audit', 'status': 'failed', 'classification': 'engine_or_infrastructure',
                        'reason': str(error), 'diagnostics': str(args.output)})
    return summarize(args, results, 'integration', continuous_and_own_reload_passed=all(r['status'] == 'passed' for r in results) and len(results) == 2)



if __name__ == '__main__':
    raise SystemExit(main())
