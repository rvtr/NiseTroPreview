
#include <windows.h>
#include <commctrl.h>
#include <locale.h>


#include <stdio.h>
#include <tchar.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <time.h>
#include "dx9render.h"

#include "cthread.h"
#include "cmutex.h"

#include "nisetro.h"
#include "main.h"

#include "_tprintd.h"

#include "cavisave.h"
#include "configio.h"

#include "res\resource.h"

#if defined(_MSC_VER)
	#if _MSC_VER >= 1500
	#include "res\res_msvc9.h"
//	#elif _MSC_VER >= 1400
	#else
	#include "res\res_msvc8.h"
//	#elif _MSC_VER >= 1310
//		// msvc71
//	#elif _MSC_VER >= 1300
//		// msvc7
	#endif
#endif

#pragma comment(lib,"winmm.lib")


CNISETRO	g_nise;

CMUTEX		g_mutex_Texture;

char g_cFps = 60;

bool		gDone = false;
HINSTANCE	g_hInst;
HWND		g_hWnd;		//ウィンドウハンドル
HWND		g_hConfigDlg = NULL;		//ウィンドウハンドル
HWND		g_hRecordDlg = NULL;		//ウィンドウハンドル

//! app config
SAPPCONFIG g_AppConfig;

LPDIRECT3D9	g_pD3D = NULL;
LPDIRECT3DDEVICE9 g_pD3DDev = NULL;

//! Topテクスチャ
LPDIRECT3DTEXTURE9 g_pTexTop = NULL;

//! Bottomテクスチャ
LPDIRECT3DTEXTURE9 g_pTexBtm = NULL;

LPD3DXSPRITE g_pD3DXSprite = NULL;

/*!
	@brief	Direct3Dのリソースを確保する
	@param	pD3DDev	[in]Direct3Dデバイス
*/
int D3DResInit( LPDIRECT3DDEVICE9* pD3DDev ){
	HRESULT hr;
	hr = (*pD3DDev)->CreateTexture( 256 , 256 , 1 , D3DUSAGE_DYNAMIC , D3DFMT_A8R8G8B8 , D3DPOOL_DEFAULT , &g_pTexTop , NULL );
	if( hr ){
		_tprintd( TEXT("Error CreateTexture\n") );
		return -1;
	}
	
	hr = (*pD3DDev)->CreateTexture( 256 , 256 , 1 , D3DUSAGE_DYNAMIC , D3DFMT_A8R8G8B8 , D3DPOOL_DEFAULT , &g_pTexBtm , NULL );
	if( hr ){
		_tprintd( TEXT("Error CreateTexture\n") );
		return -1;
	}
	
	hr = D3DXCreateSprite( (*pD3DDev) , &g_pD3DXSprite );
	if( hr ){
		_tprintd( TEXT("Error D3DXCreateSprite\n") );
		return -1;
	}
	return 0;
}

/*!
	@brief	Direct3Dのリソースを開放する
*/
void D3DResQuit( void ){
	SAFE_RELEASE( g_pD3DXSprite );
	SAFE_RELEASE( g_pTexBtm );
	SAFE_RELEASE( g_pTexTop );
}

//! バッファ変換用スレッド
class CBUFtoTEX : public CTHREAD{
public:
	CBUFtoTEX(){
		this->setSleepTime( 1000 / g_cFps );
	}
	
	virtual unsigned int ThreadFunction( void ){
		if( !g_pTexTop || !g_pTexBtm ) return -1;
		
		BYTE ScrData[NDS_SCREEN2BUF_SIZE];
		if( g_nise.GetScrData( ScrData , NDS_SCREEN2BUF_SIZE , 1000/g_cFps ) )
			return -1;
		
		// mutexのロックを取得
		if( g_mutex_Texture.lock(1000/g_cFps) == false ){
			D3DLOCKED_RECT LockRc;
			HRESULT hr;
			BYTE* pBits = NULL;
			
			// Topテクスチャのロックを取得
			hr = g_pTexTop->LockRect( 0 , &LockRc , NULL , 0 );
			if( hr ){ g_mutex_Texture.unlock(); return -1; }
			
			pBits = (BYTE*)LockRc.pBits;
			for( int y = 0; y < NDS_SCREEN_H; y++ ){
				pBits = (BYTE*)LockRc.pBits + LockRc.Pitch * y;
				BYTE* pBits2 = pBits;
				for( int x = 0; x < NDS_SCREEN_W; x++ ){
					for( int i = 2; i >= 0; i-- ){
						BYTE ucColor = (ScrData[y*NDS_SCREEN_W*3+x*3+i] & 0x3F);
						ucColor = (BYTE)(((float)ucColor / 63.0f) * 255.0f);
						(*pBits2++) = ucColor;
					}(*pBits2++) = 0xFF;
					
				}
			}
			{ // (拡大時に黒い線が入るので)1ライン追加する
				pBits = (BYTE*)LockRc.pBits + LockRc.Pitch * NDS_SCREEN_H;
				BYTE* pBits2 = pBits;
				for( int x = 0; x < NDS_SCREEN_W; x++ ){
					for( int i = 2; i >= 0; i-- ){
						BYTE ucColor = (ScrData[(NDS_SCREEN_H-1)*NDS_SCREEN_W*3+x*3+i] & 0x3F);
						ucColor = (BYTE)(((float)ucColor / 63.0f) * 255.0f);
						(*pBits2++) = ucColor;
					}(*pBits2++) = 0xFF;
					
				}
			}
			g_pTexTop->UnlockRect(0);//ロックの解放
			
			// Bottomテクスチャのロックを取得
			hr = g_pTexBtm->LockRect( 0 , &LockRc , NULL , 0 );
			if( hr ){ g_mutex_Texture.unlock(); return -1; }
			
			pBits = (BYTE*)LockRc.pBits;
			for( int y = 0; y < NDS_SCREEN_H; y++ ){
				pBits = (BYTE*)LockRc.pBits + LockRc.Pitch * y;
				BYTE* pBits2 = pBits;
				for( int x = 0; x < NDS_SCREEN_W; x++ ){
					for( int i = 2; i >= 0; i-- ){
						BYTE ucColor = (ScrData[NDS_SCREENBUF_SIZE+y*NDS_SCREEN_W*3+x*3+i] & 0x3F);
						ucColor = (BYTE)(((float)ucColor / 63.0f) * 255.0f);
						(*pBits2++) = ucColor;
					}(*pBits2++) = 0xFF;
					
				}
			}
			{ // (拡大時に黒い線が入るので)1ライン追加する
				pBits = (BYTE*)LockRc.pBits + LockRc.Pitch * NDS_SCREEN_H;
				BYTE* pBits2 = pBits;
				for( int x = 0; x < NDS_SCREEN_W; x++ ){
					for( int i = 2; i >= 0; i-- ){
						BYTE ucColor = (ScrData[NDS_SCREENBUF_SIZE+(NDS_SCREEN_H-1)*NDS_SCREEN_W*3+x*3+i] & 0x3F);
						ucColor = (BYTE)(((float)ucColor / 63.0f) * 255.0f);
						(*pBits2++) = ucColor;
					}(*pBits2++) = 0xFF;
					
				}
			}
			g_pTexBtm->UnlockRect(0);//ロックの解放
			
			g_mutex_Texture.unlock();
		}

		
		return 0;
	}
};

