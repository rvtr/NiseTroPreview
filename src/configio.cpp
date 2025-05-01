
#include <stdio.h>
#include <tchar.h>
#include "_tprintd.h"
#include "nisetro.h"
#include "configio.h"

#import "msxml4.dll" raw_interfaces_only

using namespace MSXML2;

/*
	@brief	設定からクライアントサイズを取得する
	@param	pAppConfig	[in] 設定変数へのポインタ
	@param	pRC			[out] クライアントサイズ
*/
void GetClientSizeForAppconfig( LPAPPCONFIG pAppConfig , RECT *pRC ){
	if( pAppConfig == NULL ) return;
	if( pRC == NULL ) return;
	pRC->left	= 0;
	pRC->top	= 0;
	if( pAppConfig->m_ScrSel == ECAPSCR_DSCR ){
		pRC->right = NDS_SCREEN_W;
		pRC->bottom = NDS_SCREEN2_H + pAppConfig->m_ScrSpace;
	}else{
		pRC->right = NDS_SCREEN_W;
		pRC->bottom = NDS_SCREEN_H;
	}
	
	// 幅と高さを入れ替える
	if( pAppConfig->m_DirMode != 0 ){
		long t = pRC->right;
		pRC->right = pRC->bottom;
		pRC->bottom = t;
	}
	
	pRC->right = (long)((float)pRC->right * pAppConfig->m_fScrScal);
	pRC->bottom = (long)((float)pRC->bottom * pAppConfig->m_fScrScal);
	
}

void ShowAppConfig( LPAPPCONFIG pAppConfig ){
	if( pAppConfig == NULL ) return;
	_tprintd( TEXT("space\t%d\n") , pAppConfig->m_ScrSpace );
	_tprintd( TEXT("scale\t%f\n") , pAppConfig->m_fScrScal );
	if( pAppConfig->m_ScrSel == ECAPSCR_DSCR ) _tprintd( TEXT("scr  \t%s\n") , TEXT("ECAPSCR_DSCR") );
	else if( pAppConfig->m_ScrSel == ECAPSCR_TOP ) _tprintd( TEXT("scr  \t%s\n") , TEXT("ECAPSCR_TOP") );
	else if( pAppConfig->m_ScrSel == ECAPSCR_BTM ) _tprintd( TEXT("scr  \t%s\n") , TEXT("ECAPSCR_BTM") );
	else _tprintd( TEXT("scr  \t%d\n") , pAppConfig->m_ScrSel );
	
	if( pAppConfig->m_DirMode == 0 ) _tprintd( TEXT("dir  \t%s\n") , TEXT("Normal") );
	else if( pAppConfig->m_DirMode == 1 ) _tprintd( TEXT("dir  \t%s\n") , TEXT("left") );
	else if( pAppConfig->m_DirMode == 2 ) _tprintd( TEXT("dir  \t%s\n") , TEXT("right") );
	else _tprintd( TEXT("dir  \t%d\n") , pAppConfig->m_DirMode );
	
}

