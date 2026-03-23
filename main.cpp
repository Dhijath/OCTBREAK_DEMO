/*==============================================================================

   メイン制御 [main.cpp]
														 Author : 51106
														 Date   : 2025/11/12
--------------------------------------------------------------------------------
==============================================================================*/

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <algorithm>
#include "game_window.h"
#include "game.h"
#include "direct3d.h"
#include "shader3d.h"
#include "sprite.h"
#include "sprite_anim.h"
#include "fade.h"
#include "input_hint.h"
#include "debug_text.h"
#include <sstream>
#include "system_timer.h"
#include "key_logger.h"
#include "mouse.h"
#include "scene.h"
#include "Audio.h"
#include "title.h"
#include "cube.h"
#include "grid.h"
#include "shader.h"
#include "texture.h"
#include "sampler.h"
#include "light.h"
#include "collision.h"
#include "blob_shadow.h"



#include <DirectXMath.h>
#include "shader_field.h"
#include "meshfield.h"
#include "Shadow_Map.h"
#include "player.h"
#include "WallShader.h"
#include "Game_Manager.h"
#include "pad_logger.h"
#include "Option.h"
#include "ShaderToon.h"
#include "ShaderEdge.h"

using namespace DirectX;

int APIENTRY WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nCmdShow)
{
	(void)CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	// DPIスケーリング
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	HWND hWnd = GameWindow_Create(hInstance);

	SystemTimer_Initialize();
	PadLogger_Initialize();
	KeyLogger_Initialaize();
	Mouse_Initialize(hWnd);
	InitAudio();

	Direct3D_Initialize(hWnd);

	// 
	Collision_DebugInitialize(Direct3D_GetDevice(), Direct3D_GetContext());

	BlobShadow::Initialize(Direct3D_GetDevice());

	Shader_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());



	ShaderToon_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());

	ShaderEdge_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());


	Shader3d_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());

	WallShader_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());

	Shader_field_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Option_Initialize();

	// 
	Texture_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Sampler_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());

	// 
	Sprite_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	SpriteAnim_Initialize();
	Fade_Initialize();
	InputHint_Initialize();

	Sampler_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());

	Cube_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());

	Light_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());

	MeshField_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());

	ShadowMap::Initialize(2048);

	// ゲームマネージャ起動（Title/Play/Result/Clear を統括）
	GameManager_Initialize();

	// DebugText 初期化（1回だけ）
	hal::DebugText dt(
		Direct3D_GetDevice(),
		Direct3D_GetContext(),
		L"Resource/Texture/consolab_ascii_512.png",
		Direct3D_GetBackBufferWidth(),
		Direct3D_GetBackBufferHeight(),
		0.0f, 0.0f,
		0, 0,
		0.0f, 21.0f
	);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	// 相対モード開始
	Mouse_SetMode(MOUSE_POSITION_MODE_RELATIVE);
	Mouse_SetVisible(false);

	// フレーム計測用
	double exec_last_time = SystemTimer_GetTime();
	double fps_last_time = exec_last_time;
	double current_time = 0.0;
	ULONG frame_count = 0.0;
	double fps = 0.0;

	/*メッセージループ*/
	MSG msg;

	do
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			current_time = SystemTimer_GetTime();

			

			//------------------------------
			// 1) FPS計測（1秒ごとに更新）
			//------------------------------
			{
				const double fps_elapsed = current_time - fps_last_time;
				if (fps_elapsed >= 1.0)
				{
					fps = (double)frame_count / fps_elapsed;
					fps_last_time = current_time;
					frame_count = 0;
				}
			}

			//------------------------------
			// 2) フレーム間 dt
			//------------------------------
			double elapsed_time = current_time - exec_last_time;

			//------------------------------
			// 3) 固定60fpsゲート
			//------------------------------
			if (elapsed_time >= 1.0 / 60.0)
			{
				exec_last_time = current_time;

				//==========================================================
				// フルスクリーン切替（前フレームの Present 完了後に実行）
				// Update 中に呼ぶと ResizeBuffers が GPU 使用中に走るため
				// ここ（フレーム先頭）まで遅延して安全に処理する
				//==========================================================
				GameWindow_ApplyPendingToggle();

				//==========================================================
				// 入力更新
				//==========================================================
				KeyLogger_Update();

				PadLogger_Update();                                  // パッドロガー更新


				//==========================================================
				// シーン更新
				//==========================================================
				GameManager_Update(elapsed_time);
				SpriteAnim_Update(elapsed_time);
				Fade_Update(elapsed_time);
				InputHint_Update();

				//==========================================================
				// 描画
				//==========================================================
				Direct3D_Clear();

				Sprite_Begin();

				GameManager_Draw();

#if defined(DEBUG) || defined(_DEBUG)
				// FPS表示（デバッグ時のみ）
				std::stringstream ss;
				ss << "fps: " << fps << std::endl;

				dt.SetText(ss.str().c_str());
				dt.Draw();
				dt.Clear();
#endif

				Direct3D_Present();

				//==========================================================
				// シーン切替確定
				//==========================================================
				Scene_Refresh();

				frame_count++;
			}
		}

	} while (msg.message != WM_QUIT);

	//===========================
	// 終了処理
	//===========================

	GameManager_Finalize();

	Cube_Finalize();
	Grid_Finalize();

	MeshField_Finalize();

	ShadowMap::Finalize();

	InputHint_Finalize();
	Fade_Finalize();
	SpriteAnim_Finalize();
	Sprite_Finalize();

	Scene_Finalize();

	Light_Finalize();
	Sampler_Finalize();
	Texture_Finalize();
	Shader_field_Finalize();
	WallShader_Finalize();
	Shader3d_Finalize();

	ShaderEdge_Finalize();
	ShaderToon_Finalize();

	Shader_Finalize();
	
	Collision_DebugFinalize();

	BlobShadow::Finalize();

	Direct3D_Finalize();

	Mouse_Finalize();
	
	UninitAudio();

	return (int)msg.wParam;
}