/*!
	@brief 録画用スレッド
*/
class CAVISAVE : public CAVISAVECORE{
private:
	unsigned char*	m_pBuf;
	unsigned short	m_usSpace;
public:
	CAVISAVE(){
		m_pBuf = NULL;
		
		this->setSleepTime( m_dwWaitTime_Max );
		m_pBuf = new unsigned char[ NDS_SCREEN_W * NDS_SCREEN2_H * 3 ];
	}
	
	~CAVISAVE(){
		if( m_pBuf ){
			delete [] m_pBuf;
		}
	}
	
	virtual unsigned int ThreadFunction( void ){
		DWORD dwTime1 = timeGetTime();
			BYTE ScrData[NDS_SCREEN2BUF_SIZE];
			if( g_nise.GetScrData(ScrData , NDS_SCREEN2BUF_SIZE , INFINITE ) == 0 ){
				// m_Imgをアップデート
				for( int y = 0; y < NDS_SCREEN_H; y++ ){
					unsigned char* pBit = m_pBuf +  (NDS_SCREEN_W * 3 * y);
					for( int x = 0; x < NDS_SCREEN_W; x++ ){
						for( int i = 2; i >= 0; i-- ){
							BYTE ucColor = (ScrData[y*NDS_SCREEN_W*3+x*3+i] & 0x3F);
							ucColor = (BYTE)(((float)ucColor / 63.0f) * 255.0f);
							(*pBit++) = ucColor;
						}
					}
				}
				
				for( int y = 0; y < NDS_SCREEN_H; y++ ){
					unsigned char* pBit = m_pBuf +  (NDS_SCREEN_W * 3) * (y + NDS_SCREEN_H);
					for( int x = 0; x < NDS_SCREEN_W; x++ ){
						for( int i = 2; i >= 0; i-- ){
							BYTE ucColor = (ScrData[NDS_SCREENBUF_SIZE + y*NDS_SCREEN_W*3+x*3+i] & 0x3F);
							ucColor = (BYTE)(((float)ucColor / 63.0f) * 255.0f);
							(*pBit++) = ucColor;
						}
					}
				}
				//
			}
			int erno = ffmpeg_addVFrame( GetFFMpeg() , m_pBuf , NDS_SCREEN_W , NDS_SCREEN2_H );
			if( erno ){
				TCHAR buf[256];
#if _MSC_VER >= 1400
				_stprintf_s( buf , 256 , TEXT("Error Num %d") , erno );
#else
				_stprintf( buf , TEXT("Error Num %d") , erno );
#endif
				MessageBox( NULL , buf , TEXT("error") , MB_ICONERROR | MB_OK );
			}
		DWORD dwTime2 = timeGetTime();
		DWORD dwWTime = m_dwWaitTime_Max - (dwTime2 - dwTime1);
		if( dwWTime <= 0 ){
			static int c = 0;
			_tprintd( TEXT("%d %d やばい\n") , c++ , dwWTime );
			dwWTime = 0;
		}
		this->setSleepTime( dwWTime );
		return 0;
	}
};

/*!
	@brief	FPSを取得する
	@return FPSが double で返される
	@note	※毎フレーム呼び出す必要がある
*/
double GetFPS( void )
{
	static DWORD	last = timeGetTime();
	static DWORD	frames = 0;
	static double	fps = 0;
	DWORD			current;
	
	current = timeGetTime();
	frames++;
	
	if(500 <= current - last) {// 0.5秒毎に更新
		double dt = (double)(current - last) / 1000.0f;
		fps = (double)frames / dt;
		last = current;
		frames = 0;
	}
	return fps;
}
/*!
	@brief 描画用スレッド
*/
class CRENDERTHREAD : public CTHREAD{
protected:
	DWORD m_dwMaxTime;
public:
	double m_dFPS;
	
	CRENDERTHREAD(){
		m_dFPS = 0;
		m_dwMaxTime = 1000 / g_cFps;
		this->setSleepTime( m_dwMaxTime );
	}
	
	virtual unsigned int ThreadFunction( void ){
		DWORD dwTime = timeGetTime();
		if( g_mutex_Texture.lock(1000/g_cFps) == false ){
			ReanderScreen();
			m_dFPS = GetFPS();
			g_mutex_Texture.unlock();
		}
		dwTime = timeGetTime() - dwTime;
		
//		_tprintd( TEXT("wait %d\n") , dwTime );
		if( dwTime > m_dwMaxTime )
			dwTime = 0;
		else
			dwTime = m_dwMaxTime - dwTime;
		this->setSleepTime( dwTime );
		return 0;
	}
};

CRENDERTHREAD g_thRender;

