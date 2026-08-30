//客户端

#include <stdio.h>
#include <Windows.h>
#include <atlimage.h>
#pragma comment(lib,"ws2_32.lib")

//设置新的对齐规则
#pragma pack (push,1)
struct PacketHeader
{
	int magic;		//4字节包头
	int cmd;		//四字节命令号
	int body_len;	//数据长度
};
#pragma pack (pop)


struct Packet
{
	PacketHeader header;	//包头
	char body[];			//包数据，没有固定长度

};
enum ENUM_MOUSE
{
	MOUSE_MOVE = 1,		//鼠标移动
	MOUSE_LDOWN = 2,	//鼠标左键按下
	MOUSE_LUP = 3,		//鼠标左键抬起
	MOUSE_RDOWN = 4,	//鼠标右键按下
	MOUSE_RUP = 5,		//鼠标右键抬起
	MOUSE_MDOWN = 6,		//鼠标中键按下
	MOUSE_MUP = 7,		//鼠标中键抬起
	MOUSE_LCLICK = 8,	//左键单击
	MOUSE_RCLICK = 9,		//右键单击
	MOUSE_MCLICK = 10,	//中键单击
	MOUSE_LDCLICK = 11,		//鼠标左键双击
	MOUSE_RDCLICK = 12,		//鼠标右键双击
	MOUSE_MDCLICK = 13,	//鼠标中键双击
};
struct Mouse
{
	int action;		//鼠标行为
	POINT ptXY;		//鼠标坐标 x,y坐标


};

struct KeyBoard
{
	int virtual_code;//虚拟码
	int key_status;//按下为1，松开为0

};
//枚举
enum CMD {
	CMD_SCREEN = 1,
	CMD_MOUSE = 2,
	CMD_KEYBOARD = 4,
	CMD_TESTCONNECT = 2026
};
//声明方法、宏的地方
Packet* PackPacket(int cmd, char* buffer, int buffer_len);
Packet* ParsePacket(char* buffer, int len);
int GetPacketLen(Packet* pck);
int InitSocket();
#define RECV_BUFFER_LEN 1024*1024*10
SOCKET g_server_socket;
SOCKADDR_IN g_server_addr;
HWND  g_hwnd;
CImage g_image;
DWORD WINAPI SendScreenCallBack(LPVOID lpThreadParameter);
int g_remote_width=-1;
int g_remote_height=-1;
CRITICAL_SECTION g_cri_sec;

