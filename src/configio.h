
#include "nisetro.h"

#ifndef __CONFIGIO_H__
#define __CONFIGIO_H__

//! 隙間の最小値
const unsigned char CSCRSPACE_MIN = 0;

//! 隙間の最大値
const unsigned char CSCRSPACE_MAX = 100;

//! 拡大率の変動の割合
const float CSCRSCAL_SCALE = 10.0f;	// 10%ごと(嘘)

//! 拡大率の最小値
const float CSCRSCAL_MIN = 0.5f;

//! 拡大率の最大値
const float CSCRSCAL_MAX = 3.0f;

typedef struct SAPPCONFIG {
	//! 表示する画面
	ECAPSCR			m_ScrSel;
	
	//! 画面の向き
	unsigned char	m_DirMode;

	//! 画面の隙間
	unsigned char	m_ScrSpace;

	//! 拡大率
	float			m_fScrScal;
	
	//! 表示フラグ(手前に表示する)
	bool			m_bTopWindow;

	//! エラーがあった場合、表示を更新しない
	bool			m_bDropFrame;
	
	// カメレオンUSBのＩＤ
	int				m_iCUSB2_ID;
	
	//! 表示する速度
	ECAPFPS			m_eFrmSkip;
	
	//
} SAPPCONFIG , *PAPPCONFIG , *LPAPPCONFIG;

/*!
	@brief	設定からクライアントサイズを取得する
	@param	pAppConfig	[in] 設定変数へのポインタ
	@param	pRC			[out] クライアントサイズ
*/
void GetClientSizeForAppconfig( LPAPPCONFIG pAppConfig , RECT *pRC );

void ShowAppConfig( LPAPPCONFIG pAppConfig );

/*!
	@brief	設定をファイル(xml)に書き出す
	@param	pConfig		[in] 設定変数へのポインタ
	@param	pFilename	[in] ファイル名
*/
int ConfigIOSave( PAPPCONFIG pConfig , TCHAR* pFilename );

/*!
	@brief	設定ファイル(xml)から設定を読む
	@param	pConfig		[out] 設定変数へのポインタ
	@param	pFilename	[in] ファイル名
*/
int ConfigIOLoad( PAPPCONFIG pConfig , TCHAR* pFilename );

int ConfigIOCheckCmdLine( PAPPCONFIG pConfig , LPTSTR lpszCmdLine );

#endif

