#include "StdAfx.h"
#include "FileCollector.h"
#include "CRC.h"
#include "AxCryptoMath.h"
#include "XZip.h"
#include <algorithm>
#include <mbstring.h>
#include "PatchWorker.h"

CFileCollector::CFileCollector(UINT nMeasureSize)
{
	m_hTerimateHandle = ::CreateEvent(0, FALSE, FALSE, 0);
	::ResetEvent(m_hTerimateHandle);
	m_vFileBuf.reserve(nMeasureSize);
}

CFileCollector::~CFileCollector(void)
{
	::CloseHandle(m_hTerimateHandle);
	m_hTerimateHandle = 0;
}

bool CFileCollector::CollectPath(const char* szPath, const char* szVersionFile, CString& strVersion, CAxCryptoMath* pKeyMath)
{
	assert(szVersionFile && pKeyMath);
	m_vFileBuf.clear();
	strVersion = "";

	//-----------------------------------------
	//������ʱ�ļ���
	CHAR szTempPath[MAX_PATH] = {0};
	::GetTempPath(MAX_PATH, szTempPath);

	CHAR szTempFile[MAX_PATH] = {0};
	::GetTempFileName(szTempPath, "pw", MAX_PATH, szTempFile);

	//-----------------------------------------
	//�����ļ��Ƿ�Ϸ�
	if(S_OK != pKeyMath->UnSignFile(szVersionFile, szTempFile, FALSE, TRUE))
	{
		DeleteFile(szTempFile);
		return false;
	}

	//-----------------------------------------
	//���汾��
	FILE* fp = fopen(szTempFile, "r");
	if(fp == 0) return false;

	CHAR szLine[1024] = {0};
	fgets(szLine, 1024, fp);

	//�汾��
	char* pDot = strchr(szLine, '|');
	if(!pDot) return false;
	*pDot = 0;
	strVersion = szLine;

	//�ڵ����
	int nNodeNumber = atoi(pDot+1);

	//-----------------------------------------
	//�����ļ�
	m_vFileBuf.reserve(nNodeNumber);
	for(int i=0; i<nNodeNumber; i++)
	{
		fgets(szLine, 1024, fp);

		FileNode newNode;
		
		char* pDot1 = strchr(szLine, '|');
		if(!pDot1) continue;
		*pDot1++ = 0;
		if(*pDot1 == 'x')
		{
			newNode.bFold = true;
			newNode.nCRC = newNode.nSize = 0;
			m_vFileBuf.push_back(newNode);
			continue;
		}

		char* pDot2 = strchr(pDot1, '|');
		if(!pDot2) continue; 
		*pDot2++ = 0;

		char* pDot3 = strchr(pDot2, '|');
		if(!pDot3) continue; 
		*pDot3++ = 0;
		if( strchr( pDot3, 10 ) ) *strchr( pDot3, 10 ) = 0;

		sscanf(pDot1, "%08x", &newNode.nSize);
		sscanf(pDot2, "%08x", &newNode.nCRC);
		newNode.strPackName = pDot3;
		newNode.bFold = false;

		if( newNode.strPackName.empty() ) {
			newNode.strPatchName = newNode.strFileName = szLine;
		}
		else {
			char szPName[1024];
			strcpy( szPName, szLine );
			_mbslwr( (byte*)&szPName[0] );
			char* pDot = strstr( szPName, WXFILE_PATH );
			if( pDot ) { pDot[9] = 0; pDot += 10; }
			else pDot = szPName;
			newNode.strPatchName = szPName;
			newNode.strFileName = pDot;
		}
		m_vFileBuf.push_back(newNode);
	}

	DeleteFile(szTempFile);

	m_strPath = szPath;
	m_strVersion = (LPCTSTR)strVersion;
	return true;
}

bool CFileCollector::CollectPath(const char* szPath)
{
	assert(szPath);
	m_vFileBuf.clear();

	::SendMessage(m_hProgressWnd, WM_PROGRESS_TITLE, (WPARAM)"����Ŀ¼...", 0);

	m_strPath = szPath;
	if(!_CollectPath(szPath, "", m_vFileBuf)) return false;

	return true;
}

