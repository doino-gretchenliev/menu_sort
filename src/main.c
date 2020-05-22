#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <math.h>
#include <gctypes.h>
#include <fat.h>
#include <iosuhax.h>
#include <iosuhax_devoptab.h>
#include <iosuhax_disc_interface.h>
#include "dynamic_libs/os_functions.h"
#include "dynamic_libs/fs_functions.h"
#include "dynamic_libs/gx2_functions.h"
#include "dynamic_libs/sys_functions.h"
#include "dynamic_libs/vpad_functions.h"
#include "dynamic_libs/padscore_functions.h"
#include "dynamic_libs/socket_functions.h"
#include "dynamic_libs/ax_functions.h"
#include "dynamic_libs/act_functions.h"
//#include "dynamic_libs/ccr_functions.h"
#include "fs/fs_utils.h"
#include "fs/sd_fat_devoptab.h"
#include "system/memory.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "utils/file.h"
#include "utils/screen.h"
#include "utils/dict.h"
#include "common/common.h"
#include <dirent.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

#define TITLE_TEXT "Sort Wii U Menu v1.1.0 - Yardape8000 & doino-gretchenliev"
#define HBL_TITLE_ID 0x13374842

static const char *systemXmlPath = "storage_slc:/config/system.xml";
static const char *syshaxXmlPath = "storage_slc:/config/syshax.xml";
static const char *cafeXmlPath = "storage_slc:/proc/prefs/cafe.xml";
static const char *dontmovePath = "sd:/wiiu/apps/menu_sort/dontmove";
static const char *gamemapPath = "sd:/wiiu/apps/menu_sort/titlesmap";
static const char *backupPath = "sd:/wiiu/apps/menu_sort/BaristaAccountSaveFile.dat";
static const char *languages[] = {"JA", "EN", "FR", "DE", "IT", "ES", "ZHS", "KO", "NL", "PT", "RU", "ZHT"};
static char languageText[14] = "longname_en";

int badNamingMode = 0;
int ignoreThe = 0;
int backup = 0;
int restore = 0;

struct MenuItemStruct
{
	u32 ID;
	u32 type;
	u32 titleIDPrefix;
	char name[65];
};

enum itemTypes
{
	MENU_ITEM_NAND = 0x01,
	MENU_ITEM_USB = 0x02,
	MENU_ITEM_DISC = 0x05,
	MENU_ITEM_VWII = 0x09,
	MENU_ITEM_FLDR = 0x10
};

//just to be able to call async
void someFunc(void *arg)
{
	(void)arg;
}

static int mcp_hook_fd = -1;
int MCPHookOpen()
{
	//take over mcp thread
	mcp_hook_fd = MCP_Open();
	if (mcp_hook_fd < 0)
		return -1;
	IOS_IoctlAsync(mcp_hook_fd, 0x62, (void *)0, 0, (void *)0, 0, someFunc, (void *)0);
	//let wupserver start up
	sleep(1);
	if (IOSUHAX_Open("/dev/mcp") < 0)
	{
		MCP_Close(mcp_hook_fd);
		mcp_hook_fd = -1;
		return -1;
	}
	return 0;
}

void MCPHookClose()
{
	if (mcp_hook_fd < 0)
		return;
	//close down wupserver, return control to mcp
	IOSUHAX_Close();
	//wait for mcp to return
	sleep(1);
	MCP_Close(mcp_hook_fd);
	mcp_hook_fd = -1;
}

int fsa_read(int fsa_fd, int fd, void *buf, int len)
{
	int done = 0;
	uint8_t *buf_u8 = (uint8_t *)buf;
	while (done < len)
	{
		size_t read_size = len - done;
		int result = IOSUHAX_FSA_ReadFile(fsa_fd, buf_u8 + done, 0x01, read_size, fd, 0);
		if (result < 0)
			return result;
		else
			done += result;
	}
	return done;
}

