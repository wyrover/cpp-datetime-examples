// PatchWorker.h : PROJECT_NAME Ӧ�ó������ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error �ڰ������� PCH �Ĵ��ļ�֮ǰ������stdafx.h��
#endif

#include "resource.h"		// ������


// CPatchWorkerApp:
// �йش����ʵ�֣������ PatchWorker.cpp
//

#define VERSION_FILE	"wxversion"
#define COMMAND_FILE	"wxcommand"
#define COMMAND_ADD		"+" //"new"
#define COMMAND_DEl		"-" //"del"
#define COMMAND_RPL		"~" //"replace"
#define WXFILE_PATH		"data\\pack"

class CPatchWorkerApp : public CWinApp
{
public:
	CPatchWorkerApp();

// ��д
	public:
	virtual BOOL InitInstance();

// ʵ��

	DECLARE_MESSAGE_MAP()
};

extern CPatchWorkerApp theApp;
