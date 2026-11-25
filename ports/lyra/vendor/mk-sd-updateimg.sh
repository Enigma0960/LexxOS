#!/bin/bash
set -euo pipefail

#=====================================================
#  SD Card Update Image Creator for RK3506 (Luckfox Lyra)
#=====================================================
# This script generates sd_update.img for SD card update.
#
# Partition layout is parsed from parameter.txt
#
# Requirements:
#   apt install gdisk e2fsprogs
#=====================================================

#-------------------------------
# Config
#-------------------------------
SECTOR_SIZE=512
EXTRA_GB=${2:-0}   # default 0 GB extra rootfs space

# Source files - can be overridden by environment variables
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${RK_SDK_DIR:-$(realpath "$SCRIPT_DIR/../..")}"

# Try to find input images from rockdev directory
ROCKDEV_DIR="${RK_ROCKDEV_DIR:-$PROJECT_ROOT/rockdev}"
IDBLOCK_IMG="${RK_IDBLOCK_IMG:-$PROJECT_ROOT/u-boot/rk3506_idblock_v1.04.110.img}"
UBOOT_IMG="${RK_UBOOT_IMG:-$ROCKDEV_DIR/uboot.img}"
KERNEL_IMG="$ROCKDEV_DIR/boot.img"
ROOTFS_IMG="${RK_ROOTFS_IMG:-$ROCKDEV_DIR/rootfs.img}"
PARAMETER_TXT="${RK_PARAMETER_TXT:-$ROCKDEV_DIR/parameter.txt}"

OUTPUT_IMG="${1:-}"
[[ -z "$OUTPUT_IMG" ]] && {
    echo "Usage: sudo $0 <output.img> [extra_GB]"
    echo "   or: sudo $0 [extra_GB]  (default output: sd_update.img)"
    exit 1
}

