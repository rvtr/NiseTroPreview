
//! 設定ダイアログ用のコールバック
BOOL CALLBACK ConfigDlgProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp) {
	switch (msg) {
		// 初期化
		case WM_INITDIALOG:
			{
				TCHAR buf[256];
				
				// 画面
				for( unsigned int i = 0; i < 3; i++ ){
					LoadString( g_hInst , IDS_STRING_TOP+i , buf , 256);
					SendMessage(GetDlgItem(hDlgWnd, IDC_COMBO_SCRSEL), CB_ADDSTRING, 0, (LPARAM)buf);
				}
				SendMessage(GetDlgItem(hDlgWnd, IDC_COMBO_SCRSEL), CB_SETCURSEL, 0, 0 );
				
				// 更新速度
				for( unsigned int i = 0; i < 4; i++ ){
					LoadString( g_hInst , IDS_STRING_60FPS+i , buf , 256);
					SendMessage(GetDlgItem(hDlgWnd, IDC_COMBO_FPS), CB_ADDSTRING, 0, (LPARAM)buf);
				}
				SendMessage(GetDlgItem(hDlgWnd, IDC_COMBO_FPS), CB_SETCURSEL, 0, 0 );
			
				// 画面の向き
				for( unsigned int i = 0; i < 3; i++ ){
					LoadString( g_hInst , IDS_STRING_NORMAL+i , buf , 256);
					SendMessage(GetDlgItem(hDlgWnd, IDC_COMBO_SCRDIR), CB_ADDSTRING, 0, (LPARAM)buf);
				}
				SendMessage(GetDlgItem(hDlgWnd, IDC_COMBO_SCRDIR), CB_SETCURSEL, g_AppConfig.m_DirMode, 0 );
				
				// 隙間
				SendMessage(GetDlgItem(hDlgWnd, IDC_SLIDER_SCRSPACE), TBM_SETRANGE, (WPARAM)TRUE, (LPARAM)MAKELPARAM(CSCRSPACE_MIN, CSCRSPACE_MAX));
#if _MSC_VER >= 1400
				_stprintf_s( buf , 256 , TEXT("%d px") , g_AppConfig.m_ScrSpace );
#else
				_stprintf( buf , TEXT("%d px") , g_AppConfig.m_ScrSpace );
#endif
				SetWindowText( GetDlgItem(hDlgWnd, IDC_EDIT_SCRSPACE) , buf );
				SendMessage(GetDlgItem(hDlgWnd, IDC_SLIDER_SCRSPACE), TBM_SETPOS, (WPARAM)TRUE, g_AppConfig.m_ScrSpace);
				
				SendMessage(GetDlgItem(hDlgWnd, IDC_SLIDER_SCRSCALE), TBM_SETRANGE, (WPARAM)TRUE, (LPARAM)MAKELPARAM((CSCRSCAL_MIN*CSCRSCAL_SCALE), (CSCRSCAL_MAX*CSCRSCAL_SCALE)));
				SendMessage(GetDlgItem(hDlgWnd, IDC_SLIDER_SCRSCALE), TBM_SETPOS, (WPARAM)TRUE, (LPARAM)(g_AppConfig.m_fScrScal*CSCRSCAL_SCALE));
				SendMessage( hDlgWnd , WM_HSCROLL , 0 , (LPARAM)GetDlgItem(hDlgWnd, IDC_SLIDER_SCRSCALE) );
				//
			}
			return TRUE;
		
		case WM_COMMAND:
			switch (LOWORD(wp)) {
				case IDC_COMBO_SCRSEL:
					{
						if( HIWORD(wp) == CBN_SELCHANGE ){
							g_AppConfig.m_ScrSel = (ECAPSCR)SendMessage(GetDlgItem(hDlgWnd, IDC_COMBO_SCRSEL), CB_GETCURSEL, 0,0);
							g_AppConfig.m_ScrSel = (ECAPSCR)(g_AppConfig.m_ScrSel << 2);
							D3DUpdateScreenConfig( &g_AppConfig , g_hWnd );
						}
					}
					break;
				case IDC_COMBO_SCRDIR:
					{
						if( HIWORD(wp) == CBN_SELCHANGE ){
							g_AppConfig.m_DirMode = (unsigned char)SendMessage(GetDlgItem(hDlgWnd, IDC_COMBO_SCRDIR), CB_GETCURSEL, 0,0);
							D3DUpdateScreenConfig( &g_AppConfig , g_hWnd );
						}
					}
					break;
				case IDOK:
					{
						SendMessage( hDlgWnd , WM_CLOSE , 0 , 0 );
					}
					return TRUE;
				case IDCANCEL:
					SendMessage( hDlgWnd , WM_CLOSE , 0 , 0 );
					return TRUE;
				default:
					return FALSE;
			}
		
		case WM_HSCROLL:
			{
				
				TCHAR buf[256];
				switch(GetWindowLongPtr((HWND)lp,GWL_ID)) {
					// 画面の隙間
					case IDC_SLIDER_SCRSPACE:
						{
							g_AppConfig.m_ScrSpace = (int)SendMessage(GetDlgItem(hDlgWnd, IDC_SLIDER_SCRSPACE), TBM_GETPOS, 0, 0);
							if( g_AppConfig.m_ScrSpace <= CSCRSPACE_MIN ) g_AppConfig.m_ScrSpace = CSCRSPACE_MIN;
							if( g_AppConfig.m_ScrSpace >= CSCRSPACE_MAX ) g_AppConfig.m_ScrSpace = CSCRSPACE_MAX;
#if _MSC_VER >= 1400
							_stprintf_s( buf , 256 , TEXT("%d px") , g_AppConfig.m_ScrSpace );
#else
							_stprintf( buf , TEXT("%d px") , g_AppConfig.m_ScrSpace );
#endif
							SetWindowText( GetDlgItem(hDlgWnd, IDC_EDIT_SCRSPACE) , buf );
						}
						break;
					
					// 拡大率
					case IDC_SLIDER_SCRSCALE:
						{
							int t = (int)SendMessage(GetDlgItem(hDlgWnd, IDC_SLIDER_SCRSCALE), TBM_GETPOS, 0, 0);
							g_AppConfig.m_fScrScal = (float)t / CSCRSCAL_SCALE;
							if( g_AppConfig.m_fScrScal <= CSCRSCAL_MIN ) g_AppConfig.m_fScrScal = CSCRSCAL_MIN;
							if( g_AppConfig.m_fScrScal >= CSCRSCAL_MAX ) g_AppConfig.m_fScrScal = CSCRSCAL_MAX;
#if _MSC_VER >= 1400
							_stprintf_s( buf , 256 , TEXT("x%2.2f") , g_AppConfig.m_fScrScal );
#else
							_stprintf( buf , TEXT("x%2.2f") , g_AppConfig.m_fScrScal );
#endif
							SetWindowText( GetDlgItem(hDlgWnd, IDC_EDIT_SCRSCALE) , buf );
						}
						break;
				}

				D3DUpdateScreenConfig( &g_AppConfig , g_hWnd );
				//
			}
			break;
		
		case WM_CLOSE:
			DestroyWindow(g_hConfigDlg);
			g_hConfigDlg = NULL;
//			EndDialog(hDlgWnd, IDCANCEL);
			break;
		
		default:
			return FALSE;
	}
	return FALSE;
} 

