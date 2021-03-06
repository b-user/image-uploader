/*
    Image Uploader - program for uploading images/files to Internet
    Copyright (C) 2007-2015 ZendeN <zenden2k@gmail.com>
	 
    HomePage:    http://zenden.ws/imageuploader

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef IU_GUI_MAINDLG_H
#define IU_GUI_MAINDLG_H


#pragma once

#include "atlheaders.h"
#include <atlcoll.h>

class CMainDlg;
struct UPLOADPARAMS;
struct CFileListItem
{
	CString FilePath;
	CString FileName;
	CString VirtualFileName;
	bool selected;
};

#include "Gui/Controls/myimage.h"
#include "uploaddlg.h"
#include "screenshotdlg.h"
#include "logosettings.h"
#include "uploadsettings.h"

#include "VideoGrabberPage.h"
#include "Gui/Controls/thumbsview.h"
#include "Func/MyDataObject.h"
#include "Func/MyDropSource.h"
#include <atlcrack.h>

class CMainDlg : public CDialogImpl<CMainDlg>,public CThreadImpl<CMainDlg>,public CWizardPage
{
public:
	enum { IDD = IDD_MAINDLG };

	BEGIN_MSG_MAP(CMainDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)		
		MESSAGE_HANDLER(WM_CONTEXTMENU, OnContextMenu)
		COMMAND_ID_HANDLER(IDM_DELETE, OnDelete)
		COMMAND_ID_HANDLER(IDM_ADDFOLDER, OnAddFolder)
		COMMAND_ID_HANDLER(IDM_EDIT, OnEdit)
		COMMAND_ID_HANDLER(IDM_EDITINEXTERNALEDITOR, OnEditExternal)
		COMMAND_ID_HANDLER(IDM_VIEW, OnImageView)
		COMMAND_ID_HANDLER(IDM_OPENINFOLDER, OnOpenInFolder)
		COMMAND_ID_HANDLER(IDM_COPYFILETOCLIPBOARD, OnCopyFileToClipboard)
		COMMAND_ID_HANDLER(IDC_PASTE, OnMenuItemPaste)
		COMMAND_ID_HANDLER(IDC_ADDIMAGES,  OnBnClickedAddimages)
		COMMAND_ID_HANDLER(IDC_DELETEALL,  OnBnClickedDeleteAll)
		COMMAND_ID_HANDLER(IDM_SAVEAS, OnSaveAs)
		COMMAND_HANDLER(IDC_ADDVIDEO, BN_CLICKED, OnBnClickedAddvideo)
		COMMAND_HANDLER(IDC_SCREENSHOT, BN_CLICKED, OnBnClickedScreenshot)
		COMMAND_HANDLER(IDC_PROPERTIES, BN_CLICKED, OnBnClickedProperties)
		NOTIFY_HANDLER(IDC_FILELIST, LVN_DELETEITEM, OnLvnItemDelete)
		COMMAND_HANDLER(IDC_DELETE, BN_CLICKED, OnBnClickedDelete)
		COMMAND_ID_HANDLER_EX(IDM_ADDFILES, OnAddFiles)
		REFLECT_NOTIFICATIONS()
	END_MSG_MAP()
// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	
	DWORD Run();
	LRESULT OnBnClickedScreenshot(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBnClickedAddvideo(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBnClickedAddimages(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBnClickedProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBnClickedDeleteAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnLvnItemDelete(int /*idCtrl*/, LPNMHDR pNMHDR, BOOL& /*bHandled*/);
	LRESULT OnAddFolder(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDelete(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEdit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEditExternal(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnOpenInFolder(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCopyFileToClipboard(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnImageView(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnMenuItemPaste(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSaveAs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	BOOL FileProp();
	bool AddToFileList(LPCTSTR FileName, const CString& virtualFileName=_T(""), Gdiplus::Image *Img = NULL);
	bool CheckEditInteger(int Control);
	CString getSelectedFileName();
	//CUploader Uploader;
	CListViewCtrl lv;
	CMyImage image;
	CAtlArray<CFileListItem> FileList;
	bool OnShow();
	bool OnHide();

	CThumbsView ThumbsView;
	CEvent WaitThreadStop;
	HANDLE m_EditorProcess;
	LRESULT OnBnClickedDelete(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAddFiles(WORD wNotifyCode, WORD wID, HWND hWndCtl);
};

#endif // IU_GUI_MAINDLG_H