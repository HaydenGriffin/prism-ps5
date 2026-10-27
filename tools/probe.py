#!/usr/bin/env python3
"""Read Prism's opt-in diagnostic counters over FTP. No console mutations."""
import argparse
import ftplib
import io
import json
import os
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--host', required=True)
parser.add_argument('--port', type=int, default=2121)
parser.add_argument('--seconds', type=int, default=60)
args = parser.parse_args()
if not 1 <= args.seconds <= 3600:
    parser.error('--seconds must be between 1 and 3600')
path = '/mnt/sandbox/PPSA99483_000/download0/prism/status.json'
try:
    with ftplib.FTP() as ftp:
        ftp.connect(args.host, args.port, timeout=10)
        ftp.login(os.environ.get('PS5_FTP_USER', 'anonymous'), os.environ.get('PS5_FTP_PASSWORD', 'prism'))
        deadline = time.monotonic() + args.seconds
        previous = None
        while time.monotonic() < deadline:
            data = io.BytesIO()
            ftp.retrbinary('RETR ' + path, data.write)
            value = json.loads(data.getvalue())
            stale = previous == value.get('uptime_us')
            previous = value.get('uptime_us')
            print(json.dumps({'stale': stale, **value}), flush=True)
            time.sleep(1)
except (OSError, ftplib.Error, ValueError) as error:
    raise SystemExit('Could not read Prism diagnostics. Start Prism and enable Input diagnostics. ' + str(error))
