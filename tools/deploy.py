#!/usr/bin/env python3
# Prism for PS5 - FTP deployment of the built app folder.
# Adapted from ps5-native-app-boilerplate's tools/deploy.sh (BlackBearReloaded).
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Uploads dist/<TITLE_ID>/ to /data/homebrew/<TITLE_ID>/ over FTP. Each file is
# written under a hidden .upload name and renamed once complete; eboot.bin and
# sce_sys/param.json are published last so the enabler never registers a
# half-copied title. Build first with `make app` (in the Docker builder).
#
#   PS5_HOST=192.0.2.100 python3 tools/deploy.py

from ftplib import FTP, error_perm
from pathlib import Path
from posixpath import dirname, join
import json
import os
import re
import sys

ROOT = Path(__file__).resolve().parent.parent
HOMEBREW_ROOT = "/data/homebrew"


def title_id():
    value = json.loads((ROOT / "sce_sys/param.json").read_text(encoding="utf-8"))["titleId"]
    if not re.fullmatch(r"PPSA\d{5}", value):
        sys.exit("param.json contains an invalid titleId")
    return value


def reply_code(error):
    return str(error).split(maxsplit=1)[0]


def list_names(ftp, path):
    previous = ftp.pwd()
    ftp.cwd(path)
    try:
        return {name for name, _ in ftp.mlsd() if name not in {".", ".."}}
    finally:
        ftp.cwd(previous)


def ensure_directory(ftp, path):
    current = ""
    for component in path.strip("/").split("/"):
        current += f"/{component}"
        try:
            ftp.mkd(current)
        except error_perm as error:
            if reply_code(error) != "550":
                raise


def remove_if_present(ftp, path):
    try:
        # ftpsrv answers DELE with 226, which ftplib.FTP.delete rejects.
        ftp.sendcmd(f"DELE {path}")
    except error_perm as error:
        text = str(error).lower()
        if reply_code(error) != "550" or not ("no such" in text or "not found" in text):
            raise


def upload_atomic(ftp, local, remote):
    directory = dirname(remote)
    temporary = join(directory, f".{remote.rsplit('/', 1)[-1]}.upload")
    ensure_directory(ftp, directory)
    remove_if_present(ftp, temporary)
    with local.open("rb") as source:
        ftp.storbinary(f"STOR {temporary}", source, blocksize=256 * 1024)
    remove_if_present(ftp, remote)
    ftp.rename(temporary, remote)


def main():
    host = os.environ.get("PS5_HOST", "")
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9.-]*", host):
        sys.exit("set PS5_HOST to the console's IPv4 address")
    port = int(os.environ.get("FTP_PORT", "2121"))
    tid = title_id()
    artifact = ROOT / "dist" / tid
    critical = [artifact / "eboot.bin", artifact / "sce_sys/param.json"]
    if not all(path.is_file() and path.stat().st_size for path in critical):
        sys.exit(f"{artifact} is missing eboot.bin or sce_sys/param.json; run `make app` first")
    files = sorted(p for p in artifact.rglob("*") if p.is_file() and p not in critical) + critical
    if any(p.name == "prism.conf" for p in files):
        sys.exit("refusing to upload a prism.conf from dist/")

    remote_root = join(HOMEBREW_ROOT, tid)
    with FTP() as ftp:
        ftp.connect(host, port, timeout=15)
        ftp.login(os.environ.get("PS5_FTP_USER", "anonymous"), os.environ.get("PS5_FTP_PASSWORD", "prism"))
        print(f"==> [deploy] Publishing {len(files)} files to ftp://{host}:{port}{remote_root}/")
        for index, local in enumerate(files, 1):
            relative = local.relative_to(artifact).as_posix()
            print(f"==> [deploy] [{index}/{len(files)}] {relative}")
            upload_atomic(ftp, local, join(remote_root, relative))
        if "eboot.bin" not in list_names(ftp, remote_root):
            sys.exit("upload finished but eboot.bin is not listed")
        if "param.json" not in list_names(ftp, join(remote_root, "sce_sys")):
            sys.exit("upload finished but param.json is not listed")
        try:
            ftp.quit()
        except (EOFError, OSError):
            pass
    print(f"Deployment complete: {tid}. Wait ~10 s for the enabler to register it before launching.")


if __name__ == "__main__":
    main()
