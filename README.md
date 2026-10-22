# Émulateur Oric Atmos Win32 Pur (C Unicode / MinGW64 / GDI)

Émulateur Oric Atmos autonome développé en **langage C pur** pour Windows avec **MinGW64**.
Il n'utilise **aucune bibliothèque tierce** (pas de SDL, pas d'HTML/JS/CSS, pas de makefile) : uniquement les API natives de Windows (**GDI**, **WinMM**, **Shell API**).

## Architecture Modulaire

Le code source est scindé en plusieurs modules clairs et spécialisés :
- `common.h` / `common.c` : Définitions globales, palette graphique RVB Oric, conversion Unicode et console de débogage UTF-8.
- `cpu.h` / `cpu.c` : Émulation complète du processeur MOS 6502 (BCD, IRQ, NMI) et hooks ROM K7.
- `memory.h` / `memory.c` : Routage mémoire (RAM 64 Ko, ROM 16 Ko, superposition ROM contrôleur Microdisc 8 Ko).
- `via.h` / `via.c` : Émulation du contrôleur d'E/S VIA 6522 (Timers T1 et T2, registres ORA/ORB, IFR/IER, PCR).
- `ay38912.h` / `ay38912.c` : Générateur sonore programmable PSG AY-3-8912 (3 voix, bruit, volume, WaveOut Win32).
- `keyboard.h` / `keyboard.c` : Matrice clavier 8x8, support AZERTY et QWERTY, touches accentuées, buffer d'injection de frappe automatique.
- `tape.h` / `tape.c` : Gestionnaire de cassettes Oric (.TAP), détection des blocs et transmission synchrone du flux de données.
- `video.h` / `video.c` : Rendu ULA (modes TEXT 40x28 et HIRES 240x200 + 3 lignes de texte), palette, clignotement, double hauteur et glyphes français.
- `main.c` : Procédure de fenêtre Win32, boucle de messages, glisser-déposer de fichiers (.TAP, .ROM, .DSK) et mode Plein Écran (Alt+Entrée, ratio 4:3 conservé).

## Points Clés

1. **Glisser-Déposer de cassettes .TAP (`WM_DROPFILES`)** :
   - Glissez n'importe quel fichier `.TAP` sur la fenêtre.
   - Le fichier est pris en charge avec notification et extraction automatique du nom du programme.
   - La commande `CLOAD` est saisie automatiquement sans bloquer la machine, et la ROM reçoit immédiatement les bits et octets nécessaires au chargement authentique.

2. **Clavier AZERTY / QWERTY complet** :
   - Isolation stricte entre les signaux de la cassette (CB1 / Hooks ROM) et le retour de matrice clavier (Port B bit 3).
   - Prise en charge des caractères spéciaux (Shift+3 pour les guillemets `"`, touches de curseurs, pavé numérique, etc.).

3. **Plein écran avec respect du ratio 4:3** :
   - `Alt+Entrée` permet de basculer instantanément en plein écran avec centrage automatique et respect des proportions 4:3 de l'Oric.

4. **Console de Débogage Unicode** :
   - Une console Windows s'ouvre automatiquement en UTF-8 pour afficher les événements matériels en temps réel.

## Compilation sous Windows avec MinGW64

### Avec le fichier batch fourni
Double-cliquez simplement sur `build.bat`.

### En ligne de commande
```cmd
gcc -O3 common.c cpu.c memory.c via.c ay38912.c keyboard.c tape.c video.c main.c -o oric_atmos.exe -municode -lgdi32 -lwinmm -lshell32
```

## Utilisation

1. Lancez `oric_atmos.exe`.
2. L'écran de démarrage Oric Atmos s'affiche (`ORIC BASIC 1.1`).
3. Glissez-déposez un fichier `.TAP` sur la fenêtre (par exemple `Welcome To The Oric Atmos.tap` ou `Cyclotron.tap`).
4. Touches utiles :
   - `F1` : Afficher la boîte d'aide.
   - `F5` : Réinitialisation matérielle (Reset).
   - `Alt+Entrée` : Basculer entre Plein Écran et Mode Fenêtré.