bool CFileCollector::_CollectPath(const char* szPath, const char* szRelPath, std::vector< FileNode >& vFileBuf)
{
	bool PackPath = false;
	WIN32_FIND_DATA FindData;
	
	CHAR szFileName[MAX_PATH] = {0};
	strncpy(szFileName, szPath, MAX_PATH);
	if( stricmp( szRelPath, WXFILE_PATH ) == 0 ) {
		PathAppend(szFileName, "*.wx0");
		PackPath = true;
	}
	else {
		PathAppend(szFileName, "*.*");
	}

	HANDLE hFind = FindFirstFile(szFileName, &FindData);
	if(hFind == INVALID_HANDLE_VALUE)
	{
		FindClose(hFind);
		return true;
	}
	BOOL bFind = FALSE;
	::SendMessage(m_hProgressWnd, WM_PROGRESS_COLLFILE, (WPARAM)szPath, 0);

	int nPathLen = (int)m_strPath.size();
	do
	{
		// check terminate flag
		if(WAIT_OBJECT_0 == ::WaitForSingleObject(m_hTerimateHandle, 0))
		{
			FindClose(hFind);
			return false;
		}

		CHAR szPathFileName[MAX_PATH];
		strncpy(szPathFileName, szPath, MAX_PATH);
		PathAppend(szPathFileName, FindData.cFileName);

		if(FindData.cFileName[0] != '.')
		{
			FileNode newNode;
			newNode.strPatchName = newNode.strFileName = szPathFileName+nPathLen+1;
			newNode.bFold = false;
			newNode.nCRC = 0;
			newNode.nSize = 0;

			//�ļ���
			if(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				newNode.bFold = true;
				if(!_CollectPath(szPathFileName, newNode.strPatchName.c_str(), vFileBuf))
				{
					FindClose(hFind);
					return false;
				}
			}
			//�ļ�
			else
			{
				newNode.bFold = false;
				if( PackPath ) {
					char szPackName[260];
					char szPackPath[260];
					_splitpath( newNode.strFileName.c_str(), 0, szPackPath, szPackName, 0 );
					newNode.strPackName = std::string(szPackName);
					FILE* fp = fopen( szPathFileName, "rb" );
					if( !fp ) return false;
					DWORD dwHeadSize;
					fseek( fp, 4, SEEK_SET );
					fread( &dwHeadSize, 1, 4, fp );
					char* lpszHead = new char[dwHeadSize];
					fread( lpszHead, 1, dwHeadSize, fp );
					fclose( fp );
					CPatchMaker::DecryptHead( lpszHead, dwHeadSize );
					char* p = lpszHead;
					BYTE Length;
					while( (DWORD)( p - lpszHead ) < dwHeadSize ) {
						Length = *p++;
						char szName[_MAX_PATH] = {0};
						CopyMemory( szName, p, Length ); p += Length; 
						newNode.strPatchName = szPackPath;
						newNode.strFileName = std::string( szPackPath ) + std::string( szName );
						p += 4; // Offset;
						CopyMemory( &newNode.nSize, p, 4 ); p += 4;
						CopyMemory( &newNode.nCRC, p, 4 ); p += 4;
						vFileBuf.push_back(newNode);
					}
					delete [] lpszHead;
				}
				else {
					DWORD dwCRC, dwFileSize;

CHECK_DATA:
					if(NO_ERROR != FileCrc32Win32(szPathFileName, dwCRC, dwFileSize))
					{
						char szMsg[MAX_PATH];
						_snprintf(szMsg, MAX_PATH, "�޷����ļ�:%s", szPathFileName);
						
						INT nSel = MessageBox(NULL, szMsg, "ERROR", MB_ABORTRETRYIGNORE|MB_ICONSTOP);

						if(IDABORT == nSel)
						{
							FindClose(hFind);
							return false;
						}
						else if(IDRETRY == nSel)
						{
							goto CHECK_DATA;
						}
						else
						{
							// Find Next file.
							bFind = FindNextFile(hFind, &FindData);
							continue;
						}
					}

					newNode.nCRC = dwCRC;
					newNode.nSize = dwFileSize;
					vFileBuf.push_back(newNode); //��push�ļ�����
				}
			}
		}
		// Find Next file.
		bFind = FindNextFile(hFind, &FindData);
	}while(bFind);

	FindClose(hFind);
	return true;
}

void CFileCollector::OutputVersionFile(const char* szFileName, const char* szVersion, CAxCryptoMath* pKeyMath)
{
	assert(szFileName && szVersion && pKeyMath);

	//---------------------------------------------
	//������ʱ�ļ���
	CHAR szTempPath[MAX_PATH] = {0};
	::GetTempPath(MAX_PATH, szTempPath);

	CHAR szTempFile[MAX_PATH] = {0};
	::GetTempFileName(szTempPath, "pw", MAX_PATH, szTempFile);

	//---------------------------------------------
	//���ȱ��浽��ʱ�ļ�
	FILE* fp = fopen(szTempFile, "w");
	if(fp == 0) 
	{
		char szMsg[MAX_PATH];
		_snprintf(szMsg, MAX_PATH, "�����ļ�%sʧ�ܣ�", szTempFile);
		MessageBox(NULL, szMsg, "ERROR", MB_OK|MB_ICONSTOP);
		return;
	}

	fprintf(fp, "%s|%d\n", szVersion, (int)m_vFileBuf.size());

	//���·���ĳ���
	for(int i=0; i<(int)m_vFileBuf.size(); i++)
	{
		const FileNode& theNode = m_vFileBuf[i];
		if(theNode.bFold)
		{
			fprintf(fp, "%s|x|x\n", theNode.strFileName.c_str(), theNode.nSize, theNode.nCRC);
		}
		else
		{
			fprintf(fp, "%s|%08x|%08x|%s\n", theNode.strFileName.c_str(), theNode.nSize, 
				theNode.nCRC, theNode.strPackName.c_str());
		}
	}
	fclose(fp);

	//---------------------------------------------
	//����
	if(S_OK != pKeyMath->SignFile(szTempFile, szFileName, FALSE, TRUE))
	{
		char szMsg[MAX_PATH];
		_snprintf(szMsg, MAX_PATH, "�����ļ�%sʧ�ܣ�", szFileName);
		MessageBox(NULL, szMsg, "ERROR", MB_OK|MB_ICONSTOP);
	}

	m_strVersion = szVersion;
}

void CFileCollector::ClearWorlk(void)
{
	m_vFileBuf.clear();
	::ResetEvent(m_hTerimateHandle);
}


///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
CPatchMaker::CPatchMaker(const CFileCollector& oldVerData, const CFileCollector& newVerData, const char* szPatchPath) : 
	m_oldVerData(oldVerData),
	m_newVerData(newVerData),
	m_strPatchPath(szPatchPath)
{
	m_hTerimateHandle = ::CreateEvent(0, FALSE, FALSE, 0);
	::ResetEvent(m_hTerimateHandle);

	m_hProgressWnd = 0;
}

