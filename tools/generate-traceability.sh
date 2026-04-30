#!/bin/bash
# generate-traceability.sh
# 自动扫描 docs/ 目录，生成/更新追溯矩阵

set -e

DOCS_DIR="$(cd "$(dirname "$0")/../docs" && pwd)"
MATRIX_FILE="$DOCS_DIR/traceability/matrix.md"

echo "# 追溯矩阵 (Traceability Matrix)"
echo ""
echo "自动生成于: $(date '+%Y-%m-%d %H:%M:%S')"
echo ""
echo "## 需求到设计 (Requirements to Design)"
echo ""
echo "| Requirement | Overview | Modules | Features |"
echo "|-------------|----------|---------|----------|"

# 扫描需求文件
for req_file in "$DOCS_DIR/specs/"REQ-*.md; do
    [ -f "$req_file" ] || continue
    req_id=$(basename "$req_file" .md)
    echo "| $req_id     | | | |"
done

echo ""
echo "## 设计到实现 (Design to Implementation)"
echo ""
echo "| Module/Feature | Implementation Files | Unit Tests | Status |"
echo "|---------------|---------------------|------------|--------|"

# 扫描模块设计文件
for mod_file in "$DOCS_DIR/module-design/"MOD-*.md; do
    [ -f "$mod_file" ] || continue
    mod_id=$(basename "$mod_file" .md)
    echo "| $mod_id        | | | Planned |"
done

echo ""
echo "## 硬件文档引用 (Hardware Docs Reference)"
echo ""
echo "| Hardware Doc | Used by (Requirements) |"
echo "|--------------|----------------------|"

for hw_file in $(find "$DOCS_DIR/hardware" -name "*.md" | sort); do
    hw_rel=$(echo "$hw_file" | sed "s|$DOCS_DIR/||")
    echo "| \`$hw_rel\` | |"
done

echo ""
echo "## 更新说明 (Update Notes)"
echo ""
echo "- 本文件可由脚本自动生成，也可手动编辑补充。"
echo "- 每次完成一个阶段后，请更新对应行。"
