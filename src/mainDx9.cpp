
/*!
	@file
	@bug	2台同時取り込みで画像が乱れる(2010/02/21)
*/

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
#include <string>
#include "dx9render.h"

#include "cthread.h"
#include "cmutex.h"

#include "nisetro.h"
#include "main.h"

#include "_tprintd.h"

#if defined(HAVE_FFMPEG)
#include "cavisave.h"
#endif

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

HINSTANCE	g_hInst;
HWND		g_hWnd;		//ウィンドウハンドル
HWND		g_hConfigDlg = NULL;		//ウィンドウハンドル
HWND		g_hInfoDlg = NULL;		//ウィンドウハンドル
HWND		g_hRecordDlg = NULL;		//ウィンドウハンドル

//! app config
SAPPCONFIG g_AppConfig;

LPDIRECT3D9	g_pD3D = NULL;
LPDIRECT3DDEVICE9 g_pD3DDev = NULL;

//! Topテクスチャ
LPDIRECT3DTEXTURE9 g_pTexTop = NULL;

//! Bottomテクスチャ
LPDIRECT3DTEXTURE9 g_pTexBtm = NULL;

LPD3DXSPRITE	g_pD3DXSprite = NULL;
//LPD3DXBUFFER			g_pD3DXBufShader = NULL;
//LPDIRECT3DPIXELSHADER9	g_pD3DPShader = NULL;

D3DTEXTUREFILTERTYPE g_eFilter = D3DTEXF_LINEAR;
D3DTEXTUREFILTERTYPE g_eFilterMin = D3DTEXF_ANISOTROPIC;

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
	
/*	hr = D3DXAssembleShaderFromFile( TEXT("ps.psh"),NULL, 0, NULL, &g_pD3DXBufShader, NULL );
	if( hr ){
		_tprintd( TEXT("Error D3DXAssembleShaderFromFile\n") );
		return -1;
	}
	
	if( g_pD3DXBufShader ){
		hr = (*pD3DDev)->CreatePixelShader( (DWORD*)g_pD3DXBufShader->GetBufferPointer(), &g_pD3DPShader );
		if( hr ){
			_tprintd( TEXT("Error CreatePixelShader\n") );
			return -1;
		}
	}*/
	
	return 0;
}

/*!
	@brief	Direct3Dのリソースを開放する
*/
void D3DResQuit( void ){
/*	SAFE_RELEASE(g_pD3DPShader);
	SAFE_RELEASE(g_pD3DXBufShader);*/
	
	SAFE_RELEASE( g_pD3DXSprite );
	SAFE_RELEASE( g_pTexBtm );
	SAFE_RELEASE( g_pTexTop );
}

//! バッファ変換用スレッド
class CBUFtoTEX : public CTHREAD{
public:
	CBUFtoTEX(){
		this->setSleepTime( 1000 / 60 );
	}
	
	void setFps( long fps ){
		this->setSleepTime( 1000 / fps );
	}
	
	virtual unsigned int ThreadFunction( void ){
		if( !g_pTexTop || !g_pTexBtm ) return -1;
		
		DWORD dwTime1 = timeGetTime();
		BYTE ScrData[NDS_SCREEN2BUF_SIZE];
		if( g_nise.GetScrData( ScrData , NDS_SCREEN2BUF_SIZE , getSleepTime() ) )
			return -1;
		
		// mutexのロックを取得
		if( g_mutex_Texture.lock(getSleepTime()) == false ){
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

		DWORD dwTime2 = timeGetTime();
		DWORD dwWTime = this->getSleepTime() - (dwTime2 - dwTime1);
		if( dwWTime <= 0 ){
			static int c = 0;
			_tprintd( TEXT("%d %d 処理落ちの可能性 CBUFtoTEX\n") , c++ , dwWTime );
			dwWTime = 0;
		}
		
		return 0;
	}
};


#if defined(HAVE_FFMPEG)
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
			_tprintd( TEXT("%d %d 処理落ちの可能性\n") , c++ , dwWTime );
			dwWTime = 0;
		}
		this->setSleepTime( dwWTime );
		return 0;
	}
};
#endif


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
		m_dwMaxTime = 1000 / 60;
		this->setSleepTime( m_dwMaxTime );
	}
	
	void setFps( long fps ){
		m_dwMaxTime = 1000 / fps;
		this->setSleepTime( m_dwMaxTime );
	}
	
	virtual unsigned int ThreadFunction( void ){
		DWORD dwTime1 = timeGetTime();
		if( g_mutex_Texture.lock(m_dwMaxTime) == false ){
			ReanderScreen();
			m_dFPS = GetFPS();
			g_mutex_Texture.unlock();
		}
		
		DWORD dwTime2 = timeGetTime();
		DWORD dwWTime = m_dwMaxTime - (dwTime2 - dwTime1);
		if( dwWTime <= 0 ){
			static int c = 0;
			_tprintd( TEXT("%d %d 処理落ちの可能性 CRENDERTHREAD\n") , c++ , dwWTime );
			dwWTime = 0;
		}
		this->setSleepTime( dwWTime );
		return 0;
	}
};