CPatchMaker::~CPatchMaker()
{
	::CloseHandle(m_hTerimateHandle);
	m_hTerimateHandle = 0;
}

void CPatchMaker::GeneratePatchData(std::string strFilter)
{
	GenerateFilter( strFilter );

	//���ɾͰ汾�Ƚ�����map
	CompareMap oldVerMap;
	_generateCompareMap(m_oldVerData.GetFileNode(), oldVerMap);

	CompareMap newVerMap;
	_generateCompareMap(m_newVerData.GetFileNode(), newVerMap);

	m_vNewFile.clear();
	m_vDeleteFile.clear();

	//������Ҫ���µ��ļ�

	for(int i=0; i<(int)m_newVerData.GetFileNode().size(); i++)
	{
		const FileNode& thisNode = m_newVerData.GetFileNode()[i];

		//ת���ļ���ΪСд
		char szTemp[1024] = {0};
		strncpy(szTemp, thisNode.strFileName.c_str(), 1024);
		_mbslwr((byte*)&szTemp[0]);

		if( FileNameMatch( szTemp ) ) {
			CompareMap::iterator itFind = oldVerMap.find(szTemp);
			if(itFind != oldVerMap.end()) {
				itFind->second.bMasked = true;
			}
			continue;//���˵�
		}

		//�ھɰ汾��Ѱ����ͬ�ļ�
		CompareMap::iterator itFind = oldVerMap.find(szTemp);
		if(itFind == oldVerMap.end())
		{
			//����ӵ��ļ�
			m_vNewFile.push_back(thisNode);
			continue;
		}

		//�Ƚ������ļ�
		itFind->second.bMasked = true;
		const FileNode& theOldNode = *(itFind->second.pFileNode);
		if(theOldNode.nCRC != thisNode.nCRC || theOldNode.nSize != thisNode.nSize)
		{
			//��Ҫ���µ��ļ�
			m_vReplaceFile.push_back(thisNode);

			//������� �ļ�<->Ŀ¼ �ı仯����Ҫɾ��ԭ�ļ�
			if(theOldNode.nCRC==0 || thisNode.nCRC==0)
			{
				m_vDeleteFile.push_back(theOldNode);
			}
		}
	}

	//����汾�����ļ�
	FileNode verNode;
	verNode.bFold = false;
	verNode.strPatchName = verNode.strFileName = VERSION_FILE;
	m_vReplaceFile.push_back(verNode);

	//������Ҫɾ�����ļ�
	CompareMap::iterator itOld;
	for(itOld = oldVerMap.begin(); itOld != oldVerMap.end(); itOld++)
	{
		if(itOld->second.bMasked) continue;

		//��δ��ǵ��ļ�����Ҫɾ��
		m_vDeleteFile.push_back(*(itOld->second.pFileNode));
	}

}

void CPatchMaker::_generateCompareMap(const std::vector< FileNode >& fileData, CompareMap& compareMap)
{
	for(int i=0; i<(int)fileData.size(); i++)
	{
		const FileNode& thisNode = fileData[i];
		CompareNode newNode;

		//ת���ļ���ΪСд
		char szTemp[1024] = {0};
		strncpy(szTemp, thisNode.strFileName.c_str(), 1024);
		_mbslwr((byte*)&szTemp[0]);

		newNode.pFileNode = &thisNode;
		newNode.bMasked = false;
		compareMap.insert(std::make_pair(szTemp, newNode));
	}
}

