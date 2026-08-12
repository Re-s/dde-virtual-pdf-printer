#!/usr/bin/env python3
"""deepin 论坛打印相关帖子收集脚本。

策略：bbs.deepin.org 的搜索 API (/api/v1/search) 需要 captcha 验证，
无法直接调用。改为拉取公开的帖子时间线 API (/api/v2/public/posts)，
在本地用关键词过滤出与打印相关的帖子。

输出：markdown 报告 + JSON 原始数据
"""
import json
import re
import sys
import time
import html

sys.path.insert(0, '/home/master/.local/bin')
from netfetch import auto_fetch

BASE = 'https://bbs.deepin.org/api/v2/public/posts'
KEYWORDS = ['打印', '打印机', 'pdf', 'PDF', '虚拟打印', 'print', 'Print',
            'cups', 'CUPS', '扫描', '扫描仪', '驱动', '喷墨', '激光打印',
            '惠普', 'hp', 'HP', '佳能', 'Canon', '爱普生', 'Epson', '兄弟', 'Brother',
            '联想', 'Lenovo', '奔图', 'Pantum']

# 只保留强相关关键词（避免“驱动”误报太多）
STRONG = ['打印', '打印机', 'pdf', 'PDF', '虚拟打印', 'print', 'Print',
          'cups', 'CUPS', '扫描', '扫描仪', '喷墨', '激光打印']


def clean_json(raw: str):
    """netfetch 已解码 chunked 并返回干净 body，直接解析。"""
    return json.loads(raw)


def extract_text(msg: str) -> str:
    """从 HTML/HTML实体/base64 混合的 message 中提取可读文本。"""
    # vditor 的 data-value 可能是 base64
    m = re.search(r'data-value="([^"]+)"', msg)
    if m:
        try:
            import base64
            decoded = base64.b64decode(m.group(1)).decode('utf-8', errors='ignore')
            if decoded and not decoded.startswith('\x00'):
                msg = decoded
        except Exception:
            pass
    text = re.sub(r'<[^>]+>', ' ', msg)
    text = html.unescape(text)
    text = re.sub(r'\s+', ' ', text).strip()
    return text


def main():
    total_pages = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    page_size = int(sys.argv[2]) if len(sys.argv) > 2 else 50

    all_posts = []
    for page in range(total_pages):
        offset = page * page_size
        url = f'{BASE}?offset={offset}&limit={page_size}'
        try:
            _, status, body = auto_fetch(url, timeout=15)
            if status != 200:
                print(f'page {page}: HTTP {status}', file=sys.stderr)
                continue
            posts = clean_json(body.decode('utf-8', errors='ignore'))
            if not posts:
                break
            all_posts.extend(posts)
            print(f'page {page}: +{len(posts)} (offset={offset})', file=sys.stderr)
        except Exception as e:
            print(f'page {page} FAIL: {e}', file=sys.stderr)
        time.sleep(0.5)

    print(f'总抓取帖子: {len(all_posts)}', file=sys.stderr)

    # 本地过滤
    hits = []
    for p in all_posts:
        msg = p.get('message', '') or ''
        text = extract_text(msg)
        matched = [kw for kw in STRONG if kw.lower() in (text + ' ' + msg).lower()]
        if matched:
            hits.append({
                'thread_id': p.get('thread_id'),
                'post_id': p.get('id'),
                'forum_id': p.get('forum_id'),
                'user_id': p.get('user_id'),
                'is_first': p.get('is_first'),
                'matched': matched,
                'text': text[:300],
                'url': f'https://bbs.deepin.org/post/{p.get("thread_id")}',
            })

    # 去重（同 thread 多帖）
    seen = set()
    unique = []
    for h in hits:
        if h['thread_id'] not in seen:
            seen.add(h['thread_id'])
            unique.append(h)

    # 输出 markdown 报告
    print(f'# deepin 论坛打印相关帖子调研\n')
    print(f'> 数据来源: bbs.deepin.org API 时间线，共扫描 {len(all_posts)} 帖，'
          f'命中 {len(unique)} 个主题\n')
    print(f'## 命中主题（{len(unique)}）\n')
    for i, h in enumerate(unique, 1):
        first = '【楼主】' if h['is_first'] else ''
        print(f'### {i}. [{h["thread_id"]}] {first}')
        print(f'- 链接: {h["url"]}')
        print(f'- 命中词: {", ".join(h["matched"])}')
        print(f'- 内容: {h["text"][:200]}\n')

    # 保存 JSON
    with open('/home/master/Projects/deepin-pdf-printer/docs/forum-print-posts.json', 'w',
              encoding='utf-8') as f:
        json.dump({'scanned': len(all_posts), 'hits': unique}, f, ensure_ascii=False, indent=2)
    print(f'\nJSON 已保存: docs/forum-print-posts.json', file=sys.stderr)


if __name__ == '__main__':
    main()
