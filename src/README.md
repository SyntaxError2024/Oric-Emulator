# Émulateur Oric Atmos Win32 (MinGW / Pur GDI)

Émulateur Oric Atmos ultra-léger et autonome développé en **langage C pur** pour Windows avec **MinGW**.
Il n'utilise **aucune bibliothèque tierce** (pas de SDL, pas de DirectX complexe) : uniquement les API natives de Windows (**GDI**, **WinMM**, **Shell API**).

## Points Clés

1. **Glisser-Déposer instantané (`WM_DROPFILES`)** :
   - Glissez n'importe quel fichier `.tap` directement sur la fenêtre de l'émulateur.
   - Les données du jeu sont immédiatement injectées en mémoire RAM.
   - Le jeu **démarre automatiquement** (aucun besoin de taper `CLOAD ""` au clavier !).

2. **Accélération K7 (Turbo Tape)** :
   - Fini les 5 à 15 minutes d'attente à 2400 bauds.
   - Le chargeur analyse les en-têtes Oric (`0x16 0x24`), extrait les adresses de début et de fin, copie les blocs et met à jour les pointeurs BASIC Oric `$9A-$9D` ou exécute directement le code machine.

3. **ROM Oric Atmos fournie (`110fr.rom`)** :
   - L'émulateur charge automatiquement la ROM modifiée `110fr.rom` (16 Ko) présente dans son dossier.
   - Vous pouvez également glisser-déposer un fichier `.rom` sur la fenêtre pour changer de ROM à chaud.

4. **Matériel Oric Atmos émulé** :
   - **Processeur** : MOS 6502 complet à 1.00 MHz avec gestion des interruptions IRQ/NMI et mode décimal (BCD).
   - **Vidéo ULA** : 240 x 224 pixels, modes TEXT (40x28) et HIRES (240x200 + 3 lignes de texte), gestion des attributs série Oric (couleurs ENCRE et PAPIER, clignotement, double hauteur, jeu de caractères alternatif).
   - **Périphérique VIA 6522** : Timers 1 & 2 pour la cadence d'interruption système (100 Hz) et le balayage clavier.
   - **Générateur sonore AY-3-8912** : 3 canaux de tonalité, bruit, enveloppe et port d'entrée/sortie matrice clavier via `waveOut`.
   - **Clavier** : Mapping complet pour les claviers AZERTY (et QWERTY). Touche `F5` pour faire un Reset.

## Compilation sous Windows avec MinGW

### Méthode 1 : En double-cliquant sur `build.bat`
Double-cliquez simplement sur le fichier `build.bat`.

### Méthode 2 : En ligne de commande
Ouvrez une invite de commande dans le dossier et tapez :
```bash
gcc -O3 oric_win32.c -o oric_atmos.exe -lgdi32 -lwinmm -lshell32 -mwindows
```
ou avec `make` :
```bash
make
```

## Utilisation

1. Lancez `oric_atmos.exe`.
2. L'écran de démarrage Oric Atmos s'affiche (`ORIC BASIC 1.1`).
3. Glissez-déposez n'importe quel fichier `.tap` sur la fenêtre :
   - Le jeu se charge instantanément et s'exécute immédiatement !
4. Vous pouvez aussi passer un fichier en argument de ligne de commande :
   ```bash
   oric_atmos.exe "mon_jeu.tap"
   ```
