
#include <stdio.h>
#include <tchar.h>
#include <windows.h>

#include "cusb2.h"
#include "cmutex.h"

#ifndef __NISETROINFO_H__
#define __NISETROINFO_H__
// CUSB2 に送るコマンドらしい
#define CMD_EP6IN_START		0x50	//
#define	CMD_EP6IN_STOP		0x51	//
#define	CMD_EP2OUT_START	0x52	//
#define	CMD_EP2OUT_STOP		0x53	//
#define	CMD_PORT_CFG		0x54	//addr_mask, out_pins
#define	CMD_REG_READ		0x55	//addr	(return 1byte)
#define	CMD_REG_WRITE		0x56	//addr, value
#define	CMD_PORT_READ		0x57	//(return 1byte)
#define	CMD_PORT_WRITE		0x58	//value
#define	CMD_IFCONFIG		0x59	//value
#define	CMD_MODE_IDLE		0x5a

#define BUF_LEN	(1024*64)	//USB転送に使用するバッファサイズ
#define	QUE_NUM	(128)		//USB転送に使用するバッファの個数

#define PIO_DROP	0x20
#define	PIO_RESET	0x10
#define PIO_DIR		0x08

//MAX2に渡すキャプチャモード定数

/*!
	@brief キャプチャーFPSの定数
*/
enum ECAPFPS{
	//! 60fps
	ECAPFPS_60	= 0x00,
	
	//! 30fps
	ECAPFPS_30	= 0x01,
	
	//! 20fps
	ECAPFPS_20	= 0x02,
	
	//! 15fps
	ECAPFPS_15	= 0x03,
	//
};
/*
#define FPS60 0x00
#define FPS30 0x01
#define FPS20 0x02
#define FPS15 0x03*/

/*!
	@brief キャプチャースクリーンの定数
*/
enum ECAPSCR{
	//! 上画面のみ
	ECAPSCR_TOP		= 0x00,
	
	//! 下画面のみ
	ECAPSCR_BTM		= 0x04,
	
	//! ２画面
	ECAPSCR_DSCR	= 0x08,
};
/*
#define CAPTOP    0x00
#define CAPBTM    0x04
#define CAPTOPBTM 0x08
*/

//! バージョン(勝手にバージョン付け)
enum ECAPDATAVER {
	ECAPDATAVER_UNKNOWN,
	ECAPDATAVER_FIRST,	// 2010/1/12以前
	ECAPDATAVER_60FPS,	// 2010/1/12以降
};

// 1画面分
const int NDS_SCREEN_W = 256;
const int NDS_SCREEN_H = 192;
const int NDS_SCREENBUF_SIZE = NDS_SCREEN_W * NDS_SCREEN_H * 3;

// 2画面分
const int NDS_SCREEN2_H = NDS_SCREEN_H * 2;
const int NDS_SCREEN2BUF_SIZE = NDS_SCREENBUF_SIZE * 2;

//! キャプチャー用バッファ(18Bit)
const int			CCAPBUFSIZE = NDS_SCREEN2BUF_SIZE * 2; // オーバーライトに備えて多めに確保する

/*!
	@brief	偽トロからのデータ取得用クラス
*/
class CNISETRO{
public:
	cusb2 *usb;
private:
	int m_cusb2id;
	cusb2_tcb *tcb1;	// スレッド

	LONG cmd_len,ret_len;
	u8	cmd[64];
	u8  ret[64];
	CCyUSBEndPoint *inep;
	CCyUSBEndPoint *outep;

	bool top_cut;	//先頭64Kbyteをカットするフラグ

	bool usb_init;
	
	//! データのバージョン情報
	ECAPDATAVER			m_eVer;
	
	//! エラーフレームのカウント
	unsigned long		m_ulErrFrm;
	
	//! 画面データの更新数
	double				m_dFps;
	
	FILE* pRecfile;
	
	typedef bool (*nise_func)( u8* ,u32 , _cusb2_tcb* );
	
	//! ドロップフラグ
	bool				m_bDropFrame;
	
	//! キャプチャー用カーソル(1)
	unsigned long		m_CapBufCur1;
	
	//! キャプチャー用カーソル(2)
	unsigned long		m_CapBufCur2;
	
	//! キャプチャーバッファ
	BYTE				m_CaptureBuffer[CCAPBUFSIZE];

	CMUTEX				m_mutex;
	
	//! 描画用バッファ(18Bit)
	BYTE				m_ScrData[NDS_SCREEN2BUF_SIZE];
	