int smartStrcmp(const char *a, const char *b, const u32 a_id, const u32 b_id)
{
	char *ac = malloc(strlen(a) + 1);
	char *bc = malloc(strlen(b) + 1);

	strcpy(ac, a);
	strcpy(bc, b);

	if (ignoreThe)
	{
		removeThe(ac);
		removeThe(bc);
	}

	if (badNamingMode)
	{
		int a_id_size = (int)((ceil(log10(a_id)) + 1) * sizeof(char));
		char a_id_string[a_id_size];

		int b_id_size = (int)((ceil(log10(b_id)) + 1) * sizeof(char));
		char b_id_string[b_id_size];

		itoa(a_id, a_id_string, 16);
		itoa(b_id, b_id_string, 16);

		struct nlist *np;
		np = lookup(a_id_string);
		if (np != NULL)
		{
			ac = prepend(ac, np->defn);
		}

		np = lookup(b_id_string);
		if (np != NULL)
		{
			bc = prepend(bc, np->defn);
		}
	}

	int result = strcasecmp(ac, bc);
	free(ac);
	free(bc);
	return result;
}

int fSortCond(const void *c1, const void *c2)
{
	return smartStrcmp(((struct MenuItemStruct *)c1)->name,
					   ((struct MenuItemStruct *)c2)->name,
					   ((struct MenuItemStruct *)c1)->ID,
					   ((struct MenuItemStruct *)c2)->ID);
}

void getXMLelement(const char *buff, size_t buffSize, const char *url, const char *elementName, char *text, size_t textSize)
{
	text[0] = 0;
	xmlDocPtr doc = xmlReadMemory(buff, buffSize, url, "utf-8", 0);
	xmlNode *root_element = xmlDocGetRootElement(doc);
	xmlNode *cur_node = NULL;
	xmlChar *nodeText = NULL;

	if (root_element != NULL && root_element->children != NULL)
	{
		for (cur_node = root_element->children; cur_node; cur_node = cur_node->next)
		{
			if (cur_node->type == XML_ELEMENT_NODE)
			{
				if (strcasecmp((const char *)cur_node->name, elementName) == 0)
				{
					nodeText = xmlNodeListGetString(doc, cur_node->xmlChildrenNode, 1);
					if ((nodeText != NULL) && (textSize > 1))
						strncpy(text, (const char *)nodeText, textSize - 1);
					xmlFree(nodeText);
					break;
				}
			}
		}
	}
	xmlFreeDoc(doc);
}

int getXMLelementInt(const char *buff, size_t buffSize, const char *url, const char *elementName, int base)
{
	int ret;
	char text[40] = "";
	getXMLelement(buff, buffSize, url, elementName, text, 40);
	ret = (int)strtol((char *)text, NULL, base);
	return ret;
}

int readToBuffer(char **ptr, size_t *bufferSize, const char *path)
{
	FILE *fp;
	size_t size;
	fp = fopen(path, "rb");
	if (!fp)
		return -1;
	fseek(fp, 0L, SEEK_END); // fstat.st_size returns 0, so we use this instead
	size = ftell(fp);
	rewind(fp);
	*ptr = malloc(size);
	memset(*ptr, 0, size);
	fread(*ptr, 1, size, fp);
	fclose(fp);
	*bufferSize = size;
	return 0;
}

void getIDname(u32 id, u32 titleIDPrefix, char *name, size_t nameSize, u32 type)
{
	char *xBuffer = NULL;
	u32 xSize = 0;
	char path[255] = "";
	name[0] = 0;
	sprintf(path, "storage_%s:/usr/title/%08x/%08x/meta/meta.xml", (type == MENU_ITEM_USB) ? "usb" : "mlc", titleIDPrefix, id);
	log_printf("%s\n", path);
	if (readToBuffer(&xBuffer, &xSize, path) < 0)
		log_printf("Could not open %08x meta.xml\n", id);
	else
	{
		// parse meta.xml file
		if (xBuffer != NULL)
		{
			getXMLelement(xBuffer, xSize, "meta.xml", languageText, name, nameSize);
			free(xBuffer);
			log_printf(" %s\n  %s\n", path, name);
		}
		else
		{
			log_printf("Memory not allocated for %08x meta.xml\n", id);
		}
	}
}