bool CPatchMaker::WritePatchFile(CAxCryptoMath* pKeyMath, DWORD dwType)
{
	FILE* LogFile = fopen( "./FilesAddtoZip.txt", "wb" );
	CHAR szLogText[2048];
	assert(pKeyMath);

	::SendMessage(m_hProgressWnd, WM_PROGRESS_TITLE, (WPARAM)"ѹ���ļ�...", 0);

	//--------------------------------------------------------------
	//���������ļ�
	char szTempPath[MAX_PATH] = {0};
	::GetTempPath(MAX_PATH, szTempPath);
	char szTempFileName[MAX_PATH] = {0};
	::GetTempFileName(szTempPath, "pm", MAX_PATH, szTempFileName);

	FILE* fp = fopen(szTempFileName, "w");
	if(fp == 0)
	{
		char szMsg[MAX_PATH] = {0};
		_snprintf(szMsg, MAX_PATH, "д��ʱ�ļ�%s����!", szTempFileName);
		AfxGetApp()->GetMainWnd()->MessageBox(szMsg, 0, MB_OK|MB_ICONSTOP);
		return false;
	}

	_dispatchFileName( m_vDeleteFile );
	_writeCommandFile( fp, "del" );
	_dispatchFileName( m_vReplaceFile );
	_writeCommandFile( fp, "replace" );
	_dispatchFileName( m_vNewFile );
	_writeCommandFile( fp, "new" );
	fprintf(fp, "%s %s\n", COMMAND_RPL, VERSION_FILE);

	//�����Ҫɾ�����ļ�
	//for(int i=0; i<(int)m_vDeleteFile.size(); i++)
	//{
	//	fprintf(fp, "-%s\n", m_vDeleteFile[i].strFileName.c_str());
	//}

	//�����Ҫ���ĵ��ļ�
	//for(int i=0; i<(int)m_vNewFile.size(); i++)
	//{
	//	fprintf(fp, "+%s\n", m_vNewFile[i].strFileName.c_str());
	//}
	fclose(fp); fp=0;

	//--------------------------------------------------------------
	//���������ļ�
	char szTempCmdFile[MAX_PATH] = {0};
	::GetTempFileName(szTempPath, "pmk", MAX_PATH, szTempCmdFile);
	pKeyMath->SignFile(szTempFileName, szTempCmdFile, FALSE, TRUE);
	DeleteFile(szTempFileName);
	
	//--------------------------------------------------------------
	//����Patch�ļ���
	char szPatchFileName[MAX_PATH];
	_snprintf(szPatchFileName, MAX_PATH, "WXWorld(%s-%s).zip", 
		m_oldVerData.GetVersion().c_str(), m_newVerData.GetVersion().c_str());

	char szPatchPathFileName[MAX_PATH] = {0};
	strncpy(szPatchPathFileName, m_strPatchPath.c_str(), MAX_PATH);
	PathAppend(szPatchPathFileName, szPatchFileName);
	
	m_strPatchFile = szPatchPathFileName;

	//����PatchZip
	HZIP hz = CreateZip((void *)szPatchPathFileName, 0, ZIP_FILENAME);
	_snprintf( szLogText, 2000, "CreateZip:%s\r\n", szPatchFileName  );
	if( LogFile ) fwrite( szLogText, 1, strlen(szLogText), LogFile );
	if(!hz) 
	{
		char szMsg[MAX_PATH] = {0};
		_snprintf(szMsg, MAX_PATH, "����%s����!", szTempFileName);
		AfxGetApp()->GetMainWnd()->MessageBox(szMsg, 0, MB_OK|MB_ICONSTOP);
		return false;
	}

	//���������ļ�
	ZipAdd(hz, COMMAND_FILE, (void *)szTempCmdFile, 0, ZIP_FILENAME, dwType);
	_snprintf( szLogText, 2000, "ZipAdd:%s\r\n", COMMAND_FILE  );
	if( LogFile ) fwrite( szLogText, 1, strlen(szLogText), LogFile );
	ZipAdd(hz, "nosee.zip", "e:/nosee.zip", 0, ZIP_FILENAME, dwType);
	_snprintf( szLogText, 2000, "ZipAdd:%s\r\n", "nosee.zip"  );
	if( LogFile ) fwrite( szLogText, 1, strlen(szLogText), LogFile );

	CHAR szOrgFile[MAX_PATH] = {0};
	//������Ҫ���ĵ��ļ�
	for(int i=0; i<(int)m_vReplaceFile.size(); i++)
	{
		// check terminate flag
		if(WAIT_OBJECT_0 == ::WaitForSingleObject(m_hTerimateHandle, 0))
		{
			CloseZip(hz);
			DeleteFile(szTempCmdFile);
			DeleteFile(szPatchPathFileName);
			return false;
		}

		const FileNode& thisNode = m_vReplaceFile[i];
		if(thisNode.bFold) continue;

		::SendMessage(m_hProgressWnd, WM_PROGRESS_ZIPFILE, (WPARAM)thisNode.strFileName.c_str(), 0);

		if( thisNode.strPackName.empty() ) {
			strncpy(szOrgFile, m_newVerData.GetPath().c_str(), MAX_PATH);
			PathAppend(szOrgFile, thisNode.strFileName.c_str());
			ZipAdd(hz, thisNode.strPatchName.c_str(), (void*)szOrgFile, 0, ZIP_FILENAME, dwType);
			_snprintf( szLogText, 2000, "ZipAdd:%s\r\n", thisNode.strPatchName.c_str() );
			if( LogFile ) fwrite( szLogText, 1, strlen(szLogText), LogFile );
		}
		else {
			DWORD dwSize = 0;
			strncpy(szOrgFile, m_newVerData.GetPath().c_str(), MAX_PATH);
			PathAppend(szOrgFile, thisNode.strPatchName.c_str());
			PathAppend(szOrgFile, thisNode.strPackName.c_str());
			char szPackFileName[MAX_PATH] = {0};
			strncpy( szPackFileName, szOrgFile, MAX_PATH );
			strcat( szPackFileName, ".wx0" );
			FILE* fp = fopen( szPackFileName, "rb" );
			if( !fp ) continue;
			fseek( fp, 4, SEEK_SET );
			fread( &dwSize, 1, 4, fp );
			char* pData = new char[dwSize];
			char* p = pData;
			fread( pData, 1, dwSize, fp );
			fclose( fp );
			DecryptHead(pData,dwSize);
			int nOffset = -1;
			DWORD dwFileSize = 0;
			char* pFileName = new char[thisNode.strFileName.length()+1];
			strcpy( pFileName, thisNode.strFileName.c_str() );
			//CryptName( pFileName, (BYTE)thisNode.strFileName.length() );
			pFileName[thisNode.strFileName.length()] = 0;
			while( (DWORD)( p - pData ) < dwSize ) {
				if( ( *p == thisNode.strFileName.length() ) && ( strnicmp( p + 1, pFileName, *p ) == 0 ) ) {
					p += *p + 1;
					DWORD dwOffset;
					CopyMemory( &dwOffset, p, 4 ); p += 4;
					CopyMemory( &dwFileSize, p, 4 );
					//DecryptOffset( dwOffset );
					//DecryptSize( dwFileSize );
					nOffset = (INT)dwOffset;
					break;
				}
				p += *p + 1;
				p += 12;
			}
			delete [] pData;
			delete [] pFileName;
			pData = 0;
			if( nOffset == -1 ) continue;
			dwSize = 0;
			for( int i = 1; i < 100; i ++ ) {
				sprintf( szPackFileName, "%s.w%s", szOrgFile, ConvertNumber(i) );
				fp = fopen( szPackFileName, "rb" );
				if( !fp ) continue;
				DWORD dw;
				fread( &dw, 1, 4, fp );
				if( (DWORD)nOffset < dwSize + dw ) {
					fseek( fp, nOffset - dwSize + 4, SEEK_SET );
					pData = new char[dwFileSize];
					fread( pData, 1, dwFileSize, fp );
					fclose( fp );
					break;
				}
				fclose( fp );
				dwSize += dw;
			}
			if( !pData ) continue;
			ZRESULT zs = ZipAdd(hz, ( thisNode.strPatchName + "/" + thisNode.strFileName ).c_str(), (void*)pData, dwFileSize, ZIP_MEMORY, dwType);
			_snprintf( szLogText, 2000, "ZipAdd:%s\r\n", ( thisNode.strPatchName + "/" + thisNode.strFileName ).c_str() );
			if( LogFile ) fwrite( szLogText, 1, strlen(szLogText), LogFile );
			delete [] pData; pData = 0;
			if( zs != ZR_OK )
				return( false );
		}
	}
	for(int i=0; i<(int)m_vNewFile.size(); i++)
	{
		// check terminate flag
		if(WAIT_OBJECT_0 == ::WaitForSingleObject(m_hTerimateHandle, 0))
		{
			CloseZip(hz);
			DeleteFile(szTempCmdFile);
			DeleteFile(szPatchPathFileName);
			return false;
		}

		const FileNode& thisNode = m_vNewFile[i];
		if(thisNode.bFold) continue;

		::SendMessage(m_hProgressWnd, WM_PROGRESS_ZIPFILE, (WPARAM)thisNode.strFileName.c_str(), 0);

		if( thisNode.strPackName.empty() ) {
			strncpy(szOrgFile, m_newVerData.GetPath().c_str(), MAX_PATH);
			PathAppend(szOrgFile, thisNode.strFileName.c_str());
			ZipAdd(hz, thisNode.strPatchName.c_str(), (void*)szOrgFile, 0, ZIP_FILENAME, dwType);
			_snprintf( szLogText, 2000, "ZipAdd:%s\r\n", thisNode.strPatchName.c_str() );
			if( LogFile ) fwrite( szLogText, 1, strlen(szLogText), LogFile );
		}
		else {
			DWORD dwSize = 0;
			strncpy(szOrgFile, m_newVerData.GetPath().c_str(), MAX_PATH);
			PathAppend(szOrgFile, thisNode.strPatchName.c_str());
			PathAppend(szOrgFile, thisNode.strPackName.c_str());
			char szPackFileName[MAX_PATH] = {0};
			strncpy( szPackFileName, szOrgFile, MAX_PATH );
			strcat( szPackFileName, ".wx0" );
			FILE* fp = fopen( szPackFileName, "rb" );
			if( !fp ) continue;
			fseek( fp, 4, SEEK_SET );
			fread( &dwSize, 1, 4, fp );
			char* pData = new char[dwSize];
			char* p = pData;
			fread( pData, 1, dwSize, fp );
			fclose( fp );
			DecryptHead(pData,dwSize);
			int nOffset = -1;
			DWORD dwFileSize = 0;
			char* pFileName = new char[thisNode.strFileName.length()+1];
			strcpy( pFileName, thisNode.strFileName.c_str() );
			//CryptName( pFileName, (BYTE)thisNode.strFileName.length() );
			pFileName[thisNode.strFileName.length()] = 0;
			while( (DWORD)( p - pData ) < dwSize ) {
				if( ( *p == thisNode.strFileName.length() ) && ( strnicmp( p + 1, pFileName, *p ) == 0 ) ) {
					p += *p + 1;
					DWORD dwOffset;
					CopyMemory( &dwOffset, p, 4 ); p += 4;
					CopyMemory( &dwFileSize, p, 4 );
					//DecryptOffset( dwOffset );
					//DecryptSize( dwFileSize );
					nOffset = (INT)dwOffset;
					break;
				}
				p += *p + 1;
				p += 12;
			}
			delete [] pData;
			pData = 0;
			if( nOffset == -1 ) continue;
			dwSize = 0;
			for( int i = 1; i < 100; i ++ ) {
				sprintf( szPackFileName, "%s.w%s", szOrgFile, ConvertNumber(i) );
				fp = fopen( szPackFileName, "rb" );
				if( !fp ) continue;
				DWORD dw;
				fread( &dw, 1, 4, fp );
				if( (DWORD)nOffset < dwSize + dw ) {
					fseek( fp, nOffset - dwSize + 4, SEEK_SET );
					pData = new char[dwFileSize];
					fread( pData, 1, dwFileSize, fp );
					fclose( fp );
					break;
				}
				fclose( fp );
				dwSize += dw;
			}
			if( !pData ) continue;
			ZRESULT zs = ZipAdd(hz, ( thisNode.strPatchName + "/" + thisNode.strFileName ).c_str(), (void*)pData, dwFileSize, ZIP_MEMORY, dwType);
			_snprintf( szLogText, 2000, "ZipAdd:%s\r\n", ( thisNode.strPatchName + "/" + thisNode.strFileName ).c_str() );
			if( LogFile ) fwrite( szLogText, 1, strlen(szLogText), LogFile );
			delete [] pData; pData = 0;
			if( zs != ZR_OK )
				return( false );
		}
	}
	CloseZip(hz);

	if( LogFile ) fclose( LogFile );

	DeleteFile(szTempCmdFile);

	///////////////////////////////////////////////////////////////
	// add crc for zip file

	DWORD dwCRC, dwFileSize;
	if(NO_ERROR == FileCrc32Win32(szPatchPathFileName, dwCRC, dwFileSize))
	{
		fp = fopen( szPatchPathFileName, "a" );
		if( !fp ) return false;
		fwrite( &dwCRC, 1, 4, fp );
		fclose( fp );
	}
	else 
		return false;
	///////////////////////////////////////////////////////////////
	return true;
}