LRESULT CALLBACK winProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	static ULONGLONG mouse_tick = GetTickCount64();//获取系统的启动时间 （单位ms）

	switch (msg) {

	case WM_PAINT://绘制事件
	{
		PAINTSTRUCT ps;
		HDC hdc= BeginPaint(hwnd,&ps);// _In_ HWND hWnd,_Out_ LPPAINTSTRUCT lpPaint
		
		if (!g_image.IsNull())
		{
			//做一个缩放
			//拿到窗口客户区域的大小
			RECT client_rect;
			GetClientRect(hwnd, &client_rect);
			int client_width = client_rect.right - client_rect.left;
			int client_height = client_rect.bottom - client_rect.top;
			

			//设置拉伸模式  //BLACKONWHITE     WHITEONBLACK           COLORONCOLOR  
		int oldMode=	SetStretchBltMode(hdc,HALFTONE);//HALFTONE:高清
			//设置画刷原点
			SetBrushOrgEx(hdc, 0, 0, NULL);

			//执行到这里
			EnterCriticalSection(&g_cri_sec);
			//远程图片的宽高
			int remote_width = g_image.GetWidth();
			int remote_height = g_image.GetHeight();
			g_image.StretchBlt(hdc,0,0,client_width,client_height,0,0,remote_width,remote_height,SRCCOPY);
			LeaveCriticalSection(&g_cri_sec);
			//恢复默认的拉伸模式
			SetStretchBltMode(hdc, HALFTONE);
		}
		EndPaint(hwnd, &ps);
	}
		break;
	
	case WM_MOUSEMOVE: //鼠标移动    可以通过限制鼠标发送的频率来达到提高屏幕帧率的效果
	{
		//拿到当前的鼠标位置（位于客户端的位置）
		int xPos = LOWORD(lParam);//低两字节是x的坐标
		int yPos = HIWORD(lParam);//高两字节是y的坐标
		RECT client_rect;
		GetClientRect(hwnd, &client_rect);
		int client_width = client_rect.right - client_rect.left;
		int client_height = client_rect.bottom - client_rect.top;

		if (g_remote_width != -1 && g_remote_height != -1)
		{
			static int count = 0;
			int rxPox = xPos * g_remote_width / client_width;
			int ryPos = yPos * g_remote_height / client_height;
			//发送数据
			Mouse mouse;
			mouse.action = MOUSE_MOVE;
			mouse.ptXY.x = rxPox;
			mouse.ptXY.y = ryPos;
			Packet* packet = PackPacket(CMD_MOUSE, (char*)&mouse.action, sizeof(Mouse));
			send(g_server_socket, (char*)&packet->header.magic, GetPacketLen(packet), 0);
			count++;
			char buf[20];
			sprintf_s(buf, "mouse:count%d\r\n", count);
			OutputDebugString(buf);
			free(packet);
			//mouse_tick = GetTickCount64();
			//if (GetTickCount64()-mouse_tick>500)
			//{
			//	Packet* packet = PackPacket(CMD_MOUSE, (char*)&mouse.action, sizeof(Mouse));
			//	send(g_server_socket, (char*)&packet->header.magic, GetPacketLen(packet), 0);
			//	count++;
			//	char buf[20];
			//	sprintf_s(buf, "mouse:count%d\r\n", count);
			//	OutputDebugString(buf);
			//	free(packet);
			//	mouse_tick = GetTickCount64();

			//}
		
		}

	}
		break;
	case WM_LBUTTONDOWN://鼠标左键按下
	{
	
		//拿到当前的鼠标位置（位于客户端的位置）
		int xPos = LOWORD(lParam);//低两字节是x的坐标
		int yPos = HIWORD(lParam);//高两字节是y的坐标
		RECT client_rect;
		GetClientRect(hwnd, &client_rect);
		int client_width = client_rect.right - client_rect.left;
		int client_height = client_rect.bottom - client_rect.top;

		if (g_remote_width != -1 && g_remote_height != -1)
		{
			int rxPox = xPos * g_remote_width / client_width;
			int ryPos = yPos * g_remote_height / client_height;
			//发送数据
			Mouse mouse;
			mouse.action = MOUSE_LDOWN;
			mouse.ptXY.x = rxPox;
			mouse.ptXY.y = ryPos;
			Packet* packet = PackPacket(CMD_MOUSE, (char*)&mouse.action, sizeof(Mouse));
			send(g_server_socket, (char*)&packet->header.magic, GetPacketLen(packet), 0);
			free(packet);
		}

	}
		break;	
	case WM_LBUTTONUP://鼠标左键抬起
	{
		//拿到当前的鼠标位置（位于客户端的位置）
		int xPos = LOWORD(lParam);//低两字节是x的坐标
		int yPos = HIWORD(lParam);//高两字节是y的坐标
		RECT client_rect;
		GetClientRect(hwnd, &client_rect);
		int client_width = client_rect.right - client_rect.left;
		int client_height = client_rect.bottom - client_rect.top;

		if (g_remote_width != -1 && g_remote_height != -1)
		{
			int rxPox = xPos * g_remote_width / client_width;
			int ryPos = yPos * g_remote_height / client_height;
			//发送数据
			Mouse mouse;
			mouse.action = MOUSE_LUP;
			mouse.ptXY.x = rxPox;
			mouse.ptXY.y = ryPos;
			Packet* packet = PackPacket(CMD_MOUSE, (char*)&mouse.action, sizeof(Mouse));
			send(g_server_socket, (char*)&packet->header.magic, GetPacketLen(packet), 0);
			free(packet);
		}

	}
		break;
	case WM_LBUTTONDBLCLK:
	{
		//拿到当前的鼠标位置（位于客户端的位置）
		int xPos = LOWORD(lParam);//低两字节是x的坐标
		int yPos = HIWORD(lParam);//高两字节是y的坐标
		RECT client_rect;
		GetClientRect(hwnd, &client_rect);
		int client_width = client_rect.right - client_rect.left;
		int client_height = client_rect.bottom - client_rect.top;

		if (g_remote_width != -1 && g_remote_height != -1)
		{
			int rxPox = xPos * g_remote_width / client_width;
			int ryPos = yPos * g_remote_height / client_height;
			//发送数据
			Mouse mouse;
			mouse.action = MOUSE_LDCLICK;
			mouse.ptXY.x = rxPox;
			mouse.ptXY.y = ryPos;
			Packet* packet = PackPacket(CMD_MOUSE, (char*)&mouse.action, sizeof(Mouse));
			send(g_server_socket, (char*)&packet->header.magic, GetPacketLen(packet), 0);
			free(packet);
		}

	}
		break;

	case WM_RBUTTONDOWN://鼠标右键按下
	{
		//拿到当前的鼠标位置（位于客户端的位置）
		int xPos = LOWORD(lParam);//低两字节是x的坐标
		int yPos = HIWORD(lParam);//高两字节是y的坐标
		RECT client_rect;
		GetClientRect(hwnd, &client_rect);
		int client_width = client_rect.right - client_rect.left;
		int client_height = client_rect.bottom - client_rect.top;

		if (g_remote_width != -1 && g_remote_height != -1)
		{
			int rxPox = xPos * g_remote_width / client_width;
			int ryPos = yPos * g_remote_height / client_height;
			//发送数据
			Mouse mouse;
			mouse.action = MOUSE_RDOWN;
			mouse.ptXY.x = rxPox;
			mouse.ptXY.y = ryPos;
			Packet* packet = PackPacket(CMD_MOUSE, (char*)&mouse.action, sizeof(Mouse));
			send(g_server_socket, (char*)&packet->header.magic, GetPacketLen(packet), 0);
			free(packet);
		}

	}
		break;
	case WM_RBUTTONUP://鼠标右键抬起
	{
		//拿到当前的鼠标位置（位于客户端的位置）
		int xPos = LOWORD(lParam);//低两字节是x的坐标
		int yPos = HIWORD(lParam);//高两字节是y的坐标
		RECT client_rect;
		GetClientRect(hwnd, &client_rect);
		int client_width = client_rect.right - client_rect.left;
		int client_height = client_rect.bottom - client_rect.top;

		if (g_remote_width != -1 && g_remote_height != -1)
		{
			int rxPox = xPos * g_remote_width / client_width;
			int ryPos = yPos * g_remote_height / client_height;
			//发送数据
			Mouse mouse;
			mouse.action = MOUSE_RUP;
			mouse.ptXY.x = rxPox;
			mouse.ptXY.y = ryPos;
			Packet* packet = PackPacket(CMD_MOUSE, (char*)&mouse.action, sizeof(Mouse));
			send(g_server_socket, (char*)&packet->header.magic, GetPacketLen(packet), 0);
			free(packet);
		}

	}
		break; 
	//键盘信息
	case WM_KEYDOWN:
		
	case WM_SYSKEYDOWN:
	{
		//拿到虚拟键盘码
		//int v_code = wParam;
		//发送数据
		KeyBoard key_board;
		key_board.virtual_code = wParam;
		key_board.key_status = 0;
		Packet* packet = PackPacket(CMD_KEYBOARD, (char*)&key_board.virtual_code, sizeof(KeyBoard));
		send(g_server_socket, (char*)&packet->header.magic, GetPacketLen(packet),0);
		free(packet);
	}
		break;
	
	
	default :
		return DefWindowProc(hwnd, msg, wParam, lParam);
		break;
	}

	return 0;
}
int InitWindow(HINSTANCE hInstance, int nCmdeShow);

