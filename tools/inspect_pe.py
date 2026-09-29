#!/usr/bin/env python3
"""Static PE inspector for quarantined historical binaries. NEVER executes the target.

Usage: inspect_pe.py <file.exe|dll> [...]   -> JSON on stdout
Records: size, SHA-256, PE header facts, VERSIONINFO strings, imports (dll names),
Authenticode (WIN_CERTIFICATE) presence, embedded PKCS#7 signer metadata, and
the Authenticode digest recomputed per the PE spec so it can be compared with the
digest inside the signature. Full chain verification is NOT performed here (that
requires a trust store + timestamp policy); see research/PROVENANCE.md.
"""
import sys, json, hashlib, datetime, struct
import pefile

def authenticode_digest(path, pe, alg="sha1"):
    data = open(path, "rb").read()
    h = hashlib.new(alg)
    chk = pe.OPTIONAL_HEADER.get_file_offset() + 64          # CheckSum field
    dd = pe.OPTIONAL_HEADER.get_file_offset() + (96 if pe.OPTIONAL_HEADER.Magic == 0x10b else 112) + 4 * 8  # cert table dir entry
    sec = pe.OPTIONAL_HEADER.DATA_DIRECTORY[4]
    end_hdr = pe.OPTIONAL_HEADER.SizeOfHeaders
    h.update(data[:chk]); h.update(data[chk+4:dd]); h.update(data[dd+8:end_hdr])
    sections = sorted(pe.sections, key=lambda s: s.PointerToRawData)
    pos = end_hdr
    for s in sections:
        if s.SizeOfRawData == 0: continue
        h.update(data[s.PointerToRawData:s.PointerToRawData+s.SizeOfRawData]); pos += s.SizeOfRawData
    cert_start = sec.VirtualAddress if sec.Size else len(data)
    if cert_start > pos: h.update(data[pos:cert_start])
    return h.hexdigest()

def crypto_checks(sd):
    """Verify (a) SignerInfo RSA signature over signedAttrs with the signer cert key,
    (b) messageDigest attr == digest(SpcIndirectDataContent content octets), (c) each embedded
    cert's signature by its issuer where that issuer is present in the bundle.
    Trust-root evaluation and revocation are NOT performed."""
    res = {}
    try:
        import hashlib as _h
        from cryptography import x509
        from cryptography.hazmat.primitives import hashes, serialization
        from cryptography.hazmat.primitives.asymmetric import padding
        from asn1crypto import x509 as ax
        by_subject = {}
        for c in sd["certificates"]:
            by_subject[c.chosen.subject.dump()] = (c.chosen, x509.load_der_x509_certificate(c.chosen.dump()))
        si = sd["signer_infos"][0]
        sid = si["sid"].chosen
        signer = next(cc for (ac, cc) in by_subject.values()
                      if ac.issuer.dump() == sid["issuer"].dump() and ac.serial_number == sid["serial_number"].native)
        res["signer_subject"] = signer.subject.rfc4514_string()
        alg = si["digest_algorithm"]["algorithm"].native
        h = {"sha1": hashes.SHA1(), "sha256": hashes.SHA256()}[alg]
        der = bytearray(si["signed_attrs"].dump()); der[0] = 0x31   # [0] IMPLICIT -> SET OF for signature input
        try:
            signer.public_key().verify(si["signature"].native, bytes(der), padding.PKCS1v15(), h); res["signer_signature_valid"] = True
        except Exception:
            res["signer_signature_valid"] = False
        md = next(a["values"][0].native for a in si["signed_attrs"] if a["type"].native == "message_digest")
        content = sd["encap_content_info"]["content"].contents   # content octets of SpcIndirectDataContent
        res["message_digest_matches_spc_content"] = _h.new(alg, content).digest() == md
        links = []
        for (ac, cc) in by_subject.values():
            entry = {"cert": cc.subject.rfc4514_string()[:70]}
            issuer = by_subject.get(ac.issuer.dump())
            if issuer is None:
                entry["issuer_in_bundle"] = False
            else:
                entry["issuer_in_bundle"] = True
                try:
                    issuer[1].public_key().verify(cc.signature, cc.tbs_certificate_bytes, padding.PKCS1v15(), cc.signature_hash_algorithm)
                    entry["signature_valid"] = True
                except Exception:
                    entry["signature_valid"] = False
            links.append(entry)
        res["chain_links"] = links
        res["root_trust_evaluated"] = False
    except Exception as e:
        res["error"] = repr(e)
    return res

