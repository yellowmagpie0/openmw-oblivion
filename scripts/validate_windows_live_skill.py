#!/usr/bin/env python3
"""Exercise native running skill progression in a private rendered Windows save.

This is a Lua-controls integration probe, not a normal-input gameplay course.
The editable driver and manifest are in data/oblivion_compat/windows_live_skill.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

from benchmark_windows import close_process


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    repo = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--save', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--build', type=Path, default=repo / 'build-windows')
    parser.add_argument('--exe', type=Path)
    parser.add_argument('--data', type=Path,
                        default=Path('C:/Program Files (x86)/Steam/steamapps/common/Oblivion/Data'))
    parser.add_argument('--timeout', type=float, default=180)
    parser.add_argument('--expect-legacy-errors', action='store_true')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This probe requires Windows')
    args.build = args.build.resolve()
    executable = (args.exe or args.build / 'openmw.exe').resolve()
    save = args.save.resolve()
    output = args.output.resolve()
    fixture = repo / 'scripts/data/oblivion_compat/windows_live_skill'
    for path in (executable, save, args.data / 'Oblivion.esm'):
        if not path.is_file():
            parser.error(f'Missing {path}')
    if args.timeout <= 0:
        parser.error('Timeout must be positive')
    output.mkdir(parents=True, exist_ok=False)
    (output / 'user').mkdir()
    (output / 'local').mkdir()
    config = {'resources': args.build / 'resources', 'data-local': output / 'local', 'user-data': output / 'user'}
    lines = ['replace=config'] + [f'{key}="{path.as_posix()}"' for key, path in config.items()]
    lines += [f'data="{args.data.as_posix()}"', f'data="{fixture.as_posix()}"',
              'content=Oblivion.esm', 'content=windows_live_skill.omwscripts', 'no-grab=1']
    (output / 'openmw.cfg').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    (output / 'settings.cfg').write_text(
        '[Video]\nfullscreen = false\nresolution x = 1280\nresolution y = 720\n'
        'vsync = false\nframerate limit = 0\n[Models]\nload unsupported nif files = true\n', encoding='utf-8')
    prefix = repo / 'deps/vcpkg-x64-windows-2022-2026-02-24/installed/x64-windows'
    env = {key: value for key, value in os.environ.items() if not key.startswith(('OPENMW_', 'SDL_'))}
    env['PATH'] = os.pathsep.join((str(prefix / 'bin'), str(prefix / 'bin/Release'), env['PATH']))
    env['OSG_LIBRARY_PATH'] = str(prefix / 'bin/osgPlugins-3.6.5')
    result = {'binary_sha256': sha256(executable), 'save_sha256': sha256(save),
              'fixture_sha256': {p.name: sha256(p) for p in sorted(fixture.iterdir()) if p.is_file()},
              'runner_sha256': sha256(Path(__file__)), 'expect_legacy_errors': args.expect_legacy_errors}
    forced = False
    timed_out = False
    with (output / 'stdout.log').open('wb') as stdout, (output / 'stderr.log').open('wb') as stderr:
        process = subprocess.Popen([str(executable), '--replace=config', '--config', str(output),
                                    '--game-profile=oblivion', '--load-savegame', str(save), '--random-seed=12345'],
                                   cwd=args.build, env=env, stdout=stdout, stderr=stderr)
        try:
            process.wait(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
        finally:
            if process.poll() is None:
                forced = close_process(process)
    log = (output / 'openmw.log').read_text(encoding='utf-8', errors='replace')
    result.update(exit_code=process.returncode, forced_shutdown=forced, timed_out=timed_out,
                  began='WINDOWS_LIVE_SKILL_BEGIN' in log, passed='WINDOWS_LIVE_SKILL_PASS' in log,
                  legacy_skill_errors=log.count('Error in DelayedAction StatUpdateAction: native skills require'),
                  focus_errors=log.count('Error updating focus object'), frame_errors=log.count('Error in frame'),
                  input_unchanged=sha256(save) == result['save_sha256'],
                  binary_unchanged=sha256(executable) == result['binary_sha256'])
    result['ok'] = (process.returncode == 0 and not forced and not timed_out and result['began'] and result['passed']
                    and not result['frame_errors'] and result['input_unchanged'] and result['binary_unchanged']
                    and (result['legacy_skill_errors'] > 0 if args.expect_legacy_errors else
                         result['legacy_skill_errors'] == 0 and result['focus_errors'] == 0))
    (output / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result, indent=2), flush=True)
    if not result['ok']:
        raise SystemExit('Rendered skill validation failed; inspect the retained result and logs')


if __name__ == '__main__':
    main()