/* Entry point */
int Menu_Main(void)
{
	//!*******************************************************************
	//!                   Initialize function pointers                   *
	//!*******************************************************************
	//! do OS (for acquire) and sockets first so we got logging
	InitOSFunctionPointers();
	InitSysFunctionPointers();
	InitACTFunctionPointers();
	//	InitCCRFunctionPointers();
	InitSocketFunctionPointers();

	log_init("192.168.1.16");
	log_print("\n\nStarting Main\n");

	InitFSFunctionPointers();
	InitVPadFunctionPointers();

	log_print("Function exports loaded\n");

	//!*******************************************************************
	//!                    Initialize heap memory                        *
	//!*******************************************************************
	//log_print("Initialize memory management\n");
	//! We don't need bucket and MEM1 memory so no need to initialize
	//memoryInitialize();

	int fsaFd = -1;
	int failed = 1;
	char failError[65] = "";
	char *fBuffer = NULL;
	size_t fSize = 0;
	int usb_Connected = 0;
	u32 sysmenuId = 0;
	u32 cbhcID = 0;
	int userPersistentId = 0;

	VPADInit();

	screenInit();
	screenClear();
	screenPrint(TITLE_TEXT);
	screenPrint("Choose sorting method:");
	screenPrint("Press 'B' for standard sorting.");
	screenPrint("Press 'A' for standard sorting(ignoring leading 'The').");
	screenPrint("Press 'X' for bad naming mode sorting.");
	screenPrint("Press 'Y' for bad naming mode sorting(ignoring leading 'The').");
	screenPrint("Press '+' for backup of the current order(incl. folders).");
	screenPrint("Press '-' for restore of the current order(incl. folders).");

	int vpadError;
	VPADData vpad;

	char modeText[25] = "";
	char ignoreTheText[25] = "";

	while (1)
	{

		VPADRead(0, &vpad, 1, &vpadError);
		u32 pressedBtns = 0;

		if (!vpadError)
			pressedBtns = vpad.btns_d | vpad.btns_h;

		if (pressedBtns & VPAD_BUTTON_B)
		{
			badNamingMode = 0;
			ignoreThe = 0;
			strcpy(modeText, "standard sorting");
			break;
		}

		if (pressedBtns & VPAD_BUTTON_A)
		{
			badNamingMode = 0;
			ignoreThe = 1;
			strcpy(modeText, "standard sorting");
			strcpy(ignoreTheText, "(ignoring leading 'The')");
			break;
		}

		if (pressedBtns & VPAD_BUTTON_X)
		{
			badNamingMode = 1;
			ignoreThe = 0;
			strcpy(modeText, "bad naming mode sorting");
			break;
		}

		if (pressedBtns & VPAD_BUTTON_Y)
		{
			badNamingMode = 1;
			ignoreThe = 1;
			strcpy(modeText, "bad naming mode sorting");
			strcpy(ignoreTheText, "(ignoring leading 'The')");
			break;
		}

		if (pressedBtns & VPAD_BUTTON_PLUS)
		{
			backup = 1;
			strcpy(modeText, "backup");
			strcpy(ignoreTheText, "");
			break;
		}

		if (pressedBtns & VPAD_BUTTON_MINUS)
		{
			restore = 1;
			strcpy(modeText, "restore");
			strcpy(ignoreTheText, "");
			break;
		}

		usleep(1000);
	}

	screenClear();
	screenSetPrintLine(0);
	screenPrint(TITLE_TEXT);
	char modeSelectedText[50] = "";
	sprintf(modeSelectedText, "Starting %s%s...", modeText, ignoreTheText);
	screenPrint(modeSelectedText);
	log_printf(modeSelectedText);

	// Get Wii U Menu Title
	// Do this before mounting the file system.
	// It screws it up if done after.
	uint64_t sysmenuIdUll = _SYSGetSystemApplicationTitleId(0);
	if ((sysmenuIdUll & 0xffffffff00000000) == 0x0005001000000000)
		sysmenuId = sysmenuIdUll & 0x00000000ffffffff;
	else
	{
		strcpy(failError, "Failed to get Wii U Menu Title!");
		goto prgEnd;
	}
	log_printf("Wii U Menu Title = %08X\n", sysmenuId);

	// Get current user account slot
	nn_act_initialize();
	unsigned char userSlot = nn_act_getslotno();
	userPersistentId = nn_act_GetPersistentIdEx(userSlot);
	log_printf("User Slot = %d, ID = %08x\n", userSlot, userPersistentId);
	nn_act_finalize();

	//!*******************************************************************
	//!                        Initialize FS                             *
	//!*******************************************************************
	log_printf("Mount partitions\n");

	int res = IOSUHAX_Open(NULL);
	if (res < 0)
		res = MCPHookOpen();
	if (res < 0)
	{
		strcpy(failError, "IOSUHAX_open failed\n");
		goto prgEnd;
	}
	else
	{
		fatInitDefault();
		fsaFd = IOSUHAX_FSA_Open();
		if (fsaFd < 0)
		{
			strcpy(failError, "IOSUHAX_FSA_Open failed\n");
			goto prgEnd;
		}

		if (mount_fs("storage_slc", fsaFd, NULL, "/vol/system") < 0)
		{
			strcpy(failError, "Failed to mount SLC!");
			goto prgEnd;
		}
		if (mount_fs("storage_mlc", fsaFd, NULL, "/vol/storage_mlc01") < 0)
		{
			strcpy(failError, "Failed to mount MLC!");
			goto prgEnd;
		}
		res = mount_fs("storage_usb", fsaFd, NULL, "/vol/storage_usb01");
		usb_Connected = res >= 0;
	}

	// Get Country Code
	int language = 0;
	// NEED TO FIGURE OUT WHAT RPL TO LOAD
	//	int (*SCIGetCafeLanguage)(int *language);
	//	OSDynLoad_FindExport(xxxx, 0, "SCIGetCafeLanguage", &SCIGetCafeLanguage);
	//	ret = SCIGetCafeLanguage(&language);
	// This doesn't work either
	//	u32 languageCCR = 0;
	//	CCRSysGetLanguage(&languageCCR);
	//	log_printf("CCR language = %d\n", languageCCR);

	// really should use SCIGetCafeLanguage(), but until then, just
	// read cafe.xml file
	if (readToBuffer(&fBuffer, &fSize, cafeXmlPath) < 0)
	{
		strcpy(failError, "Could not open cafe.xml\n");
		goto prgEnd;
	}
	// parse cafe.xml file
	if (fBuffer != NULL)
	{
		language = getXMLelementInt(fBuffer, fSize, "cafe.xml", "language", 10);
		sprintf(languageText, "longname_%s", languages[language]);
		log_printf("cafe.xml size = %d, language = %d %s\n", fSize, language, languages[language]);
		free(fBuffer);
	}
	else
	{
		strcpy(failError, "Memory not allocated for cafe.xml\n");
		goto prgEnd;
	}

	// Get CBHC Title
	// If syshax.xml exists, then assume cbhc exists
	FILE *fp = fopen(syshaxXmlPath, "rb");
	if (fp)
	{
		fclose(fp);
		// read system.xml file
		if (readToBuffer(&fBuffer, &fSize, systemXmlPath) < 0)
		{
			strcpy(failError, "Could not open system.xml\n");
			goto prgEnd;
		}
		// parse system.xml file
		if (fBuffer != NULL)
		{
			cbhcID = (u32)getXMLelementInt(fBuffer, fSize, "system.xml", "default_title_id", 10);
			free(fBuffer);
			log_printf("system.xml size = %d, cbhcID = %d\n", fSize, cbhcID);
		}
		else
		{
			strcpy(failError, "Memory not allocated for system.xml\n");
			goto prgEnd;
		}
	}

	// Read Don't Move titles.
	// first try dontmoveX.hex where X is the user ID
	// if that fails try dontmove.txt
	char dmPath[65] = "";
	u32 *dmItem = NULL;
	int dmTotal = 0;
	size_t titleIDSize = 8;
	sprintf(dmPath, "%s%1x.txt", dontmovePath, userPersistentId & 0x0000000f);
	fp = fopen(dmPath, "rb");
	if (!fp)
	{
		sprintf(dmPath, "%s.txt", dontmovePath);
		fp = fopen(dmPath, "rb");
	}
	if (fp)
	{
		log_printf("Loading %s\n", dmPath);
		int ch, lines = 0;
		do
		{
			ch = fgetc(fp);
			if (ch == '\n')
				lines++;
		} while (ch != EOF);
		rewind(fp);

		dmTotal = lines;
		dmItem = (u32 *)malloc(sizeof(u32) * lines);

		log_printf("Number of games to load(dontmove): %d\n", lines);

		for (int i = 0; i < lines; i++)
		{
			size_t len = titleIDSize;
			char *currTitleID = (char *)malloc(len + 1);
			getline(&currTitleID, &len, fp);
			dmItem[i] = (u32)strtol(currTitleID, NULL, 16);
			free(currTitleID);
		}

		fclose(fp);
	}
	else
	{
		log_printf("Could not open %s\n", dmPath);
	}

	if (badNamingMode)
	{
		char gmPath[65] = "";
		sprintf(gmPath, "%s%1x.psv", gamemapPath, userPersistentId & 0x0000000f);
		fp = fopen(gmPath, "rb");
		if (!fp)
		{
			sprintf(gmPath, "%s.psv", gamemapPath);
			fp = fopen(gmPath, "rb");
		}
		if (fp)
		{
			log_printf("Loading %s\n", gmPath);
			int ch, max_ch_count, ch_count, lines = 0;
			do
			{
				ch = fgetc(fp);
				ch_count++;
				if (ch == '\n')
				{
					lines++;
					if (ch_count > max_ch_count)
						max_ch_count = ch_count;
				}
			} while (ch != EOF);
			rewind(fp);

			log_printf("Number of games in the map to load(dontmove): %d\n", lines);

			for (int i = 0; i < lines; i++)
			{
				size_t len = max_ch_count;
				char *currTitleID = (char *)malloc(len + 1);
				getline(&currTitleID, &len, fp);

				char *id = strtok(currTitleID, "|");
				char *group = strtok(NULL, "|");
				group = strtok(group, "\n");
				install(id, group);
				free(currTitleID);
			}

			fclose(fp);
		}
		else
		{
			log_printf("Could not open %s\n", gmPath);
		}
	}

	// Read Menu Data
	struct MenuItemStruct menuItem[300];
	bool folderExists[61] = {false};
	bool moveableItem[300];
	char baristaPath[255] = "";
	folderExists[0] = true;
	sprintf(baristaPath, "storage_mlc:/usr/save/00050010/%08x/user/%08x/BaristaAccountSaveFile.dat", sysmenuId, userPersistentId);
	log_printf("%s\n", baristaPath);

	if (backup)
	{
		fcopy(baristaPath, backupPath);
	}
	else if (restore)
	{
		fcopy(backupPath, baristaPath);
	}
	else
	{
		if (readToBuffer(&fBuffer, &fSize, baristaPath) < 0)
		{
			strcpy(failError, "Could not open BaristaAccountSaveFile.dat\n");
			goto prgEnd;
		}
		if (fBuffer == NULL)
		{
			strcpy(failError, "Memory not allocated for BaristaAccountSaveFile.dat\n");
			goto prgEnd;
		}

		log_printf("BaristaAccountSaveFile.dat size = %d\n", fSize);
		// Main Menu - First pass - Get names
		// Only movable items will be added.
		for (int fNum = 0; fNum <= 60; fNum++)
		{
			if (!folderExists[fNum])
				continue;
			log_printf("\nReading - Folder %d\n", fNum);
			int itemNum = 0;
			int itemTotal = 0;
			int itemSpaces = 300;
			int folderOffset = 0;
			if (fNum != 0)
			{
				// sort sub folder
				itemSpaces = 60;
				folderOffset = 0x002D24 + ((fNum - 1) * (60 * 16 * 2 + 56));
			}
			int usbOffset = itemSpaces * 16;
			for (int i = 0; i < itemSpaces; i++)
			{
				moveableItem[i] = true;
				int itemOffset = i * 16 + folderOffset;
				u32 id = 0;
				u32 type = 0;
				memcpy(&id, fBuffer + itemOffset + 4, sizeof(u32));
				memcpy(&type, fBuffer + itemOffset + 8, sizeof(u32));

				if ((id == HBL_TITLE_ID)		  // HBL
					|| (cbhcID && (id == cbhcID)) // CBHC
					|| (type == MENU_ITEM_DISC)	  // Disc
					|| (type == MENU_ITEM_VWII))  // vWii
				{
					moveableItem[i] = false;
					continue;
				}
				// Folder item in main menu?
				if ((fNum == 0) && (type == MENU_ITEM_FLDR))
				{
					if ((id > 0) && (id <= 60))
						folderExists[id] = true;
					moveableItem[i] = false;
					continue;
				}
				// NAND or USB?
				if (type == MENU_ITEM_NAND)
				{
					// Settings, MiiMaker, etc?
					u32 idH = 0;
					memcpy(&idH, fBuffer + itemOffset, sizeof(u32));
					if ((idH != 0x00050000) && (idH != 0x00050002) && (idH != 0))
					{
						moveableItem[i] = false;
						continue;
					}
					// If not NAND then check USB
					if (id == 0)
					{
						if (!usb_Connected)
							continue;
						itemOffset += usbOffset;
						id = 0;
						memcpy(&id, fBuffer + itemOffset + 4, sizeof(u32));
						type = fBuffer[itemOffset + 0x0b];
						if ((id == 0) || (type != MENU_ITEM_USB))
							continue;
					}
					// Is ID on Don't Move list?
					for (int j = 0; j < dmTotal; j++)
					{
						if (id == dmItem[j])
						{
							moveableItem[i] = false;
							break;
						}
					}

					if (!moveableItem[i])
						continue;

					// found sortable item
					memcpy(&menuItem[itemNum].titleIDPrefix, fBuffer + itemOffset, sizeof(u32));
					getIDname(id, menuItem[itemNum].titleIDPrefix, menuItem[itemNum].name, 65, type);
					menuItem[itemNum].ID = id;
					menuItem[itemNum].type = type;
					itemNum++;
				}
			}
			itemTotal = itemNum;
			log_printf("\nDone reading folders \n");

			// Sort Folder
			qsort(menuItem, itemTotal, sizeof(struct MenuItemStruct), fSortCond);

			// Move Folder Items
			log_printf("\nNew Order - Folder %d\n", fNum);
			itemNum = 0;
			for (int i = 0; i < itemSpaces; i++)
			{
				if (!moveableItem[i])
					continue;
				int itemOffset = i * 16 + folderOffset;
				u32 idNAND = 0;
				u32 idNANDh = 0;
				u32 idUSB = 0;
				u32 idUSBh = 0;
				if (itemNum < itemTotal)
				{
					if (menuItem[itemNum].type == MENU_ITEM_NAND)
					{
						idNAND = menuItem[itemNum].ID;
						idNANDh = menuItem[itemNum].titleIDPrefix;
					}
					else
					{
						idUSB = menuItem[itemNum].ID;
						idUSBh = menuItem[itemNum].titleIDPrefix;
					}
					log_printf("[%d][%08x] %s\n", i, menuItem[itemNum].ID, menuItem[itemNum].name);
					itemNum++;
				}

				memcpy(fBuffer + itemOffset, &idNANDh, sizeof(u32));
				memcpy(fBuffer + itemOffset + 4, &idNAND, sizeof(u32));
				memset(fBuffer + itemOffset + 8, 0, 8);
				fBuffer[itemOffset + 0x0b] = 1;

				itemOffset += usbOffset;

				memcpy(fBuffer + itemOffset, &idUSBh, sizeof(u32));
				memcpy(fBuffer + itemOffset + 4, &idUSB, sizeof(u32));
				memset(fBuffer + itemOffset + 8, 0, 8);
				fBuffer[itemOffset + 0x0b] = 2;
			}
		}

		fp = fopen(baristaPath, "wb");
		if (fp)
		{
			fwrite(fBuffer, 1, fSize, fp);
			fclose(fp);
		}
		else
		{
			strcpy(failError, "Could not write to BaristaAccountSaveFile.dat\n");
			goto prgEnd;
		}

		free(fBuffer);
		free(dmItem);
	}

	screenPrintAt(strlen(modeSelectedText), screenGetPrintLine(), "done.");

	char text[20] = "";
	sprintf(text, "User ID: %1x", userPersistentId & 0x0000000f);
	screenPrint(text);
	screenPrint("Press Home to exit");

	while (1)
	{
		VPADRead(0, &vpad, 1, &vpadError);
		u32 pressedBtns = 0;

		if (!vpadError)
			pressedBtns = vpad.btns_d | vpad.btns_h;

		if (pressedBtns & VPAD_BUTTON_HOME)
			break;

		usleep(1000);
	}
	failed = 0;

prgEnd:
	if (failed)
	{
		screenPrint(failError);
		sleep(5);
	}
	log_printf("Unmount\n");

	fatUnmount("sd");
	fatUnmount("usb");
	IOSUHAX_sdio_disc_interface.shutdown();
	IOSUHAX_usb_disc_interface.shutdown();
	unmount_fs("storage_slc");
	unmount_fs("storage_mlc");
	unmount_fs("storage_usb");
	IOSUHAX_FSA_Close(fsaFd);
	if (mcp_hook_fd >= 0)
		MCPHookClose();
	else
		IOSUHAX_Close();

	log_printf("Exiting\n");
	//memoryRelease();
	log_deinit();

	return EXIT_SUCCESS;
}

