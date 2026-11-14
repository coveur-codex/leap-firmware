#!/usr/bin/env python3
"""Inspect public RSS enclosures and bounded MP3 prefixes; never download full episodes."""
import argparse
import json
import urllib.error
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET


def prefix(url, start=0, length=16384):
    request = urllib.request.Request(url, headers={
        'User-Agent': 'LEAP-podcast-diagnostic/1.7.1',
        'Range': f'bytes={start}-{start+length-1}',
        'Accept-Encoding': 'identity',
    })
    with urllib.request.urlopen(request, timeout=20) as response:
        info = {'http': response.status, 'host': urllib.parse.urlsplit(response.url).hostname,
                'contentType': response.headers.get('Content-Type'),
                'contentEncoding': response.headers.get('Content-Encoding'),
                'contentLength': response.headers.get('Content-Length'),
                'contentRange': response.headers.get('Content-Range'),
                'acceptRanges': response.headers.get('Accept-Ranges')}
        data = response.read(length)
    return info, data


def inspect(feed_url, count=3):
    request = urllib.request.Request(feed_url, headers={'User-Agent': 'LEAP-podcast-diagnostic/1.7.1'})
    with urllib.request.urlopen(request, timeout=20) as response:
        xml = response.read(8 * 1024 * 1024 + 1)
    if len(xml) > 8 * 1024 * 1024:
        raise ValueError('Feed exceeds 8 MiB')
    root = ET.fromstring(xml)
    episodes = root.findall('.//item')
    results = []
    for item in episodes[:count]:
        enclosures = item.findall('enclosure')
        if not enclosures:
            continue
        enclosure = next((e for e in enclosures if e.get('type') == 'audio/mpeg'), enclosures[0])
        url = urllib.parse.urljoin(feed_url, enclosure.get('url', ''))
        info, data = prefix(url)
        info['feedType'] = enclosure.get('type')
        info['prefixHex'] = data[:10].hex()
        if data[:3] == b'ID3' and len(data) >= 10:
            info['id3Version'] = data[3]
            info['id3Flags'] = data[5]
            info['id3SizeValid'] = all(value < 128 for value in data[6:10])
            tag_size = sum(value << (7 * (3-i)) for i, value in enumerate(data[6:10]))
            info['id3Bytes'] = 10 + tag_size + (10 if data[3] == 4 and data[5] & 16 else 0)
            if info['id3Bytes'] < 16 * 1024 * 1024:
                after_info, after = prefix(url, info['id3Bytes'])
                info['afterTagHttp'] = after_info['http']
                info['afterTagRange'] = after_info['contentRange']
                info['afterTagHex'] = after[:16].hex()
        results.append(info)
    return {'feed': feed_url, 'episodesInFeed': len(episodes), 'samples': results}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('feeds', nargs='+')
    parser.add_argument('--count', type=int, default=3)
    args = parser.parse_args()
    for feed in args.feeds:
        try:
            result = inspect(feed, max(1, min(args.count, 10)))
        except urllib.error.HTTPError as exc:
            result = {'feed': feed, 'error': f'HTTP {exc.code}'}
        except Exception as exc:
            result = {'feed': feed, 'error': type(exc).__name__}
        print(json.dumps(result, ensure_ascii=False), flush=True)


if __name__ == '__main__':
    main()
