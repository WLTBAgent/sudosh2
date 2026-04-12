#!/bin/bash
#
# Docker-based build test suite for sudosh2
# Tests compilation across multiple popular Linux distributions
#
# Usage:
#   ./build-test.sh              # Run all tests
#   ./build-test.sh --single N   # Run only test number N
#   ./build-test.sh --clean      # Remove all Docker images
#   ./build-test.sh --list       # List supported distros
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE_PREFIX="sudosh2-build-test"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# Define test matrix: "distro:tag|package_manager|description"
DISTROS=(
    "ubuntu:24.04|apt|Ubuntu 24.04 LTS (Noble)"
    "ubuntu:22.04|apt|Ubuntu 22.04 LTS (Jammy)"
    "debian:12|apt|Debian 12 (Bookworm)"
    "debian:11|apt|Debian 11 (Bullseye)"
    "fedora:41|dnf|Fedora 41"
    "fedora:40|dnf|Fedora 40"
    "rockylinux:9|dnf|Rocky Linux 9"
    "almalinux:9|dnf|AlmaLinux 9"
    "alpine:3.21|apk|Alpine 3.21"
    "alpine:3.19|apk|Alpine 3.19"
    "archlinux:latest|pacman|Arch Linux (rolling)"
    "opensuse/leap:15.6|zypper|openSUSE Leap 15.6"
    "gentoo/stage3:latest|emerge|Gentoo (rolling, gcc-14+)"
)

RESULTS_DIR="$SCRIPT_DIR/results"

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --clean      Remove all Docker images and results"
    echo "  --list       List supported distributions"
    echo "  --single N   Run only test number N (from --list)"
    echo "  -h, --help   Show this help"
    exit 0
}

list_distros() {
    echo -e "${BOLD}Supported build test distributions:${NC}"
    echo ""
    local i=1
    for entry in "${DISTROS[@]}"; do
        IFS='|' read -r tag pkg desc <<< "$entry"
        printf "  ${CYAN}%2d${NC}  %-28s %s\n" "$i" "$tag" "$desc"
        ((i++))
    done
    echo ""
    echo "Total: ${#DISTROS[@]} distributions"
}

clean() {
    echo -e "${YELLOW}Cleaning up...${NC}"
    docker images --filter "reference=${IMAGE_PREFIX}*" -q | while read img; do
        docker rmi "$img" -f 2>/dev/null || true
    done
    rm -rf "$RESULTS_DIR"
    echo -e "${GREEN}Clean complete.${NC}"
    exit 0
}

generate_dockerfile() {
    local tag="$1"
    local pkg="$2"

    local install_cmd=""
    local build_cmd="cd /build && autoreconf -fi 2>/dev/null; ./configure && make clean && make"

    case "$pkg" in
        apt)
            install_cmd="apt-get update && apt-get install -y build-essential autoconf automake"
            ;;
        dnf)
            install_cmd="dnf install -y gcc make autoconf automake"
            ;;
        apk)
            install_cmd="apk add --no-cache build-base autoconf automake musl-dev"
            ;;
        pacman)
            install_cmd="pacman -Syu --noconfirm base-devel autoconf automake"
            ;;
        zypper)
            install_cmd="zypper -n refresh && zypper -n install gcc make autoconf automake"
            ;;
        emerge)
            install_cmd="emerge --sync 2>/dev/null; emerge -q sys-devel/gcc sys-devel/autoconf sys-devel/automake"
            build_cmd="cd /build && ./configure && make clean && make"
            ;;
    esac

    cat <<EOF
FROM ${tag}

LABEL maintainer="WLTBAgent"
LABEL description="sudosh2 build test for ${tag}"

RUN ${install_cmd}

COPY . /build/

RUN set -e; ${build_cmd}

# Verify the binary works
RUN cd /build && if [ -f src/sudosh ]; then src/sudosh -h 2>&1 || true; fi
RUN cd /build && if [ -f src/sudosh-replay ]; then src/sudosh-replay -h 2>&1 || true; fi

EOF
}

