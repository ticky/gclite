/*
 Game Categories Light v 1.3
 Copyright (C) 2011, Bubbletune

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <pspsdk.h>
#include <pspkernel.h>
#include <psprtc.h>
#include "psppaf.h"
#include <string.h>
#include "categories_lite.h"
#include "vshitem.h"
#include "logger.h"

/* Function pointers */
int (*CategorizeGame)(void *unk, int folder, int unk2);

/* Global variables */
extern char currfw[5];
int by_category_mode;
extern u32 text_addr_game, text_size_game;
void *GetSelectionArg;
void *class_buffer = NULL;
extern char user_buffer[256];

Category *folder_list[2] = { NULL, NULL };

int (*scePafAddGameItems)(void *unk, int count);

/* Functions */
int CategorizeGamePatched(void *unk, int folder, int unk2) {
    int i;
    u32 *array = (u32 *) *(u32 *) ((*(u32 *) (text_addr_game + patches.struct_addr[patch_index])) + ((u32) folder << 2));
    char *title = (char *) array[68 / 4];
    kprintf("called\n");
    Category *p = GetNextCategory(folder_list, NULL, global_pos);

    for (i = patches.index[patch_index]; p; i++) {
        char *name = &p->name;
        kprintf("name: %s\n", name);
        int len = sce_paf_private_strlen(name);

        if (sce_paf_private_strncmp(name, title, len) == 0) {
            if (title[len] == '/') {
                return CategorizeGame(unk, i, unk2);
            }
        }

        p = GetNextCategory(folder_list, p, 0);
    }

    /* uncategorized */
    return CategorizeGame(unk, i - 1, unk2);
}

int scePafAddGameItemsPatched(void *unk, int count UNUSED) {
    kprintf("called\n");
    return scePafAddGameItems(unk, CountCategories(folder_list, 0));
}

wchar_t* GetGameSubtitle(void *arg0 UNUSED, SfoInfo *sfo) {
    int i;
    const char *game_type;
    char subtitle[128];
    char firmware[5];

    kprintf("called\n");

    // fixme
    if (patch_index) {
        // unk0[0x174] -> unk0[0xEC] in 6.30+
        sfo = (SfoInfo *) (u32) sfo - (u32) (0x174 - 0xEC);
    }
    sce_paf_private_strcpy(firmware, sfo->firmware);

    if (sce_paf_private_strcmp(sfo->category, "EG") == 0) {
        game_type = "PSN Game";

        if (sfo->firmware[0] == 0) {
            sce_paf_private_strcpy(firmware, "5.00");
        }
    } else if (sce_paf_private_strcmp(sfo->category, "ME") == 0) {
        game_type = "PS1 Game";

        if (sfo->firmware[0] == 0) {
            sce_paf_private_strcpy(firmware, "3.03");
        }
    } else {
        if (sfo->gamecode[0] == 0 || sce_paf_private_strcmp(sfo->gamecode, "UCJS10041") == 0) {
            game_type = "Homebrew Game";
            sce_paf_private_strcpy(firmware, "2.71");
        } else {
            game_type = "Game";

            if (sfo->firmware[0] == 0) {
                sce_paf_private_strcpy(firmware, "1.00");
            }
        }
    }

    if (firmware[0] >= currfw[0] && firmware[2] >= currfw[2] && firmware[3] >= currfw[3]) {
        sce_paf_private_snprintf(subtitle, 128, "%s (requires %s)", game_type, firmware);
    } else {
        sce_paf_private_snprintf(subtitle, 128, "%s (for %s - %s)", game_type,
                firmware, currfw);
    }

    for (i = 0; i == 0 || subtitle[i - 1]; i++) {
        ((wchar_t *) user_buffer)[i] = subtitle[i];
    }

    return (wchar_t *) user_buffer;
}

wchar_t *GetCategoryTitle(int number) {
    int i;

    kprintf("called, number: %i\n", number);
    Category *p = GetNextCategory(folder_list, NULL, global_pos);

    for (i = patches.index[patch_index]; p; i++) {
        if (i == number) {
            for (i = 0; i == 0 || ((char *) &p->name)[i - 1]; i++) {
                ((wchar_t *) user_buffer)[i] = ((char *) &p->name)[i];
            }

            return (wchar_t *) user_buffer;
        }

        p = GetNextCategory(folder_list, p, global_pos);
    }

    return NULL;
}