/*
	@brief	設定をファイル(xml)に書き出す
	@param	pConfig		[in] 設定変数へのポインタ
	@param	pFilename	[in] ファイル名
*/
int ConfigIOSave( PAPPCONFIG pConfig , TCHAR* pFilename ){
	if( pConfig == NULL || pFilename == NULL ) return -1;
	
	MSXML2::IXMLDOMDocument2Ptr pDoc;
	MSXML2::IXMLDOMProcessingInstructionPtr pPI;
	MSXML2::IXMLDOMElementPtr	pEmtConfig;
	MSXML2::IXMLDOMElementPtr	pElement;
	MSXML2::IXMLDOMTextPtr		pEmtBLine;
	TCHAR strBuf[256];
	
	try{
		pDoc.CreateInstance(__uuidof(MSXML2::DOMDocument30));
		pDoc->put_async( VARIANT_FALSE );
		
		pDoc->createProcessingInstruction( TEXT("xml"), TEXT("version=\'1.0\' encoding=\'UTF-8\'") , &pPI );
		pDoc->appendChild( pPI , NULL );
		pPI = NULL;
		
		// <config>を作成
		pDoc->createElement(_T("config"), &pEmtConfig);
		
		pDoc->createTextNode(TEXT("\n") , &pEmtBLine );
		pEmtConfig->appendChild( pEmtBLine , NULL ); // 改行
		pEmtBLine = NULL;
		
		// <scrsel>を作成
		pDoc->createElement(_T("scrsel"), &pElement);
	#if _MSC_VER >= 1400
		_stprintf_s( strBuf , 256 , TEXT("%d") , pConfig->m_ScrSel );
	#else
		_stprintf( strBuf , TEXT("%d") , pConfig->m_ScrSel );
	#endif
		pElement->put_text( strBuf );
		// <config>に<scrsel>を追加する
		pEmtConfig->appendChild(pElement, NULL);
		pElement = NULL;
		pDoc->createTextNode(TEXT("\n") , &pEmtBLine );
		pEmtConfig->appendChild( pEmtBLine , NULL ); // 改行
		pEmtBLine = NULL;
		
		// <dir>を作成
		pDoc->createElement(_T("dir"), &pElement);
	#if _MSC_VER >= 1400
		_stprintf_s( strBuf , 256 , TEXT("%d") , pConfig->m_DirMode );
	#else
		_stprintf( strBuf , TEXT("%d") , pConfig->m_DirMode );
	#endif
		pElement->put_text( strBuf );
		// <config>に<dir>を追加する
		pEmtConfig->appendChild(pElement, NULL);
		pElement = NULL;
		pDoc->createTextNode(TEXT("\n") , &pEmtBLine );
		pEmtConfig->appendChild( pEmtBLine , NULL ); // 改行
		pEmtBLine = NULL;
		
		// <space>を作成
		pDoc->createElement(_T("space"), &pElement);
	#if _MSC_VER >= 1400
		_stprintf_s( strBuf , 256 , TEXT("%d") , pConfig->m_ScrSpace );
	#else
		_stprintf( strBuf , TEXT("%d") , pConfig->m_ScrSpace );
	#endif
		pElement->put_text( strBuf );
		// <config>に<space>を追加する
		pEmtConfig->appendChild(pElement, NULL);
		pElement = NULL;
		pDoc->createTextNode(TEXT("\n") , &pEmtBLine );
		pEmtConfig->appendChild( pEmtBLine , NULL ); // 改行
		pEmtBLine = NULL;
		
		// <scale>を作成
		pDoc->createElement(_T("scale"), &pElement);
	#if _MSC_VER >= 1400
		_stprintf_s( strBuf , 256 , TEXT("%f") , pConfig->m_fScrScal );
	#else
		_stprintf( strBuf , TEXT("%f") , pConfig->m_fScrScal );
	#endif
		pElement->put_text( strBuf );
		// <config>に<scale>を追加する
		pEmtConfig->appendChild(pElement, NULL);
		pElement = NULL;
		pDoc->createTextNode(TEXT("\n") , &pEmtBLine );
		pEmtConfig->appendChild( pEmtBLine , NULL ); // 改行
		pEmtBLine = NULL;
		
		// <topwindow>を作成
		pDoc->createElement(_T("topwindow"), &pElement);
	#if _MSC_VER >= 1400
		_stprintf_s( strBuf , 256 , TEXT("%d") , pConfig->m_bTopWindow );
	#else
		_stprintf( strBuf , TEXT("%d") , pConfig->m_bTopWindow );
	#endif
		pElement->put_text( strBuf );
		// <config>に<topwindow>を追加する
		pEmtConfig->appendChild(pElement, NULL);
		pElement = NULL;
		pDoc->createTextNode(TEXT("\n") , &pEmtBLine );
		pEmtConfig->appendChild( pEmtBLine , NULL ); // 改行
		pEmtBLine = NULL;
		
		// <dropframe>を作成
		pDoc->createElement(_T("dropframe"), &pElement);
	#if _MSC_VER >= 1400
		_stprintf_s( strBuf , 256 , TEXT("%d") , pConfig->m_bDropFrame );
	#else
		_stprintf( strBuf , TEXT("%d") , pConfig->m_bDropFrame );
	#endif
		pElement->put_text( strBuf );
		// <config>に<dropframe>を追加する
		pEmtConfig->appendChild(pElement, NULL);
		pElement = NULL;
		pDoc->createTextNode(TEXT("\n") , &pEmtBLine );
		pEmtConfig->appendChild( pEmtBLine , NULL ); // 改行
		pEmtBLine = NULL;
		
		// <config> をドキュメントに追加する
		pDoc->appendChild(pEmtConfig, NULL);
		pEmtConfig = NULL;
		
		_variant_t strOutPath(::SysAllocString(pFilename));
		pDoc->save(strOutPath);
		
	}catch(_com_error &e){
		_tprintd( TEXT("xml error %s\n") , e.Description() );
	}
	
	pEmtBLine = NULL;
	pElement = NULL;
	pEmtConfig = NULL;
	pPI = NULL;
	pDoc = NULL;

	_tprintd( TEXT("config save end\n") );
	
	return 0;
}

