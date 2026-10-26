#ifndef FDC_H
#define FDC_H

#include "common.h"

#define MAX_FDC_DRIVES 4
#define FDC_TRACK_LEN  6400

/* État d'un secteur MFM repéré dans la piste courante */
typedef struct {
    uint8_t *id_ptr;   /* Pointeur sur 0xFE dans le tampon brut */
    uint8_t *data_ptr; /* Pointeur sur 0xFB ou 0xF8 dans le tampon brut */
} FdcSector;

/* Structure représentant une unité de disquette virtuelle */
typedef struct {
    bool loaded;
    uint32_t num_sides;
    uint32_t num_tracks;
    uint32_t geometry;
    uint8_t *raw_image;
    size_t raw_size;
    wchar_t filename[MAX_PATH];
    bool modified;

    /* Piste en cache pour l'accès rapide aux secteurs */
    int cached_track;
    int cached_side;
    int num_sectors;
    FdcSector sectors[32];
} FdcDrive;

/* Contrôleur de disquette Western Digital WD1793 */
typedef struct {
    uint8_t r_status;  /* Registre d'état (0x0310 lecture) */
    uint8_t r_track;   /* Registre de piste (0x0311) */
    uint8_t r_sector;  /* Registre de secteur (0x0312) */
    uint8_t r_data;    /* Registre de données (0x0313) */

    uint8_t c_drive;   /* Unité sélectionnée (0..3) */
    uint8_t c_side;    /* Face sélectionnée (0..1) */
    uint8_t c_track;   /* Position physique de la tête de lecture */
    uint8_t c_sector;  /* Indice secteur en cours */

    uint8_t md_control; /* Latch de contrôle Microdisc 0x0314 */
    uint8_t md_drq;     /* Ligne DRQ inversée pour 0x0318 (0=actif, 0x80=inactif) */
    uint8_t md_intrq;   /* Ligne INTRQ inversée pour 0x0314 (0=actif, 0x80=inactif) */

    int current_op;     /* Opération en cours (COP_*) */
    int curr_offs;      /* Décalage dans le secteur ou l'en-tête */
    int curr_sec_len;   /* Taille du secteur (128, 256, 512, 1024) */
    uint8_t sec_type;   /* Type d'enregistrement (0x00 standard, 0x20 supprimé) */
    FdcSector *curr_sector;

    int32_t delayed_drq; /* Compteur de cycles avant assertion de DRQ */
    int32_t delayed_int; /* Compteur de cycles avant assertion de INTRQ */
    int di_status;       /* Nouvel état à appliquer lors du delayed_int */

    bool last_step_in;   /* Direction du dernier pas pour la commande Step */
} WD1793;

/* Masques de statut WD1793 */
#define WSF_BUSY      0x01
#define WSF_DRQ       0x02
#define WSF_LOSTDAT   0x04
#define WSF_CRCERR    0x08
#define WSF_RNF       0x10
#define WSF_WRITEERR  0x20
#define WSF_WRPROT    0x40
#define WSF_NOTREADY  0x80

/* Bits spécifiques commandes Type I */
#define WSFI_PULSE    0x02
#define WSFI_TRK0     0x04
#define WSFI_SEEKERR  0x10
#define WSFI_HEADL    0x20

/* Bits spécifiques lecture de secteur */
#define WSFR_RECTYP   0x20

/* Commandes en cours */
enum {
    COP_NONE = 0,
    COP_READ_SECTOR,
    COP_READ_SECTORS,
    COP_WRITE_SECTOR,
    COP_WRITE_SECTORS,
    COP_READ_ADDRESS,
    COP_READ_TRACK,
    COP_WRITE_TRACK
};

/* Drapeaux du registre Microdisc Control 0x0314 */
#define MDSF_INTENA  0x01 /* Bit 0 : Activer les interruptions CPU */
#define MDSF_ROMDIS  0x02 /* Bit 1 : 0 = Désactiver ROM BASIC (RAM overlay), 1 = ROM active */
#define MDSF_DENSITY 0x08 /* Bit 3 : Double densité MFM */
#define MDSF_SIDE    0x10 /* Bit 4 : Sélection de face */
#define MDSF_DRIVE   0x60 /* Bits 5-6 : Sélection d'unité (0..3) */
#define MDSF_EPROM   0x80 /* Bit 7 : 0 = EPROM Microdisc active à 0xE000, 1 = inactive */

extern WD1793 g_wd;
extern FdcDrive g_drives[MAX_FDC_DRIVES];
extern bool g_microdisc_active;

void fdc_init(void);
void fdc_reset(void);
void fdc_step(uint32_t cycles);
uint8_t fdc_read(uint16_t addr);
void fdc_write(uint16_t addr, uint8_t val);

bool fdc_insert_dsk(int drive, const wchar_t *path);
void fdc_eject_dsk(int drive);
bool fdc_save_dsk(int drive);
bool fdc_has_irq(void);

#endif /* FDC_H */
