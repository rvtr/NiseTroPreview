
/*!
	@file
*/
#include <time.h>
#include "nisetro.h"
#include "_tprintd.h"

#ifdef DEBUG_FW
u8 fw_iic[1024*8];
#else
u8 fw_iic[]=
#include "fw.inc"
#endif

/*!
	@brief	FPSを取得する
	@return FPSが double で返される
	@note	※毎フレーム呼び出す必要がある
*/
double CNISETRO::checkFPS( void )
{
	static time_t	last = 0;
//	static DWORD	last = timeGetTime();
	static DWORD	frames = 0;
	static double	fps = 0;
	time_t			current;
	
	time( &current );
	frames++;
	
	if( current != last) {// 1秒毎に更新
		double dt = (double)(current - last);
		fps = (double)frames / dt;
		last = current;
		frames = 0;
	}
	return fps;
}

void CNISETRO::func_first( u8* buf , u32 len ){
	CNISETRO* pNise = this;
	
	for( u32 i = 0; i < len; i++ ){
		u32 cur = 0;
		u8 d = buf[i];
		u8 color = d & 0x3F;
//		bool bScr = ((d & 0x40) == 0x40);
		bool bVSync = ((d & 0x80) == 0x80);
		
		// なんか微妙
		if( (30*3) < pNise->m_CapBufCur1 && bVSync ){
			bool bError = false;
			if( pNise->m_CapBufCur1 < NDS_SCREEN2BUF_SIZE ){
				pNise->m_ulErrFrm++;
				bError = true;
			}
			
			// スクリーンデータに移す
			if( !(pNise->m_bDropFrame && bError) ){
				if( pNise->m_mutex.lock(INFINITE) == false){
					m_dFps = checkFPS();
#if _MSC_VER >= 1400
					errno_t e = memcpy_s( pNise->m_ScrData , NDS_SCREEN2BUF_SIZE , pNise->m_CaptureBuffer , NDS_SCREEN2BUF_SIZE );
#else
					memcpy( pNise->m_ScrData , pNise->m_CaptureBuffer , NDS_SCREEN2BUF_SIZE );
#endif
					pNise->m_mutex.unlock();
				}
			}
			
			pNise->m_CapBufCur1 = pNise->m_CapBufCur2 = 0;
		}
		
/*		// 書き込む画面を選択
		if( bScr == false )
			cur = pNise->m_CapBufCur1++;
		else
			cur = pNise->m_CapBufCur2++ + NDS_SCREENBUF_SIZE;*/
		cur = pNise->m_CapBufCur1++;
		
		pNise->m_CaptureBuffer[cur] = d;
		
		// バッファーオーバーを防ぐために
		if( pNise->m_CapBufCur1 > NDS_SCREEN2BUF_SIZE )			
			pNise->m_CapBufCur1 = pNise->m_CapBufCur2 = 0;
		//
	}
}

void CNISETRO::func_60fps( u8* buf , u32 len ){
	CNISETRO* pNise = this;
	
	for( u32 i = 0; i < len; i++ ){
		u32 cur = 0;
		u8 d = buf[i];
		u8 color = d & 0x3F;
		bool bScr = ((d & 0x40) == 0x40);
		bool bVSync = ((d & 0x80) == 0x80);
		
		// なんか微妙
		if( ((30/2)*3) < pNise->m_CapBufCur1 && bVSync ){
			bool bError = false;
			if( pNise->m_CapBufCur1 < NDS_SCREENBUF_SIZE ){
				pNise->m_ulErrFrm++;
				bError = true;
			}
			
			// スクリーンデータに移す
			if( !(pNise->m_bDropFrame && bError) ){
				if( pNise->m_mutex.lock(INFINITE) == false){
					m_dFps = checkFPS();
#if _MSC_VER >= 1400
					errno_t e = memcpy_s( pNise->m_ScrData , NDS_SCREEN2BUF_SIZE , pNise->m_CaptureBuffer , NDS_SCREEN2BUF_SIZE );
#else
					memcpy( pNise->m_ScrData , pNise->m_CaptureBuffer , NDS_SCREEN2BUF_SIZE );
#endif
					pNise->m_mutex.unlock();
				}
			}
			
			pNise->m_CapBufCur1 = pNise->m_CapBufCur2 = 0;
		}
		
		// 書き込む画面を選択
		if( bScr == false )
			cur = pNise->m_CapBufCur1++;
		else
			cur = pNise->m_CapBufCur2++ + NDS_SCREENBUF_SIZE;
		
		pNise->m_CaptureBuffer[cur] = d;
		
		// バッファーオーバーを防ぐために
		if( pNise->m_CapBufCur1 > NDS_SCREENBUF_SIZE )			
			pNise->m_CapBufCur1 = pNise->m_CapBufCur2 = 0;
		//
	}
}

