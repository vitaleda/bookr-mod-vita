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

#ifndef BKPDF_H
#define BKPDF_H

#include <string>

#include "fzscreen.h"
#include "fzfont.h"

using namespace std;

#include "bkdocument.h"

struct PDFContext;
class BKPDF : public BKDocument {
	PDFContext* ctx;

	string fileName;

	int panX;
	int panY;
	bool loadNewPage;
	bool pageError;
	void panBuffer(int nx, int ny);
	void clipCoords(float& nx, float& ny);
	void redrawBuffer();

	string title;

	protected:
	BKPDF(string& f);
	~BKPDF();

	public:
	virtual int updateContent();
	virtual int resume();
	virtual void renderContent();

	virtual void getFileName(string&);
	virtual void getTitle(string&);
	virtual void getType(string&);

	virtual bool isPaginated();
	virtual int getTotalPages();
	virtual int getCurrentPage();
	virtual int setCurrentPage(int);

	virtual bool isZoomable();
	virtual void getZoomLevels(vector<BKDocument::ZoomLevel>& v);
	virtual int getCurrentZoomLevel();
	virtual int setZoomLevel(int);

	virtual int pan(int, int);

	virtual int screenUp();
	virtual int screenDown();
	virtual int screenLeft();
	virtual int screenRight();

	virtual bool isRotable();
	virtual int getRotation();
	virtual int setRotation(int);

	virtual bool isBookmarkable();
	virtual void getBookmarkPosition(map<string, int>&);
	virtual int setBookmarkPosition(map<string, int>&);

	static BKPDF* create(string& file);

#if 0
	virtual int update(unsigned int buttons);
	virtual void render();
	virtual void setBookmark(bool lastview);
	virtual void getPath(string&);
	virtual void setPage(int position);
	// this forces a redraw
	virtual void reloadPage(int position);
#endif
};

#endif