//主函数入口

//窗口入口函数			当前句柄实例	前一个实例句柄				命令行参数		窗口参数
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPreventInstance, PSTR pCmdLine, int nCmdeShow) {
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	//初始化关键代码段
	InitializeCriticalSection(&g_cri_sec);




	InitWindow(hInstance, nCmdeShow);
	//链接服务器
	InitSocket();
	//OutputDebugString  打印日志的方法
	if (connect(g_server_socket, (sockaddr*)&g_server_addr, sizeof(SOCKADDR_IN))==SOCKET_ERROR) //nt PASCAL FAR connect (_In_ SOCKET s,_In_reads_bytes_(namelen) const struct sockaddr FAR* name,_In_ int namelen);
		{
		OutputDebugString("链接失败服务器\r\n");
			return 0;
		}
	//开一条流水线来发送请求屏幕的数据以及解析
	//CreateThread(
	//	_In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
	//	_In_ SIZE_T dwStackSize,
	//	_In_ LPTHREAD_START_ROUTINE lpStartAddress,
	//	_In_opt_ __drv_aliasesMem LPVOID lpParameter,
	//	_In_ DWORD dwCreationFlags,
	//	_Out_opt_ LPDWORD lpThreadId
	//);

	
	unsigned long	send_screen_thread_id = 0;
	HANDLE handle_send_screen = CreateThread(NULL,0, SendScreenCallBack,NULL,0,&send_screen_thread_id);


	OutputDebugString("链接成功\r\n");
	
	//发送命令
	//


	//创建消息循环队列
	//GetMessageW(
	//	_Out_ LPMSG lpMsg,
	//	_In_opt_ HWND hWnd,
	//	_In_ UINT wMsgFilterMin,
	//	_In_ UINT wMsgFilterMax);

	MSG msg = {0};
	while( GetMessage(&msg,NULL,0,0)){
		TranslateMessage(&msg);//翻译
		DispatchMessage(&msg);//分发消息



	}



}

