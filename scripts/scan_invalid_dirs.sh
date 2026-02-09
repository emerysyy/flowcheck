#!/bin/bash

# 清理无效的流目录
# 删除条件：没有 TX_*.bin 文件的目录

# 获取脚本所在目录的父目录（项目根目录）
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DATA_DIR="$PROJECT_ROOT/data"

echo "扫描需要删除的目录..."
echo "========================================"

total_dirs=0
empty_dirs=0
no_tx_dirs=0

# 统计 TCP 目录
echo -e "\n检查 TCP 目录..."
for dir in "$DATA_DIR/tcp"/*; do
    if [ -d "$dir" ]; then
        total_dirs=$((total_dirs + 1))

        # 检查是否有 TX 文件
        tx_count=$(ls "$dir"/TX_*.bin 2>/dev/null | wc -l)

        if [ $tx_count -eq 0 ]; then
            no_tx_dirs=$((no_tx_dirs + 1))
            echo "  [删除] $(basename "$dir") - 没有 TX 文件"
        fi
    fi
done

# 统计 UDP 目录
echo -e "\n检查 UDP 目录..."
for dir in "$DATA_DIR/udp"/*; do
    if [ -d "$dir" ]; then
        total_dirs=$((total_dirs + 1))

        # 检查是否有 TX 文件
        tx_count=$(ls "$dir"/TX_*.bin 2>/dev/null | wc -l)

        if [ $tx_count -eq 0 ]; then
            no_tx_dirs=$((no_tx_dirs + 1))
            echo "  [删除] $(basename "$dir") - 没有 TX 文件"
        fi
    fi
done

echo -e "\n========================================"
echo "统计结果:"
echo "  总目录数: $total_dirs"
echo "  需要删除: $no_tx_dirs"
echo "  保留: $((total_dirs - no_tx_dirs))"
echo "========================================"
