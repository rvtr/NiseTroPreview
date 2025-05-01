
//! 録画ダイアログ用のコールバック
BOOL CALLBACK RecordDlgProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp) {
	switch (msg) {
		// 初期化
		case WM_INITDIALOG:
			{
				// WaveInデバイスを列挙する
				unsigned int c = waveInGetNumDevs();
				for( unsigned int i = 0; i < c; i++ ){
					WAVEINCAPS wCaps;
					waveInGetDevCaps( (UINT_PTR)i , &wCaps , sizeof(WAVEINCAPS) );
					SendMessage(GetDlgItem(hDlgWnd, IDC_COMBO_WAVEINDEV), CB_ADDSTRING, 0, (LPARAM)wCaps.szPname );
				}
				SendMessage(GetDlgItem(hDlgWnd, IDC_COMBO_WAVEINDEV), CB_ADDSTRING, 0, (LPARAM)TEXT("WAVE_MAPPER"));
				SendMessage(GetDlgItem(hDlgWnd, IDC_COMBO_WAVEINDEV), CB_SETCURSEL, c, 0 );
				
				// ファイル名を設定する
				TCHAR buf[256];
				LoadString( g_hInst , IDS_STRING_DEF_CAPNAME , buf , 256 );
				SetWindowText( GetDlgItem(hDlgWnd, IDC_EDIT_FILENAME) , buf );
				
//				SetWindowText( GetDlgItem(hDlgWnd, IDC_EDIT_TIME) , TEXT("00 : 00 : 00 . 00") );
				SetWindowText( GetDlgItem(hDlgWnd, IDC_EDIT_TIME) , TEXT("NULL") );
			}
			return TRUE;
		
		case WM_COMMAND:
			switch (LOWORD(wp)) {
				case IDC_BTN_RECORD:
					{
						HMENU hMenu = GetMenu( g_hWnd );
						EnableWindow( GetDlgItem(hDlgWnd, IDC_BTN_RECORD) , FALSE );
						if( g_thAviSave.IsRec() ){
							g_thAviSave.Stop();
							g_thAviSave.Quit();
							//CheckMenuItem( hMenu , ID_MENU_REC , (g_nise.pRecfile?MF_CHECKED:MF_UNCHECKED) );
							SetWindowText(GetDlgItem(hDlgWnd, IDC_BTN_RECORD) , TEXT("録画") );
						}else{
							TCHAR strMovieName[MAX_PATH];
/*							TCHAR strFilenameCommon[MAX_PATH];
							TCHAR strAVIName[MAX_PATH];
							TCHAR strWaveName[MAX_PATH];*/
							
							UINT uID = WAVE_MAPPER;
							UINT uIDMax = waveInGetNumDevs();
							
							GetWindowText( GetDlgItem(hDlgWnd, IDC_EDIT_FILENAME) , strMovieName , MAX_PATH );
/*	#if _MSC_VER >= 1400
							_stprintf_s( strAVIName , MAX_PATH , TEXT("%s.avi") , strFilenameCommon );
							_stprintf_s( strWaveName , MAX_PATH , TEXT("%s.wav") , strFilenameCommon );
	#else
							_stprintf( strAVIName , TEXT("%s.avi") , strFilenameCommon );
							_stprintf( strWaveName , TEXT("%s.wav") , strFilenameCommon );
	#endif*/
							
							uID = (UINT)SendMessage(GetDlgItem(hDlgWnd, IDC_COMBO_WAVEINDEV), CB_GETCURSEL, 0, 0 );
							if( uID >= uIDMax )
								uID = WAVE_MAPPER;
							
							g_thAviSave.Init( strMovieName , MKTAG('X', 'V', 'I', 'D') , NDS_SCREEN_W , NDS_SCREEN2_H , 24 , uID );
							if( g_thAviSave.IsInit() ){
								g_thAviSave.Start();
								SetWindowText(GetDlgItem(hDlgWnd, IDC_BTN_RECORD) , TEXT("停止") );
							}else{
								_tprintd( TEXT("error\n") );
							}
	//						SendMessage( hDlgWnd , WM_CLOSE , 0 , 0 );
						}
						
						//
						EnableWindow( GetDlgItem(hDlgWnd, IDC_BTN_RECORD) , TRUE );
						EnableWindow( GetDlgItem(hDlgWnd, IDC_COMBO_WAVEINDEV) , (!g_thAviSave.IsRec()?TRUE:FALSE) );
						EnableWindow( GetDlgItem(hDlgWnd, IDC_BTN_UPDATE) , (!g_thAviSave.IsRec()?TRUE:FALSE) );
						EnableWindow( GetDlgItem(hDlgWnd, IDC_BTN_FILESEL) , (!g_thAviSave.IsRec()?TRUE:FALSE) );
						EnableWindow( GetDlgItem(hDlgWnd, IDCLOSE) , (!g_thAviSave.IsRec()?TRUE:FALSE) );
						
						EnableMenuItem( hMenu , ID_MENU_REC , (g_thAviSave.IsRec()?MF_GRAYED:MF_UNCHECKED) );
						
					}
					return TRUE;
				
				case IDC_BTN_UPDATE:
					{
						// 一覧を削除する
						while(SendMessage(GetDlgItem(hDlgWnd, IDC_COMBO_WAVEINDEV), CB_GETCOUNT, 0, 0) != 0)
							SendMessage(GetDlgItem(hDlgWnd, IDC_COMBO_WAVEINDEV), CB_DELETESTRING, 0, 0);
						
						// WaveInデバイスを列挙する
						unsigned int c = waveInGetNumDevs();
						for( unsigned int i = 0; i < c; i++ ){
							WAVEINCAPS wCaps;
							waveInGetDevCaps( (UINT_PTR)i , &wCaps , sizeof(WAVEINCAPS) );
							SendMessage(GetDlgItem(hDlgWnd, IDC_COMBO_WAVEINDEV), CB_ADDSTRING, 0, (LPARAM)wCaps.szPname );
						}
						SendMessage(GetDlgItem(hDlgWnd, IDC_COMBO_WAVEINDEV), CB_ADDSTRING, 0, (LPARAM)TEXT("WAVE_MAPPER"));
						SendMessage(GetDlgItem(hDlgWnd, IDC_COMBO_WAVEINDEV), CB_SETCURSEL, c, 0 );
						
					}
					return TRUE;
				
				case IDC_BTN_FILESEL:
					{
						OPENFILENAME ofn;
						TCHAR strFilename[MAX_PATH];
						
						memset( &ofn , 0 , sizeof(OPENFILENAME) );
						memset( strFilename , 0 , sizeof(TCHAR) * MAX_PATH );
						
						ofn.lStructSize = sizeof(OPENFILENAME);
						ofn.hwndOwner	= hDlgWnd;
						ofn.lpstrFile	= strFilename;
						ofn.nMaxFile	= MAX_PATH;
						ofn.lpstrFilter	= TEXT("*.avi\0*.avi\0\0");
						ofn.lpstrDefExt	= TEXT("avi\0\0");
						ofn.hInstance	= (HINSTANCE)GetWindowLongPtr( hDlgWnd , GWLP_HINSTANCE );
						ofn.Flags		= OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_OVERWRITEPROMPT;
						
						if( GetSaveFileName(&ofn) != 0 ){
							SetWindowText( GetDlgItem(hDlgWnd, IDC_EDIT_FILENAME) , strFilename );
						}
					}
					return TRUE;
				
				case IDCLOSE:
						SendMessage( hDlgWnd , WM_CLOSE , 0 , 0 );
					return TRUE;
				default:
					return FALSE;
			}
		
		case WM_CLOSE:
			
			if( g_thAviSave.IsRec() ){
				g_thAviSave.Stop();
				g_thAviSave.Quit();
			}
			if( g_hRecordDlg ){
				DestroyWindow(g_hRecordDlg);
				g_hRecordDlg = NULL;
			}
//			EndDialog(hDlgWnd, IDCANCEL);
			break;
		
		default:
			return FALSE;
	}
	return FALSE;
}