//方法声明

DWORD WINAPI SendScreenCallBack(LPVOID lpThreadParameter) {
	
	char* recv_buffer = (char*)malloc(RECV_BUFFER_LEN);
	//不停的发送解析数据

	while (true) {
		Packet* packet = PackPacket(CMD_SCREEN,NULL,0);
		//发送获取屏幕的请求
	int sen_len=	send(g_server_socket,(char*) & packet->header.magic, GetPacketLen(packet), 0);
	if (sen_len>0)
	{
		OutputDebugString("成功发送数据");

	}
		free(packet);
		//等待接收数据
	int len=	recv(g_server_socket, recv_buffer, RECV_BUFFER_LEN,0);
	if (len>0)
	{
		Packet* pack = ParsePacket(recv_buffer, len);
		if (pack!=NULL)
		{
		HGLOBAL hMen=	 GlobalAlloc(GMEM_MOVEABLE, 0);
		if (hMen==NULL)
		{
			continue;
		}

		IStream* pStream = NULL;
		HRESULT ret = CreateStreamOnHGlobal(hMen, true, &pStream);
		//h_bitmap
		if (ret==S_OK)
		{
			ULONG length = 0;
			pStream->Write(pack->body, pack->header.body_len, &length);
			free(pack);

			
			//将pStream 指针移动到末尾去

			LARGE_INTEGER lg = { 0 };
			pStream->Seek(lg, STREAM_SEEK_SET, NULL);

			EnterCriticalSection(&g_cri_sec);
			if (!g_image.IsNull())
			{
				g_image.Destroy();
			}

			//将数据移动到缓存里面去
		
			g_image.Load(pStream);
			if (g_remote_width==-1&&g_remote_height ==-1)
			{
				 g_remote_width = g_image.GetWidth();
				 g_remote_height = g_image.GetHeight();
			}
			LeaveCriticalSection(&g_cri_sec);
			//更新画面——将图片绘制到窗口，所有和UI相关的操作都不能在非UI流水线（线程）操作
			//通知UI现场重绘
			InvalidateRect(g_hwnd,NULL,FALSE);// _In_opt_ HWND hWnd,_In_opt_ CONST RECT* lpRect,_In_ BOOL bErase
			UpdateWindow(g_hwnd);





		}
		}
	}
	}
};

int InitWindow(HINSTANCE hInstance, int nCmdeShow) {

	//1.注册一个窗口类
	WNDCLASS ws = {};
	LPCSTR	CLASS_NAME = "MainWindow";
	ws.lpfnWndProc = winProc;//窗口消息的处理函数 
	ws.hInstance = hInstance;//实例句柄
	ws.lpszClassName = CLASS_NAME;
	ws.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	ws.hCursor = LoadCursor(NULL, IDC_ARROW);//光标
	ws.hIcon = LoadIconA(NULL, IDI_APPLICATION);//图标
	ws.style = CS_HREDRAW | CS_VREDRAW;//窗口大小发生变化时会重绘窗口
	if (!RegisterClass(&ws)) {
		MessageBox(NULL, "窗口注册失败", "错误", MB_OK | MB_ICONERROR);
		return 0;
	}

	//2.创建窗口
	  g_hwnd = CreateWindow(
		CLASS_NAME,//窗口类名
		"远程控制",//窗口标题
		WS_OVERLAPPEDWINDOW,//窗口样式
		CW_USEDEFAULT, CW_USEDEFAULT,//窗口坐标x,y的位置
		600, 400,//窗口宽高
		NULL,//父窗口的句柄
		NULL,//菜单句柄
		hInstance,//实例句柄
		NULL//附加参数
	);
	if (g_hwnd == NULL) {
		MessageBox(NULL, "窗口创建失败", "错误", MB_OK | MB_ICONERROR);
		return 0;
	}

	//3.显示窗口
	ShowWindow(g_hwnd, nCmdeShow);

	//4.更新窗口
	UpdateWindow(g_hwnd);



}