//! 描画関数
void ReanderScreen( void )
{
	PAPPCONFIG pAppConfig = &g_AppConfig;
	g_pD3DDev->BeginScene();
	g_pD3DDev->Clear( 0 , NULL , D3DCLEAR_TARGET , D3DCOLOR_XRGB(0,0,0) , 1.0f , 0 );
/*
	g_pD3DDev->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS , TRUE );
	g_pD3DDev->SetSamplerState( 0 , D3DSAMP_MAGFILTER , D3DTEXF_GAUSSIANQUAD );
	g_pD3DDev->SetSamplerState( 0 , D3DSAMP_MINFILTER , D3DTEXF_GAUSSIANQUAD );
	g_pD3DDev->SetSamplerState( 0 , D3DSAMP_MIPFILTER , D3DTEXF_GAUSSIANQUAD );*/
	
	//ここで描画
	if( g_pTexTop && g_pD3DXSprite ){
		if( pAppConfig->m_ScrSel == ECAPSCR_DSCR || pAppConfig->m_ScrSel == ECAPSCR_TOP ){
			RECT SrcRc = { 0 , 0 , NDS_SCREEN_W , NDS_SCREEN_H };
			D3DXVECTOR3 vecCenter( 0.0f , 0.0f ,0.0f );
			D3DXVECTOR3 vecPos( 0.0f , 0.0f ,0.0f );
			D3DXMATRIX mat;
			D3DXMATRIX matRot;
			D3DXMATRIX matTrans;
			D3DXMATRIX matScal;
			D3DXMatrixScaling( &matScal , pAppConfig->m_fScrScal , pAppConfig->m_fScrScal , 0.0f );
			float rot = 0.0f;
			float rx = 0.0f, ry = 0.0f;
			float x = 0.0f , y = 0.0f;
			
			if( pAppConfig->m_DirMode == 0 ){
				rot	= 0.0f;
				rx	= 0;
				ry	= 0;
				x = 0;
				y = 0;
			}else if( pAppConfig->m_DirMode == 1 ){
				rot	= -90.0f;
				rx	= 0;
				ry	= NDS_SCREEN_W;
				x = 0;
				y = 0;
			}else if( pAppConfig->m_DirMode == 2 ){
				rot	= 90.0f;
				rx	= NDS_SCREEN_H;
				ry	= 0;
				x = (float)(pAppConfig->m_ScrSpace + NDS_SCREEN_H);
				y = 0;
			}
			if( pAppConfig->m_ScrSel == ECAPSCR_TOP ){
				x = 0.0f;
				y = 0.0f;
			}
			{
				D3DXMatrixRotationZ( &matRot, D3DXToRadian( rot ) );
				matRot._41 = rx;   // X軸
				matRot._42 = ry;    // Y軸
				D3DXMatrixTranslation( &matTrans, ((float)x) , ((float)y) , 0.0f );
				D3DXMatrixMultiply( &mat , &matRot , &matTrans );
				D3DXMatrixMultiply( &mat , &mat , &matScal );
				g_pD3DXSprite->SetTransform( &mat );
			}

			g_pD3DXSprite->Begin( D3DXSPRITE_ALPHABLEND | D3DXSPRITE_DONOTSAVESTATE );
			g_pD3DXSprite->Draw( g_pTexTop , &SrcRc , &vecCenter , &vecPos , 0xFFFFFFFF );
			g_pD3DXSprite->End();
		}
	}
	
	if( g_pTexBtm && g_pD3DXSprite ){
		if( pAppConfig->m_ScrSel == ECAPSCR_DSCR || pAppConfig->m_ScrSel == ECAPSCR_BTM ){
			RECT SrcRc = { 0 , 0 , NDS_SCREEN_W , NDS_SCREEN_H };
			D3DXVECTOR3 vecCenter( 0.0f , 0.0f ,0.0f );
			D3DXVECTOR3 vecPos( 0.0f , 0.0f ,0.0f );
			D3DXMATRIX mat;
			D3DXMATRIX matRot;
			D3DXMATRIX matTrans;
			D3DXMATRIX matScal;
			D3DXMatrixScaling( &matScal , pAppConfig->m_fScrScal , pAppConfig->m_fScrScal , 0.0f );
			
			float rot = 0.0f;
			float rx = 0.0f, ry = 0.0f;
			float x = 0.0f , y = 0.0f;
			
			if( pAppConfig->m_DirMode == 0 ){
				y = (float)(pAppConfig->m_ScrSpace + NDS_SCREEN_H);
			//	g_pD3DXSprite->SetTransform( &matScal );
			//	vecPos.y = (float)(pAppConfig->m_ScrSpace + NDS_SCREEN_H);
			}else if( pAppConfig->m_DirMode == 1 ){
				rot = -90.0f;
				rx = 0.0f;
				ry = NDS_SCREEN_W;
				x = (float)(pAppConfig->m_ScrSpace + NDS_SCREEN_H);
				y = 0.0f;
			}else if( pAppConfig->m_DirMode == 2 ){
				rot = 90.0f;
				rx = NDS_SCREEN_H;
				ry = 0.0f;
				x = 0.0f;
				y = 0.0f;
			}
			if( pAppConfig->m_ScrSel == ECAPSCR_BTM ){
				x = 0.0f;
				y = 0.0f;
			}
			{
				D3DXMatrixRotationZ( &matRot, D3DXToRadian( rot ) );
				matRot._41 = rx;   // X軸
				matRot._42 = ry;    // Y軸
				D3DXMatrixTranslation( &matTrans, ((float)x) , ((float)y) , 0.0f );
				D3DXMatrixMultiply( &mat , &matRot , &matTrans );
				D3DXMatrixMultiply( &mat , &mat , &matScal );
				g_pD3DXSprite->SetTransform( &mat );
			}
			
			g_pD3DXSprite->Begin( D3DXSPRITE_ALPHABLEND | D3DXSPRITE_DONOTSAVESTATE );
			g_pD3DXSprite->Draw( g_pTexBtm , &SrcRc , &vecCenter , &vecPos , 0xFFFFFFFF );
			g_pD3DXSprite->End();
		}
	}
	g_pD3DDev->EndScene();
	g_pD3DDev->Present( NULL , NULL , NULL , NULL );
}

CAVISAVE	g_thAviSave;

#include "dlgproc_config.h"
#include "dlgproc_record.h"

