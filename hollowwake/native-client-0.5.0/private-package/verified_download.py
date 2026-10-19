"""Bounded resumable transfer with an immutable final size/SHA-256 pin."""
import hashlib
import http.client
import json
import re
import time
import urllib.error
import urllib.request
from fetch_boot import NoRedirect

def download(url, destination, size, pin):
    count, failures, etag = 0, 0, None
    digest = hashlib.sha256()
    last = time.monotonic()
    opener = urllib.request.build_opener(NoRedirect())
    with destination.open('xb') as output:
        while count < size:
            start = count
            headers = {'User-Agent': 'Hollowwake-PrivateStaticBuild/0.5'}
            if start:
                headers['Range'] = f'bytes={start}-'
                if etag: headers['If-Range'] = etag
            response = None
            try:
                response = opener.open(urllib.request.Request(url, headers=headers), timeout=90)
            except urllib.error.HTTPError as error:
                if error.code not in (408, 429, 500, 502, 503, 504):
                    raise OSError(f'Archive HTTP status {error.code}') from None
            except (OSError, http.client.HTTPException):
                pass
            if response is not None:
                with response:
                    received_etag = response.headers.get('ETag')
                    if etag and received_etag != etag:
                        raise ValueError('Archive identity changed while resuming')
                    if start:
                        match = re.fullmatch(r'bytes (\d+)-(\d+)/(\d+)', response.headers.get('Content-Range', ''))
                        if response.status != 206 or not match or tuple(map(int, match.groups())) != (start, size-1, size):
                            raise ValueError('Archive resume range was not honored exactly')
                    elif response.status != 200:
                        raise ValueError('Unexpected initial archive status')
                    if response.headers.get('Content-Length') not in (None, str(size-start)):
                        raise ValueError('Archive response length differs from pin')
                    if etag is None: etag = received_etag
                    while count < size:
                        try:
                            block = response.read(min(4 * 1024**2, size-count))
                        except http.client.IncompleteRead as error:
                            block = error.partial
                        except (OSError, http.client.HTTPException):
                            break
                        if not block: break
                        if count + len(block) > size: raise ValueError('Archive exceeds pinned size')
                        output.write(block); digest.update(block); count += len(block)
                        if time.monotonic()-last >= 20:
                            print(json.dumps({'downloadedBytes': count, 'expectedBytes': size}), flush=True)
                            last = time.monotonic()
            if count < size:
                failures += 1
                if failures > 8: raise OSError(f'Archive transfer exhausted bounded retries at byte {count}')
                output.flush()
                print(json.dumps({'resumingArchiveAt': count, 'attempt': failures, 'expectedBytes': size}), flush=True)
                time.sleep(min(failures, 5))
    actual = digest.hexdigest()
    if count != size or actual != pin:
        raise ValueError(f'Complete archive mismatch: bytes={count}, sha256={actual}')
    print(json.dumps({'completeArchiveBytes': count, 'sha256': actual, 'resumeAttempts': failures}), flush=True)