/*******************************************************************************
 * 
 * Menu Title IDs
 * USA 00050010-10040100
 * EUR 00050010-10040200
 * JPN
 * 
 * USA Wii U Menu layout file location: (8000000X where X is the User ID)
 * storage_mlc\usr\save\00050010\10040100\user\8000000X\BaristaAccountSaveFile.dat
 * 
 * File Format:
 * 0x000000 - 0x0012BF  NAND Title Entries (300 x 16 bytes)
 * 0x0012C0 - 0x00257F  USB Title Entries  (300 x 16 bytes)
 * 0x002587 - Last ID Launched
 * 0x0025A4 - 0x002963  ? Folder 0 ? NAND Title Entries (60 x 16 bytes)
 * 0x002964 - 0x002D23  ? Folder 0 ? USB Title Entries  (60 x 16 bytes)
 * 0x002D24 - 0x0030E3  Folder 1 NAND Title Entries (60 x 16 bytes)
 * 0x0030E4 - 0x0030E3  Folder 1 USB Title Entries  (60 x 16 bytes)
 * 0x0034A4 - 0x0034DB  Folder 1 Info (56 Bytes)
 * 0x0034DC - 0x00389B  Folder 2 NAND Title Entries (60 x 16 bytes)
 * 0x00389C - 0x003C5B  Folder 2 USB Title Entries  (60 x 16 bytes)
 * 0x003C5C - 0x003C93  Folder 2 Info (56 Bytes)
 * 0X003C94 .... FOLDERS 3 - 60 AS ABOVE
 * 
 * Title Entry Data Format:
 * Each entry is 16 bytes.
 * 0x00 - 0x07  Title ID
 * 0x0B  Type:
 *  0x01 = NAND
 *  0x02 = USB
 *  0X05 = Disc Launcher
 *  0X09 = vWii (Not Eshop vWii titles)
 *  0x10 = Folder (Bytes 0x00-0x06 = 0, 0x07 = Folder#)
 * 
 * If a 16 byte entry is set in NAND the corresponding entry in USB is blank
 * (except for the type 0x02 at offset 0x0b) and vice-versa.
 * 
 * Folders are all in - 0x000000 - 0x0012BF NAND IDs.
 * USB entry would be blank (except for the type 0x02 at offset 0x0b)
 * 
 * Folder Entry Data Format:
 * 0x00 - 0x21  Name (17 Unicode characters)
 * 0x32         Color
 *  0x00 = Blue
 *  0x01 = Green
 *  0x02 = Yellow
 *  0x03 = Orange
 *  0x04 = Red
 *  0x05 = Pink
 *  0x06 = Purple
 *  0x07 = Grey
 * 
 ******************************************************************************/