//! 描画ウィンドウ用のコールバック
LRESULT CALLBACK WindowProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	PAINTSTRUCT ps;
//	_tprintd( TEXT("WindowProc start\n") );
//	ShowAppConfig();
	
/*	if( uMsg == WM_CREATE )		_tprintd( TEXT("%s , w(0x%08x) , l(0x%08x)\n") , TEXT("WM_CREATE") , wParam , lParam );
	if( uMsg == WM_QUIT )		_tprintd( TEXT("%s , w(0x%08x) , l(0x%08x)\n") , TEXT("WM_QUIT") , wParam , lParam );
	if( uMsg == WM_CLOSE )		_tprintd( TEXT("%s , w(0x%08x) , l(0x%08x)\n") , TEXT("WM_CLOSE") , wParam , lParam );
	if( uMsg == WM_COMMAND )	_tprintd( TEXT("%s , w(0x%08x) , l(0x%08x)\n") , TEXT("WM_COMMAND") , wParam , lParam );
	if( uMsg == WM_RBUTTONUP )	_tprintd( TEXT("%s , w(0x%08x) , l(0x%08x)\n") , TEXT("WM_RBUTTONUP") , wParam , lParam );
	if( uMsg == WM_SIZE )		_tprintd( TEXT("%s , w(0x%08x) , l(0x%08x)\n") , TEXT("WM_SIZE") , wParam , lParam );
	if( uMsg == WM_PAINT )		_tprintd( TEXT("%s , w(0x%08x) , l(0x%08x)\n") , TEXT("WM_PAINT") , wParam , lParam );*/
	
	switch( uMsg )
	{
		case WM_CREATE:
			{
				HMENU hMenu = GetMenu( hwnd );
				if( hMenu ){
					CheckMenuItem( hMenu , ID_MENU_TOPWINDOW , (g_AppConfig.m_bTopWindow?MF_CHECKED:MF_UNCHECKED) );
					CheckMenuItem( hMenu , ID_MENU_DROP_ERRFRAME , (g_AppConfig.m_bDropFrame?MF_CHECKED:MF_UNCHECKED) );
				}
				if( g_AppConfig.m_bTopWindow ){
					g_AppConfig.m_bTopWindow = false;
					SendMessage( hwnd , WM_COMMAND , MAKEWPARAM(ID_MENU_TOPWINDOW,0) , 0 );
				}
				SetTimer( hwnd , 1421356 , (1000 / 2) , NULL );
			}
			break;
		
		case WM_DESTROY:
			{
				KillTimer( hwnd, 1421356 );
			}
			break;
		
		case WM_TIMER:
			{
				TCHAR strBuf[256];
				TCHAR strTitle[256];
				LoadString( (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE) , IDS_STRING_TITLE , strTitle , 256 );
#if _MSC_VER >= 1400
				_stprintf_s( strBuf , 256 , TEXT("%s(FPS %2.1f) - (Data %2.1f) - ErrorFrame:%d") , strTitle , g_thRender.m_dFPS , g_nise.GetFps() , g_nise.GetErrorFrameCount() );
#else
				_stprintf( strBuf , TEXT("%s(FPS %2.1f) - (Data %2.1f) - ErrorFrame:%d") , strTitle , g_thRender.m_dFPS , g_nise.GetFps() , g_nise.GetErrorFrameCount() );
#endif
				SetWindowText( g_hWnd , strBuf );
			}
			break;
		
		case WM_QUIT:
			gDone = true;
			break;
		
		case WM_CLOSE:
			if( g_thAviSave.IsRec() == false ){
				if( g_hRecordDlg ){
					DestroyWindow( g_hRecordDlg );
					g_hRecordDlg = NULL;
				}
				PostQuitMessage(0);
			}else{
				TCHAR strBuf[256];
				LoadString( (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE) , IDS_STRING_WAR_EXIT_REC , strBuf , 256 );
				MessageBox( hwnd , strBuf , TEXT("Warning") , MB_ICONWARNING );
				return FALSE;
			}
			break;
		
		// デバイスロスト等
		case WM_DEVICECHANGE:
			{
				_tprintd( TEXT("WM_DEVICECHANGE\n") );
				if( g_nise.usb ){
					if( g_nise.usb->PnpEvent( wParam , lParam) ){
						_tprintd( TEXT("PnpEvent\n") );
						g_nise.NisetroQuit();
						g_nise.NisetroInit( ECAPFPS_60 , ECAPSCR_DSCR , g_AppConfig.m_iCUSB2_ID , hwnd );
					}
				}
			}
			break;
		
		case WM_COMMAND:
			{
				int id = LOWORD(wParam);
				switch (id) {
					case ID_MENU_REC:
						{
//							break;	// 一時、録画機能を凍結
							if( g_hRecordDlg )break;
							g_hRecordDlg = CreateDialog( g_hInst , MAKEINTRESOURCE(IDD_DLG_RECORD) , hwnd , (DLGPROC)RecordDlgProc );
							ShowWindow(g_hRecordDlg, SW_SHOW);
							UpdateWindow(g_hRecordDlg); 
						}
						break;
					
					case ID_MENU_SETTING:
						{
							if( g_hConfigDlg )break;
							g_hConfigDlg = CreateDialog( g_hInst , MAKEINTRESOURCE(IDD_DLG_CONFIG) , hwnd , (DLGPROC)ConfigDlgProc );
							ShowWindow(g_hConfigDlg, SW_SHOW);
							UpdateWindow(g_hConfigDlg); 
						}
						break;
					
					// 常に手前に表示する
					case ID_MENU_TOPWINDOW:
						{
							RECT rc;
							GetWindowRect( hwnd , &rc );
							if( g_AppConfig.m_bTopWindow ){
								SetWindowPos( hwnd , HWND_NOTOPMOST , rc.left , rc.top , rc.right - rc.left , rc.bottom - rc.top , 0 );
							}else{
								SetWindowPos( hwnd , HWND_TOPMOST , rc.left , rc.top , rc.right - rc.left , rc.bottom - rc.top , 0 );
							}
							g_AppConfig.m_bTopWindow = (g_AppConfig.m_bTopWindow ? false : true);
							HMENU hMenu = GetMenu( hwnd );
							if( hMenu )
								CheckMenuItem( hMenu , ID_MENU_TOPWINDOW , (g_AppConfig.m_bTopWindow?MF_CHECKED:MF_UNCHECKED) );
						}
						break;
					
					// エラーフレームを表示しない
					case ID_MENU_DROP_ERRFRAME:
						{
							g_AppConfig.m_bDropFrame = (g_AppConfig.m_bDropFrame ? false : true);
							g_nise.SetDropFrame( g_AppConfig.m_bDropFrame );
							HMENU hMenu = GetMenu( hwnd );
							if( hMenu )
								CheckMenuItem( hMenu , ID_MENU_DROP_ERRFRAME , (g_AppConfig.m_bDropFrame?MF_CHECKED:MF_UNCHECKED) );
						}
						break;
					
					case ID_MENU_END:
						SendMessage( hwnd , WM_CLOSE , 0 , 0 );
						break;
					
					// 画面の向き
					case ID_MENU_SRCROT_NORMAL:
					case ID_MENU_SRCROT_LEFT:
					case ID_MENU_SRCROT_RIGHT:
						{
							if( id == ID_MENU_SRCROT_NORMAL )	g_AppConfig.m_DirMode = 0;
							if( id == ID_MENU_SRCROT_LEFT )		g_AppConfig.m_DirMode = 1;
							if( id == ID_MENU_SRCROT_RIGHT )	g_AppConfig.m_DirMode = 2;
							D3DUpdateScreenConfig( &g_AppConfig , g_hWnd );
							if( g_hConfigDlg )SendMessage( GetDlgItem(g_hConfigDlg, IDC_COMBO_SCRDIR) , CB_SETCURSEL , g_AppConfig.m_DirMode , 0 );
						}
						break;
					
					// 倍率
					case ID_MENU_SRCSCAL_X1:
					case ID_MENU_SRCSCAL_X2:
					case ID_MENU_SRCSCAL_X3:
						{
							if( id == ID_MENU_SRCSCAL_X1 )	g_AppConfig.m_fScrScal = 1.0f;
							if( id == ID_MENU_SRCSCAL_X2 )	g_AppConfig.m_fScrScal = 2.0f;
							if( id == ID_MENU_SRCSCAL_X3 )	g_AppConfig.m_fScrScal = 3.0f;
							D3DUpdateScreenConfig( &g_AppConfig , g_hWnd );
							if( g_hConfigDlg ){
								SendMessage( GetDlgItem(g_hConfigDlg, IDC_SLIDER_SCRSCALE) , TBM_SETPOS, (WPARAM)TRUE, (LPARAM)(g_AppConfig.m_fScrScal*100));
								SendMessage( g_hConfigDlg , WM_HSCROLL, (WPARAM)TRUE, (LPARAM)GetDlgItem(g_hConfigDlg, IDC_SLIDER_SCRSCALE));
							}
						}
						break;
					
					// 
					case ID_MENU_SRCSEL_TOP:
					case ID_MENU_SRCSEL_BTM:
					case ID_MENU_SRCSEL_DSCR:
						{
							if( id == ID_MENU_SRCSEL_TOP )	g_AppConfig.m_ScrSel = ECAPSCR_TOP;
							if( id == ID_MENU_SRCSEL_BTM )	g_AppConfig.m_ScrSel = ECAPSCR_BTM;
							if( id == ID_MENU_SRCSEL_DSCR )	g_AppConfig.m_ScrSel = ECAPSCR_DSCR;
							D3DUpdateScreenConfig( &g_AppConfig , g_hWnd );
							if( g_hConfigDlg )SendMessage( GetDlgItem(g_hConfigDlg, IDC_COMBO_SCRSEL) , CB_SETCURSEL , (id-ID_MENU_SRCSEL_TOP) , 0 );
						}
						break;
					
					// 画面の隙間
					case ID_MENU_SRCSPACE_000:
					case ID_MENU_SRCSPACE_010:
					case ID_MENU_SRCSPACE_020:
					case ID_MENU_SRCSPACE_030:
					case ID_MENU_SRCSPACE_040:
					case ID_MENU_SRCSPACE_050:
					case ID_MENU_SRCSPACE_060:
					case ID_MENU_SRCSPACE_070:
					case ID_MENU_SRCSPACE_080:
					case ID_MENU_SRCSPACE_090:
					case ID_MENU_SRCSPACE_100:
						{
							g_AppConfig.m_ScrSpace = (id - ID_MENU_SRCSPACE_000) * 10;
							D3DUpdateScreenConfig( &g_AppConfig , g_hWnd );
							if( g_hConfigDlg ){
								SendMessage( GetDlgItem(g_hConfigDlg, IDC_SLIDER_SCRSPACE) , TBM_SETPOS, (WPARAM)TRUE, (LPARAM)g_AppConfig.m_ScrSpace);
								SendMessage( g_hConfigDlg , WM_HSCROLL, (WPARAM)TRUE, (LPARAM)GetDlgItem(g_hConfigDlg, IDC_SLIDER_SCRSPACE));
							}
						}
						break;
						
					// スクリーンショット
					case ID_MENU_SCREENSHOT:
						{
							// どのタイミングでロックするか・・・
							D3DSURFACE_DESC desc;
							LPDIRECT3DTEXTURE9 pTex = NULL;
							LPDIRECT3DSURFACE9 pSurSave = NULL;
							TCHAR strFileName[MAX_PATH];
							TCHAR strFileTitle[MAX_PATH];
							memset( strFileName , 0 , sizeof(TCHAR) * MAX_PATH );
							memset( strFileTitle , 0 , sizeof(TCHAR) * MAX_PATH );
							
							
							OPENFILENAME ofn;
							memset( &ofn , 0 , sizeof(OPENFILENAME) );
							
							ofn.lStructSize		= sizeof(OPENFILENAME); 
							ofn.hInstance		= (HINSTANCE)GetWindowLongPtr( hwnd , GWLP_HINSTANCE );
							ofn.hwndOwner		= hwnd;
							ofn.lpstrFilter		= TEXT("pngfile(*.png)\0*.png\0\01");
							ofn.Flags			= OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_OVERWRITEPROMPT;
							ofn.lpstrDefExt		= TEXT("png");
							ofn.lpstrFileTitle	= strFileTitle;
							ofn.nMaxFileTitle	= MAX_PATH;
							ofn.lpstrFile		= strFileName;
							ofn.nMaxFile		= MAX_PATH;
							
							// 保存するサーフェイスを確保する
							if( g_mutex_Texture.lock(INFINITE) == false ){
								if( g_pD3DDev ){
									LPDIRECT3DSURFACE9 pSur = NULL;
									g_pD3DDev->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO , &pSur );
									pSur->GetDesc(&desc);
									
									// テクスチャのサイズを求める(2の倍数でなければならない)
									UINT w , h;
									for( w = 1; w <= desc.Width; w*= 2 );
									for( h = 1; h <= desc.Height; h*= 2 );
									
									_tprintd( TEXT("tex %d , %d\n") , w , h );
									g_pD3DDev->CreateTexture( w , h , 1 , D3DUSAGE_DYNAMIC , D3DFMT_A8R8G8B8 , D3DPOOL_DEFAULT , &pTex , NULL );
									if( pTex )pTex->GetSurfaceLevel( 0 , &pSurSave ); else{ MessageBox(NULL , TEXT("e") , TEXT("error texture") , MB_OK ); }
									// このままではファイル名を指定中にサーフェイスの内容が変わってしまうのでコピーする
									if( pSur && pSurSave ){
										D3DXLoadSurfaceFromSurface( pSurSave , NULL , NULL , pSur , NULL , NULL , D3DX_FILTER_NONE , 0 );
									} else{ MessageBox(NULL , TEXT("e") , TEXT("error surface copy") , MB_OK ); }
									SAFE_RELEASE( pSur );
								}
#if defined(_DEBUG)
								LPDIRECT3DSURFACE9 pSurTop = NULL;
								LPDIRECT3DSURFACE9 pSurBtm = NULL;
								
								if( g_pTexTop ){
									g_pTexTop->GetSurfaceLevel( 0 , &pSurTop );
									if( pSurTop ){
										D3DXSaveSurfaceToFile( TEXT("ScreenShot_Top.png") , D3DXIFF_PNG , pSurTop , NULL , NULL );
										SAFE_RELEASE( pSurTop );
									}
								} // if( g_pTexTop ){
								if( g_pTexBtm ){
									g_pTexBtm->GetSurfaceLevel( 0 , &pSurBtm );
									if( pSurBtm ){
										D3DXSaveSurfaceToFile( TEXT("ScreenShot_Btm.png") , D3DXIFF_PNG , pSurBtm , NULL , NULL );
										SAFE_RELEASE( pSurBtm );
									}
								} // if( g_pTexBtm ){
#endif
								g_mutex_Texture.unlock();
							}
							
							// 保存する場所を問い合せる
							if( GetSaveFileName( &ofn ) ){
								//	break point
								strFileTitle[MAX_PATH - 1]	= NULL;
								strFileName[MAX_PATH - 1]	= NULL;
								ofn.lpstrFileTitle			= ofn.lpstrFileTitle;
								ofn.lpstrFile				= ofn.lpstrFile;
								
								bool bError = false;
								
								if( pSurSave ){
									RECT rc;
									rc.left		= 0;
									rc.top		= 0;
									rc.right	= desc.Width;
									rc.bottom	= desc.Height;
//									MessageBox( NULL , ofn.lpstrFile , strFileName , 0 );
//									_tprintd( TEXT("file %s\n") , strFileName );
//									_tprintd( TEXT("rc %d , %d , %d , %d\n") , rc.left , rc.top , rc.right , rc.bottom );
//									if( D3DXSaveTextureToFile( ofn.lpstrFile , D3DXIFF_PNG , pTex , NULL ) ){
									if( D3DXSaveSurfaceToFile( ofn.lpstrFile , D3DXIFF_PNG , pSurSave , NULL , &rc ) ){
										bError = true;
									}
								} else bError = true;
								
								if( bError ){
									TCHAR buf[256];
									LoadString( (HINSTANCE)GetWindowLongPtr( hwnd , GWLP_HINSTANCE ) , IDS_STRING_ERR_SCREENSHOT , buf , 256 );
									MessageBox( hwnd , buf , TEXT("ScreenShot Error") , MB_ICONERROR | MB_OK );
								}
							}// if( GetSaveFileName( &ofn ) ){
							
							SAFE_RELEASE( pSurSave );
							SAFE_RELEASE( pTex );
						}
						break;
					
					default:
						break;
				}
			}
			break;
		
		case WM_RBUTTONUP:
			{ // ポップアップメニューを表示
				POINT pt; 
				pt.x = LOWORD(lParam);
				pt.y = HIWORD(lParam);
				HMENU hmenu = LoadMenu((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), MAKEINTRESOURCE(IDR_MENU_POPUP) );
				HMENU hSubmenu = GetSubMenu(hmenu, 0);
				// チェックマークを設定する
				CheckMenuItem( hSubmenu , ID_MENU_TOPWINDOW , (g_AppConfig.m_bTopWindow?MF_CHECKED:MF_UNCHECKED) );
				CheckMenuItem( hSubmenu , ID_MENU_DROP_ERRFRAME , (g_AppConfig.m_bDropFrame?MF_CHECKED:MF_UNCHECKED) );
				
				ClientToScreen(hwnd, &pt);
				TrackPopupMenu(hSubmenu, TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
				DestroyMenu(hmenu);
			}
			break;
		
		case WM_SIZE:
			{
				RECT rcC;
				_tprintd( TEXT("WM_SIZE start\n") );
				GetClientRect( hwnd , &rcC );
				long Cw , Ch;
				Cw = rcC.right - rcC.left;	Ch = rcC.bottom - rcC.top;
				_tprintd( TEXT("client %d %d\n") , Cw , Ch );
				
				if( g_pD3DDev ){
				if(g_mutex_Texture.lock(INFINITE) == false){
					D3DResQuit();
					D3DPRESENT_PARAMETERS d3dpp;
					D3DGetPParameters( &d3dpp , hwnd , Cw , Ch );
					g_pD3DDev->Reset( &d3dpp );
					
					D3DVIEWPORT9 vp;
					vp.X	= 0;
					vp.Y	= 0;
					vp.Width = d3dpp.BackBufferWidth;
					vp.Height = d3dpp.BackBufferHeight;
					vp.MinZ = 0.0f;
					vp.MaxZ = 0.0f;
					g_pD3DDev->SetViewport( &vp );
					
					D3DResInit( &g_pD3DDev );
					g_mutex_Texture.unlock();
				}
					//
				}

				_tprintd( TEXT("WM_SIZE end\n") );
			}
			break;
		
		case WM_PAINT:
			{
				BeginPaint( hwnd , &ps );
				EndPaint( hwnd , &ps );
			}
			break;
		
	}
	
