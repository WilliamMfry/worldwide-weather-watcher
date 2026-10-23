#!/usr/bin/env bash
# ============================================================================
# deploy.sh — Worldwide Weather Watcher
# Compile + flashe + ouvre le moniteur, en une seule commande.
#
# Usage (depuis n'importe où, le script se place lui-même dans le projet) :
#   ./deploy.sh           -> compile + flash + moniteur (rituel complet)
#   ./deploy.sh upload    -> compile + flash seulement
#   ./deploy.sh build     -> compile seulement
#   ./deploy.sh monitor   -> moniteur seulement
#   ./deploy.sh clean     -> nettoyage (après un git pull ou setup.c changé)
#   ./deploy.sh fresh     -> clean + compile + flash + moniteur
#
# Première fois sous Linux :  chmod +x deploy.sh
# ============================================================================
set -euo pipefail

BAUD=115200            # doit correspondre à monitor_speed dans platformio.ini
ACTION="${1:-all}"

rouge() { printf '\033[31m%s\033[0m\n' "$1"; }
vert()  { printf '\033[32m%s\033[0m\n' "$1"; }
jaune() { printf '\033[33m%s\033[0m\n' "$1"; }

# --- 0. Se placer dans le dossier du script (racine du projet) -------------
cd "$(dirname "$0")"

# --- 1. PIO installé ? ------------------------------------------------------
PIO=""
if command -v pio >/dev/null 2>&1; then
    PIO="pio"
elif [ -x "$HOME/.platformio/penv/bin/pio" ]; then
    PIO="$HOME/.platformio/penv/bin/pio"
elif [ -x "$HOME/.platformio/penv/Scripts/pio.exe" ]; then
    PIO="$HOME/.platformio/penv/Scripts/pio.exe"
fi

if [ -z "${PIO}" ]; then
    rouge "ERREUR : pio introuvable."
    echo "  Installation : curl -fsSL -o get-platformio.py \\"
    echo "    https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py"
    echo "  puis : python3 get-platformio.py"
    echo "  et   : echo 'export PATH=\$PATH:\$HOME/.platformio/penv/bin' >> ~/.bashrc && source ~/.bashrc"
    exit 1
fi

# --- 2. Dossier projet ? ----------------------------------------------------
[ -f platformio.ini ] || {
    rouge "ERREUR : platformio.ini absent à côté de deploy.sh."
    echo "  Place ce script à la racine du projet (là où se trouve platformio.ini)."
    exit 1
}

# --- 3. Port série présent ? occupé ? ---------------------------------------
PORT=""
trouver_port() {
    case "$(uname -s)" in
        Linux*)  PORT="$(ls /dev/ttyACM* 2>/dev/null | head -n1 || true)" ;;
        Darwin*) PORT="$(ls /dev/cu.usbmodem* 2>/dev/null | head -n1 || true)" ;;
        *)       PORT="" ; jaune "Windows : port détecté automatiquement par PlatformIO." ; return 0 ;;
    esac

    if [ -z "${PORT}" ]; then
        jaune "Pas de port série détecté : ST-LINK débranché ou détaché de la VM ?"
        echo "  -> Appareils > USB > rattacher « STLink » à la VM, puis relancer"
        exit 1
    fi
    vert "Port détecté : ${PORT}"

    # ModemManager peut saisir le port au branchement — best effort
    if command -v systemctl >/dev/null 2>&1 && sudo -n true 2>/dev/null; then
        sudo -n systemctl stop ModemManager 2>/dev/null || true
    fi

    # Un seul moniteur à la fois : libérer si occupé
    if command -v fuser >/dev/null 2>&1 && fuser "${PORT}" >/dev/null 2>&1; then
        jaune "${PORT} est occupé (moniteur VS Code ouvert ?)"
        read -r -p "  Le libérer maintenant (fuser -k) ? [o/N] " rep
        if [ "${rep}" = "o" ] || [ "${rep}" = "O" ]; then
            fuser -k "${PORT}" 2>/dev/null || true
            sleep 1
            vert "Port libéré."
        else
            rouge "Port occupé — ferme l'autre moniteur et relance."
            exit 1
        fi
    fi
}

# --- 4. Actions -------------------------------------------------------------
build() {
    vert "=== Compilation (pio run) ==="
    "${PIO}" run
}

upload() {
    vert "=== Compilation + flash (pio run -t upload) ==="
    "${PIO}" run -t upload
    vert "=== [SUCCESS] La carte démarre toute seule ==="
}

monitor() {
    vert "=== Moniteur à ${BAUD} bauds — Ctrl+C pour quitter ==="
    if [ -n "${PORT}" ]; then
        "${PIO}" device monitor -b "${BAUD}" -p "${PORT}"
    else
        "${PIO}" device monitor -b "${BAUD}"
    fi
}

clean() {
    jaune "=== Nettoyage (pio run -t clean) ==="
    "${PIO}" run -t clean
}

case "${ACTION}" in
    build)   build ;;
    upload)  upload ;;
    monitor) trouver_port; monitor ;;
    clean)   clean; vert "Nettoyé. Repars sur ./deploy.sh" ;;
    fresh)   clean; upload; trouver_port; monitor ;;
    all)     upload; trouver_port; monitor ;;
    -h|--help|help)
        echo "Usage : ./deploy.sh [all|build|upload|monitor|clean|fresh]" ;;
    *)
        rouge "Action inconnue : ${ACTION}"
        echo "Usage : ./deploy.sh [all|build|upload|monitor|clean|fresh]"
        exit 1 ;;
esac
