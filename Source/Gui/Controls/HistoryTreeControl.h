// This file was generated by WTL subclass control wizard 
// LogListBox.h : Declaration of the CLogListBox

#pragma once
#include "../../resource.h"
#include <atlmisc.h>
#include <atlcrack.h>
#include "../../Func/HistoryManager.h"
#include "CustomTreeControl.h"
#include "../../3rdpart/thread.h"
#include <queue>
#include "../../Core/FileDownloader.h"
#include <map>
#include <atltheme.h>
struct HistoryTreeItem 
{
/*public:
	~HistoryTreeItem()
	{
		if(thumbnail)
			DeleteObject(thumbnail);
	}*/
	HistoryItem hi;
	HBITMAP thumbnail;
	bool ThumbnailRequested;
	std::string thumbnailSource;
};

class CHistoryTreeControl :  
	public CCustomTreeControlImpl<CHistoryTreeControl>,
	public CThemeImpl <CHistoryTreeControl>, 
	protected CThreadImpl<CHistoryTreeControl>,
	public TreeItemCallback
{
	public:
		CHistoryTreeControl();
		~CHistoryTreeControl();
		DECLARE_WND_SUPERCLASS(_T("CHistoryTreeControl"), CListViewCtrl::GetWndClassName())
		
		BEGIN_MSG_MAP(CHistoryTreeControl)
			MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
			CHAIN_MSG_MAP(CCustomTreeControlImpl<CHistoryTreeControl>)
		 END_MSG_MAP()

		 // Handler prototypes:
		 //  LRESULT MessageHandler(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		 //  LRESULT CommandHandler(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		 //  LRESULT NotifyHandler(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam,BOOL& bHandled);
		void DrawTreeItem(HDC dc, RECT rc, UINT itemState,  TreeItem *item);
		DWORD OnItemPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/);
		DWORD OnSubItemPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/);
		bool LoadThumbnail(HistoryTreeItem * ItemID);
	public:
		FastDelegate0<> onThreadsFinished;
		FastDelegate0<> onThreadsStarted;
		void setDownloadingEnabled(bool enabled);
		int NotifyParent(int nItem);
		bool m_bIsRunning;
		int m_thumbWidth;
		bool downloading_enabled_;
		void addSubEntry(TreeItem* res, HistoryItem it, bool autoExpand);
		TreeItem*  addEntry(CHistorySession* session, const CString text);
		void Init();
		void Clear();
		bool IsItemAtPos(int x, int y, bool &isRoot);
		TreeItem * selectedItem();
		void TreeItemSize( TreeItem *item, SIZE *sz);
		DWORD Run();
		bool isRunning() const;
		void StartLoadingThumbnails();
		void OnTreeItemDelete(TreeItem* item) ;
		void CreateDownloader();
		void abortLoadingThreads();
		LRESULT ReflectContextMenu(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	private:
		HistoryItem* getItemData(TreeItem* res);
		std::map<CString, HICON> m_fileIconCache;
		std::map<CString, HICON> m_serverIconCache;
		HICON getIconForExtension(const CString serverName);
		HICON getIconForServer(const CString serverName);
		int CalcItemHeight(TreeItem* item); 
		LRESULT OnLButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		void _DrawItem(TreeItem* res, HDC dc, DWORD itemState,RECT invRC, int* outHeight);
		void DrawSubItem(TreeItem* res, HDC dc, DWORD itemState, RECT invRC,  int* outHeight);
		LRESULT OnLButtonDoubleClick(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		HBITMAP GetItemThumbnail(HistoryTreeItem* item);
		std::queue<HistoryTreeItem*> m_thumbLoadingQueue;
		CFileDownloader *m_FileDownloader;
		bool OnFileFinished(bool ok, DownloadFileListItem it);
		void DownloadThumb(HistoryTreeItem * it);
		int m_SessionItemHeight;
		int m_SubItemHeight;	
		void QueueFinishedEvent();
		void threadsFinished();

};

