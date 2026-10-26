#include "fdc.h"
#include "memory.h"
#include "cpu.h"
#include <stdlib.h>
#include <string.h>

WD1793 g_wd;
FdcDrive g_drives[MAX_FDC_DRIVES];
bool g_microdisc_active = false;

/* Prototypes internes */
static void fdc_cache_track(FdcDrive *d, int track, int side);
static FdcSector *fdc_find_sector(uint8_t sec_id);
static void fdc_seek_track(uint8_t track);

void fdc_init(void) {
    memset(&g_wd, 0, sizeof(g_wd));
    memset(g_drives, 0, sizeof(g_drives));
    g_microdisc_active = false;
    fdc_reset();
}

void fdc_reset(void) {
    g_wd.r_status = WSFI_TRK0; /* Piste 0 au démarrage */
    g_wd.r_track = 0;
    g_wd.r_sector = 1;
    g_wd.r_data = 0;

    g_wd.c_drive = 0;
    g_wd.c_side = 0;
    g_wd.c_track = 0;
    g_wd.c_sector = 0;

    g_wd.md_control = 0x00;
    g_wd.md_drq = 0x80;   /* DRQ inactif (actif à 0) */
    g_wd.md_intrq = 0x80; /* INTRQ inactif (actif à 0) */

    g_wd.current_op = COP_NONE;
    g_wd.curr_offs = 0;
    g_wd.curr_sec_len = 0;
    g_wd.sec_type = 0;
    g_wd.curr_sector = NULL;

    g_wd.delayed_drq = 0;
    g_wd.delayed_int = 0;
    g_wd.di_status = -1;
    g_wd.last_step_in = false;

    if (g_drives[0].loaded) {
        g_microdisc_active = true;
        g_romdis = true;          /* Désactive ROM BASIC Oric (overlay RAM accessible) */
        g_microdis_eprom = true;  /* Mappe l'EPROM Microdisc à 0xE000-0xFFFF */
        fdc_cache_track(&g_drives[0], 0, 0);
    } else {
        g_microdisc_active = false;
        g_romdis = false;
        g_microdis_eprom = false;
    }

    cpu_update_irq();
}

bool fdc_has_irq(void) {
    if (!g_microdisc_active) return false;
    /* Interruption demandée si bit 0 (MDSF_INTENA) armé et INTRQ actif (md_intrq == 0) */
    return ((g_wd.md_control & MDSF_INTENA) != 0) && (g_wd.md_intrq == 0);
}