/*
	@brief	設定ファイル(xml)から設定を読む
	@param	pConfig		[out] 設定変数へのポインタ
	@param	pFilename	[in] ファイル名
*/
int ConfigIOLoad( PAPPCONFIG pConfig , TCHAR* pFilename ){
	if( pConfig == NULL || pFilename == NULL ) return -1;
	
	MSXML2::IXMLDOMDocument2Ptr pDoc;
	MSXML2::IXMLDOMElementPtr pRoot;
	MSXML2::IXMLDOMNodePtr pNodeRet;
	VARIANT_BOOL isSuc;
	
	try{
		pDoc.CreateInstance(__uuidof(MSXML2::DOMDocument30));
		pDoc->put_async(VARIANT_FALSE);
		pDoc->load(_variant_t(pFilename) , &isSuc );
		pDoc->get_documentElement(&pRoot);
		
		pRoot->selectSingleNode( TEXT("scrsel") , &pNodeRet );
		if( pNodeRet ){
			BSTR    value;
			pNodeRet->get_text( &value );
			long v = _tstol(value);
			_tprintd( TEXT("scrsel %d\n") , v );
			if( v == ECAPSCR_BTM || v == ECAPSCR_TOP || v == ECAPSCR_DSCR )
				pConfig->m_ScrSel = (ECAPSCR)v;
			pNodeRet = NULL;
		}
		
		pRoot->selectSingleNode( TEXT("dir") , &pNodeRet );
		if( pNodeRet ){
			BSTR    value;
			pNodeRet->get_text( &value );
			long v = _tstol(value);
			_tprintd( TEXT("dir %d\n") , v );
			if( v >= 0 && v <= 2 )
				pConfig->m_DirMode = (unsigned char)v;
			pNodeRet = NULL;
		}
		
		pRoot->selectSingleNode( TEXT("space") , &pNodeRet );
		if( pNodeRet ){
			BSTR    value;
			pNodeRet->get_text( &value );
			long v = _tstol(value);
			_tprintd( TEXT("space %d\n") , v );
			if( v <= CSCRSPACE_MIN ) v = CSCRSPACE_MIN;
			if( v >= CSCRSPACE_MAX ) v = CSCRSPACE_MAX;
			pConfig->m_ScrSpace = (unsigned char)v;
			pNodeRet = NULL;
		}
		
		pRoot->selectSingleNode( TEXT("scale") , &pNodeRet );
		if( pNodeRet ){
			BSTR    value;
			pNodeRet->get_text( &value );
			float v = (float)_tstof(value);
			_tprintd( TEXT("scale %f\n") , v );
			if( v <= CSCRSCAL_MIN ) v = CSCRSCAL_MIN;
			if( v >= CSCRSCAL_MAX ) v = CSCRSCAL_MAX;
			pConfig->m_fScrScal = v;
			pNodeRet = NULL;
		}
		
		pRoot->selectSingleNode( TEXT("topwindow") , &pNodeRet );
		if( pNodeRet ){
			BSTR    value;
			pNodeRet->get_text( &value );
			long v = _tstol(value);
			_tprintd( TEXT("topwindow %d\n") , v );
			pConfig->m_bTopWindow = (v ? true : false );
			pNodeRet = NULL;
		}
		
		pRoot->selectSingleNode( TEXT("dropframe") , &pNodeRet );
		if( pNodeRet ){
			BSTR    value;
			pNodeRet->get_text( &value );
			long v = _tstol(value);
			_tprintd( TEXT("dropframe %d\n") , v );
			pConfig->m_bDropFrame = (v ? true : false );
			pNodeRet = NULL;
		}
		
	}catch(_com_error &e){
		_tprintd( TEXT("xml error %s\n") , e.Description() );
	}
	
	pNodeRet	= NULL;
	pRoot		= NULL;
	pDoc		= NULL;
	//
	_tprintd( TEXT("config load end\n") );
	
	return 0;
}