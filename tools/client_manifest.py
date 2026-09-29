#!/usr/bin/env python3
"""Build a provenance manifest for a quarantined historical client directory. STATIC ONLY - nothing is executed.
usage: client_manifest.py <client_dir> <source_repo_clone_dir> <out.json>"""
import sys, os, json, hashlib, subprocess, datetime
sys.path.insert(0, os.path.dirname(__file__))
from inspect_pe import inspect
cdir, clone, out = sys.argv[1:4]
def git(*a): return subprocess.check_output(["git", "-C", clone, *a]).decode().strip()
files = []
total = 0
for root, _, names in os.walk(cdir):
    for n in sorted(names):
        p = os.path.join(root, n); rel = os.path.relpath(p, cdir).replace(os.sep, "/")
        data = open(p, "rb").read(); total += len(data)
        e = {"path": rel, "size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
        if n.lower().endswith((".exe", ".dll")):
            try:
                r = inspect(p); e["pe"] = {"machine": r["pe"]["machine"], "timestamp_utc": r["pe"]["timestamp_utc"], "versioninfo": {k: v for k, v in r["versioninfo"].items() if k in ("FileVersion", "ProductVersion", "CompanyName", "OriginalFilename")},
                                          "signed": r["authenticode"]["present"]}
                if r["authenticode"]["present"]:
                    cs = r["authenticode"].get("crypto_checks", {}); e["pe"]["signer"] = cs.get("signer_subject"); e["pe"]["signature_valid"] = cs.get("signer_signature_valid")
            except Exception as ex: e["pe"] = {"error": repr(ex)}
        files.append(e)
files.sort(key=lambda f: f["path"])
man = {"schema": "bloxen.client-manifest/1", "generated_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
       "target": {"product": "Roblox Windows Player", "version": "0.205.0.61876", "deploy_guid": "version-0d46087630eb46cd"},
       "source": {"repo": "https://github.com/KloBraticc/2015-Client", "path": "July 23 (0.205.0.61876)", "commit": git("rev-parse", "HEAD"), "commit_date": git("log", "-1", "--format=%cI"),
                  "stated_upstream": "https://archive.roblonium.com (per repo README)"},
       "file_count": len(files), "total_bytes": total, "files": files}
json.dump(man, open(out, "w"), indent=1); print(len(files), total)