bool fdc_insert_dsk(int drive, const wchar_t *path) {
    if (drive < 0 || drive >= MAX_FDC_DRIVES || !path) return false;

    FILE *f = _wfopen(path, L"rb");
    if (!f) {
        debug_log(L"FDC: Impossible d'ouvrir l'image disque %ls", path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz < 256 + FDC_TRACK_LEN) {
        fclose(f);
        debug_log(L"FDC: Fichier disque trop petit (%ld octets)", sz);
        return false;
    }

    uint8_t *buf = (uint8_t *)malloc(sz);
    if (!buf) {
        fclose(f);
        return false;
    }

    if (fread(buf, 1, sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return false;
    }
    fclose(f);

    /* Vérification de la signature MFM_DISK ou ORICDISK */
    if (memcmp(buf, "MFM_DISK", 8) != 0 && memcmp(buf, "ORICDISK", 8) != 0) {
        free(buf);
        debug_log(L"FDC: En-tête de disquette invalide dans %ls", path);
        return false;
    }

    uint32_t sides = buf[8] | (buf[9] << 8) | (buf[10] << 16) | (buf[11] << 24);
    uint32_t tracks = buf[12] | (buf[13] << 8) | (buf[14] << 16) | (buf[15] << 24);
    uint32_t geom = buf[16] | (buf[17] << 8) | (buf[18] << 16) | (buf[19] << 24);

    if (sides < 1 || sides > 2 || tracks < 1 || tracks > 128) {
        free(buf);
        debug_log(L"FDC: Géométrie non supportée (faces=%u, pistes=%u)", sides, tracks);
        return false;
    }

    /* Libérer l'ancienne image si présente */
    if (g_drives[drive].raw_image) {
        if (g_drives[drive].modified) {
            fdc_save_dsk(drive);
        }
        free(g_drives[drive].raw_image);
    }

    g_drives[drive].raw_image = buf;
    g_drives[drive].raw_size = (size_t)sz;
    g_drives[drive].num_sides = sides;
    g_drives[drive].num_tracks = tracks;
    g_drives[drive].geometry = geom;
    g_drives[drive].loaded = true;
    g_drives[drive].modified = false;
    g_drives[drive].cached_track = -1;
    g_drives[drive].cached_side = -1;
    g_drives[drive].num_sectors = 0;
    wcsncpy(g_drives[drive].filename, path, MAX_PATH - 1);
    g_drives[drive].filename[MAX_PATH - 1] = 0;

    /* S'assurer que la ROM Microdisc est bien chargée */
    if (!g_microdis_loaded) {
        if (!oric_load_microdis_rom(L"./roms/MICRODIS.ROM")) {
            if (!oric_load_microdis_rom(L"../roms/MICRODIS.ROM")) {
                oric_load_microdis_rom(L"MICRODIS.ROM");
            }
        }
    }

    g_microdisc_active = true;
    debug_log(L"FDC: Disquette insérée dans drive %d: %ls (faces=%u, pistes=%u)",
              drive, path, sides, tracks);

    return true;
}

void fdc_eject_dsk(int drive) {
    if (drive < 0 || drive >= MAX_FDC_DRIVES) return;
    if (g_drives[drive].loaded) {
        if (g_drives[drive].modified) {
            fdc_save_dsk(drive);
        }
        if (g_drives[drive].raw_image) {
            free(g_drives[drive].raw_image);
            g_drives[drive].raw_image = NULL;
        }
        g_drives[drive].loaded = false;
        g_drives[drive].raw_size = 0;
        g_drives[drive].num_sectors = 0;
        g_drives[drive].cached_track = -1;
        g_drives[drive].cached_side = -1;
    }
}

bool fdc_save_dsk(int drive) {
    if (drive < 0 || drive >= MAX_FDC_DRIVES) return false;
    if (!g_drives[drive].loaded || !g_drives[drive].raw_image || !g_drives[drive].modified) {
        return false;
    }
    FILE *f = _wfopen(g_drives[drive].filename, L"wb");
    if (!f) return false;
    size_t written = fwrite(g_drives[drive].raw_image, 1, g_drives[drive].raw_size, f);
    fclose(f);
    if (written == g_drives[drive].raw_size) {
        g_drives[drive].modified = false;
        debug_log(L"FDC: Disquette sauvegardée avec succès (%u octets)", (unsigned)written);
        return true;
    }
    return false;
}

static void fdc_cache_track(FdcDrive *d, int track, int side) {
    if (!d || !d->loaded || !d->raw_image) return;
    if (d->cached_track == track && d->cached_side == side) return;

    d->cached_track = -1;
    d->cached_side = -1;
    d->num_sectors = 0;

    for (int i = 0; i < 32; i++) {
        d->sectors[i].id_ptr = NULL;
        d->sectors[i].data_ptr = NULL;
    }

    if (side >= (int)d->num_sides || track >= (int)d->num_tracks) return;

    size_t trk_offset = 256 + ((size_t)side * d->num_tracks + (size_t)track) * FDC_TRACK_LEN;
    if (trk_offset + FDC_TRACK_LEN > d->raw_size) return;

    uint8_t *ptr = d->raw_image + trk_offset;
    uint8_t *eot = ptr + FDC_TRACK_LEN;
    int sector_count = 0;

    while (ptr < eot && sector_count < 32) {
        /* Recherche du marqueur d'adresse ID: 0xA1 0xA1 0xA1 0xFE */
        while ((ptr + 3 < eot) && !(ptr[0] == 0xA1 && ptr[1] == 0xA1 && ptr[2] == 0xA1 && ptr[3] == 0xFE)) {
            ptr++;
        }
        ptr += 3; /* Se positionne sur 0xFE */
        if (ptr >= eot) break;

        d->sectors[sector_count].id_ptr = ptr;
        d->sectors[sector_count].data_ptr = NULL;
        sector_count++;

        uint8_t n = ptr[4]; /* Code de taille du secteur */
        ptr += 7;           /* Saute l'en-tête ID (FE, C, H, R, N + 2 CRC) */

        /* Recherche du marqueur de données : 0xFB (normal) ou 0xF8 (supprimé) */
        while (ptr < eot && ptr[0] != 0xFB && ptr[0] != 0xF8) {
            ptr++;
        }
        if (ptr >= eot) break;

        d->sectors[sector_count - 1].data_ptr = ptr;
        /* Saute le champ de données : 1 octet de marqueur + (128 << n) octets + 2 octets CRC */
        ptr += (1 << (n + 7)) + 3;
    }

    if (sector_count > 0) {
        d->num_sectors = sector_count;
        d->cached_track = track;
        d->cached_side = side;
    }
}

static FdcSector *fdc_find_sector(uint8_t sec_id) {
    FdcDrive *d = &g_drives[g_wd.c_drive];
    if (!d->loaded) return NULL;

    fdc_cache_track(d, g_wd.c_track, g_wd.c_side);
    if (d->num_sectors < 1) return NULL;

    /* Recherche séquentielle sur 2 rotations de la piste */
    for (int rev = 0; rev < 2; rev++) {
        for (int i = 0; i < d->num_sectors; i++) {
            g_wd.c_sector = (g_wd.c_sector + 1) % d->num_sectors;
            if (g_wd.c_sector == 0) {
                g_wd.r_status |= WSFI_PULSE;
            }
            if (d->sectors[g_wd.c_sector].id_ptr && d->sectors[g_wd.c_sector].id_ptr[3] == sec_id) {
                return &d->sectors[g_wd.c_sector];
            }
        }
    }
    return NULL;
}

static void fdc_seek_track(uint8_t track) {
    FdcDrive *d = &g_drives[g_wd.c_drive];
    if (d->loaded) {
        if (track >= d->num_tracks) {
            track = (d->num_tracks > 0) ? (uint8_t)(d->num_tracks - 1) : 0;
            g_wd.di_status = WSFI_HEADL | WSFI_SEEKERR;
        } else {
            g_wd.di_status = WSFI_HEADL | WSFI_PULSE;
        }

        fdc_cache_track(d, track, g_wd.c_side);
        g_wd.c_track = track;
        g_wd.c_sector = 0;
        g_wd.r_track = track;

        g_wd.delayed_int = 20;
        if (g_wd.c_track == 0) {
            g_wd.di_status |= WSFI_TRK0;
        }
        return;
    }

    /* Aucune disquette dans l'unité */
    g_wd.r_track = 0;
    g_wd.md_intrq = 0; /* INTRQ actif */
    g_wd.r_status = WSF_NOTREADY | WSFI_SEEKERR;
    cpu_update_irq();
}

void fdc_step(uint32_t cycles) {
    /* Gestion du déclenchement différé de l'interruption INTRQ */
    if (g_wd.delayed_int > 0) {
        g_wd.delayed_int -= (int32_t)cycles;
        if (g_wd.delayed_int <= 0) {
            g_wd.delayed_int = 0;
            if (g_wd.di_status != -1) {
                g_wd.r_status = (uint8_t)g_wd.di_status;
                g_wd.di_status = -1;
            }
            g_wd.md_intrq = 0; /* Active la ligne INTRQ (active à l'état bas) */
            cpu_update_irq();
        }
    }

    /* Gestion du déclenchement différé de DRQ */
    if (g_wd.delayed_drq > 0) {
        g_wd.delayed_drq -= (int32_t)cycles;
        if (g_wd.delayed_drq <= 0) {
            g_wd.delayed_drq = 0;
            g_wd.r_status |= WSF_DRQ;
            g_wd.md_drq = 0; /* Active la ligne DRQ (active à l'état bas) */
        }
    }
}

uint8_t fdc_read(uint16_t addr) {
    uint8_t reg = addr & 0x07;

    switch (reg) {
        case 0: /* 0x0310 : Registre de Statut */
            g_wd.md_intrq = 0x80; /* La lecture du statut acquitte INTRQ */
            cpu_update_irq();
            return g_wd.r_status;

        case 1: /* 0x0311 : Registre de Piste */
            return g_wd.r_track;

        case 2: /* 0x0312 : Registre de Secteur */
            return g_wd.r_sector;

        case 3: /* 0x0313 : Registre de Données */
            switch (g_wd.current_op) {
                case COP_READ_SECTOR:
                case COP_READ_SECTORS: {
                    if (!g_wd.curr_sector || !g_wd.curr_sector->data_ptr) {
                        g_wd.r_status &= ~WSF_DRQ;
                        g_wd.r_status |= WSF_RNF;
                        g_wd.md_drq = 0x80;
                        g_wd.current_op = COP_NONE;
                        break;
                    }

                    /* Premier octet : vérification marqueur deleted */
                    if (g_wd.curr_offs == 0) {
                        g_wd.sec_type = (g_wd.curr_sector->data_ptr[g_wd.curr_offs++] == 0xF8) ? WSFR_RECTYP : 0x00;
                    }

                    g_wd.r_data = g_wd.curr_sector->data_ptr[g_wd.curr_offs++];
                    g_wd.r_status &= ~WSF_DRQ;
                    g_wd.md_drq = 0x80; /* Acquitte DRQ */

                    /* Fin du secteur ? */
                    if (g_wd.curr_offs > g_wd.curr_sec_len) {
                        if (g_wd.current_op == COP_READ_SECTORS) {
                            g_wd.r_sector++;
                            g_wd.curr_offs = 0;
                            g_wd.curr_sector = fdc_find_sector(g_wd.r_sector);
                            if (!g_wd.curr_sector) {
                                g_wd.delayed_int = 20;
                                g_wd.di_status = g_wd.sec_type;
                                g_wd.current_op = COP_NONE;
                                g_wd.r_status &= ~WSF_DRQ;
                                g_wd.md_drq = 0x80;
                                break;
                            }
                            g_wd.delayed_drq = 180;
                            break;
                        }
                        /* Secteur unique */
                        g_wd.delayed_int = 32;
                        g_wd.di_status = g_wd.sec_type;
                        g_wd.current_op = COP_NONE;
                        g_wd.r_status &= ~WSF_DRQ;
                        g_wd.md_drq = 0x80;
                    } else {
                        /* Octet suivant prêt après 32 cycles */
                        g_wd.delayed_drq = 32;
                    }
                    break;
                }

                case COP_READ_TRACK: {
                    FdcDrive *d = &g_drives[g_wd.c_drive];
                    if (d->loaded) {
                        size_t trk_offset = 256 + ((size_t)g_wd.c_side * d->num_tracks + (size_t)g_wd.c_track) * FDC_TRACK_LEN;
                        if (trk_offset + g_wd.curr_offs < d->raw_size) {
                            g_wd.r_data = d->raw_image[trk_offset + g_wd.curr_offs];
                        } else {
                            g_wd.r_data = 0;
                        }
                    } else {
                        g_wd.r_data = 0;
                    }
                    g_wd.curr_offs++;
                    g_wd.r_status &= ~WSF_DRQ;
                    g_wd.md_drq = 0x80;

                    if (g_wd.curr_offs >= FDC_TRACK_LEN) {
                        g_wd.delayed_int = 20;
                        g_wd.di_status = 0;
                        g_wd.current_op = COP_NONE;
                    } else {
                        g_wd.delayed_drq = 32;
                    }
                    break;
                }

                case COP_READ_ADDRESS: {
                    if (!g_wd.curr_sector || !g_wd.curr_sector->id_ptr) {
                        g_wd.r_status &= ~WSF_DRQ;
                        g_wd.md_drq = 0x80;
                        g_wd.current_op = COP_NONE;
                        break;
                    }
                    if (g_wd.curr_offs == 0) {
                        g_wd.r_sector = g_wd.curr_sector->id_ptr[1];
                    }
                    g_wd.r_data = g_wd.curr_sector->id_ptr[++g_wd.curr_offs];
                    g_wd.r_status &= ~WSF_DRQ;
                    g_wd.md_drq = 0x80;
                    if (g_wd.curr_offs >= 6) {
                        g_wd.delayed_int = 20;
                        g_wd.di_status = 0;
                        g_wd.current_op = COP_NONE;
                    } else {
                        g_wd.delayed_drq = 32;
                    }
                    break;
                }

                default:
                    break;
            }
            return g_wd.r_data;

        case 4: /* 0x0314 : Microdisc Control / INTRQ State */
            /* Bit 7 reflète l'état inversé de INTRQ (0 = actif, 0x80 = inactif) */
            return g_wd.md_intrq | 0x7F;

        default:
            /* 0x0318 : Microdisc DRQ State */
            if (reg == 8 || (addr & 0x0F) == 8) {
                return g_wd.md_drq | 0x7F;
            }
            return 0xFF;
    }
}

void fdc_write(uint16_t addr, uint8_t val) {
    uint8_t reg = addr & 0x07;

    switch (reg) {
        case 0: /* 0x0310 : Registre de Commande */
            g_wd.md_intrq = 0x80;
            cpu_update_irq();

            switch (val & 0xE0) {
                case 0x00: /* Restore ou Seek (Type I) */
                    if ((val & 0x10) == 0x00) {
                        /* Restore : tête sur piste 0 */
                        g_wd.r_status = WSF_BUSY;
                        if (val & 0x08) g_wd.r_status |= WSFI_HEADL;
                        fdc_seek_track(0);
                        g_wd.current_op = COP_NONE;
                    } else {
                        /* Seek : tête sur la piste indiquée par le registre de données */
                        g_wd.r_status = WSF_BUSY;
                        if (val & 0x08) g_wd.r_status |= WSFI_HEADL;
                        fdc_seek_track(g_wd.r_data);
                        g_wd.current_op = COP_NONE;
                    }
                    break;

                case 0x20: /* Step (Type I) */
                    g_wd.r_status = WSF_BUSY;
                    if (val & 0x08) g_wd.r_status |= WSFI_HEADL;
                    if (g_wd.last_step_in) {
                        fdc_seek_track((uint8_t)(g_wd.c_track + 1));
                    } else {
                        fdc_seek_track((g_wd.c_track > 0) ? (uint8_t)(g_wd.c_track - 1) : 0);
                    }
                    g_wd.current_op = COP_NONE;
                    break;

                case 0x40: /* Step-In (Type I) */
                    g_wd.r_status = WSF_BUSY;
                    if (val & 0x08) g_wd.r_status |= WSFI_HEADL;
                    fdc_seek_track((uint8_t)(g_wd.c_track + 1));
                    g_wd.last_step_in = true;
                    g_wd.current_op = COP_NONE;
                    break;

                case 0x60: /* Step-Out (Type I) */
                    g_wd.r_status = WSF_BUSY;
                    if (val & 0x08) g_wd.r_status |= WSFI_HEADL;
                    if (g_wd.c_track > 0) {
                        fdc_seek_track((uint8_t)(g_wd.c_track - 1));
                    } else {
                        fdc_seek_track(0);
                    }
                    g_wd.last_step_in = false;
                    g_wd.current_op = COP_NONE;
                    break;

                case 0x80: /* Read Sector (Type II) */
                    g_wd.curr_offs = 0;
                    g_wd.curr_sector = fdc_find_sector(g_wd.r_sector);
                    if (!g_wd.curr_sector) {
                        g_wd.r_status = WSF_RNF;
                        g_wd.md_drq = 0x80;
                        g_wd.md_intrq = 0; /* Assert INTRQ */
                        g_wd.current_op = COP_NONE;
                        cpu_update_irq();
                        break;
                    }
                    g_wd.curr_sec_len = 1 << (g_wd.curr_sector->id_ptr[4] + 7);
                    g_wd.r_status = WSF_BUSY | WSF_NOTREADY;
                    g_wd.delayed_drq = 60;
                    g_wd.current_op = (val & 0x10) ? COP_READ_SECTORS : COP_READ_SECTOR;
                    break;

                case 0xA0: /* Write Sector (Type II) */
                    g_wd.curr_offs = 0;
                    g_wd.curr_sector = fdc_find_sector(g_wd.r_sector);
                    if (!g_wd.curr_sector) {
                        g_wd.r_status = WSF_RNF;
                        g_wd.md_drq = 0x80;
                        g_wd.md_intrq = 0;
                        g_wd.current_op = COP_NONE;
                        cpu_update_irq();
                        break;
                    }
                    g_wd.curr_sec_len = 1 << (g_wd.curr_sector->id_ptr[4] + 7);
                    g_wd.r_status = WSF_BUSY | WSF_NOTREADY;
                    g_wd.delayed_drq = 500;
                    g_wd.current_op = (val & 0x10) ? COP_WRITE_SECTORS : COP_WRITE_SECTOR;
                    break;

                case 0xC0:
                    if ((val & 0x10) == 0x00) {
                        /* Read Address (Type III) */
                        g_wd.curr_offs = 0;
                        FdcDrive *d = &g_drives[g_wd.c_drive];
                        if (d->loaded) {
                            fdc_cache_track(d, g_wd.c_track, g_wd.c_side);
                            g_wd.curr_sector = (d->num_sectors > 0) ? &d->sectors[0] : NULL;
                        } else {
                            g_wd.curr_sector = NULL;
                        }
                        if (!g_wd.curr_sector) {
                            g_wd.r_status = WSF_RNF;
                            g_wd.md_drq = 0x80;
                            g_wd.current_op = COP_NONE;
                            g_wd.md_intrq = 0;
                            cpu_update_irq();
                            break;
                        }
                        g_wd.r_status = WSF_NOTREADY | WSF_BUSY | WSF_DRQ;
                        g_wd.md_drq = 0;
                        g_wd.current_op = COP_READ_ADDRESS;
                    } else {
                        /* Force Interrupt (Type IV) */
                        g_wd.r_status = (g_wd.c_track == 0) ? WSFI_TRK0 : 0;
                        g_wd.md_drq = 0x80;
                        g_wd.delayed_int = 0;
                        g_wd.delayed_drq = 0;
                        g_wd.current_op = COP_NONE;
                        if (val & 0x0F) {
                            g_wd.md_intrq = 0;
                            cpu_update_irq();
                        } else {
                            g_wd.md_intrq = 0x80;
                            cpu_update_irq();
                        }
                    }
                    break;

                case 0xE0:
                    if ((val & 0x10) == 0x00) {
                        /* Read Track (Type III) */
                        g_wd.curr_offs = 0;
                        g_wd.r_status = WSF_BUSY | WSF_NOTREADY;
                        g_wd.delayed_drq = 60;
                        g_wd.current_op = COP_READ_TRACK;
                    } else {
                        /* Write Track (Format) */
                        g_wd.curr_offs = 0;
                        g_wd.r_status = WSF_NOTREADY | WSF_BUSY;
                        g_wd.delayed_drq = 500;
                        g_wd.md_drq = 0x80;
                        g_wd.md_intrq = 0x80;
                        g_wd.delayed_int = 0;
                        g_wd.current_op = COP_WRITE_TRACK;
                    }
                    break;
            }
            break;

        case 1: /* 0x0311 : Registre de Piste */
            g_wd.r_track = val;
            break;

        case 2: /* 0x0312 : Registre de Secteur */
            g_wd.r_sector = val;
            break;

        case 3: /* 0x0313 : Registre de Données */
            g_wd.r_data = val;
            switch (g_wd.current_op) {
                case COP_WRITE_SECTOR:
                case COP_WRITE_SECTORS:
                    if (!g_wd.curr_sector || !g_wd.curr_sector->data_ptr) {
                        g_wd.r_status &= ~WSF_DRQ;
                        g_wd.r_status |= WSF_RNF;
                        g_wd.md_drq = 0x80;
                        g_wd.current_op = COP_NONE;
                        break;
                    }
                    if (g_wd.curr_offs == 0) {
                        g_wd.curr_sector->data_ptr[g_wd.curr_offs++] = 0xFB;
                    }
                    g_wd.curr_sector->data_ptr[g_wd.curr_offs++] = g_wd.r_data;
                    g_drives[g_wd.c_drive].modified = true;
                    g_wd.r_status &= ~WSF_DRQ;
                    g_wd.md_drq = 0x80;

                    if (g_wd.curr_offs > g_wd.curr_sec_len) {
                        if (g_wd.current_op == COP_WRITE_SECTORS) {
                            g_wd.r_sector++;
                            g_wd.curr_offs = 0;
                            g_wd.curr_sector = fdc_find_sector(g_wd.r_sector);
                            if (!g_wd.curr_sector) {
                                g_wd.delayed_int = 20;
                                g_wd.di_status = g_wd.sec_type;
                                g_wd.current_op = COP_NONE;
                                g_wd.r_status &= ~WSF_DRQ;
                                g_wd.md_drq = 0x80;
                                break;
                            }
                            g_wd.delayed_drq = 180;
                            break;
                        }
                        g_wd.delayed_int = 32;
                        g_wd.di_status = g_wd.sec_type;
                        g_wd.current_op = COP_NONE;
                        g_wd.r_status &= ~WSF_DRQ;
                        g_wd.md_drq = 0x80;
                    } else {
                        g_wd.delayed_drq = 32;
                    }
                    break;

                case COP_WRITE_TRACK: {
                    FdcDrive *d = &g_drives[g_wd.c_drive];
                    if (d->loaded) {
                        size_t trk_offset = 256 + ((size_t)g_wd.c_side * d->num_tracks + (size_t)g_wd.c_track) * FDC_TRACK_LEN;
                        if (trk_offset + g_wd.curr_offs < d->raw_size) {
                            d->raw_image[trk_offset + g_wd.curr_offs] = g_wd.r_data;
                            d->modified = true;
                        }
                    }
                    g_wd.curr_offs++;
                    g_wd.r_status &= ~WSF_DRQ;
                    g_wd.md_drq = 0x80;
                    if (g_wd.curr_offs >= FDC_TRACK_LEN) {
                        g_wd.delayed_int = 20;
                        g_wd.di_status = 0;
                        g_wd.current_op = COP_NONE;
                    } else {
                        g_wd.delayed_drq = 32;
                    }
                    break;
                }

                default:
                    break;
            }
            break;

        case 4: /* 0x0314 : Microdisc Control Latch */
            g_wd.md_control = val;
            g_wd.c_drive = (val & MDSF_DRIVE) >> 5;
            g_wd.c_side  = (val & MDSF_SIDE) ? 1 : 0;
            g_romdis     = (val & MDSF_ROMDIS) ? false : true;
            g_microdis_eprom = (val & MDSF_EPROM) ? false : true;

            cpu_update_irq();
            break;

        default:
            if (reg == 8 || (addr & 0x0F) == 8) {
                /* 0x0318 : écriture directe sur la ligne DRQ */
                g_wd.md_drq = val & 0x80;
            }
            break;
    }
}