	/*!
		@brief	FPSを取得する
		@return FPSが double で返される
		@note	※毎フレーム呼び出す必要がある
	*/
	double checkFPS( void );
	
	static bool getdata_from_cusb2_func(u8 *buf, u32 len , _cusb2_tcb* tcb);
	
	//! 以前のバージョン
	void func_first( u8* buf , u32 len );
	
	//! 60fps対応版
	void func_60fps( u8* buf , u32 len );
	
	//! パラメータの初期化
	void param_init( void ){
		cmd_len=0;
		ret_len=0;
		top_cut = false;
//		inep = pUsb->get_endpoint(0x81);
//		outep = pUsb->get_endpoint(0x01);
		
		memset( &cmd[0] , 0 , sizeof(u8) * 64 );
		memset( &ret[0] , 0 , sizeof(u8) * 64 );
		
		m_bDropFrame = false;
		m_eVer = ECAPDATAVER_UNKNOWN;
		m_ulErrFrm = 0;
		m_dFps = 0;
		
		m_CapBufCur1 = 0;
		m_CapBufCur2 = 0;
		memset( m_CaptureBuffer , 0 , sizeof(BYTE) * CCAPBUFSIZE );
		memset( m_ScrData , 0xFF , sizeof(BYTE) * NDS_SCREEN2BUF_SIZE );
	}
	
public:
	CNISETRO(){
		usb_init = false;
		usb = NULL;
		pRecfile = NULL;
		m_mutex.create();
		param_init();
		//
	}
	
	~CNISETRO(){
		rec_stop();
		if( usb_init ){
			NisetroQuit();
		}
		m_mutex.destroy();
	}
	
	/*!
		@brief	初期化処理
		@param	eFps	[in] フレーム数
		@param	eScr	[in] 取得する画面
		@param	id		[in] カメレオンusbのID
	*/
	int NisetroInit( ECAPFPS eFps , ECAPSCR eScr , int id = 0 , HWND hWnd = NULL );
	
	//! 終了処理
	void NisetroQuit( void );
	
	/*!
		@brief	スクリーンデータを取得する
		@param	pScrData	[in] 受け取るバッファへのポインタ
		@param	BufSize		[in] 受け取るバッファサイズ
		@param	dwWaitMS	[in] 受け取るまでの制限時間(ms)
	*/
	int GetScrData( BYTE* pScrData , size_t BufSize , DWORD dwWaitMS = INFINITE ){
		if( !(BufSize >= NDS_SCREEN2BUF_SIZE && pScrData) )
			return -1;
		
		if( m_mutex.lock(dwWaitMS) == false ){
#if _MSC_VER >= 1400
			memcpy_s( pScrData , BufSize , m_ScrData , NDS_SCREEN2BUF_SIZE );
#else
			memcpy( pScrData , m_ScrData , NDS_SCREEN2BUF_SIZE );
#endif
			m_mutex.unlock();
		}else
			return -1;
		return 0;
	}
	
	//! エラーフレームの設定を取得
	bool GetDropFrame( void ){
		return m_bDropFrame;
	}
	
	//! エラーフレームを設定する
	bool SetDropFrame( bool bDropFrame ){
		return (m_bDropFrame = bDropFrame);
	}
	
	//! 取得したエラーフレームの数を取得する
	int GetErrorFrameCount( void ){ return m_ulErrFrm; }
	
	//! データ更新頻度を取得する
	double GetFps( void ){ return m_dFps; }
	
	//! データのバージョンを取得
	ECAPDATAVER GetDataVersion( void ){ return m_eVer; }
	
	/*!
		@brief	フレームデータの保存を開始する
		@param	filename	[in] 保存するファイル名
	*/
	int rec_start( TCHAR* filename ){
		if( FILENAME_MAX == NULL ) return -1;
		
#if _MSC_VER >= 1400
		errno_t e = _tfopen_s( &pRecfile , filename , TEXT("wb") );
		if( e != 0 )
			return -1;
#else
		pRecfile = _tfopen( filename , TEXT("wb") );
		if( pRecfile == NULL )
			return -1;
#endif
		
		return 0;
	}
	
	//! フレームデータの保存を中止する
	void rec_stop( void ){
		if( pRecfile ){
			fclose( pRecfile );
			pRecfile = NULL;
		}
	}
	
	//! フレーム保存状態を取得する
	bool IsRec( void ){
		return (this->pRecfile != NULL);
	}
	
	//
};
#endif

