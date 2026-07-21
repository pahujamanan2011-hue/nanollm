#!/usr/bin/env bash

set -e

echo "=========================================="
echo "    Installing NanoLLM Editor (Feather-AI)  "
echo "=========================================="

OS="$(uname -s)"

install_linux() {
    echo "[+] Detected Linux OS"
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        case "$ID" in
            ubuntu|debian|pop|mint)
                echo "[+] Installing dependencies via apt..."
                sudo apt update
                sudo apt install -y build-essential libgtk-3-dev libcurl4-openssl-dev libcjson-dev
                ;;
            arch|manjaro)
                echo "[+] Installing dependencies via pacman..."
                sudo pacman -Sy --needed --noconfirm base-devel gtk3 curl cjson
                ;;
            fedora|rhel|centos)
                echo "[+] Installing dependencies via dnf..."
                sudo dnf install -y gcc make gtk3-devel libcurl-devel cjson-devel
                ;;
            *)
                echo "[-] Unsupported Linux distribution: $ID"
                echo "    Please manually install: gcc, make, gtk3-dev, libcurl-dev, libcjson-dev"
                ;;
        esac
    else
        echo "[-] Unable to determine Linux distribution."
    fi
}

install_mac() {
    echo "[+] Detected macOS"
    if ! command -v brew &> /dev/null; then
        echo "[-] Homebrew is not installed. Installing Homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    fi
    echo "[+] Installing dependencies via Homebrew..."
    brew install gcc make gtk+3 cjson curl pkg-config
}

case "$OS" in
    Linux*)     install_linux;;
    Darwin*)    install_mac;;
    *)          echo "[-] Unsupported operating system: $OS"
                exit 1
                ;;
esac

echo "[+] Dependencies installed successfully."
echo "[+] Compiling NanoLLM Editor..."

make clean
make

echo "=========================================="
echo "    Build Complete!                       "
echo "    Launch using: ./nanollm_editor       "
echo "=========================================="
