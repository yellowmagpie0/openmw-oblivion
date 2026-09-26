#!/usr/bin/env python3
"""Verify a GoogleTest XML result against an independent list-tests inventory."""
import argparse
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET


def inventory(text):
    names, suite = set(), None
    for raw in text.splitlines():
        line = raw.split('#', 1)[0].rstrip()
        if not line:
            continue
        if not line.startswith(' ') and line.endswith('.'):
            suite = line
        elif line.startswith('  ') and suite:
            name = suite + line.strip()
            if name in names:
                raise ValueError(f'duplicate inventory entry: {name}')
            names.add(name)
    if not names:
        raise ValueError('empty test inventory')
    return names


def matches(name, expression):
    positive, sep, negative = expression.partition('-')
    def any_match(patterns):
        return any(re.fullmatch(re.escape(p).replace(r'\*', '.*').replace(r'\?', '.'), name)
                   for p in patterns.split(':') if p)
    return any_match(positive or '*') and not (sep and any_match(negative))


def verify(list_path, xml_path, expression='*'):
    expected = {n for n in inventory(Path(list_path).read_text()) if matches(n, expression)}
    if not expected:
        raise ValueError('filter selects no tests')
    root = ET.parse(xml_path).getroot()
    nodes = list(root.iter('testcase'))
    names = [n.attrib['classname'] + '.' + n.attrib['name'] for n in nodes]
    if len(names) != len(set(names)):
        raise ValueError('duplicate XML test result')
    actual = set(names)
    if actual != expected:
        raise ValueError(f'inventory mismatch: missing={sorted(expected - actual)}, unexpected={sorted(actual - expected)}')
    bad = [name for name, node in zip(names, nodes)
           if node.get('status') != 'run' or node.get('result') != 'completed'
           or any(child.tag in {'failure', 'error', 'skipped'} for child in node)]
    if bad or any(n.tag in {'failure', 'error', 'skipped'} for n in root.iter()):
        raise ValueError(f'failed, skipped or unexecuted tests: {bad}')
    for attr in ('failures', 'errors', 'disabled'):
        if int(root.get(attr, '0')):
            raise ValueError(f'nonzero XML {attr} count')
    if int(root.get('tests', len(nodes))) != len(nodes):
        raise ValueError('XML aggregate test count disagrees with cases')
    return {'executed': len(nodes), 'inventory_matches': True, 'failed': 0, 'skipped': 0, 'filter': expression}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inventory', required=True, type=Path)
    parser.add_argument('--xml', required=True, type=Path)
    parser.add_argument('--filter', default='*')
    args = parser.parse_args()
    try:
        print(json.dumps(verify(args.inventory, args.xml, args.filter), indent=2))
    except (ValueError, KeyError, OSError, ET.ParseError) as error:
        parser.exit(1, f'verification failed: {error}\n')


if __name__ == '__main__':
    main()