void CPatchMaker::_dispatchFileName( std::vector< FileNode >& fileData )
{
	m_vModel.clear(); 
	m_vMaterial.clear(); 
	m_vEffect.clear();
	m_vInterface.clear();
	m_vSound.clear(); 
	m_vScene.clear(); 
	m_vBrushes.clear(); 
	m_vNormal.clear(); 
	m_vConfig.clear();

	for(int i=0; i<(int)fileData.size(); i++)
	{
		FileNode& fi = fileData[i];
		if( stricmp( fi.strPackName.c_str(), "Object" ) == 0 ) {
			m_vModel.push_back( fi.strPatchName + "/" + fi.strFileName );
		}
		else if( stricmp( fi.strPackName.c_str(), "Resource" ) == 0 ) {
			m_vMaterial.push_back( fi.strPatchName + "/" + fi.strFileName );
		}
		else if( stricmp( fi.strPackName.c_str(), "Special" ) == 0 ) {
			m_vEffect.push_back( fi.strPatchName + "/" + fi.strFileName );
		}
		else if( stricmp( fi.strPackName.c_str(), "Interface" ) == 0 ) {
			m_vInterface.push_back( fi.strPatchName + "/" + fi.strFileName );
		}
		else if( stricmp( fi.strPackName.c_str(), "Sound" ) == 0 ) {
			m_vSound.push_back( fi.strPatchName + "/" + fi.strFileName );
		}
		else if( stricmp( fi.strPackName.c_str(), "Terrain" ) == 0 ) {
			m_vScene.push_back( fi.strPatchName + "/" + fi.strFileName );
		}
		else if( stricmp( fi.strPackName.c_str(), "Surface" ) == 0 ) {
			m_vBrushes.push_back( fi.strPatchName + "/" + fi.strFileName );
		}
		else if( stricmp( fi.strPackName.c_str(), "Common" ) == 0 ) {
			m_vConfig.push_back( fi.strPatchName + "/" + fi.strFileName );
		}
		else {
			m_vNormal.push_back( fi.strFileName );
		}
		continue;
	}
}

