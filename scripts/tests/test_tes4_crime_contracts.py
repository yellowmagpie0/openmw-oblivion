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