//! USBデータ受信時に呼ばれる関数
bool CNISETRO::getdata_from_cusb2_func(u8 *buf, u32 len , _cusb2_tcb* tcb){
	CNISETRO* pNise = NULL;
	
	pNise = (CNISETRO*)tcb->userpointer;
	if( pNise == NULL ) return true;

	if( pNise->top_cut ){
		pNise->top_cut = false;
		return true;
	}
	
	if( pNise->IsRec() ){
		fwrite( buf , sizeof(u8) , len , pNise->pRecfile );
	}
	
	// バージョンチェック
	if( pNise->m_eVer == ECAPDATAVER_UNKNOWN ){
		if( len <= (NDS_SCREEN_W*3) )
			pNise->m_eVer = ECAPDATAVER_UNKNOWN;
		else{
			pNise->m_eVer = ECAPDATAVER_FIRST;
			// 画面フラグが立っているか確認
			for( u32 i = (len/2); i < len; i++ ){// 適当に初めの部分を飛ばす
				u32 cur = 0;
				u8 d = buf[i];
				u8 color = d & 0x3F;
				bool bScr = ((d & 0x40) == 0x40);
				bool bVSync = ((d & 0x80) == 0x80);
				if( bScr ){
					pNise->m_eVer = ECAPDATAVER_60FPS;
				}
			}
			
			
			_tprintd( TEXT("protocol %d\n") , pNise->m_eVer );
		}
	}

	switch( pNise->m_eVer ){
		case ECAPDATAVER_60FPS:
			pNise->func_60fps( buf , len );
			break;
		default:
			pNise->func_first( buf , len );
			break;
	}
	
	
	//終了条件のチェック
	if( tcb->looping == true ){
		// スレッドを終わらせる場合は false
		return true;
	}

	// 終了処理
	return false;
}

/*
	@brief	初期化処理
	@param	eFps	[in] フレーム数
	@param	eScr	[in] 取得する画面
	@param	id		[in] カメレオンusbのID
	@param	hWnd	[in] ウィンドウハンドル
*/
int CNISETRO::NisetroInit( ECAPFPS eFps , ECAPSCR eScr , int id , HWND hWnd ){
	if( usb_init )
		return -1;
	
	usb = new cusb2( hWnd );
	if( usb == NULL )
		return -1;
	
	if( usb->fwload((m_cusb2id = id)/* id */, fw_iic,(u8 *)"CFX2LOG") == false ){
		return -1;
	}
	
	inep = usb->get_endpoint(0x81);
	outep = usb->get_endpoint(0x01);
	
	memset( &cmd[0] , 0 , sizeof(u8) * 64 );
	memset( &ret[0] , 0 , sizeof(u8) * 64 );
	//FX2 I/Oポート設定　＆　MAX2初期パラメータ設定 ＆ MAX2リセット
	cmd[cmd_len++]=CMD_PORT_CFG;
	cmd[cmd_len++]=0x07;					//ADDR[2:0]
	cmd[cmd_len++]=PIO_RESET | PIO_DIR;		//RESET,DIR(out_pins)
	cmd[cmd_len++]=CMD_MODE_IDLE;
	cmd[cmd_len++]=CMD_IFCONFIG;
	cmd[cmd_len++]=0xE3;					//slave FIFO, IFCLK=48MHz, IFCLK出力,  同期(Sync)モード

	cmd[cmd_len++]=CMD_REG_WRITE;
	cmd[cmd_len++]=0;						//ADDR=0
	cmd[cmd_len] = 0;
	cmd[cmd_len] |= (u8)eFps;				// frame rate
	cmd[cmd_len] |= (u8)eScr;				// 2画面
	cmd_len++;
	cmd[cmd_len++]=CMD_MODE_IDLE;
	usb->xfer(outep, cmd, cmd_len);
	cmd_len=0;
//		time( &start_time );
	// スレッドの開始
	tcb1 = usb->start_thread(0x86, BUF_LEN, QUE_NUM, this->getdata_from_cusb2_func );
	tcb1->userpointer = (void*)this;

	cmd[cmd_len++]=CMD_PORT_WRITE;
	cmd[cmd_len++]=PIO_RESET;
	cmd[cmd_len++]=CMD_EP6IN_START;
	cmd[cmd_len++]=CMD_PORT_WRITE;
	cmd[cmd_len++]=0;
	usb->xfer(outep, cmd, cmd_len);
	cmd_len=0;
	usb_init = true;
	return 0;
}

// 終了処理
void CNISETRO::NisetroQuit( void ){
	if( usb_init == false ){
		if( usb ){
			delete usb;
			usb = NULL;
		}
		return;
	}
	
	tcb1->looping = false;
	usb->delete_thread(tcb1);
	
	cmd[cmd_len++]=CMD_PORT_WRITE;
	cmd[cmd_len++]=0;
	cmd[cmd_len++]=CMD_EP6IN_STOP;
	cmd[cmd_len++]=CMD_MODE_IDLE;
	usb->xfer(outep, cmd, cmd_len);
	cmd_len=0;
	
	cmd[cmd_len++]=CMD_REG_READ;
	cmd[cmd_len++]=0x00;			//all_cnt[7:0]
	cmd[cmd_len++]=CMD_REG_READ;
	cmd[cmd_len++]=0x01;			//all_cnt[15:8]
	cmd[cmd_len++]=CMD_REG_READ;
	cmd[cmd_len++]=0x02;			//all_cnt[23:16]
	cmd[cmd_len++]=CMD_REG_READ;
	cmd[cmd_len++]=0x03;			//all_cnt[31:24]
	cmd[cmd_len++]=CMD_REG_READ;
	cmd[cmd_len++]=0x04;			//err_cnt[7:0]
	cmd[cmd_len++]=CMD_REG_READ;
	cmd[cmd_len++]=0x05;			//err_cnt[15:8]
	cmd[cmd_len++]=CMD_REG_READ;
	cmd[cmd_len++]=0x06;			//err_cnt[23:16]
	cmd[cmd_len++]=CMD_REG_READ;
	cmd[cmd_len++]=0x07;			//err_cnt[31:24]
	if( outep )usb->xfer(outep, cmd, cmd_len);
	cmd_len=0;
	ret_len=8;
	inep = usb->get_endpoint(0x81);	// アドレスが変わっていることがあったので取得するようにした
	if( inep )usb->xfer(inep, ret, ret_len);
	
	delete usb;
	usb = NULL;
	
	usb_init = false;
	param_init();
}