void CPatchMaker::_writeCommandFile( FILE* fp, std::string strCommand )
{
/*
pdel fname from pname  ��pname����ɾ��һ����fname���ļ�
prpl fname from pname  ��pname�����滻һ����fname���ļ�
pnew fname from pname  ��pname��������һ����fname���ļ�
fdel fname ɾ��һ��fname�ļ�
frpl fname �滻һ��fname�ļ�
fnew fname ����һ��fname�ļ�
padd ����һ���°�(Ŀǰû��֧��)
pdel ɾ��һ����(Ŀǰû��֧��)
*/
	if( !fp ) return;
	for(int i=0; i<(int)m_vModel.size(); i++)
	{
		if( strCommand == "del" )
			fprintf(fp, "%s %s from Object\n", COMMAND_DEl, m_vModel[i].c_str());
		else if( strCommand == "replace" )
			fprintf(fp, "%s %s from Object\n", COMMAND_RPL, m_vModel[i].c_str());
		else if( strCommand == "new" )
			fprintf(fp, "%s %s from Object\n", COMMAND_ADD, m_vModel[i].c_str());
	}
	for(int i=0; i<(int)m_vMaterial.size(); i++)
	{
		if( strCommand == "del" )
			fprintf(fp, "%s %s from Resource\n", COMMAND_DEl, m_vMaterial[i].c_str());
		else if( strCommand == "replace" )		 
			fprintf(fp, "%s %s from Resource\n", COMMAND_RPL, m_vMaterial[i].c_str());
		else if( strCommand == "new" )			 
			fprintf(fp, "%s %s from Resource\n", COMMAND_ADD, m_vMaterial[i].c_str());
	}
	for(int i=0; i<(int)m_vEffect.size(); i++)
	{
		if( strCommand == "del" )
			fprintf(fp, "%s %s from Special\n", COMMAND_DEl, m_vEffect[i].c_str());
		else if( strCommand == "replace" )		
			fprintf(fp, "%s %s from Special\n", COMMAND_RPL, m_vEffect[i].c_str());
		else if( strCommand == "new" )			
			fprintf(fp, "%s %s from Special\n", COMMAND_ADD, m_vEffect[i].c_str());
	}
	for(int i=0; i<(int)m_vInterface.size(); i++)
	{
		if( strCommand == "del" )
			fprintf(fp, "%s %s from Interface\n", COMMAND_DEl, m_vInterface[i].c_str());
		else if( strCommand == "replace" )		  
			fprintf(fp, "%s %s from Interface\n", COMMAND_RPL, m_vInterface[i].c_str());
		else if( strCommand == "new" )			  
			fprintf(fp, "%s %s from Interface\n", COMMAND_ADD, m_vInterface[i].c_str());
	}
	for(int i=0; i<(int)m_vSound.size(); i++)
	{
		if( strCommand == "del" )
			fprintf(fp, "%s %s from Sound\n", COMMAND_DEl, m_vSound[i].c_str());
		else if( strCommand == "replace" )	  
			fprintf(fp, "%s %s from Sound\n", COMMAND_RPL, m_vSound[i].c_str());
		else if( strCommand == "new" )		  
			fprintf(fp, "%s %s from Sound\n", COMMAND_ADD, m_vSound[i].c_str());
	}
	for(int i=0; i<(int)m_vScene.size(); i++)
	{
		if( strCommand == "del" )
			fprintf(fp, "%s %s from Terrain\n", COMMAND_DEl, m_vScene[i].c_str());
		else if( strCommand == "replace" )		
			fprintf(fp, "%s %s from Terrain\n", COMMAND_RPL, m_vScene[i].c_str());
		else if( strCommand == "new" )			
			fprintf(fp, "%s %s from Terrain\n", COMMAND_ADD, m_vScene[i].c_str());
	}
	for(int i=0; i<(int)m_vBrushes.size(); i++)
	{
		if( strCommand == "del" )
			fprintf(fp, "%s %s from Surface\n", COMMAND_DEl, m_vBrushes[i].c_str());
		else if( strCommand == "replace" )		
			fprintf(fp, "%s %s from Surface\n", COMMAND_RPL, m_vBrushes[i].c_str());
		else if( strCommand == "new" )			
			fprintf(fp, "%s %s from Surface\n", COMMAND_ADD, m_vBrushes[i].c_str());
	}
	for(int i=0; i<(int)m_vConfig.size(); i++)
	{
		if( strCommand == "del" )
			fprintf(fp, "%s %s from Common\n", COMMAND_DEl, m_vConfig[i].c_str());
		else if( strCommand == "replace" )	   
			fprintf(fp, "%s %s from Common\n", COMMAND_RPL, m_vConfig[i].c_str());
		else if( strCommand == "new" )		   
			fprintf(fp, "%s %s from Common\n", COMMAND_ADD, m_vConfig[i].c_str());
	}
	for(int i=0; i<(int)m_vNormal.size(); i++)
	{
		if( m_vNormal[i] == VERSION_FILE ) continue;
		if( strCommand == "del" )
			fprintf(fp, "%s %s\n", COMMAND_DEl, m_vNormal[i].c_str());
		else if( strCommand == "replace" )
			fprintf(fp, "%s %s\n", COMMAND_RPL, m_vNormal[i].c_str());
		else if( strCommand == "new" )
			fprintf(fp, "%s %s\n", COMMAND_ADD, m_vNormal[i].c_str());
	}
}

