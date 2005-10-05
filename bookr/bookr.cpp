/*
 * Bookr: document reader for the Sony PSP 
 * Copyright (C) 2005 Carlos Carrasco Martinez (carloscm at gmail dot com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "fzscreen.h"
#include "bkbook.h"
#include "bkpdf.h"
#include "bkfilechooser.h"
#include "bkcolorchooser.h"
#include "bkmainmenu.h"
#include "bklogo.h"
#include "bkpopup.h"

#ifdef PSP
#include <pspkernel.h>
PSP_MODULE_INFO("Bookr", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
#endif

extern "C" {
	int main(int argc, char* argv[]);
};

static bool isPDF(string& file) {
	char header[4];
	memset((void*)header, 0, 4);
	FILE* f = fopen(file.c_str(), "r");
	fread(header, 4, 1, f);
	fclose(f);
	return header[0] == 0x25 && header[1] == 0x50 && header[2] == 0x44 && header[3] == 0x46;
}

int main(int argc, char* argv[]) {
	bool isPdf = false;
	BKLayer* pdfOrTextLayer = NULL;
	FZScreen::setupCallbacks();
	FZScreen::open(argc, argv);
	FZScreen::setupCtrl();
	BKUser::init();
	FZ_DEBUG_SCREEN_INIT

	BKLayer::load();
	bkLayers layers;
	BKFileChooser* fs = 0;
	BKColorChooser* cs = 0;
	BKMainMenu* mm = BKMainMenu::create();
	layers.push_back(BKLogo::create());
	layers.push_back(mm);

	FZScreen::dcacheWritebackAll();

	bool dirty = true;
	bool exitApp = false;
	while (!exitApp) {
		if (dirty) {
			FZScreen::startDirectList();
			// render layers back to front
			bkLayersIt it(layers.begin());
			bkLayersIt end(layers.end());
			while (it != end) {
				(*it)->render();
				++it;
			}
			FZScreen::endAndDisplayList();
		}

		FZScreen::waitVblankStart();

		if (dirty) {
			FZScreen::swapBuffers();
		}

		FZScreen::checkEvents();

		FZ_DEBUG_SCREEN_SET00

		int buttons = FZScreen::readCtrl();
		dirty = buttons != 0;

		// hack!
		/*if (buttons & FZ_CTRL_SQUARE)
			break;*/

		// the last layer always owns the input focus
		bkLayersIt it(layers.end());
		--it;
		int command = (*it)->update(buttons);
		switch (command) {
			case BK_CMD_CLOSE_TOP_LAYER: {
				bkLayersIt it(layers.end());
				--it;
				(*it)->release();
				layers.erase(it);
			} break;
			case BK_CMD_MARK_DIRTY:
				dirty = true;
			break;
			case BK_CMD_INVOKE_OPEN_FILE:
				// add a file chooser layer
				fs = BKFileChooser::create();
				layers.push_back(fs);
			break;
			case BK_CMD_OPEN_FILE:
			case BK_CMD_RELOAD: {
				// open a file as a document
				string s;
				FZDirent de;
				if (command == BK_CMD_RELOAD) {
					// reload current file
					if (isPdf) {
						((BKPDF*)pdfOrTextLayer)->getPath(s);
					} else {
						((BKBook*)pdfOrTextLayer)->getPath(s);
						de.size = ((BKBook*)pdfOrTextLayer)->getSize();
					}
				}
				if (command == BK_CMD_OPEN_FILE) {
					// open selected file
					fs->getCurrentDirent(de);
					fs->getFullPath(s);
					fs = 0;
				}
				// clear layers
				bkLayersIt it(layers.begin());
				bkLayersIt end(layers.end());
				while (it != end) {
					(*it)->release();
					++it;
				}
				layers.clear();
				// little hack to display a loading screen
				BKLogo* l = BKLogo::create();
				l->setLoading(true);
				FZScreen::startDirectList();
				l->render();
				FZScreen::endAndDisplayList();
				FZScreen::waitVblankStart();
				FZScreen::swapBuffers();
				FZScreen::checkEvents();
				l->release();
				// detect file type and add a new display layer
				if (isPdf = isPDF(s)) {
					pdfOrTextLayer = BKPDF::create(s);
				} else {
					pdfOrTextLayer = BKBook::create(s, de.size);
				}
				if (pdfOrTextLayer == 0) {
					// error, back to logo screen
					BKLogo* l = BKLogo::create();
					l->setError(true);
					layers.push_back(l);
				} else {
					// file loads ok, add the layer
					layers.push_back(pdfOrTextLayer);
				}
			} break;
			case BK_CMD_INVOKE_MENU:
				// add a main menu layer
				mm = BKMainMenu::create(isPdf, pdfOrTextLayer);
				layers.push_back(mm);
			break;
			case BK_CMD_MAINMENU_POPUP:
				layers.push_back(BKPopup::create(
						mm->getPopupMode(),
						mm->getPopupText()
					)
				);
			break;
			case BK_CMD_INVOKE_COLOR_CHOOSER_TXTFG:
				// add a colot chooser layer
				cs = BKColorChooser::create(BKUser::options.txtFGColor, BK_CMD_SET_TXTFG);
				layers.push_back(cs);
			break;
			case BK_CMD_INVOKE_COLOR_CHOOSER_TXTBG:
				// add a colot chooser layer
				cs = BKColorChooser::create(BKUser::options.txtBGColor, BK_CMD_SET_TXTBG);
				layers.push_back(cs);
			break;
			case BK_CMD_SET_TXTFG: {
				BKUser::options.txtFGColor = cs->getColor();
				BKUser::save();
				mm->rebuildMenu();
				bkLayersIt it(layers.end());
				--it;
				(*it)->release();
				layers.erase(it);
			} break;
			case BK_CMD_SET_TXTBG: {
				BKUser::options.txtBGColor = cs->getColor();
				BKUser::save();
				mm->rebuildMenu();
				bkLayersIt it(layers.end());
				--it;
				(*it)->release();
				layers.erase(it);
			} break;
			case BK_CMD_EXIT: {
					exitApp = true;
			}
			break;
		}
	}

	bkLayersIt it(layers.begin());
	bkLayersIt end(layers.end());
	while (it != end) {	
		(*it)->release();
		++it;
	}
	layers.clear();

	FZScreen::close();
	FZScreen::exit();

	return 0;
}

