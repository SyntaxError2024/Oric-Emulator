#ifndef BROWSER_H
#define BROWSER_H

#include "common.h"

#define MAX_BROWSER_ENTRIES 512

typedef enum {
    ENTRY_UP,
    ENTRY_DIR,
    ENTRY_TAP,
    ENTRY_DSK,
    ENTRY_ROM
} BrowserEntryType;

typedef struct {
    wchar_t name[MAX_PATH];
    BrowserEntryType type;
} BrowserEntry;

typedef struct {
    bool active;
    wchar_t current_dir[MAX_PATH];
    wchar_t default_rom[MAX_PATH];
    BrowserEntry entries[MAX_BROWSER_ENTRIES];
    int count;
    int selected;
    int scroll_offset;
} FileBrowser;

extern FileBrowser g_browser;

void browser_init(void);
void browser_refresh(void);
void browser_render(void);
void browser_draw(void);
bool browser_handle_key(UINT vk, HWND hwnd);
void browser_load_settings(void);
void browser_save_settings(void);

#endif /* BROWSER_H */