void CPatchMaker::toLowerCase( std::string& str )
{
	if( str.size() > 0 ) {
		byte* tmp = new byte[str.size()+1];
		memcpy( tmp, str.c_str(), str.size() );
		tmp[str.size()] = 0;
		_mbslwr((byte*)tmp);
		str = (char*)tmp;
		delete [] tmp;
	}
}

bool CPatchMaker::StringMatch( std::string str, std::string pattern, bool caseSensitive )
{
    std::string tmpStr = str;
	std::string tmpPattern = pattern;
    if (!caseSensitive)
    {
        toLowerCase(tmpStr);
        toLowerCase(tmpPattern);
    }

    std::string::const_iterator strIt = tmpStr.begin();
    std::string::const_iterator patIt = tmpPattern.begin();
	std::string::const_iterator lastWildCardIt = tmpPattern.end();
    while (strIt != tmpStr.end() && patIt != tmpPattern.end())
    {
        if (*patIt == '*')
        {
			lastWildCardIt = patIt;
            // Skip over looking for next character
            ++patIt;
            if (patIt == tmpPattern.end())
			{
				// Skip right to the end since * matches the entire rest of the string
				strIt = tmpStr.end();
			}
			else
            {
				// scan until we find next pattern character
                while(strIt != tmpStr.end() && *strIt != *patIt)
                    ++strIt;
            }
        }
        else
        {
            if (*patIt != *strIt)
            {
				if (lastWildCardIt != tmpPattern.end())
				{
					// The last wildcard can match this incorrect sequence
					// rewind pattern to wildcard and keep searching
					patIt = lastWildCardIt;
					lastWildCardIt = tmpPattern.end();
				}
				else
				{
					// no wildwards left
					return false;
				}
            }
            else
            {
                ++patIt;
                ++strIt;
            }
        }

    }
	// If we reached the end of both the pattern and the string, we succeeded
	if (patIt == tmpPattern.end() && strIt == tmpStr.end())
	{
        return true;
	}
	else
	{
		return false;
	}
}

void CPatchMaker::GenerateFilter( std::string filter )
{
	m_vFilter.clear();
	if( filter.size() == 0 ) return;

	while( true )
	{
		size_t ret = filter.find( "\r\n" );
		if( ret != -1 )
		{
			std::string strTemp = filter.substr( 0, ret );
			filter = filter.substr( ret + 2 );
			m_vFilter.push_back( strTemp );
		}
		else
		{
			break;
		}
	}
	if( filter.empty() == false ) m_vFilter.push_back( filter );
}