//! バッファからテクスチャーへ変換するスレッド
CBUFtoTEX	g_thBuftoTEX;
CRENDERTHREAD g_thRender;

#if defined(HAVE_FFMPEG)
CAVISAVE	g_thAviSave;
#endif

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
			
			g_pD3DDev->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS , TRUE );
			g_pD3DDev->SetSamplerState( 0 , D3DSAMP_MAGFILTER , g_eFilter );
			g_pD3DDev->SetSamplerState( 0 , D3DSAMP_MINFILTER , g_eFilter );
			g_pD3DDev->SetSamplerState( 0 , D3DSAMP_MIPFILTER , g_eFilter );
			
//			if( g_pD3DPShader )g_pD3DDev->SetPixelShader( g_pD3DPShader );
			
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
			g_pD3DDev->SetSamplerState( 0 , D3DSAMP_MAGFILTER , g_eFilter );
			g_pD3DDev->SetSamplerState( 0 , D3DSAMP_MINFILTER , g_eFilter );
			g_pD3DDev->SetSamplerState( 0 , D3DSAMP_MIPFILTER , g_eFilter );
			
//			if( g_pD3DPShader )g_pD3DDev->SetPixelShader( g_pD3DPShader );
			
			g_pD3DXSprite->Draw( g_pTexBtm , &SrcRc , &vecCenter , &vecPos , 0xFFFFFFFF );
			g_pD3DXSprite->End();
		}
	}
	g_pD3DDev->EndScene();
	g_pD3DDev->Present( NULL , NULL , NULL , NULL );
}


#include "dlgproc_config.h"
#include "dlgproc_info.h"
#if defined(HAVE_FFMPEG)
#include "dlgproc_record.h"
#endif