def signature_info(path, pe):
    sec = pe.OPTIONAL_HEADER.DATA_DIRECTORY[4]
    out = {"present": bool(sec.Size), "offset": sec.VirtualAddress, "size": sec.Size}
    if not sec.Size: return out
    data = open(path, "rb").read()
    blob = data[sec.VirtualAddress:sec.VirtualAddress+sec.Size]
    length, rev, ctype = struct.unpack("<IHH", blob[:8])
    out.update({"win_cert_length": length, "revision": hex(rev), "type": hex(ctype)})
    try:
        from asn1crypto import cms
        ci = cms.ContentInfo.load(blob[8:length])
        sd = ci["content"]
        out["digest_algorithms"] = [d["algorithm"].native for d in sd["digest_algorithms"]]
        certs = []
        for c in sd["certificates"]:
            t = c.chosen
            certs.append({"subject": t.subject.human_friendly, "issuer": t.issuer.human_friendly,
                          "serial": hex(t.serial_number), "not_before": str(t["tbs_certificate"]["validity"]["not_before"].native),
                          "not_after": str(t["tbs_certificate"]["validity"]["not_after"].native),
                          "sha1": hashlib.sha1(t.dump()).hexdigest()})
        out["certificates"] = certs
        si = sd["signer_infos"][0]
        out["signer_digest_algorithm"] = si["digest_algorithm"]["algorithm"].native
        for a in si["signed_attrs"] or []:
            if a["type"].native == "message_digest":
                out["signed_message_digest"] = a["values"][0].native.hex()
        out["pkcs7_parsed"] = True
        out["crypto_checks"] = crypto_checks(sd)
        out["_pkcs7_raw_hex"] = blob[8:length].hex()
    except Exception as e:
        out["pkcs7_parsed"] = False; out["pkcs7_error"] = repr(e)
    return out

def inspect(path):
    raw = open(path, "rb").read()
    pe = pefile.PE(path, fast_load=False)
    r = {"file": path, "size": len(raw), "sha256": hashlib.sha256(raw).hexdigest(),
         "sha1": hashlib.sha1(raw).hexdigest(), "md5": hashlib.md5(raw).hexdigest()}
    r["pe"] = {"machine": hex(pe.FILE_HEADER.Machine), "timestamp_utc": datetime.datetime.fromtimestamp(pe.FILE_HEADER.TimeDateStamp, datetime.timezone.utc).isoformat(),
               "subsystem": pe.OPTIONAL_HEADER.Subsystem, "image_base": hex(pe.OPTIONAL_HEADER.ImageBase),
               "entry_point_rva": hex(pe.OPTIONAL_HEADER.AddressOfEntryPoint), "linker": f"{pe.OPTIONAL_HEADER.MajorLinkerVersion}.{pe.OPTIONAL_HEADER.MinorLinkerVersion}",
               "sections": [{"name": s.Name.rstrip(b"\0").decode("latin1"), "vsize": s.Misc_VirtualSize, "rawsize": s.SizeOfRawData, "entropy": round(s.get_entropy(), 3)} for s in pe.sections]}
    vi = {}
    for fi in getattr(pe, "FileInfo", []) or []:
        for e in fi:
            if hasattr(e, "StringTable"):
                for st in e.StringTable:
                    vi.update({k.decode("latin1"): v.decode("latin1") for k, v in st.entries.items()})
    r["versioninfo"] = vi
    r["imports"] = sorted({i.dll.decode("latin1") for i in getattr(pe, "DIRECTORY_ENTRY_IMPORT", [])})
    r["authenticode"] = signature_info(path, pe)
    if r["authenticode"]["present"]:
        a = r["authenticode"]
        a["recomputed_digests"] = {alg: authenticode_digest(path, pe, alg) for alg in ("sha1", "sha256")}
        raw_hex = a.pop("_pkcs7_raw_hex", "")
        # The Authenticode PE digest is embedded in SpcIndirectDataContent inside the PKCS#7 blob;
        # look for the recomputed digest as an exact byte string inside the signature.
        a["recomputed_digest_found_in_signature"] = {alg: (d in raw_hex) for alg, d in a["recomputed_digests"].items()}
        # RFC3161-less legacy countersignature: signingTime attribute (OID 1.2.840.113549.1.9.5) => UTCTime
        import re
        m = re.search(r"06092a864886f70d010905310f170d([0-9a-f]{26})", raw_hex)
        if m:
            t = bytes.fromhex(m.group(1)).decode("ascii")
            a["countersignature_signing_time_utc"] = f"20{t[0:2]}-{t[2:4]}-{t[4:6]}T{t[6:8]}:{t[8:10]}:{t[10:12]}Z"
        a["chain_verification"] = "NOT PERFORMED (tool records cert validity windows and digest self-consistency only; no trust-store/revocation check)"
    return r

if __name__ == "__main__":
    print(json.dumps([inspect(p) for p in sys.argv[1:]], indent=2))