void HijackGameClass(int items) {
    if (patch_index) {
        // SCE made it a little more dynamic in 6.30+, so this hack is no longer needed :)
        return;
    }

    /* There's a class inside game_plugin_module that is way to small for
     all the crap we want to do. We need to take control of it to supply
     a buffer of the size we really want. */

    u32 text_addr = text_addr_game;
    u32 text_end = text_addr + text_size_game;
    int size = 816 + (items * 12);

    /* Allocate the buffer */
    if (class_buffer) {
        sce_paf_private_free(class_buffer);
    }

    class_buffer = sce_paf_private_malloc(size);

    void *original = (void *) (text_addr + 0x2C8E8);

    /* Copy the original to the buffer */
    sce_paf_private_memset(class_buffer, 0, size);

    /* Set a pointer to the buffer */
    *(void **) original = class_buffer;

    /* Patch the size */
    _sh(items - 1, text_addr + 0x197D4);
    _sh(items - 1, text_addr + 0x19ECC);

    /* Hijack the opcodes */
    u32 code = 0x24000000 | ((u32) original & 0xFFFF);

    for (; text_addr < text_end; text_addr += 4) {
        u32 read = _lw(text_addr);

        /* Check for addiu */
        if ((read & 0xFC00FFFF) == code) {
            /* Check if the registers are not $zero */
            if ((read & 0x03E00000) && (read & 0x001F0000)) {
                /* addiu -> lw */
                _sw((read & 0x03FFFFFF) | 0x8C000000, text_addr);
            }
        }
    }
}

typedef struct
{
    u32 addr;
    u32 opcode;
} ToggleCategoryPatch;

ToggleCategoryPatch ToggleCategoryPatches_620[] = {
/* Force the branch to "msgvideoms_info_expired" */
        { 0x0000EBF0, 0x100000C7 }, // beq $s2, $v0, loc_EF10 -> b loc_EF10
        { 0x00011EA4, 0x10000065 }, // beq $v1, $v0, loc_1203C -> b loc_1203C

        /* Move a value we need later to a callee-saved register */
        { 0x00012040, 0x00608821 }, // lw $a0, 4($v0) -> move $s1, $v1

        /* Patch the call of scePafGetText to GetCategoryTitle */
        { 0x0000EF1C, (u32) GetCategoryTitle }, // jal scePaf_CB608DE5 -> jal GetCategoryTitle
        { 0x0000EF20, 0x26440001 }, // addiu $a1, $a1, -21896 -> addiu $a0, $s2, 1
        { 0x00012048, (u32) GetCategoryTitle }, // jal scePaf_CB608DE5 -> jal GetCategoryTitle
        { 0x0001204C, 0x26240001 }, // addiu $a1, $a1, -21896 -> addiu $a0, $s1, 1

        /* Patch a usually hardcoded value to a dynamic one from earlier in the code */
        { 0x0000EF34, 0x26450001 }, // li $a1, 2 -> addiu $a1, $s2, 1
        { 0x00012060, 0x26250001 }, // li $a1, 2 -> addiu $a1, $s1, 1

        /* Force a branch to be taken regardless of the timelimit situation */
        { 0x00001524, 0x10000012 }, // beqz $v0, loc_1570 -> b loc_1570

        /* Change a call for hardcoded organization to our own category-based one */
        { 0x0001570, (u32) CategorizeGamePatched }, // jal sub_19B5C -> jal CategorizeGamePatched
        { 0x0001528, 0x8E050004 }, // move $a1, $zr -> lw $a1, 4($s0)

        /* Force a branch to be taken regardless of the timelimit situation */
        { 0x0000A52C, 0x100000DC }, // beqz $v0, loc_A8A0 -> b loc_A8A0

        /* Patch the call of scePafGetText to GetGameSubtitle */
        { 0x0000A8AC, (u32) GetGameSubtitle }, // jal scePaf_CB608DE5 -> jal GetGameSubtitle
        { 0x0000A8B0, 0x02402821 }, // addiu $a1, $a1, -21952 -> move $a1, $s2

        /* Patch the call of scePafAddGameItems to change the number */
        { 0x0000DC38, (u32) scePafAddGameItemsPatched }, // jal scePaf_FBC4392D -> jal scePafAddGameItemsPatched

        /* Patch some checks regarding the number of folders */
        { 0x00019940, 0x00000000 }, { 0x00019A18, 0x00000000 }, { 0x00019AE8, 0x00000000 }, { 0x00019B94, 0x10000006 }, };

