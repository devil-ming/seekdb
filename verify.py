#!/usr/bin/env python3
"""
验证脚本：对比两个 commit 之间的文件差异，区分"真正修改"和"仅空白差异"。
用法：
  cd /data/1/repo/syncer_bot/mr_sync_workspace/public
  python3 /tmp/verify_ws_diff.py f955f3dfc174faf3a5a835f10c720c30b2a04249 cebb57bd34c221b55009cfe2a6b189f19d1f2d4e
"""
import subprocess
import sys
import tempfile
import os
def get_file_content(repo_dir, commit, filepath):
    """从 git 对象中读取文件内容。"""
    result = subprocess.run(
        ["git", "show", f"{commit}:{filepath}"],
        cwd=repo_dir, capture_output=True
    )
    if result.returncode != 0:
        return None
    return result.stdout
def is_binary(data):
    return b"\x00" in data[:8192]
def text_equal_ignore_ws(data_a, data_b):
    """忽略行尾空白和末尾空行后比较。"""
    try:
        lines_a = [l.rstrip() for l in data_a.decode("utf-8", errors="surrogateescape").splitlines()]
        lines_b = [l.rstrip() for l in data_b.decode("utf-8", errors="surrogateescape").splitlines()]
    except Exception:
        return False
    while lines_a and lines_a[-1] == "":
        lines_a.pop()
    while lines_b and lines_b[-1] == "":
        lines_b.pop()
    return lines_a == lines_b
def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <commit_a> <commit_b> [repo_dir]")
        sys.exit(1)
    commit_a = sys.argv[1]
    commit_b = sys.argv[2]
    repo_dir = sys.argv[3] if len(sys.argv) > 3 else "."
    # 获取两个 commit 之间有差异的文件列表
    result = subprocess.run(
        ["git", "diff", "--name-only", commit_a, commit_b],
        cwd=repo_dir, capture_output=True, text=True
    )
    files = [f for f in result.stdout.strip().split("\n") if f]
    print(f"git diff 报告共 {len(files)} 个文件有差异\n")
    real_diff = []
    ws_only = []
    binary_diff = []
    missing = []
    for filepath in files:
        content_a = get_file_content(repo_dir, commit_a, filepath)
        content_b = get_file_content(repo_dir, commit_b, filepath)
        # 文件新增或删除
        if content_a is None or content_b is None:
            real_diff.append(filepath)
            continue
        # 字节完全相同（不应出现，但防御性检查）
        if content_a == content_b:
            ws_only.append(filepath)
            continue
        # 二进制文件
        if is_binary(content_a) or is_binary(content_b):
            binary_diff.append(filepath)
            real_diff.append(filepath)
            continue
        # 文本比较：忽略行尾空白和末尾空行
        if text_equal_ignore_ws(content_a, content_b):
            ws_only.append(filepath)
        else:
            real_diff.append(filepath)
    print(f"=== 结果 ===")
    print(f"真正有内容差异的文件: {len(real_diff)}")
    print(f"仅空白差异（会被新逻辑忽略）: {len(ws_only)}")
    print(f"其中二进制文件: {len(binary_diff)}")
    print()
    if ws_only:
        print(f"--- 仅空白差异的文件（前20个）---")
        for f in ws_only[:20]:
            print(f"  {f}")
        if len(ws_only) > 20:
            print(f"  ... 还有 {len(ws_only) - 20} 个")
    print()
    if real_diff:
        print(f"--- 真正有内容差异的文件 ---")
        for f in real_diff:
            print(f"  {f}")
if __name__ == "__main__":
    main()

