import copy
import json
import struct
import unittest
from pathlib import Path

from scripts import tes4_crime_contracts as crime_io
from scripts import tes4_runtime_state as state_io
from scripts.tests.test_tes4_runtime_state import make_state


def fixture():
    return json.loads((Path(__file__).parent / 'data/tes4-crime42.json').read_text())


def current_state():
    state = make_state()
    state['schema_version'] = 42
    state['ai_rng_state'] = 1
    state_io._upgrade_inventory(state['player']['inventory'])
    state['native_crime'] = crime_io.empty_state()
    return state


class CrimeContracts42Tests(unittest.TestCase):
    def state(self):
        state = current_state()
        state['physical_actions'] = {'next': 10, 'pending': []}
        state['native_crime'] = fixture()
        return state

    def test_populated_native_codec_and_property_presence(self):
        state = self.state()
        payload = state_io.encode_payload(state)
        golden = bytes.fromhex((Path(__file__).parent / 'data/tes4-crime42-wire.hex').read_text())
        self.assertEqual(payload[-len(golden):], golden)
        for offset in range(1, 6):
            bad = bytearray(payload); bad[-offset] = 2
            with self.assertRaises(state_io.RuntimeStateError): state_io.decode_payload(bytes(bad))
        decoded = state_io.decode_payload(payload)
        self.assertEqual(decoded['native_crime'], state['native_crime'])
        self.assertEqual(state_io.encode_payload(decoded), state_io.encode_payload(state))
        crime = decoded['native_crime']; item = crime['jails'][0]['property'][0]
        self.assertEqual(item['condition'], 37.125)
        self.assertEqual(item['charge'], 9.25)
        self.assertEqual(item['original_ownership_rank'], -2)
        self.assertTrue(item['quest_item'])

    def test_empty_suffix_has_independent_wire_layout(self):
        state = current_state()
        payload = state_io.encode_payload(state)
        self.assertEqual(payload[-36:], struct.pack('<QQQIII', 1, 1, 1, 0, 0, 0))
        corrupt = payload[:-12] + struct.pack('<I', 1_000_001) + payload[-8:]
        with self.assertRaises(state_io.RuntimeStateError): state_io.decode_payload(corrupt)
        for remove in range(1, 37):
            with self.assertRaises(state_io.RuntimeStateError): state_io.decode_payload(payload[:-remove])
        with self.assertRaises(state_io.RuntimeStateError): state_io.decode_payload(payload + b'\0')

    def test_versions_one_through_41_keep_original_layout_and_do_not_infer_crime(self):
        # Existing minimal old-version fields are maintained by the writer.
        for version in range(1, 42):
            state = make_state(); state['schema_version'] = version
            state['ai_rng_state'] = 1
            if version < 2:
                state['script_event_sequence'] = 0; state['script_instances'] = []; state['quests'] = []
            for item in state['player']['inventory']:
                item.pop('ownership_rank', None); item.pop('ownership_global', None)
                if version < 4:
                    for name in ['condition', 'charge', 'equipped_slots', 'hotkey', 'owner', 'remaining_usage_time']:
                        item.pop(name, None)
            if version < 3:
                for name in ['name','race','class','birthsign','female','character_generation_flags']:
                    state['player'].pop(name, None)
            if version < 4:
                for ref in state['references']:
                    for item in ref['inventory']:
                        for name in ['condition','charge','equipped_slots','hotkey','owner','remaining_usage_time']:
                            item.pop(name, None)
            payload = state_io.encode_payload(state)
            decoded = state_io.decode_payload(payload)
            self.assertNotIn('native_crime', decoded)
            self.assertEqual(state_io.encode_payload(decoded), payload)
            state['native_crime'] = fixture()
            with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(state)

    def test_causal_deduplication_commit_order_and_wire_domains(self):
        state = self.state()
        mutations = [
            lambda s: s['physical_actions'].update(next=9),
            lambda s: s['physical_actions'].update(pending=[9]),
            lambda s: s['native_crime']['arrests'][0].update(incident=2),
            lambda s: s['native_crime']['jails'][0].update(property_committed=False),
            lambda s: s['native_crime']['incidents'][0]['request'].update(action=2**64),
            lambda s: s['native_crime']['incidents'][0]['outcome'].update(consequences_committed=2),
            lambda s: s['native_crime']['jails'][0]['property'][0].update(condition=float('nan')),
            lambda s: s['native_crime']['incidents'].append(copy.deepcopy(s['native_crime']['incidents'][0])),
            lambda s: s['native_crime']['incidents'][0]['request'].update(victim='content:Oblivion.esm:000001'),
        ]
        for mutate in mutations:
            bad = copy.deepcopy(state); mutate(bad)
            with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(bad)
        first = state['native_crime']['incidents'][0]
        second = copy.deepcopy(first); second['outcome']['incident'] = 2
        state['native_crime']['incidents'].append(second)
        with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(state)
        second['request']['victim'] = 'content:oblivion.esm:000099'
        self.assertEqual(state_io.decode_payload(state_io.encode_payload(state))['native_crime'], state['native_crime'])

    def test_player_reference_alias_comparison_preserves_wire_keys(self):
        player = 'dynamic:player:0000000000000001'
        alias = 'content:oblivion.esm:000014'
        state = self.state(); crime = state['native_crime']
        crime['arrests'][0]['actor'] = alias
        crime['jails'][0]['actor'] = alias
        self.assertEqual(state_io.decode_payload(state_io.encode_payload(state))['native_crime'], crime)
        duplicate = copy.deepcopy(crime['incidents'][0]); duplicate['outcome']['incident'] = 2
        duplicate['request']['perpetrator'] = alias; crime['incidents'].append(duplicate)
        with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(state)
        for distinct in ['content:other.esm:000014', 'content:oblivion.esm:000007']:
            duplicate['request']['perpetrator'] = distinct
            self.assertEqual(state_io.decode_payload(state_io.encode_payload(state))['native_crime'], crime)
        crime['incidents'].pop()
        witnesses = [{'witness': player, 'observed': True, 'will_report': False},
                     {'witness': alias, 'observed': True, 'will_report': False}]
        crime['incidents'][0]['outcome']['witnesses'] = witnesses
        with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(state)
        witnesses[-1]['witness'] = 'content:other.esm:000014'
        self.assertEqual(state_io.decode_payload(state_io.encode_payload(state))['native_crime'], crime)
        crime['incidents'][0]['request'].update(victim=player, affected_reference=player)
        duplicate = copy.deepcopy(crime['incidents'][0]); duplicate['outcome']['incident'] = 2
        duplicate['request'].update(victim=alias, affected_reference=alias)
        crime['incidents'].append(duplicate)
        with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(state)

    def test_fractional_bounty_43_and_lossless_legacy_int32_migration(self):
        state = self.state(); state['schema_version'] = 43
        for amount in [.5, .25, -.25, -0.0, 16777217, 2147483647, -2147483648]:
            state['native_crime']['incidents'][0]['outcome']['bounty_delta'] = amount
            payload = state_io.encode_payload(state)
            if amount == .5:
                golden = bytes.fromhex((Path(__file__).parent / "data/tes4-crime43-wire.hex").read_text())
                self.assertEqual(payload[-len(golden):], golden)
            decoded = state_io.decode_payload(payload)
            self.assertEqual(struct.pack('<d', decoded['native_crime']['incidents'][0]['outcome']['bounty_delta']),
                             struct.pack('<d', amount))
            self.assertEqual(state_io.encode_payload(decoded), payload)
            old = copy.deepcopy(state); old['schema_version'] = 42
            if amount != int(amount) or (amount == 0 and struct.pack('<d', amount)[-1] == 128):
                with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(old)
            else:
                old_payload = state_io.encode_payload(old)
                self.assertEqual(state_io.encode_payload(state_io.decode_payload(old_payload)), old_payload)
                self.assertEqual(state_io.decode_payload(old_payload)['native_crime'], old['native_crime'])
        for bad in [float('inf'), float('nan'), 1e100, -1e100]:
            state['native_crime']['incidents'][0]['outcome']['bounty_delta'] = bad
            with self.assertRaises(state_io.RuntimeStateError): state_io.encode_payload(state)
