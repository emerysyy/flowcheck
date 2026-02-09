#!/bin/bash

# 删除没有 TX_*.bin 文件的流目录

DATA_DIR="/Users//Documents/work/flowcheck/data"

echo "开始删除无效目录..."
echo "========================================"

deleted_count=0

# 删除 TCP 目录
echo -e "\n清理 TCP 目录..."
for dir in "$DATA_DIR/tcp"/*; do
    if [ -d "$dir" ]; then
        # 检查是否有 TX 文件
        tx_count=$(ls "$dir"/TX_*.bin 2>/dev/null | wc -l)

        if [ $tx_count -eq 0 ]; then
            echo "  删除: $(basename "$dir")"
            rm -rf "$dir"
            deleted_count=$((deleted_count + 1))
        fi
    fi
done

# 删除 UDP 目录
echo -e "\n清理 UDP 目录..."
for dir in "$DATA_DIR/udp"/*; do
    if [ -d "$dir" ]; then
        # 检查是否有 TX 文件
        tx_count=$(ls "$dir"/TX_*.bin 2>/dev/null | wc -l)

        if [ $tx_count -eq 0 ]; then
            echo "  删除: $(basename "$dir")"
            rm -rf "$dir"
            deleted_count=$((deleted_count + 1))
        fi
    fi
done

echo -e "\n========================================"
echo "删除完成！"
echo "  已删除: $deleted_count 个目录"
echo "========================================"
