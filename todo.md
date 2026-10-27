# Statut du projet Émulateur Oric Atmos Win32

## Problèmes résolus :
- Touche guillemet '"' et '3' :
  Mappage conforme à la table ROM Atmos ($FF78). La touche guillemet '"' correspond à la ligne 3, colonne 7 avec Shift (ligne 4, colonne 4).
  La saisie automatique et le clavier gèrent désormais correctement '"', Shift+3 et la touche apostrophe/guillemet sur claviers AZERTY et QWERTY.

- Auto-CLOAD :
  La commande injectée est désormais `CLOAD ""` (avec guillemets obligatoires pour la ROM BASIC Oric Atmos), évitant l'abandon immédiat de la commande.

- Compatibilité cassette (.TAP) et "Welcome To The Oric Atmos" :
  Correction des hooks ROM $E6C9 (lecture octet) et $E735 (synchronisation 0x16).
  Suppression du hook $E71C qui corrompait le flux de bits série.
  Initialisation de X=0 dans le hook $E735 pour remettre à zéro le registre d'erreur $02B1 de la ROM.
  Rembobinage automatique lors du redémarrage du moteur K7 pour autoriser les chargements successifs.
  Chargement complet des blocs multiples (le bloc 1 charge l'écran de titre, le bloc 2 charge le programme BASIC et l'exécute automatiquement).

- Architecture modulaire :
  Code séparé en fichiers distincts :
  - ay38912.c / ay38912.h : Émulation sonore AY-3-8912 & sortie WaveOut WinMM
  - keyboard.c / keyboard.h : Matrice clavier 8x8, conversion caractères, saisie fluide
  - tape.c / tape.h : Gestion du format .TAP, signaux CB1 et hooks de chargement
  - video.c / video.h : Rendu ULA (texte, HIRES, attributs, palette RVB)
  - cpu.c / cpu.h : Émulateur CPU 6502 cycle-exact
  - via.c / via.h : VIA 6522, timers, interruptions et gestion du port B
  - memory.c / memory.h : Espace mémoire 64 Ko, ROM Atmos et Microdisc
  - common.c / common.h : Débogage console UTF-8 et utilitaires
  - main.c : Fenêtre Win32 GDI, boucle d'émulation 50 Hz, glisser-déposer .TAP/.DSK/.ROM

- Audio et son de chargement K7 :
  - Restauration de l'appel `audio_init(hwnd)` à l'ouverture de la fenêtre Win32.
  - Extension du pool de tampons WaveOut à 8 tampons annulaires (160 ms) avec sélection dynamique des tampons libres.
  - Synthèse audio FSK douce pour le son de modulation de la cassette pendant le chargement.

- Contrôleur de disquette Microdisc (FDC WD1793) :
  - Correction du décodage d'adresse de registres `addr & 0x0F` (l'ancien masque `0x07` confondait le registre DRQ 0x0318 avec 0x0310).
  - Couverture complète de la plage d'E/S FDC Microdisc 0x0310-0x031F dans `memory.c`.
  - Éjection automatique de l'unité de disquette lors du chargement d'un fichier .TAP pour garantir le démarrage en BASIC pur.

- Conformité Windows :
  - Compilable avec MinGW64 (gcc) avec option -municode
  - Aucune bibliothèque externe (uniquement GDI32, WinMM, Shell32 natifs)
  - Fichiers C en UTF-8
  - Fichiers batch (.bat) en Windows-1252 / DOS CRLF
  - Aucun fichier .exe compilé laissé dans le dépôt
