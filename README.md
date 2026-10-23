# Worldwide Weather Watcher

Station météo embarquée sur une carte **STM32 Nucleo-F401RE**. Elle mesure la température, l'humidité et la pression, récupère la position GPS et la date, puis enregistre tout sur une carte SD sous forme de fichiers texte. Une LED RGB indique le mode en cours et les erreurs, et deux boutons permettent de changer de mode.

Projet réalisé avec **PlatformIO** et le framework **STM32Cube (HAL)**, en C.

---

## Sommaire

1. [Matériel](#matériel)
2. [Branchements](#branchements)
3. [Installation et compilation](#installation-et-compilation)
4. [Les modes de fonctionnement](#les-modes-de-fonctionnement)
5. [La LED d'état](#la-led-détat)
6. [Les fichiers enregistrés](#les-fichiers-enregistrés)
7. [Le mode configuration (console série)](#le-mode-configuration-console-série)
8. [Organisation du code](#organisation-du-code)
9. [Réglages pour les tests](#réglages-pour-les-tests)

---

## Matériel

| Élément | Rôle |
|---|---|
| STM32 Nucleo-F401RE | Microcontrôleur principal |
| Grove BME280 (ou BMP280) | Température, humidité, pression (I2C) |
| Grove RTC DS1307 | Horloge (date et heure) (I2C) |
| Grove GPS Air530Z | Position GPS (UART) |
| Grove Chainable RGB LED (P9813) | LED d'état |
| Grove Dual Button | Bouton vert + bouton rouge |
| Shield / module carte SD | Stockage des mesures (SPI, carte formatée en **FAT32**) |

> Avec un BMP280 à la place du BME280, l'humidité n'est pas disponible et sera notée `NA`.

## Branchements

| Module | Port Grove / broches |
|---|---|
| LED RGB | D6 (horloge sur D6, données sur D7) |
| Double bouton | D8 (bouton 1 = **vert** sur D8, bouton 2 = **rouge** sur D9) |
| GPS | D2 (seulement la réception, le GPS envoie vers la carte) |
| RTC + BME280 | I2C (D14 = SDA, D15 = SCL) |
| Carte SD | SPI1 (SCK, MISO, MOSI) et CS sur D4 |

Toutes les broches sont regroupées dans [`include/board_pins.h`](include/board_pins.h). Si les deux boutons sont inversés, il suffit d'échanger les deux blocs dans ce fichier.

## Installation et compilation

### Prérequis

- [VS Code](https://code.visualstudio.com/) avec l'extension **PlatformIO IDE**, ou PlatformIO en ligne de commande (`pio`).
- Le câble USB de la Nucleo (la sonde ST-LINK sert à la fois à flasher et à la console série).

### Depuis VS Code

Ouvrir le dossier du projet, puis utiliser les boutons de la barre PlatformIO : **Build** (✓), **Upload** (→) et **Serial Monitor** (prise).

### Avec le script `deploy.sh` (Linux / macOS / Git Bash)

Le script compile, flashe la carte et ouvre le moniteur série en une seule commande.

```bash
chmod +x deploy.sh   # la première fois seulement
./deploy.sh          # compile + flash + moniteur
```

| Commande | Effet |
|---|---|
| `./deploy.sh` | compile + flash + moniteur |
| `./deploy.sh build` | compile seulement |
| `./deploy.sh upload` | compile + flash |
| `./deploy.sh monitor` | moniteur série seulement |
| `./deploy.sh clean` | nettoie le build |
| `./deploy.sh fresh` | nettoie puis compile + flash + moniteur |

Le moniteur série tourne à **115200 bauds**.

### Avec PlatformIO en ligne de commande

```bash
pio run -t upload
```

```bash
pio device monitor -b 115200
```

## Les modes de fonctionnement

Les changements de mode se font avec un **appui long de 5 secondes** sur un bouton.

| Mode | Comment y entrer | Ce qu'il fait |
|---|---|---|
| **Standard** | Mode par défaut au démarrage | Une mesure toutes les `LOG_INTERVAL` minutes, enregistrée sur la carte SD |
| **Économique** | Bouton **vert** 5 s depuis le mode standard | Intervalle doublé, et le GPS n'est lu qu'une mesure sur deux |
| **Maintenance** | Bouton **rouge** 5 s depuis standard ou économique | La carte SD est démontée (on peut la retirer sans risque) et les mesures s'affichent en direct sur la console toutes les 5 s |
| **Configuration** | Garder le bouton **rouge** appuyé pendant le démarrage | Plus de mesures : on règle la station depuis la console série |

Pour sortir :
- du mode **économique** : bouton vert 5 s (retour en standard) ou bouton rouge 5 s (maintenance) ;
- du mode **maintenance** : bouton rouge 5 s, on revient au mode d'avant ;
- du mode **configuration** : automatiquement après 30 minutes sans rien taper sur la console.

## La LED d'état

Couleur fixe selon le mode :

| Couleur | Mode |
|---|---|
| Vert | Standard |
| Bleu | Économique |
| Orange | Maintenance |
| Jaune | Configuration |

En cas d'erreur, la LED clignote en **rouge** et dans une deuxième couleur, environ une fois par seconde :

| Clignotement | Erreur |
|---|---|
| Rouge / bleu | Problème avec l'horloge (RTC) |
| Rouge / jaune | GPS qui ne répond pas |
| Rouge / vert (autant de chaque) | Capteur inaccessible |
| Rouge / vert (plus de vert) | Valeur du capteur incohérente (hors des limites réglées) |
| Rouge / blanc (autant de chaque) | Carte SD pleine |
| Rouge / blanc (plus de blanc) | Erreur d'accès à la carte SD |

## Les fichiers enregistrés

Les mesures sont écrites sur la carte SD dans un fichier par jour, nommé `AAMMJJ_0.LOG` (par exemple `261023_0.LOG` pour le 23 octobre 2026). Si l'heure n'est pas disponible, le fichier s'appelle `NODATE_0.LOG`.

Quand le fichier dépasse `FILE_MAX_SIZE` octets, il est renommé en `AAMMJJ_1.LOG`, puis `_2`, etc., et un nouveau `_0` est commencé.

Chaque ligne a ce format (les valeurs manquantes sont notées `NA`) :

```
AAAA-MM-JJ HH:MM:SS;latitude;longitude;pression;temperature;humidite
```

Exemple :

```
2026-10-23 14:30:00;45.76400;4.83570;1013.2;18.4;62.0
```

Chaque ligne est aussi affichée sur la console série, précédée de `[LOG]`.

## Le mode configuration (console série)

Démarrer la carte en gardant le bouton rouge appuyé, puis ouvrir le moniteur série (115200 bauds).

La console est protégée par un mot de passe. Le mot de passe par défaut est `1234` : **pensez à le changer** avec `NEWPASSWORD`. Après 3 essais ratés, la console est bloquée 30 secondes.

```
PASSWORD=1234
HELP
```

### Commandes

| Commande | Effet |
|---|---|
| `PARAM=valeur` | Change un paramètre (voir le tableau ci-dessous) et le sauvegarde |
| `SHOW` | Affiche tous les paramètres et l'heure de la RTC |
| `RESET` | Remet les valeurs par défaut (le mot de passe est gardé) |
| `VERSION` | Affiche la version du programme |
| `CLOCK=HH:MM:SS` | Règle l'heure |
| `DATE=MOIS,JOUR,ANNEE` | Règle la date |
| `DAY=MON` … `SUN` | Règle le jour de la semaine |
| `NEWPASSWORD=xxxx` | Change le mot de passe (4 à 15 caractères) |
| `LOGOUT` | Reverrouille la console |
| `HELP` | Affiche l'aide |

### Paramètres

| Paramètre | Défaut | Plage | Description |
|---|---|---|---|
| `LOG_INTERVAL` | 10 | 1 – 1440 | Temps entre deux mesures (minutes) |
| `FILE_MAX_SIZE` | 2048 | 512 – 65536 | Taille max d'un fichier avant archivage (octets) |
| `TIMEOUT` | 30 | 1 – 600 | Temps max d'attente d'un capteur (secondes) |
| `TEMP_AIR` | 1 | 0 – 1 | Active la mesure de température |
| `MIN_TEMP_AIR` / `MAX_TEMP_AIR` | -10 / 60 | -40 – 85 | Limites au-delà desquelles la température est jugée incohérente (°C) |
| `HYGR` | 1 | 0 – 1 | Active la mesure d'humidité |
| `HYGR_MINT` / `HYGR_MAXT` | 0 / 50 | -40 – 85 | L'humidité n'est enregistrée que si la température est dans cette plage (°C) |
| `PRESSURE` | 1 | 0 – 1 | Active la mesure de pression |
| `PRESSURE_MIN` / `PRESSURE_MAX` | 850 / 1080 | 300 – 1100 | Limites au-delà desquelles la pression est jugée incohérente (hPa) |

Les paramètres sont gardés dans la mémoire flash de la carte : ils ne sont pas perdus quand on la débranche.

## Organisation du code

```
├── platformio.ini        configuration PlatformIO (carte, framework, vitesse du moniteur)
├── deploy.sh             script compile + flash + moniteur
├── include/              fichiers .h
└── src/
    ├── main.c            point d'entrée : init puis boucle App_Step()
    ├── setup.c           init de l'horloge, des GPIO, I2C, SPI et UART
    ├── app_state.c       gestion des modes, des boutons et du rythme des mesures
    ├── acquisition.c     lecture des capteurs et mise en forme de la ligne
    ├── log_manager.c     écriture des fichiers sur la carte SD et archivage
    ├── config_store.c    sauvegarde des paramètres dans la flash
    ├── config_cli.c      console de configuration (mot de passe, commandes)
    ├── led_status.c      couleur de la LED selon le mode et l'erreur
    ├── button_events.c   détection des appuis longs
    ├── bme280.c          pilote du capteur BME280
    ├── grove_gps_air530z.c   lecture des trames GPS (GGA)
    ├── grove_rtc_ds1307.c    pilote de l'horloge RTC
    ├── grove_rgb.c       pilote de la LED RGB
    ├── grove_button.c    lecture des boutons
    ├── sd_logger.c       pilote bas niveau de la carte SD (SPI)
    ├── diskio.c          lien entre FatFs et le pilote SD
    └── ff.c, ffsystem.c, ffunicode.c   bibliothèque FatFs (système de fichiers FAT)
```

La bibliothèque [FatFs](http://elm-chan.org/fsw/ff/) (fichiers `ff*` et `diskio.h`) est l'œuvre de ChaN et garde sa propre licence.

## Réglages pour les tests

Dans [`include/app_config.h`](include/app_config.h) :

| Constante | Utilité |
|---|---|
| `LOG_INTERVAL_UNIT_MS` | `60000` = intervalle en minutes. Mettre `1000` pour que `LOG_INTERVAL` soit en secondes et tester plus vite |
| `CONFIG_INACTIVITY_MS` | Temps avant de quitter le mode configuration (30 min par défaut) |
| `APP_DEBUG_BUTTONS` | Mettre à `1` pour afficher l'état des boutons en boucle au démarrage (test du câblage) |
| `ECO_EXIT_WITH_GREEN` | `1` : on sort du mode économique avec le bouton vert. `0` : avec le bouton rouge |
