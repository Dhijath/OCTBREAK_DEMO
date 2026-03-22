/*==============================================================================

   デバッグ出力ストリーム [debug_ostream.h]
                                                         Author : 51106
                                                         Date   : 2026/02/15
--------------------------------------------------------------------------------
   Visual Studio の出力ウィンドウ（OutputDebugString）に
   std::ostream 形式で文字列を出力するユーティリティ。

   ■使い方
     hal::dout << "値: " << someValue << std::endl;

   ■仕組み
     std::basic_stringbuf を継承した debugbuf が sync() 時に
     OutputDebugStringA を呼ぶ。debug_ostream はそのストリームラッパー。
     hal::dout はグローバルインスタンス（debug_ostream.cpp で定義）。

   ■注意
     デバッグビルドのみ使用推奨。リリースビルドでは何も出力しないように
     マクロで切り替えるとよい。

==============================================================================*/
#ifndef DEBUG_OSTREAM_H
#define DEBUG_OSTREAM_H

#include <Windows.h>
#include <sstream>

namespace hal
{
	class debugbuf : public std::basic_stringbuf<char, std::char_traits<char>>
	{
	public:
		virtual ~debugbuf()
		{
			sync();
		}

	protected:

		int sync()
		{
			OutputDebugStringA(str().c_str());
			str(std::basic_string<char>());
			return 0;
		}
	};

	class debug_ostream : public std::basic_ostream<char, std::char_traits<char>>
	{
	public:
		debug_ostream()
			: std::basic_ostream<char, std::char_traits<char>>(new debugbuf()) {}
		~debug_ostream() { delete rdbuf(); }
	};

	extern debug_ostream dout;
	}
#endif // BASIC_DEBUG_OSTREAM_H