# If output path doesn't contain directory, put it in rockdev
if [[ "$OUTPUT_IMG" != */* ]]; then
    OUTPUT_IMG="$ROCKDEV_DIR/$OUTPUT_IMG"
fi

#-------------------------------
# Tool check
#-------------------------------
for tool in sgdisk e2fsck resize2fs dd sha256sum; do
    command -v $tool >/dev/null || {
        echo "Error: missing tool '$tool'. Please install gdisk e2fsprogs."
        exit 1
    }
done

#-------------------------------
# Detect OS stat command
#-------------------------------
case "$(uname -s)" in
    Linux)  STAT_SIZE_CMD="stat -Lc%s" ;;
    Darwin) STAT_SIZE_CMD="stat -f%z" ;;
    *)      STAT_SIZE_CMD="stat -c%s" ;;
esac

#-------------------------------
# Helper functions
#-------------------------------
function file_size() { $STAT_SIZE_CMD "$1"; }

function check_exists() {
    [[ -f "$1" ]] || { echo "Missing file: $1"; exit 1; }
}

#-------------------------------
# Parse parameter.txt
#-------------------------------
echo ">>> Parsing partition layout from $PARAMETER_TXT"
check_exists "$PARAMETER_TXT"

# Parse CMDLINE from parameter.txt
CMDLINE=$(grep "^CMDLINE:" "$PARAMETER_TXT" | grep -o "0x.*")
if [[ -z "$CMDLINE" ]]; then
    echo "Error: Failed to parse CMDLINE from $PARAMETER_TXT"
    exit 1
fi

# Extract partition information: name, size (sectors), offset (sectors)
# Format: size@offset(name) or -@offset(name:grow)
# Store as: name size offset [grow]
PARTITIONS=()
while IFS= read -r part; do
    # Extract size, offset, name
    if [[ "$part" =~ ^0x([0-9a-fA-F]+)@0x([0-9a-fA-F]+)\(([^:]+)(:grow)?\)$ ]]; then
        size_hex="${BASH_REMATCH[1]}"
        offset_hex="${BASH_REMATCH[2]}"
        name="${BASH_REMATCH[3]}"
        grow_flag="${BASH_REMATCH[4]}"
        size_dec=$((16#$size_hex))
        offset_dec=$((16#$offset_hex))
        if [ -n "$grow_flag" ]; then
            PARTITIONS+=("$name $size_dec $offset_dec grow")
        else
            PARTITIONS+=("$name $size_dec $offset_dec")
        fi
    elif [[ "$part" =~ ^-@0x([0-9a-fA-F]+)\(([^:]+)(:grow)?\)$ ]]; then
        # Size '-' means use remaining space
        offset_hex="${BASH_REMATCH[1]}"
        name="${BASH_REMATCH[2]}"
        grow_flag="${BASH_REMATCH[3]}"
        offset_dec=$((16#$offset_hex))
        if [ -n "$grow_flag" ]; then
            PARTITIONS+=("$name - $offset_dec grow")
        else
            PARTITIONS+=("$name - $offset_dec")
        fi
    fi
done < <(echo "$CMDLINE" | tr ',' '\n')

# Display parsed partitions
echo "Parsed partitions:"
for part_info in "${PARTITIONS[@]}"; do
    read -r name size offset grow_flag <<< "$part_info"
    grow_flag="${grow_flag:-}"
    if [[ "$size" == "-" ]]; then
        printf "  %-10s %10s (grow) @ sector %d\n" "$name" "auto" "$offset"
    else
        printf "  %-10s %10d sectors @ sector %d\n" "$name" "$size" "$offset"
    fi
done
#-------------------------------
# Map partitions to images
#-------------------------------
# Find uboot, boot, rootfs partitions
UBOOT_PART=""
BOOT_PART=""
ROOTFS_PART=""
for part_info in "${PARTITIONS[@]}"; do
    read -r name size offset grow_flag <<< "$part_info"
    grow_flag="${grow_flag:-}"
    case "$name" in
        uboot) UBOOT_PART="$part_info" ;;
        boot) BOOT_PART="$part_info" ;;
        rootfs) ROOTFS_PART="$part_info" ;;
    esac
done

if [[ -z "$UBOOT_PART" ]]; then
    echo "Error: 'uboot' partition not found in parameter.txt"
    exit 1
fi
if [[ -z "$BOOT_PART" ]]; then
    echo "Error: 'boot' partition not found in parameter.txt"
    exit 1
fi
if [[ -z "$ROOTFS_PART" ]]; then
    echo "Error: 'rootfs' partition not found in parameter.txt"
    exit 1
fi

# Extract offset and size for each partition
read -r _ UBOOT_SIZE UBOOT_OFFSET <<< "$UBOOT_PART"
read -r _ BOOT_SIZE BOOT_OFFSET <<< "$BOOT_PART"
read -r _ ROOTFS_SIZE ROOTFS_OFFSET <<< "$ROOTFS_PART"

#-------------------------------
# Check input images
#-------------------------------
echo ">>> Checking source images"
for f in "$IDBLOCK_IMG" "$UBOOT_IMG" "$KERNEL_IMG" "$ROOTFS_IMG"; do
    check_exists "$f"
    printf "  %-20s %10s bytes\n" "$f" "$(file_size "$f")"
done

# For rootfs with grow size ('-'), derive partition size from the actual
EXTRA_SECTORS=$(( EXTRA_GB * 1024 * 1024 * 1024 / SECTOR_SIZE ))
if [[ "$ROOTFS_SIZE" == "-" ]]; then
    ROOTFS_BYTES=$(file_size "$ROOTFS_IMG")
    ROOTFS_SIZE=$(( (ROOTFS_BYTES + SECTOR_SIZE - 1) / SECTOR_SIZE + EXTRA_SECTORS ))
    echo "DEBUG: Rootfs partition size set to $ROOTFS_SIZE sectors (rootfs.img $ROOTFS_BYTES bytes + $EXTRA_SECTORS extra sectors)"
fi

#-------------------------------
# Compute total image size
#-------------------------------
# Find the maximum offset + size
TOTAL_SECTORS=0
for part_info in "${PARTITIONS[@]}"; do
    read -r name size offset grow_flag <<< "$part_info"
    grow_flag="${grow_flag:-}"
    if [[ "$name" == "rootfs" ]]; then
        end_sector=$(( offset + ROOTFS_SIZE ))
    elif [[ "$size" != "-" ]]; then
        end_sector=$(( offset + size ))
    else
        continue
    fi
    if (( end_sector > TOTAL_SECTORS )); then
        TOTAL_SECTORS=$end_sector
    fi
done

# Add padding (512KB)
TOTAL_SECTORS=$(( TOTAL_SECTORS + 8192 ))
IMAGE_SIZE=$(( TOTAL_SECTORS * SECTOR_SIZE ))

echo ""
echo "RootFS size: $ROOTFS_SIZE sectors"
echo "Extra space: ${EXTRA_GB}GB (${EXTRA_SECTORS:-0} sectors)"
echo "Total image size: $(( IMAGE_SIZE / 1024 / 1024 )) MB"
echo ""

#-------------------------------
# Create empty image
#-------------------------------
echo ">>> Creating empty image: $OUTPUT_IMG"
dd if=/dev/zero of="$OUTPUT_IMG" bs=$SECTOR_SIZE count=0 seek=$TOTAL_SECTORS status=none

#-------------------------------
# Partition setup
#-------------------------------
echo ">>> Writing GPT partitions"
# NOTE: sgdisk >= 1.0.x: '-o' means --clear (NOT --new), and '-q' suppresses
# actual writes. Zap-all first, then create partitions - sgdisk auto-creates the GPT.
sgdisk -Z "$OUTPUT_IMG"

# Create partitions based on parsed layout
PART_NUM=1
for part_info in "${PARTITIONS[@]}"; do
    read -r name size offset grow_flag <<< "$part_info"
    grow_flag="${grow_flag:-}"
    if [[ "$name" == "rootfs" ]]; then
        size=$ROOTFS_SIZE
    elif [[ "$size" == "-" ]]; then
        # Skip partitions with unknown size
        continue
    fi
    
    # Calculate end sector
    end_sector=$(( offset + size - 1 ))
    
    # Create partition
    sgdisk -n $PART_NUM:$offset:$end_sector -t $PART_NUM:8300 -c $PART_NUM:"$name" "$OUTPUT_IMG"
    
    # Set rootfs partition UUID
    if [[ "$name" == "rootfs" ]]; then
        sgdisk -u $PART_NUM:614e0000-0000-4b53-8000-1d28000054a9 "$OUTPUT_IMG"
    fi

    # Mark grow partition: set GPT attributes bit 48
    if [ "$grow_flag" = "grow" ]; then
        sgdisk -A $PART_NUM:set:48 "$OUTPUT_IMG"
        echo "Marked $name as grow (attr bit 48)"
    fi
    
    PART_NUM=$((PART_NUM + 1))
done

#-------------------------------
# Write each partition
#-------------------------------
function write_part() {
    local src=$1 dst=$2 sector=$3
    echo ">>> Writing $src → $dst @ sector $sector"
    echo "DEBUG: src=$src, dst=$dst, sector=$sector"
    echo "DEBUG: src size=$(stat -Lc%s "$src") bytes"
    echo "DEBUG: dst offset=$((sector * 512)) bytes"
    dd if="$src" of="$dst" bs=$SECTOR_SIZE seek=$sector conv=notrunc status=progress
    # Verify write
    local src_size=$(stat -Lc%s "$src")
    local verify_sectors=$(( (src_size + 511) / 512 ))
    dd if="$dst" of=/tmp/verify.img bs=$SECTOR_SIZE skip=$sector count=$verify_sectors 2>/dev/null
    if cmp -s "$src" /tmp/verify.img; then
        echo "DEBUG: Write verification PASSED"
    else
        echo "DEBUG: Write verification FAILED"
        echo "DEBUG: Expected $(md5sum "$src")"
        echo "DEBUG: Got $(md5sum /tmp/verify.img)"
    fi
    rm -f /tmp/verify.img
}

# Write idblock at fixed offset (sector 64)
write_part "$IDBLOCK_IMG" "$OUTPUT_IMG" 64

# Write other partitions
write_part "$UBOOT_IMG" "$OUTPUT_IMG" $UBOOT_OFFSET
write_part "$KERNEL_IMG" "$OUTPUT_IMG" $BOOT_OFFSET
write_part "$ROOTFS_IMG" "$OUTPUT_IMG" $ROOTFS_OFFSET

#-------------------------------
# Verify image
#-------------------------------
echo ">>> Partition table summary"
sgdisk -p "$OUTPUT_IMG" | sed 's/^/  /'

echo ""
echo "SD update image created successfully: $OUTPUT_IMG"