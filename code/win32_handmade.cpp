#include <windows.h>
#include <stdint.h>

#define internal static
#define local_persist static
#define global_variable static

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

struct win32_offscreen_buffer
{
  // NOTE: Pixels are always 32-bits wide, Memory Order BB GG RR xx
  BITMAPINFO Info;
  void *Memory;
  int Width;
  int Height;
  int Pitch;
};

// TODO: This is a global for now
global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;

struct win32_window_dimension
{
  int Width;
  int Height;
};

win32_window_dimension Win32GetWindowDimension(HWND Window)
{
  win32_window_dimension Result;
  RECT ClientRect;
  GetClientRect(Window, &ClientRect);
  Result.Width = ClientRect.right - ClientRect.left;
  Result.Height = ClientRect.bottom - ClientRect.top;

  return (Result);
}

internal void
RenderWeirdGradient(win32_offscreen_buffer Buffer, int BlueOffSet, int GreenOffSet)
{
  // Lets see what the optimiZer does
  
  uint8 *Row = (uint8 *)Buffer.Memory;
  for (int Y = 0;
	   Y < Buffer.Height;
	   ++Y)
  {
	uint32 *Pixel = (uint32 *)Row;
	for(int X = 0;
		  X < Buffer.Width;
		  ++X)
	{
	  /*            0  1  2  3 -> padding
	    Memory:    BB GG RR xx
		Register:  xx RR GG BB(Inverse because of little endian)
		Pixel (32-bits)
		LITTLE ENDIAN
		0x 00 00 00 00
		0x xx RR GG BB
	  */
	  uint8 Blue = (uint8)(X + BlueOffSet);
	  uint8 Green = (uint8)(Y + GreenOffSet);

	  *Pixel++ = ((Green << 8) | Blue);
	}

	Row += Buffer.Pitch;
  }

}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
  // TODO: Bulletproof this.
  // Maybe don't free first, free after, then free first if that fails

  if(Buffer->Memory)
  {
	VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
  }

  Buffer->Width = Width;
  Buffer->Height = Height;
  int BytesPerPixel = 4;
  
  // NOTE: when the biHeight field is negative, this is the clue to
  // Windows to treat this bitmao as top-down, not bottom-up, meaning
  // that the first three bytes of the image are the color for the
  // top left pixel in the bitmap!
  Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
  Buffer->Info.bmiHeader.biWidth = Buffer->Width;
  Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
  Buffer->Info.bmiHeader.biPlanes = 1;
  Buffer->Info.bmiHeader.biBitCount = 32;
  Buffer->Info.bmiHeader.biCompression = BI_RGB;
  
  int BitmapMemorySize = (Buffer->Width*Buffer->Height)*BytesPerPixel;
  Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
  Buffer->Pitch = Width*BytesPerPixel;

  // TODO: Probably clear this to black
}

internal void Win32DisplayBufferInWindow(HDC DeviceContext,
										 int WindowWidth, int WindowHeight,
										 win32_offscreen_buffer Buffer)
{
  // TODO: Aspect ratio correction
  // TODO: Play with stretch modes
  StretchDIBits(DeviceContext,
				/*
				  X, Y, Width, Height,
				  X, Y, Width, Height,
				*/
				0, 0, WindowWidth, WindowHeight,
				0, 0, Buffer.Width, Buffer.Height,
				Buffer.Memory,
				&Buffer.Info,
				DIB_RGB_COLORS, SRCCOPY);
  
}

LRESULT CALLBACK
Win32MainWindowCallback(HWND Window,
						UINT Message,
						WPARAM WParam,
						LPARAM LParam)
{
  LRESULT Result = 0;
  
  switch(Message)
  {
    case WM_SIZE:
	{
	} break;
	
    case WM_DESTROY:
	{
	  // TODO: Handle this as an error - recreate window?
	  GlobalRunning = false;
	} break;
	  
	case WM_CLOSE:
	{
	  // TODO: Handle this with a message to the user?
	  GlobalRunning = false;
	} break;
	  
	case WM_ACTIVATEAPP:
	{
	  OutputDebugStringA("WM_ACTIVATEAPP\n");
	} break;
	  
	case WM_PAINT:
	{
	  PAINTSTRUCT Paint;
	  HDC DeviceContext = BeginPaint(Window, &Paint);
	  win32_window_dimension Dimension = Win32GetWindowDimension(Window);

	  Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height,
								 GlobalBackBuffer);
	  EndPaint(Window, &Paint);
	} break;
	  
	default:
	{
	  //OutputDebugStringA("default\n");
	  Result = DefWindowProc(Window, Message, WParam, LParam);
	} break;
  }

  return(Result);
}

int CALLBACK
WinMain(
    HINSTANCE Instance,
    HINSTANCE PrevInstance,
    LPSTR     CommandLine,
    int       ShowCode)
{
  WNDCLASS WindowClass = {};

  Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);
	
  WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
  WindowClass.lpfnWndProc = Win32MainWindowCallback;
  WindowClass.hInstance = Instance;
  //WindowClass.hIcon;
  WindowClass.lpszClassName = "HandmadeHeroWindowClass";

  if(RegisterClass(&WindowClass)){
	HWND Window =
	  CreateWindowExA(
					  0,
					  WindowClass.lpszClassName,
					  "Handmade Hero",
					  WS_OVERLAPPEDWINDOW|WS_VISIBLE,
					  CW_USEDEFAULT,
					  CW_USEDEFAULT,
					  CW_USEDEFAULT,
					  CW_USEDEFAULT,
					  0,
					  0,
					  Instance,
					  0);
	
	if(Window)
	{
	  // NOTE: Since we specified CS_OWNDC, we can just get one device
	  // context and use it forever vecause we are not sharing it with
	  // anyone.
	  HDC DeviceContext = GetDC(Window);
	  int XOffset = 0;
	  int YOffset = 0;
	  GlobalRunning = true;
	  while(GlobalRunning)
	  {
		MSG Message;
		while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
		{
		  if(Message.message == WM_QUIT)
		  {
			GlobalRunning = false;
		  }
		  TranslateMessage(&Message);
		  DispatchMessage(&Message);
		}
		RenderWeirdGradient(GlobalBackBuffer, XOffset, YOffset);
		
		win32_window_dimension Dimension = Win32GetWindowDimension(Window);
	    Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height,
								   GlobalBackBuffer);
		ReleaseDC(Window, DeviceContext);
		  
		++XOffset;
		YOffset += 2;
	  }
	}
	else
	{
	  // TODO: Logging
	}
  }
  else
  {
	//TODO: Logging
  }
  
  return(0);
}