bool CPatchMaker::FileNameMatch( std::string fname )
{
	if( fname.size() == 0 ) return false;
	for( int i = 0; i < (int)m_vFilter.size(); i ++ ) {
		if( StringMatch( fname, m_vFilter[i], false ) ) {
			return true;
		}
	}
	return false;
}

char* CPatchMaker::ConvertNumber( DWORD num )
{
	static char result[3] = {'x','0', 0};
	if( num >= 100 ) return "";

	DWORD b1 = num / 10;
	DWORD b2 = num % 10;

	switch( b1 )
	{
	case 0:
		result[0] = 'x';
		break;
	case 1:
		result[0] = 'y';
		break;
	case 2:
		result[0] = 'z';
		break;
	case 3:
		result[0] = 'h';
		break;
	case 4:
		result[0] = 'j';
		break;
	case 5:
		result[0] = 'l';
		break;
	case 6:
		result[0] = 'n';
		break;
	case 7:
		result[0] = 'p';
		break;
	case 8:
		result[0] = 'r';
		break;
	case 9:
		result[0] = 't';
		break;
	}

	switch( b2 )
	{
	case 0:
		result[1] = '0';
		break;
	case 1:
		result[1] = '1';
		break;
	case 2:
		result[1] = '2';
		break;
	case 3:
		result[1] = '3';
		break;
	case 4:
		result[1] = '4';
		break;
	case 5:
		result[1] = '5';
		break;
	case 6:
		result[1] = '6';
		break;
	case 7:
		result[1] = '7';
		break;
	case 8:
		result[1] = '8';
		break;
	case 9:
		result[1] = '9';
		break;
	}

	return result;
}

char* CPatchMaker::CryptName( char* pName, BYTE len )
{
	BYTE lo = len*2;
	if( lo > 28 ) lo = 28;
	for( BYTE i = 0; i < len; i ++ )
	{
		if( pName[i] % 2 ) //����
		{
			pName[i] = pName[i] - 22; //�����˺���Ȼ������
		}
		else //ż��
		{
			pName[i] = pName[i] + lo; //�����˺���Ȼ��ż��
		}
	}

	return pName;
}

void CPatchMaker::CryptOffset( DWORD& dwOffset )
{
	WORD wh,wl;
	wh = (WORD)( ( dwOffset & 0xFFFF0000 ) >> 16 );
	wl = (WORD)( dwOffset & 0x0000FFFF );

	DWORD dw = (wl+20) << 16;
	wh -= 7;
	dwOffset = dw + wh;
}

void CPatchMaker::CryptSize( DWORD& dwSize )
{
	BYTE flag = (BYTE)( dwSize & 0x0000000F );
	if( flag > 7 )
	{
		dwSize += 0x00012340;
	}
	else
	{
		dwSize += 0x00567800;
	}
}

char* CPatchMaker::DecryptName(char* name, int len)
{
	int lo = len * 2;
	if( lo > 28 ) lo = 28;
	for( int i = 0; i < len; i ++ )
	{
		if( name[i] %2 ) //����
		{
			name[i] = name[i] + 22;
		}
		else //ż��
		{
			name[i] = name[i] - lo;
		}
	}
	//name[len] = 0;
	return name;
}

void  CPatchMaker::DecryptOffset(unsigned long& dwOffset)
{
	unsigned short wh;
	unsigned short wl;

	wh = (unsigned short)( ( dwOffset & 0xFFFF0000 ) >> 16 );
	wl = (unsigned short)( dwOffset & 0x0000FFFF );

	wh -= 20;
	wl += 7;
	unsigned long dw = wl << 16;
	dwOffset = dw + wh;
}

void  CPatchMaker::DecryptSize(unsigned long& dwSize)
{
	unsigned char flag = (unsigned char)(dwSize & 0x0000000F);
	if( flag > 7 )
	{
		dwSize -= 0x00012340;
	}
	else
	{
		dwSize -= 0x00567800;
	}
}

void CPatchMaker::DecryptHead( char* lpPacketHead, DWORD dwHeadLen )
{
	char* lpHead = lpPacketHead;
	BYTE Length;
	while( (DWORD)( lpHead - lpPacketHead ) < dwHeadLen ) {
		Length = *lpHead ++;
		DecryptName( lpHead, Length ); lpHead += Length;
		DecryptOffset( *(DWORD*)lpHead ); lpHead += 4;
		DecryptSize( *(DWORD*)lpHead ); lpHead += 4;
		lpHead += 4; //CRC
	}
}

void CPatchMaker::CryptZipFile( char* lpFileName )
{
	static DWORD nouse_buf[16] = 
	{
		0XE011CFD0, 0XE11AB1A1, 0X00000000, 0X00000000,
		0X00000000, 0X00000000, 0X0003003E, 0X0009FFFE,
		0X00000006, 0X00000000, 0X00000000, 0X00000001,
		0X0000002F, 0X00000000, 0X00001000, 0X00000031,
	};
	static char buf[64];
	FILE* fp;
	fp = fopen( lpFileName, "r+b" );
	if( fp )
	{
		fseek( fp, 0, SEEK_END );
		long size = ftell( fp );
		if( size <= 64 ) { fclose( fp ); return; }

		fseek( fp, 0, SEEK_SET );
		fread( &buf[0], 1, 64, fp );
		CryptName( buf, 64 );
		fseek( fp, 0, SEEK_SET );
		fwrite( &nouse_buf[0], 4, 16, fp );
		fseek( fp, 0, SEEK_END );
		fwrite( buf, 1, 64, fp );
		fclose( fp );
	}
}




