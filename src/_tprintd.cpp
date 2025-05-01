
#include <stdio.h>
#include <tchar.h>
#include <windows.h>


/*
	@brief	エラー表示用関数
	@note	使い方は_tprintfとほぼ一緒
*/
void _tprintd( const TCHAR * _Format, ... ){
	va_list	arglist;
	int		len;
	TCHAR	*buffer = NULL;
	
	va_start(arglist, _Format);
	len = _vtprintf( _Format, arglist )+1;
	if( len > 1 )buffer = new TCHAR[ len ];
	if( buffer ){
#if _MSC_VER >= 1400
		_vstprintf_s( buffer , len , _Format, arglist);
#else
		_vstprintf( buffer , len , _Format, arglist);
#endif
	}
	if( buffer ){
#if defined(_CONSOLE)
		printf( buffer );
#else
		OutputDebugString(buffer);
#endif
	}
	if( buffer )delete [] buffer;
	va_end(arglist);
}