//	_tprintd( TEXT("WindowProc end\n") );
//	ShowAppConfig();
	
	return DefWindowProc( hwnd, uMsg, wParam, lParam );
}

HWND InitWindow( HINSTANCE hInst , WNDPROC CallBack )
{
	WNDCLASS	WinClass;	//ウィンドウの形の登録
	
	//0で初期化
	ZeroMemory(&WinClass,sizeof(WNDCLASS));
	//ウィンドウ　クラスを設定
	WinClass.style			= CS_CLASSDC;								//ウインドウスタイルを設定 
	WinClass.lpfnWndProc	= (WNDPROC)CallBack;						//コールバックプロシージャへのポインタ
	WinClass.hInstance		= hInst;									//インスタンスハンドルを設定
	WinClass.hCursor		= LoadCursor(NULL, IDC_ARROW);				//カーソルの設定(Windows標準リソースを使用)
	WinClass.hbrBackground	= (HBRUSH)COLOR_WINDOW;						//ウインドウの背景を設定(デフォルトカラー)
	WinClass.lpszClassName	= TEXT("Programming Library");					//クラス名の設定
	WinClass.hIcon			= LoadIcon( hInst , MAKEINTRESOURCE(IDI_ICON1) );
	WinClass.lpszMenuName	= NULL;										//メニューの設定

	//ウィンドウクラスの登録
	if( RegisterClass( &WinClass ) == 0 )
	{
		MessageBox( NULL , TEXT("登録エラー") , TEXT("エラー") , 0 );
		return 0;
	}
	
	//メインウインドウの生成
	TCHAR buf[256];
	LoadString( hInst , IDS_STRING_TITLE , buf , 256 );
	return CreateWindowEx(
		WS_EX_CONTROLPARENT | WS_EX_WINDOWEDGE ,	//拡張ウインドウスタイル
		TEXT("Programming Library") ,				//登録されたクラス名のアドレス 
		buf ,										//ウインドウ名
		WS_OVERLAPPEDWINDOW ^ (WS_MAXIMIZEBOX | WS_THICKFRAME) ,		//ウインドウスタイルを設定(最大化ボタンとサイズの変更を無効にする)
		CW_USEDEFAULT ,								//X座標の位置設定
		CW_USEDEFAULT ,								//Y座標の位置設定
		512 ,										//横幅を設定
		512 ,										//縦幅を設定
		NULL ,										//親ウインドウを設定
		LoadMenu( hInst , MAKEINTRESOURCE(IDR_MENU_MAIN) ) ,										//メニューを設定
		hInst ,										//インスタンスを識別
		NULL );										//作成したウインドウに渡すデータへのポインタ
}

