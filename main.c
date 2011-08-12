/*
 *  this file is part of Game Categories Lite
 *
 *  Copyright (C) 2011  Bubbletune
 *  Copyright (C) 2011  Codestation
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <pspsdk.h>
#include <pspkernel.h>
#include <string.h>
#include "categories_lite.h"
#include "psppaf.h"
#include "gcpatches.h"
#include "pspdefs.h"
#include "config.h"
#include "logger.h"

PSP_MODULE_INFO("Game_Categories_Light", 0x0007, 1, 3);
PSP_NO_CREATE_MAIN_THREAD();

/* Global variables */
//GCPatches *PATCHES;
int patch_index;

STMOD_HANDLER previous;
int game_plug = 0;
int sysconf_plug = 0;

int me_fw = 0;
int model;

int checkME() {
    // lets hope that PRO doesn't change/remove this nid and ME doesn't implement it
    if(!sctrlHENFindFunction("SystemControl", "VersionSpoofer", 0x5B18622C)) {
        me_fw = 1;
    }
    return me_fw;
}

int OnModuleStart(SceModule2 *mod) {
    //kprintf(">> %s: loading %s, text_addr: %08X\n", __func__, mod->modname, mod->text_addr);
	if (sce_paf_private_strcmp(mod->modname, "game_plugin_module") == 0) {

	    kprintf("loading %s, text_addr: %08X\n", mod->modname, mod->text_addr);
	    game_plug = 1;
		PatchGamePluginForGCread(mod->text_addr);
		ClearCaches();

	} else if (sce_paf_private_strcmp(mod->modname, "vsh_module") == 0) {
	    kprintf("loading %s, text_addr: %08X\n", mod->modname, mod->text_addr);
        PatchVshmain(mod->text_addr);
        PatchVshmainForSysconf(mod->text_addr);
        PatchVshmainForContext(mod->text_addr);

		/* Make sceKernelGetCompiledSdkVersion clear the caches,
			so that we don't have to create a kernel module just 
			to be able to clear the caches from user mode.*/

		//6.20: 0xFC114573 [0x00009B0C] - SysMemUserForUser_FC114573
		//6.35:	0xFC114573 [0x000099EC] - SysMemUserForUser_FC114573

		MAKE_JUMP(patches.get_compiled_sdk_version[patch_index], ClearCaches);
		ClearCaches();

	} else if (sce_paf_private_strcmp(mod->modname, "sysconf_plugin_module") == 0) {

	    kprintf("loading %s, text_addr: %08X\n", mod->modname, mod->text_addr);
	    sysconf_plug = 1;
	    PatchSysconf(mod->text_addr);
	    ClearCaches();

	} else if (sce_paf_private_strcmp(mod->modname, "scePaf_Module") == 0) {

	    kprintf("loading %s, text_addr: %08X\n", mod->modname, mod->text_addr);
        PatchPaf(mod->text_addr);
        PatchPafForSysconf(mod->text_addr);
        ClearCaches();

    } else if (sce_paf_private_strcmp(mod->modname, "sceVshCommonGui_Module") == 0) {

        kprintf("loading %s, text_addr: %08X\n", mod->modname, mod->text_addr);
        PatchVshCommonGui(mod->text_addr);
        ClearCaches();

    }

	return previous ? previous(mod) : 0;
}

int module_start(SceSize args, void *argp) {
	UNUSED_ARGUMENT(args);
	UNUSED_ARGUMENT(argp);

    model = kuKernelGetModel();
    sce_paf_private_strcpy(filebuf, "xx0:/category_lite.log");
    SET_DEVICENAME(filebuf, model == 4 ? INTERNAL_STORAGE : MEMORY_STICK);
    // paf isn't loaded yet
    kwrite(filebuf, "GCLite 1.3 starting\n", 20);
    // check if the plugin was loaded from ME
    if(checkME()) {
        kwrite(filebuf, "ME compatibility enabled\n", 25);
    }
    // Determine fw group
    u32 devkit = sceKernelDevkitVersion();
    if (devkit == 0x06020010) {
        patch_index = FW_620;
        ResolveNIDs(FW_620);
    } else if (devkit >= 0x06030010 && devkit < 0x06040010) {
        patch_index = FW_630;
    } else if (devkit >= 0x06060010 && devkit < 0x06070010) {
        patch_index = FW_660;
        ResolveNIDs(FW_660);
    } else {
        return 1;
    }

    previous = sctrlHENSetStartModuleHandler(OnModuleStart);
    return 0;
}