//! 描画ウィンドウ用のコールバック
LRESULT CALLBACK WindowProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	PAINTSTRUCT ps;
	
	switch( uMsg )
	{
		case WM_CREATE:
			{
				HMENU hMenu = GetMenu( hwnd );
				if( hMenu ){
					CheckMenuItem( hMenu , ID_MENU_TOPWINDOW , (g_AppConfig.m_bTopWindow?MF_CHECKED:MF_UNCHECKED) );
					CheckMenuItem( hMenu , ID_MENU_DROP_ERRFRAME , (g_AppConfig.m_bDropFrame?MF_CHECKED:MF_UNCHECKED) );
#if !defined(HAVE_FFMPEG)
					// 録画ボタンを押せないようにする
					EnableMenuItem( hMenu , ID_MENU_REC , MF_DISABLED | MF_GRAYED );
#endif
				}
				if( g_AppConfig.m_bTopWindow ){
					g_AppConfig.m_bTopWindow = false;
					SendMessage( hwnd , WM_COMMAND , MAKEWPARAM(ID_MENU_TOPWINDOW,0) , 0 );
				}
				SetTimer( hwnd , 1421356 , (1000 / 2) , NULL );
				//
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
				_stprintf_s( strBuf , 256 , TEXT("%s ID:%d") , strTitle , g_AppConfig.m_iCUSB2_ID );
#else
				_stprintf( strBuf , TEXT("%s ID:%d") , strTitle , g_AppConfig.m_iCUSB2_ID );
#endif
				SetWindowText( g_hWnd , strBuf );
			}
			break;
		
		case WM_CLOSE:
#if defined(HAVE_FFMPEG)
			if( g_thAviSave.IsRec() == false ){
#endif
				if( g_hRecordDlg ){
					DestroyWindow( g_hRecordDlg );
					g_hRecordDlg = NULL;
				}
				PostQuitMessage(0);
#if defined(HAVE_FFMPEG)
			}else{
				TCHAR strBuf[256];
				LoadString( (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE) , IDS_STRING_WAR_EXIT_REC , strBuf , 256 );
				MessageBox( hwnd , strBuf , TEXT("Warning") , MB_ICONWARNING );
				return FALSE;
			}
#endif
			break;
		
		// デバイスロスト等
		case WM_DEVICECHANGE:
			{
				HMENU hMenu;
				_tprintd( TEXT("WM_DEVICECHANGE\n") );
				
				
				PDEV_BROADCAST_HDR lpdb = (PDEV_BROADCAST_HDR)lParam;
				lpdb = lpdb;
				if( wParam == DBT_CONFIGCHANGECANCELED )	_tprintd( TEXT(" DBT_CONFIGCHANGECANCELED\n") );
				if( wParam == DBT_CONFIGCHANGED )			_tprintd( TEXT(" DBT_CONFIGCHANGED\n") );
				if( wParam == DBT_CUSTOMEVENT )				_tprintd( TEXT(" DBT_CUSTOMEVENT\n") );
				if( wParam == DBT_DEVICEARRIVAL )			_tprintd( TEXT(" DBT_DEVICEARRIVAL\n") );
				if( wParam == DBT_DEVICEQUERYREMOVE )		_tprintd( TEXT(" DBT_DEVICEQUERYREMOVE\n") );
				if( wParam == DBT_DEVICEQUERYREMOVEFAILED )	_tprintd( TEXT(" DBT_DEVICEQUERYREMOVEFAILED\n") );
				if( wParam == DBT_DEVICEREMOVECOMPLETE )	_tprintd( TEXT(" DBT_DEVICEREMOVECOMPLETE\n") );
				if( wParam == DBT_DEVICEREMOVEPENDING )		_tprintd( TEXT(" DBT_DEVICEREMOVEPENDING\n") );
				if( wParam == DBT_DEVICETYPESPECIFIC )		_tprintd( TEXT(" DBT_DEVICETYPESPECIFIC\n") );
				if( wParam == DBT_DEVNODES_CHANGED )		_tprintd( TEXT(" DBT_DEVNODES_CHANGED\n") );
				if( wParam == DBT_QUERYCHANGECONFIG )		_tprintd( TEXT(" DBT_QUERYCHANGECONFIG\n") );
				if( wParam == DBT_USERDEFINED )				_tprintd( TEXT(" DBT_USERDEFINED\n") );
				
				cusb2* pUsb = g_nise.GetCUSB2();
				if( g_nise.IsInit() ){
					if( pUsb )
					if( pUsb->PnpEvent( wParam , lParam) ){
						_tprintd( TEXT("PnpEvent\n") );
						
						
						hMenu = GetMenu( hwnd );
						for( int i = 0; i < 4; i++ ){
							if( hMenu )EnableMenuItem( hMenu , (ID_MENU_CUSB2_ID0+i) , MF_GRAYED );
						}
						g_nise.NisetroQuit();
						if( g_nise.NisetroInit( g_AppConfig.m_eFrmSkip , g_AppConfig.m_ScrSel , g_AppConfig.m_iCUSB2_ID , hwnd ) ){
							// error
							g_nise.NisetroQuit();
						}
						
						hMenu = GetMenu( hwnd );
						for( int i = 0; i < 4; i++ ){
							if( hMenu )EnableMenuItem( hMenu , (ID_MENU_CUSB2_ID0+i) , MF_ENABLED );
						}
						
					}
				}else{
						g_nise.NisetroQuit();
						if( g_nise.NisetroInit( g_AppConfig.m_eFrmSkip , g_AppConfig.m_ScrSel , g_AppConfig.m_iCUSB2_ID , hwnd ) ){
							g_nise.NisetroQuit();
						}
						
						hMenu = GetMenu( hwnd );
						for( int i = 0; i < 4; i++ ){
							if( hMenu )EnableMenuItem( hMenu , (ID_MENU_CUSB2_ID0+i) , MF_ENABLED );
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
#if defined(HAVE_FFMPEG)
							if( g_hRecordDlg )break;
							g_hRecordDlg = CreateDialog( g_hInst , MAKEINTRESOURCE(IDD_DLG_RECORD) , hwnd , (DLGPROC)RecordDlgProc );
							ShowWindow(g_hRecordDlg, SW_SHOW);
							UpdateWindow(g_hRecordDlg); 
#endif
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
					
					case ID_MENU_INFO:
						{
							if( g_hInfoDlg )break;
							g_hInfoDlg = CreateDialog( g_hInst , MAKEINTRESOURCE(IDD_DLG_INFO) , hwnd , (DLGPROC)InfoDlgProc );
							ShowWindow(g_hInfoDlg, SW_SHOW);
							UpdateWindow(g_hInfoDlg); 
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
							g_nise.NisetroQuit();
							g_nise.NisetroInit( g_AppConfig.m_eFrmSkip , g_AppConfig.m_ScrSel , g_AppConfig.m_iCUSB2_ID , g_hWnd );
							if( g_hConfigDlg )SendMessage( GetDlgItem(g_hConfigDlg, IDC_COMBO_SCRSEL) , CB_SETCURSEL , (id-ID_MENU_SRCSEL_TOP) , 0 );
						}
						break;
					
					case ID_MENU_60FPS:
					case ID_MENU_30FPS:
					case ID_MENU_20FPS:
					case ID_MENU_15FPS:
						{
							long fps = 0;
							if( id == ID_MENU_60FPS ){	g_AppConfig.m_eFrmSkip = ECAPFPS_60; fps = 60;}
							if( id == ID_MENU_30FPS ){	g_AppConfig.m_eFrmSkip = ECAPFPS_30; fps = 30;}
							if( id == ID_MENU_20FPS ){	g_AppConfig.m_eFrmSkip = ECAPFPS_20; fps = 20;}
							if( id == ID_MENU_15FPS ){	g_AppConfig.m_eFrmSkip = ECAPFPS_15; fps = 15;}
							if( g_hConfigDlg ){
								WPARAM send = (id-ID_MENU_60FPS);
								SendMessage( GetDlgItem(g_hConfigDlg, IDC_COMBO_FPS) , CB_SETCURSEL , send , 0 );
							}
							g_thRender.setFps( fps );
							g_thBuftoTEX.setFps( fps );
							g_nise.NisetroQuit();
							g_nise.NisetroInit( g_AppConfig.m_eFrmSkip , g_AppConfig.m_ScrSel , g_AppConfig.m_iCUSB2_ID , g_hWnd );
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
						
					case ID_MENU_CUSB2_ID0:
					case ID_MENU_CUSB2_ID1:
					case ID_MENU_CUSB2_ID2:
					case ID_MENU_CUSB2_ID3:
					case ID_MENU_CUSB2_ID4:
					case ID_MENU_CUSB2_ID5:
					case ID_MENU_CUSB2_ID6:
					case ID_MENU_CUSB2_ID7:
					case ID_MENU_CUSB2_ID8:
					case ID_MENU_CUSB2_ID9:
						{
							int cusb2id = id - ID_MENU_CUSB2_ID0;
							if( cusb2id >= 0 && cusb2id <= 9 ){
								g_AppConfig.m_iCUSB2_ID = cusb2id;
								g_nise.NisetroQuit();
								g_nise.NisetroInit( g_AppConfig.m_eFrmSkip , g_AppConfig.m_ScrSel , g_AppConfig.m_iCUSB2_ID , hwnd );
							}
						}
						break;
						
					case ID_MENU_D3DFILTER_NONE:
					case ID_MENU_D3DFILTER_POINT:
					case ID_MENU_D3DFILTER_LINEAR:
					case ID_MENU_D3DFILTER_ANISOTROPIC:
					case ID_MENU_D3DFILTER_PYRAMIDALQUAD:
					case ID_MENU_D3DFILTER_GAUSSIANQUAD:
						{
							if( id == ID_MENU_D3DFILTER_NONE )			g_eFilter = D3DTEXF_NONE;
							if( id == ID_MENU_D3DFILTER_POINT )			g_eFilter = D3DTEXF_POINT;
							if( id == ID_MENU_D3DFILTER_LINEAR )		g_eFilter = D3DTEXF_LINEAR;
							if( id == ID_MENU_D3DFILTER_ANISOTROPIC )	g_eFilter = D3DTEXF_ANISOTROPIC;
							if( id == ID_MENU_D3DFILTER_PYRAMIDALQUAD )	g_eFilter = D3DTEXF_PYRAMIDALQUAD;
							if( id == ID_MENU_D3DFILTER_GAUSSIANQUAD )	g_eFilter = D3DTEXF_GAUSSIANQUAD;
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

/*
	@fn void AppQuit( void )
	@brief アプリケーションの終了処理
*/
void AppQuit( void )
{
#if defined(HAVE_FFMPEG)
	g_thAviSave.Quit();
#endif
	
	g_thBuftoTEX.stopThread();
	g_mutex_Texture.destroy();
	D3DResQuit();
	D3DQuit( &g_pD3D , &g_pD3DDev );
	
	ConfigIOSave( &g_AppConfig , L"config.xml" );
	
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

	// ffmpegの前準備
#if defined(HAVE_FFMPEG)
	av_register_all();
	avcodec_init();
	avcodec_register_all();
#endif
	
	memset( &g_AppConfig , 0 , sizeof(g_AppConfig) );
	g_AppConfig.m_ScrSel = ECAPSCR_DSCR;
	g_AppConfig.m_fScrScal = 1.0f;
	
	// config.xmlから設定を読み込む
	ConfigIOLoad( &g_AppConfig , L"config.xml" );
	
	// コマンドラインの処理
	if( _tcslen(lpszCmdLine) )
		ConfigIOCheckCmdLine( &g_AppConfig , lpszCmdLine );
	
	// 設定からスクリーンサイズを取得する
	RECT rc = { 0 , 0 , NDS_SCREEN_W , NDS_SCREEN2_H };
	GetClientSizeForAppconfig( &g_AppConfig , &rc );
	
	// ウィンドウを作成
	g_hWnd = InitWindow( hInst , WindowProc );
	
	{
		long fps = 60;
		if( g_AppConfig.m_eFrmSkip == ECAPFPS_60){ fps = 60;}
		if( g_AppConfig.m_eFrmSkip == ECAPFPS_30){ fps = 30;}
		if( g_AppConfig.m_eFrmSkip == ECAPFPS_20){ fps = 20;}
		if( g_AppConfig.m_eFrmSkip == ECAPFPS_15){ fps = 15;}
		
		g_thRender.setFps( fps );
		g_thBuftoTEX.setFps( fps );
	}
	
	// usb start
	if( g_nise.NisetroInit( g_AppConfig.m_eFrmSkip , g_AppConfig.m_ScrSel , g_AppConfig.m_iCUSB2_ID , g_hWnd ) )
		g_nise.NisetroQuit();
	
	// エラーフレームの対処を設定
	g_nise.SetDropFrame(g_AppConfig.m_bDropFrame);
	
	g_mutex_Texture.create();
	g_thBuftoTEX.startThread();
	
	D3DInit( &g_pD3D , &g_pD3DDev , g_hWnd , rc.right , rc.bottom );
	D3DResInit( &g_pD3DDev );
	
	//ウィンドウの表示
	ShowWindow( g_hWnd, nCmdShow );
	UpdateWindow( g_hWnd );
	
	// ウィンドウサイズを更新
	D3DUpdateScreenConfig( &g_AppConfig , g_hWnd );
	
	// ショートカットの設定を取得
	HACCEL hAccel = LoadAccelerators( hInst,  MAKEINTRESOURCE(IDR_ACCELERATOR1) );
	
	// 描画スレッドの開始
	g_thRender.startThread();
	
	//ループ開始
	while (GetMessage(&msg, NULL, 0, 0)) { 
		if (!TranslateAccelerator(msg.hwnd, hAccel, &msg)) {
			TranslateMessage(&msg); 
			DispatchMessage(&msg);
		}
	}
	
	g_thRender.stopThread();
	
	g_nise.NisetroQuit();
	
	AppQuit();
	return ((int)msg.wParam);
}

