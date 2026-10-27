#!/usr/bin/env python3
"""Exercise the real native websocket client against a disposable Home Assistant."""
import base64
import hashlib
import json
from pathlib import Path
import socket
import struct
import subprocess
import threading

fixture = json.loads(Path('tests/fixture.json').read_text())
# A real install's entity registry is one ~3 MB message; the client must accept it.
registry_filler = [{'entity_id': f'sensor.filler_{i}', 'unique_id': f'filler_{i}', 'device_id': None,
                    'disabled_by': None, 'original_name': 'x' * 900,
                    'created_at': 1700000000.123456 + i, 'modified_at': 1.7e9} for i in range(3200)]
errors = []
commands = []
connections = 0
most_fetches = 0
reject_auth = False
stop = threading.Event()
server = socket.socket()
server.bind(('127.0.0.1', 0))
server.listen()
server.settimeout(.2)

def exact(conn, count):
    data = b''
    while len(data) < count:
        block = conn.recv(count - len(data))
        if not block:
            raise EOFError()
        data += block
    return data

def receive(conn):
    a, b = exact(conn, 2)
    n = b & 127
    if n == 126:
        n = struct.unpack('!H', exact(conn, 2))[0]
    elif n == 127:
        n = struct.unpack('!Q', exact(conn, 8))[0]
    assert n < 2 * 1024 * 1024
    assert b & 128, 'client frame must be masked'
    mask = exact(conn, 4)
    data = exact(conn, n)
    payload = bytes(value ^ mask[i % 4] for i, value in enumerate(data))
    if a & 15 == 8:
        raise EOFError()
    if a & 15 == 10:
        return {}
    assert a & 15 == 1
    return json.loads(payload)

def frame(conn, data, opcode=1, final=True):
    header = bytes([(128 if final else 0) | opcode])
    n = len(data)
    if n < 126:
        header += bytes([n])
    elif n < 65536:
        header += bytes([126]) + struct.pack('!H', n)
    else:
        header += bytes([127]) + struct.pack('!Q', n)
    conn.sendall(header + data)

def send(conn, value, fragmented=False):
    data = json.dumps(value, ensure_ascii=False).encode()
    if fragmented:
        middle = len(data) // 2
        frame(conn, data[:middle], final=False)
        frame(conn, b'alive', opcode=9)
        frame(conn, data[middle:], opcode=0)
    else:
        frame(conn, data)

def session(conn):
    global connections
    connections += 1
    conn.settimeout(12)
    request = b''
    while b'\r\n\r\n' not in request:
        request += exact(conn, 1)
    assert request.startswith(b'GET /api/websocket HTTP/1.1')
    headers = dict(line.split(b': ', 1) for line in request.split(b'\r\n')[1:] if b': ' in line)
    key = headers[b'Sec-WebSocket-Key']
    assert len(base64.b64decode(key)) == 16
    accept = base64.b64encode(hashlib.sha1(key + b'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest())
    conn.sendall(b'HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ' + accept + b'\r\n\r\n')
    send(conn, {'type': 'auth_required', 'ha_version': 'test'})
    assert receive(conn) == {'type': 'auth', 'access_token': 'fixture-token'}
    if reject_auth:
        send(conn, {'type': 'auth_invalid', 'message': 'fixture rejection'})
        return
    send(conn, {'type': 'auth_ok'})
    global most_fetches
    watch = None
    fetches = 0
    while not stop.is_set():
        message = receive(conn)
        if not message:
            continue
        kind, ident = message['type'], message['id']
        answer = {'id': ident, 'type': 'result', 'success': True, 'result': None}
        if kind == 'config/entity_registry/list':
            fetches += 1
            most_fetches = max(most_fetches, fetches)
            answer['result'] = fixture['entities'] + registry_filler
        elif kind == 'config/device_registry/list':
            answer['result'] = fixture['devices']
        elif kind == 'subscribe_events':
            if message['event_type'] == 'entity_registry_updated':
                watch = ident
        elif kind == 'subscribe_entities':
            assert all(entity.startswith(('select.', 'sensor.', 'button.', 'number.', 'text.')) for entity in message['entity_ids'])
            send(conn, answer)
            # Initial event may be fragmented, with a websocket ping interleaved.
            send(conn, {'id': ident, 'type': 'event', 'event': fixture['states']}, fragmented=True)
            continue
        elif kind == 'call_service':
            commands.append(message)
            entity = message['target']['entity_id']
            if entity.endswith('remote_up'):
                answer['success'] = False
                answer['error'] = {'code': 'service_not_found', 'message': 'fixture refusal'}
                send(conn, answer)
                for entry in fixture['entities']:
                    if entry['entity_id'] == entity:
                        entry['entity_id'] = 'button.renamed_up'
                fixture['states']['a']['button.renamed_up'] = fixture['states']['a'].pop(entity)
                # Unrelated registry churn must not interrupt the subscription.
                send(conn, {'id': watch, 'type': 'event', 'event': {'event_type': 'entity_registry_updated',
                     'data': {'action': 'update', 'entity_id': 'sensor.filler_1', 'changes': {}}}})
                send(conn, {'id': watch, 'type': 'event', 'event': {'event_type': 'entity_registry_updated',
                     'data': {'action': 'update', 'entity_id': 'button.renamed_up', 'old_entity_id': entity,
                              'changes': {'entity_id': entity}}}})
                continue
            if entity.endswith('remote_down'):
                # Drop before acknowledgement. This command must not be replayed.
                return
        elif kind == 'ping':
            send(conn, {'id': ident, 'type': 'pong'})
            continue
        elif kind != 'unsubscribe_events':
            raise AssertionError(kind)
        send(conn, answer)

def run():
    while not stop.is_set():
        try:
            conn, _ = server.accept()
        except socket.timeout:
            continue
        with conn:
            try:
                session(conn)
            except (EOFError, ConnectionResetError, BrokenPipeError):
                pass
            except Exception as exc:
                errors.append(repr(exc))
                return

thread = threading.Thread(target=run)
thread.start()
try:
    result = subprocess.run(['build/tests/prism-tests', '--integration', str(server.getsockname()[1])], timeout=25)
    assert result.returncode == 0
    reject_auth = True
    before = connections
    rejected = subprocess.run(['build/tests/prism-tests', '--auth-rejected', str(server.getsockname()[1])], timeout=10)
    assert rejected.returncode == 0
    assert connections == before + 1, 'rejected token retried automatically'
finally:
    stop.set()
    thread.join(timeout=3)
    server.close()
assert not errors, errors
assert result.returncode == 0
assert connections >= 2
assert most_fetches == 2, f'unrelated registry events refetched the registry ({most_fetches} fetches)'
assert sum(c['target']['entity_id'].endswith('remote_down') for c in commands) == 1, 'a disconnected command was replayed'
print('fake Home Assistant: handshake, auth, fragmented state, service ack/refusal, registry rename, reconnect and no replay passed')