ToggleCategoryPatch ToggleCategoryPatches_63x[] = {
/* Change the mode to 'All' in order to avoid all the mess and get to categorizing immediatly */
        { 0x000014C8, 0x10000027 }, // beqz $v1, loc_1568 -> b loc_1568

        /* Change a call for hardcoded organization to our own category-based one */
        { 0x00001568, (u32) CategorizeGamePatched }, // jal sub_1ABF4 -> jal CategorizeGamePatched
        { 0x000014CC, 0x8E050004 }, // li $a1, -1 -> lw $a1, 4($s0)

        /* Patch the call of scePafAddGameItems to change the number */
        { 0x0000E98C, (u32) scePafAddGameItemsPatched }, // jal scePaf_FBC4392D -> jal scePafAddGameItemsPatched

        /* Force the branch to "msgvideoms_info_expired" */
        { 0x0000FDE4, 0x10000019 }, // beq $s3, $v0, loc_FE4C -> b loc_FE4C
        { 0x0001288C, 0x1000001B }, // beq $a0, $v0, loc_128FC -> b loc_128FC

        /* Move a value we need later to a callee-saved register */
        { 0x00012900, 0x00808821 }, // lw $a0, 4($v0) -> move $s1, $a0

        /* Patch the call of scePafGetText to GetCategoryTitle */
        { 0x00012908, (u32) GetCategoryTitle }, // jal scePaf_70082F6F -> jal GetCategoryTitle
        { 0x0001290C, 0x26240000 }, // addiu $a1, $a1, -13004 -> addiu $a0, $s1, 0
        { 0x0000FE54, (u32) GetCategoryTitle }, // jal scePaf_70082F6F -> jal GetCategoryTitle
        { 0x0000FE58, 0x26640000 }, // addiu $a1, $a1, -13716 -> addiu $a0, $s3, 0

        /* Patch a usually hardcoded value to a dynamic one from earlier in the code */
        /* Where it gets subtitle from? ;-) */
        { 0x00012920, 0x26250000 }, // li $a1, 1 -> addiu $a1, $s1, 0
        { 0x0000FE6C, 0x26650000 }, // li $a1, 1 -> addiu $a1, $s3, 0

        /* Force a branch to be taken regardless of the timelimit situation */
        { 0x0000A0AC, 0x100000D8 }, // beqz $v0, loc_A410 -> b loc_A410

        /* Patch the call of scePafGetText to GetGameSubtitle */
        { 0x0000A420, (u32) GetGameSubtitle }, // jal scePaf_CB608DE5 -> jal GetGameSubtitle
        { 0x0000A424, 0x02602821 }, // addiu $a1, $a1, -21952 -> move $a1, $s3
        };

u32 backup[sizeof(ToggleCategoryPatches_620) / sizeof(ToggleCategoryPatch)];

int ToggleCategoryMode(int mode) {
    int i, count;
    kprintf("called, mode: %i\n", mode);
    int total;
    ToggleCategoryPatch *ToggleCategoryPatches;

    if (patch_index == 0) {
        ToggleCategoryPatches = ToggleCategoryPatches_620;
        total = sizeof(ToggleCategoryPatches_620) / sizeof(ToggleCategoryPatch);
    }

    else if (patch_index == 1) {
        ToggleCategoryPatches = ToggleCategoryPatches_63x;
        total = sizeof(ToggleCategoryPatches_63x) / sizeof(ToggleCategoryPatch);
    }

    else {
        return -1;
    }

    u32 text_addr = text_addr_game;
    kprintf("preparing to patch\n");
    if (by_category_mode == 0 && mode == 1) {
        kprintf("by_category_mode == 0 && mode == 1\n");
        by_category_mode = 1;
        count = CountCategories(folder_list, global_pos);
        kprintf("categories count: %i\n", count);
        for (i = 0; i < total; i++) {
            u32 addr = text_addr + ToggleCategoryPatches[i].addr;
            u32 opcode = ToggleCategoryPatches[i].opcode;
            backup[i] = _lw(addr);
            kprintf("checking patch #%i\n", i);
            if ((opcode & 0xFF000000) == 0x08000000) {
                if(opcode == (u32)scePafAddGameItemsPatched) {
                    kprintf("saving scePafAddGameItems jal addr\n");
                    scePafAddGameItems = (void *)U_EXTRACT_CALL(addr);
                } else if(opcode == (u32)CategorizeGamePatched) {
                    kprintf("saving CategorizeGamePatched jal addr\n");
                    CategorizeGame = (void *)U_EXTRACT_CALL(addr);
                }
                MAKE_CALL(addr, opcode);
            }

            else {
                _sw(opcode, addr);
            }
            kprintf("patch done\n");
        }
        kprintf("clearing caches #1\n");
        ClearCachesForUser();
        kprintf("exit #1\n");
        return 0;
    }

    else if (by_category_mode == 1 && mode == 0) {
        kprintf("by_category_mode == 1 && mode == 0\n");
        by_category_mode = 0;

        for (i = 0; i < total; i++) {
            _sw(backup[i], text_addr + ToggleCategoryPatches[i].addr);
        }

        ClearCachesForUser();

        return 0;
    }
    kprintf("exit -1\n");
    return -1;
}