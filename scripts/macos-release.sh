#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DIST_DIR="$ROOT/dist"

read_version() {
    sed -n 's/.*ShowJCR::version = "\(v[^"]*\)".*/\1/p' "$ROOT/showjcr.cpp" | head -n 1
}

VERSION="$(read_version)"
TAG="$VERSION"
ARCHIVE="$DIST_DIR/ShowJCR-${VERSION}-macos.zip"
REPO="${1:-}"

if [[ -z "$REPO" ]]; then
    if git -C "$ROOT" remote get-url fork >/dev/null 2>&1; then
        REPO="$(gh repo view fork --json nameWithOwner -q .nameWithOwner 2>/dev/null || true)"
    fi
    if [[ -z "$REPO" ]]; then
        REPO="$(gh repo view --json nameWithOwner -q .nameWithOwner)"
    fi
fi

"$ROOT/scripts/macos-package.sh"

if [[ ! -f "$ARCHIVE" ]]; then
    echo "Release archive not found: $ARCHIVE" >&2
    exit 1
fi

if git -C "$ROOT" rev-parse "$TAG" >/dev/null 2>&1; then
    echo "Tag $TAG already exists locally"
else
    git -C "$ROOT" tag -a "$TAG" -m "Release $TAG"
fi

if git -C "$ROOT" remote get-url fork >/dev/null 2>&1; then
    git -C "$ROOT" push fork "$TAG" || true
fi

NOTES_FILE="$(mktemp)"
cat >"$NOTES_FILE" <<EOF
## ShowJCR ${TAG}

macOS 改进版发布包（${TAG}）。

### 主要更新
- macOS 构建/安装脚本与 Dock 图标
- UI：设置/菜单面板、关于与选表对话框、Tab 列宽优化
- 搜索补全：支持鼠标选择与方向键 + Enter
- 数据：导入中科院分区 2021–2023（FQBJCR），修正表名显示
- 展示：中科院/JCR 等多年份数据合并为单个 Tab
- 国际预警表：仅命中预警期刊时显示 Tab

### 安装
1. 解压 \`ShowJCR-${TAG}-macos.zip\`
2. 将 \`ShowJCR.app\` 拖入「应用程序」
3. 首次运行若被拦截，请在「系统设置 → 隐私与安全性」中允许

### 向原项目贡献
本版本改动已提交 Pull Request 至 https://github.com/hitfyd/ShowJCR
EOF

if gh release view "$TAG" --repo "$REPO" >/dev/null 2>&1; then
    echo "Release $TAG already exists on $REPO, uploading asset"
    gh release upload "$TAG" "$ARCHIVE" --repo "$REPO" --clobber
else
    gh release create "$TAG" "$ARCHIVE" \
        --repo "$REPO" \
        --title "ShowJCR ${TAG} (macOS)" \
        --notes-file "$NOTES_FILE"
fi

rm -f "$NOTES_FILE"
echo "Release published: https://github.com/${REPO}/releases/tag/${TAG}"