//! バッファからテクスチャーへ変換するスレッド
CBUFtoTEX	g_thBuftoTEX;

/*
	@fn void AppQuit( void )
	@brief アプリケーションの終了処理
*/
void AppQuit( void )
{
	g_thAviSave.Quit();
	
	g_thBuftoTEX.stopThread();
	g_mutex_Texture.destroy();
	D3DResQuit();
	D3DQuit( &g_pD3D , &g_pD3DDev );
	
	ConfigIOSave( &g_AppConfig , TEXT("config.xml") );
	
	CoUninitialize();
	//
}

/*!
	@brief メイン関数
*/
int WINAPI _tWinMain( HINSTANCE hInst , HINSTANCE hPrevInstance , LPTSTR lpszCmdLine , int nCmdShow )
{
	MSG			msg;		//メッセージ
	
	g_hInst = hInst;
	
	setlocale( LC_ALL, "Japanese" );
	
	HRESULT hr = CoInitialize(NULL);
	if (FAILED(hr)) {
		_tprintd( TEXT("CoInitialize Error\n") );
		return -1;
	}

	av_register_all ();
	avcodec_init();
	avcodec_register_all();
	
	memset( &g_AppConfig , 0 , sizeof(g_AppConfig) );
	g_AppConfig.m_ScrSel = ECAPSCR_DSCR;
	g_AppConfig.m_fScrScal = 1.0f;
	
	// config.xmlを開く
	ConfigIOLoad( &g_AppConfig , TEXT("config.xml") );
	
	// コマンドライン
	if( _tcslen(lpszCmdLine) ){
		LPTSTR pStr = NULL;
		
#if _MSC_VER >= 1400
		LPTSTR pStrNext = NULL;
		pStr = _tcstok_s( lpszCmdLine , TEXT(" ") , &pStrNext );
#else
		pStr = _tcstok( lpszCmdLine , TEXT(" "));
#endif
		while( pStr ){
//			MessageBox( NULL , pStr , pStr , MB_OK );
			
			// カメレオンusbのＩＤ
			if( _tcscmp( pStr , TEXT("--cusb2id") ) == 0 ){
#if _MSC_VER >= 1400
				pStr = _tcstok_s( NULL , TEXT(" ") , &pStrNext );
#else
				pStr = _tcstok( NULL , TEXT(" ") );
#endif
				g_AppConfig.m_iCUSB2_ID = _tstol(pStr);
				g_AppConfig.m_iCUSB2_ID = g_AppConfig.m_iCUSB2_ID;	// break point
			}
			
			// 画面の隙間
			if( _tcscmp( pStr , TEXT("--space") ) == 0 ){
#if _MSC_VER >= 1400
				pStr = _tcstok_s( NULL , TEXT(" ") , &pStrNext );
#else
				pStr = _tcstok( NULL , TEXT(" ") );
#endif
				g_AppConfig.m_ScrSpace = (unsigned char)_tstol(pStr);
				if( g_AppConfig.m_ScrSpace <= CSCRSPACE_MIN ) g_AppConfig.m_ScrSpace = CSCRSPACE_MIN;
				if( g_AppConfig.m_ScrSpace >= CSCRSPACE_MAX ) g_AppConfig.m_ScrSpace = CSCRSPACE_MAX;
				g_AppConfig.m_ScrSpace = g_AppConfig.m_ScrSpace;	// break point
			}
			
			// 画面の拡大率
			if( _tcscmp( pStr , TEXT("--scale") ) == 0 ){
#if _MSC_VER >= 1400
				pStr = _tcstok_s( NULL , TEXT(" ") , &pStrNext );
#else
				pStr = _tcstok( NULL , TEXT(" ") );
#endif
				g_AppConfig.m_fScrScal = (float)_tstof(pStr);
				if( g_AppConfig.m_fScrScal <= CSCRSCAL_MIN ) g_AppConfig.m_fScrScal = CSCRSCAL_MIN;
				if( g_AppConfig.m_fScrScal >= CSCRSCAL_MAX ) g_AppConfig.m_fScrScal = CSCRSCAL_MAX;
				g_AppConfig.m_fScrScal = g_AppConfig.m_fScrScal;	// break point
			}
			
			// ドロップフレーム
			if( _tcscmp( pStr , TEXT("--dropframe") ) == 0 ){
				g_AppConfig.m_bDropFrame = true;
				g_AppConfig.m_bDropFrame = g_AppConfig.m_bDropFrame;	// break point
			}
			
			// 常に手前に表示
			if( _tcscmp( pStr , TEXT("--topwindow") ) == 0 ){
				g_AppConfig.m_bTopWindow = true;
				g_AppConfig.m_bTopWindow = g_AppConfig.m_bTopWindow;	// break point
			}
			
			// 左回転
			if( _tcscmp( pStr , TEXT("--rotation-left") ) == 0 ){
				g_AppConfig.m_DirMode = 1;
				g_AppConfig.m_DirMode = g_AppConfig.m_DirMode;	// break point
			}
			
			// 右回転
			if( _tcscmp( pStr , TEXT("--rotation-right") ) == 0 ){
				g_AppConfig.m_DirMode = 2;
				g_AppConfig.m_DirMode = g_AppConfig.m_DirMode;	// break point
			}
			
			// 表示画面(top)
			if( _tcscmp( pStr , TEXT("--scrsel-top") ) == 0 ){
				g_AppConfig.m_ScrSel = ECAPSCR_TOP;
				g_AppConfig.m_ScrSel = g_AppConfig.m_ScrSel;	// break point
			}
			
			// 表示画面(bottom)
			if( _tcscmp( pStr , TEXT("--scrsel-bottom") ) == 0 ){
				g_AppConfig.m_ScrSel = ECAPSCR_BTM;
				g_AppConfig.m_ScrSel = g_AppConfig.m_ScrSel;	// break point
			}
			
			// 表示画面(2 screen)
			if( _tcscmp( pStr , TEXT("--scrsel-double") ) == 0 ){
				g_AppConfig.m_ScrSel = ECAPSCR_DSCR;
				g_AppConfig.m_ScrSel = g_AppConfig.m_ScrSel;	// break point
			}
			
#if _MSC_VER >= 1400
				pStr = _tcstok_s( NULL , TEXT(" ") , &pStrNext );
#else
				pStr = _tcstok( NULL , TEXT(" ") );
#endif
		}
	}
	int debugcount = 0;
	RECT rc = { 0 , 0 , NDS_SCREEN_W , NDS_SCREEN2_H };
	GetClientSizeForAppconfig( &g_AppConfig , &rc );
//	ShowAppConfig();
	
	
	g_hWnd = InitWindow( hInst , WindowProc );
	
	// usb start
#if defined(NDEBUG) // 安全装置
//	g_AppConfig.m_iCUSB2_ID = 0;
#endif
	if( g_nise.NisetroInit( ECAPFPS_60 , ECAPSCR_DSCR , g_AppConfig.m_iCUSB2_ID , g_hWnd ) ){
//		g_nise.NisetroQuit();
/*		TCHAR buf[256];
		LoadString( g_hInst , IDS_STRING_ERR_USB_CONNECT , buf , 256 );
		MessageBox( NULL , buf , TEXT("USB Error") , MB_ICONWARNING | MB_OK );*/
	}
	
	g_nise.SetDropFrame(g_AppConfig.m_bDropFrame);
	g_mutex_Texture.create();
	
	g_thBuftoTEX.startThread();
	
	D3DInit( &g_pD3D , &g_pD3DDev , g_hWnd , rc.right , rc.bottom );
	D3DResInit( &g_pD3DDev );
	
	//ウィンドウの表示
	ShowWindow( g_hWnd, nCmdShow );
	UpdateWindow( g_hWnd );
	D3DUpdateScreenConfig( &g_AppConfig , g_hWnd );
	
	HACCEL hAccel = LoadAccelerators( hInst,  MAKEINTRESOURCE(IDR_ACCELERATOR1) );
	
	g_thRender.startThread();
	
	//ループ開始
	while( gDone == false ){
		//コールバック
		if(PeekMessage( &msg , NULL , 0 , 0 , PM_REMOVE ))
		{
			if (!TranslateAccelerator(msg.hwnd, hAccel, &msg)) {
            if( msg.message == WM_QUIT ){
				gDone = true;
			}
			
			TranslateMessage(&msg); 
			DispatchMessage(&msg);
			}
		}
		
		//
		
	}
	
	g_thRender.stopThread();
	
	g_nise.NisetroQuit();
	
	AppQuit();
	return ((int)msg.wParam);
}