//int main() {
//	//getchar()
//	//InitSocket();
//	////链接服务器
//	//if (connect(g_server_socket, (sockaddr*)&g_server_addr, sizeof(SOCKADDR_IN))==SOCKET_ERROR) //nt PASCAL FAR connect (_In_ SOCKET s,_In_reads_bytes_(namelen) const struct sockaddr FAR* name,_In_ int namelen);
//	//{
//	//	printf("链接失败服务器\r\n");
//	//	return 0;
//	//}
//	//printf("链接成功\r\n");
//
//	////向服务器发送数据
//	//char buffer[1024]="hello world";
//
//	//char* recv_buffer=(char*)malloc(RECV_BUFFER_LEN);
//	//int count = 0;//记录发送数据个数
//	//while (true) {
//	//	count++;
//
//	//	//sprintf_s(buffer,"packet:%d",count);
//	//	//接收用户输入的消息，stdin :标准输入
//	//	printf("请输入要发送的数据：");
//	//	fgets(buffer,1024,stdin);
//	//	//int send_len = strlen(buffer);
//
//
//	//	//获取屏幕
//	//	Packet* send_packet = PackPacket(1, buffer, 10);
//	//
//	//
//	//	
//	//	//发送
//
//	//	send(g_server_socket, (char *)&send_packet->header.magic, send_packet->header.body_len + sizeof(PacketHeader), 0);//int PASCAL FAR send (_In_ SOCKET s,_In_reads_bytes_(len) const char FAR* buf,_In_ int len,_In_ int flags);
//	//	free(send_packet);//动态内存释放
//	//	printf("client send data: %s\r\n", buffer);
//	//	
//	//	//等待接受数据
//	//	int len = recv(g_server_socket, recv_buffer, 1024 * 1024 * 1, 0);
//	//	
//	//	if (len > 0)
//	//	{
//	//		Packet* recv_pck = ParsePacket(recv_buffer, len);
//	//		printf("recv data:%s\r\n", recv_pck->body);
//	//		printf("recv pck->header.body_len:%d\r\n", recv_pck->header.body_len);
//	//		printf("recv pck->header.cmd:%d\r\n", recv_pck->header.cmd);
//	//		printf("recv pck->header.magic:%x\r\n", recv_pck->header.magic);
//	//		if (recv_pck->header.cmd == 1) {
//	//			//服务器返回了屏幕数据
//	//			//解析显示器
//
//	//		}
//	//		free(recv_pck);
//
//	//	}
//
//
//	//	//休眠100ms
//	//	//Sleep(10);
//
//
//	//}
//
//
//	//
//
//	//closesocket(g_server_socket);
//
//	//WSACleanup();
//
//	//return 0;
//	
//}

Packet* PackPacket(int cmd,char* buffer, int buffer_len) {
	Packet* pck = (Packet*)malloc(sizeof(PacketHeader) + buffer_len);
	pck->header.magic = 0x55AA77CC;
	pck->header.cmd = cmd;
	pck->header.body_len = buffer_len;
	if (buffer!=NULL&&buffer_len>0)
	{
		memcpy(pck->body, buffer, buffer_len);
	}

	return pck;
}

int GetPacketLen(Packet* pck) {

	if (pck != NULL)
	{
		return pck->header.body_len + sizeof(PacketHeader);
	}

}




Packet* ParsePacket(char* buffer, int len) {

	Packet pck;
	Packet* ppck;
	int index = 0;
	//组成：4字节包头+4字节命令号+4字节数据长度+数据
	for (;index < len;index++)
	{
		//包头
		if (*(int*)(buffer + index) == 0x55AA77CC) {
			pck.header.magic = *(int*)(buffer + index);
			index += 4;
			break;
		}

	}
	pck.header.cmd = (*(int*)(buffer + index));
	index += 4;
	pck.header.body_len = (*(int*)(buffer + index));
	index += 4;
	if (pck.header.body_len > 0) {
		ppck = (Packet*)malloc(sizeof(PacketHeader) + pck.header.body_len);//创建缓冲区
		//拷贝数据
		memcpy(ppck->body, buffer + index, pck.header.body_len);
		//拷贝包头
		memcpy(&ppck->header, &pck.header, sizeof(PacketHeader));
		return ppck;
	}

}


int InitSocket() {
	//初始化网络环境
	WSADATA wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata);

	//创建服务器
	 g_server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (g_server_socket == INVALID_SOCKET)
	{
		printf("创建socket失败\r\n");
		return 0;
	}


	 g_server_addr;

	g_server_addr.sin_family = AF_INET;
	g_server_addr.sin_port = htons(9999);  //转为网络字节序,要是报错就改回ntohs()
	g_server_addr.sin_addr.S_un.S_addr = inet_addr("192.168.229.128"); //192.168.1.10   192.168.229.128


}