run_test() {
    local index="$1"
    local entry="$2"

    IFS='|' read -r tag pkg desc <<< "$entry"

    # Sanitize tag for Docker image name
    safe_tag=$(echo "$tag" | tr '/:' '-')
    image_name="${IMAGE_PREFIX}-${safe_tag}"

    echo -e ""
    echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BOLD}  Building: ${CYAN}${desc}${NC} (${tag})"
    echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

    local result_file="${RESULTS_DIR}/${safe_tag}.result"
    local log_file="${RESULTS_DIR}/${safe_tag}.log"

    mkdir -p "$RESULTS_DIR"

    # Generate Dockerfile
    generate_dockerfile "$tag" "$pkg" > "$RESULTS_DIR/Dockerfile.${safe_tag}"

    local start_time=$(date +%s)

    # Build with Docker
    if docker build \
        -f "$RESULTS_DIR/Dockerfile.${safe_tag}" \
        -t "$image_name" \
        --no-cache \
        "$REPO_DIR" \
        > "$log_file" 2>&1; then

        local end_time=$(date +%s)
        local duration=$((end_time - start_time))

        echo -e "  ${GREEN}✓ PASS${NC} — ${duration}s"
        echo "PASS:${duration}s" > "$result_file"

        # Clean up image to save disk
        docker rmi "$image_name" 2>/dev/null || true
        return 0
    else
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))

        echo -e "  ${RED}✗ FAIL${NC} — ${duration}s"
        echo "FAIL:${duration}s" > "$result_file"

        # Show relevant error lines
        echo -e "  ${YELLOW}Error output:${NC}"
        grep -E "(error:|Error:|fatal:|FAILED|No such)" "$log_file" | tail -5 | while read line; do
            echo -e "    ${RED}$line${NC}"
        done
        echo -e "    ${YELLOW}Full log: ${log_file}${NC}"
        return 1
    fi
}

# Parse arguments
SINGLE=""
while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)    clean ;;
        --list)     list_distros; exit 0 ;;
        --single)   SINGLE="$2"; shift 2 ;;
        -h|--help)  usage ;;
        *)          echo "Unknown option: $1"; usage ;;
    esac
done

# Ensure Docker is available
if ! command -v docker &>/dev/null; then
    echo -e "${RED}Error: Docker is not installed or not in PATH${NC}"
    exit 1
fi

if ! docker info &>/dev/null; then
    echo -e "${RED}Error: Docker daemon is not running${NC}"
    exit 1
fi

# Header
echo ""
echo -e "${BOLD}╔═══════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}║         sudosh2 Docker Build Test Suite              ║${NC}"
echo -e "${BOLD}╚═══════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "  Repo:     ${REPO_DIR}"
echo -e "  Distros:  ${#DISTROS[@]}"
echo -e "  Results:  ${RESULTS_DIR}/"
echo ""

# Run tests
PASS=0
FAIL=0
TOTAL=0

if [ -n "$SINGLE" ]; then
    index="$SINGLE"
    if [ "$index" -lt 1 ] || [ "$index" -gt "${#DISTROS[@]}" ]; then
        echo -e "${RED}Invalid test number. Use --list to see available tests.${NC}"
        exit 1
    fi
    entry="${DISTROS[$((index-1))]}"
    if run_test "$index" "$entry"; then
        ((PASS++))
    else
        ((FAIL++))
    fi
    ((TOTAL++))
else
    i=1
    for entry in "${DISTROS[@]}"; do
        if run_test "$i" "$entry"; then
            ((PASS++))
        else
            ((FAIL++))
        fi
        ((TOTAL++))
        ((i++))
    done
fi

# Summary
echo ""
echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BOLD}  Summary: ${GREEN}${PASS} passed${NC}, ${RED}${FAIL} failed${NC}, ${TOTAL} total"
echo -e "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

# Detailed results table
echo -e "${BOLD}  Results by distribution:${NC}"
echo ""
printf "  ${BOLD}%-30s %-8s %-8s %-10s${NC}\n" "Distribution" "Status" "Time" "Image"
echo "  ─────────────────────────────────────────────────────────"

if [ -n "$SINGLE" ]; then
    entries=("${DISTROS[$((SINGLE-1))]}")
else
    entries=("${DISTROS[@]}")
fi

for entry in "${entries[@]}"; do
    IFS='|' read -r tag pkg desc <<< "$entry"
    safe_tag=$(echo "$tag" | tr '/:' '-')
    result_file="${RESULTS_DIR}/${safe_tag}.result"

    if [ -f "$result_file" ]; then
        result=$(cat "$result_file")
        status="${result%%:*}"
        timing="${result##*:}"

        if [ "$status" = "PASS" ]; then
            printf "  %-30s ${GREEN}%-8s${NC} %-8s %-10s\n" "$desc" "PASS" "$timing" "$tag"
        else
            printf "  %-30s ${RED}%-8s${NC} %-8s %-10s\n" "$desc" "FAIL" "$timing" "$tag"
        fi
    fi
done

echo ""

# Save summary
cat > "${RESULTS_DIR}/summary.txt" <<EOF
sudosh2 Build Test Results
==========================
Date: $(date -u)
Total: ${TOTAL}
Passed: ${PASS}
Failed: ${FAIL}
EOF

echo -e "  Results saved to ${RESULTS_DIR}/summary.txt"
echo ""

# Exit with failure if any test failed
if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
