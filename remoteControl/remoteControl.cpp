//服务端


#include <stdio.h>
#include <Windows.h>
#include <atlimage.h>
#include <ShellScalingApi.h>
#pragma comment(lib,"ws2_32.lib")//编译指令：链接时带上网络库 ws2_32.lib
#define RECV_BUFFER_SIZE 1024*1024*1

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
//鼠标信息分类：1.左键，中键，右键，滚轮（算中键）  2.状态：按下 ，松开，双击
//鼠标行为=按键+状态
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



//枚举区1
enum CMD {
	CMD_SCREEN = 1,
	CMD_MOUSE = 2,
	CMD_KEYBOARD = 4,
	CMD_TESTCONNECT = 2026
};






//声明
Packet* ParsePacket(char* buffer, int len);
int GetPacketLen(Packet* pck);
Packet* PackPacket(int cmd, char* buffer, int buffer_len);
int InitServer();
SOCKET g_server_socket;
SOCKET g_client_socket;
int HandleCommand(Packet* packet);
int HandleScreen(Packet* packet);
int HandleMouse(Packet* packet);
int HandleKeyBoard(Packet* packet);
int HandleTestConnect(Packet* packet);

unsigned long g_screen_thread_id = 0;
unsigned long g_mouse_thread_id = 0;
unsigned long g_keybroad_thread_id = 0;
#define WM_HANDLE_SCREEN (WM_USER+1)
#define WM_HANDLE_MOUSE (WM_USER+2)
#define WM_HANDLE_KEYBROAD (WM_USER+3)
#define WM_HANDLE_INVOKE_MSG_LOOP (WM_USER+4)

DWORD WINAPI HandleScreenThreadFuc(LPVOID lpThreadParameter) {
	MSG msg;
	//等待 第一次向线程里面post数据是，是收不到的（消息队列还未启动起来），需要提前向内post一个消息
	while (GetMessage(&msg, 0, 0, 0)) {
		if (msg.message == WM_HANDLE_SCREEN)
		{
			Packet* packet = (Packet*)msg.lParam;
			HandleScreen(packet);
			free(packet);
		}
	}
	return 0;
}

DWORD WINAPI HandleMouseThreadFuc(LPVOID lpThreadParameter) {
	MSG msg;
	while (GetMessage(&msg, 0, 0, 0)) {
		if (msg.message == WM_HANDLE_MOUSE)
		{
			Packet* packet = (Packet*)msg.lParam;
			HandleMouse(packet);
			free(packet);
		}
	}
	return 0;
}
DWORD WINAPI HandleKeyBroadThreadFuc(LPVOID lpThreadParameter) {
	MSG msg;
	while (GetMessage(&msg, 0, 0, 0)) {
		if (msg.message == WM_HANDLE_KEYBROAD)
		{
			Packet* packet = (Packet*)msg.lParam;
			HandleKeyBoard(packet);
			free(packet);
		}
	}
	return 0;
}



int main() {
	if (InitServer() != 0)
	{
		printf("启动服务器失败。");
		return 0;
	}

	//为每一个命令单独开辟一个流水线
	CreateThread(NULL, 0, HandleScreenThreadFuc, NULL, 0, &g_screen_thread_id);
	CreateThread(NULL, 0, HandleMouseThreadFuc, NULL, 0, &g_mouse_thread_id);
	CreateThread(NULL, 0, HandleKeyBroadThreadFuc, NULL, 0, &g_keybroad_thread_id);
	//InitServer();
	//先向线程内post一个消息	用于激活消息队列
	PostThreadMessage(g_screen_thread_id, WM_HANDLE_INVOKE_MSG_LOOP, NULL, NULL);
	PostThreadMessage(g_mouse_thread_id, WM_HANDLE_INVOKE_MSG_LOOP, NULL, NULL);
	PostThreadMessage(g_keybroad_thread_id, WM_HANDLE_INVOKE_MSG_LOOP, NULL, NULL);
	Sleep(100);
	// 5. 等待客户端连接,会返回客户端的socket
	SOCKADDR_IN client_addr;
	int client_addr_len = sizeof(SOCKADDR_IN);
	printf("等待客户端链接\r\n");
	//阻塞，等待客户端链接
	g_client_socket = accept(g_server_socket, (sockaddr*)&client_addr, &client_addr_len);//SOCKET PASCAL FAR accept(_In_ SOCKET s,_Out_writes_bytes_opt_(*addrlen) struct sockaddr FAR * addr,_Inout_opt_ int FAR * addrlen);
	printf("客户端链接成功\r\n");
	//6. 等待客户端(客户端socket)发送数据

	char* buffer = (char*)malloc(RECV_BUFFER_SIZE);

	//记录缓冲区长度
	static int index = 0;

	while (true) {
		//返回接收到的数据长度
		//重新定义了缓冲区的大小RECVG_BUFFER_SIZE-index
		int len = recv(g_client_socket, buffer + index, RECV_BUFFER_SIZE - index, 0);//int PASCAL FAR recv ( _In_ SOCKET s, _Out_writes_bytes_to_(len, return) __out_data_source(NETWORK) char FAR* buf,_In_ int len, _In_ int flags);
		if (len > 0)
		{
			index += len;
			printf("接收成功：%d\r\n", len);
		}
		else
		{
			printf("接收失败\r\n");
		}

		if (index > 0)
		{
			//接收后解析数据
			//传入缓冲区长度
			printf("开始解析\r\n");
			Packet* packet = ParsePacket(buffer, index);
			while (packet != NULL && index > 0) {
				if (packet == NULL)
				{
					printf("解析失败\r\n");
				}
				printf("解析成功\r\n");
				if (packet != NULL)
				{
					index -= GetPacketLen(packet);
					memmove(buffer, buffer + GetPacketLen(packet), index);

					HandleCommand(packet);
					//free(packet);
				}
				packet = ParsePacket(buffer, index);
			}

		}



	}

	//程序结束关闭套接字
	closesocket(g_client_socket);

	closesocket(g_server_socket);


	//释放内存



	WSACleanup();
	//HandleScreen(NULL);
	return 0;
}

//函数声明


int HandleCommand(Packet* packet) {
	printf("Handle cmd：%d\r\n", packet->header.cmd);
	int ret = 0;
	switch (packet->header.cmd) {
		//发送屏幕
	case CMD_SCREEN:
		//向线程投递消息是异步的
		PostThreadMessage(g_screen_thread_id, WM_HANDLE_SCREEN, NULL, (LPARAM)packet);
		//ret = HandleScreen(packet);
		break;
		//鼠标事件
	case CMD_MOUSE:
		PostThreadMessage(g_mouse_thread_id, WM_HANDLE_MOUSE, NULL, (LPARAM)packet);
		//ret = HandleMouse(packet);
		break;
		//键盘命令
	case CMD_KEYBOARD:
		PostThreadMessage(g_keybroad_thread_id, WM_HANDLE_KEYBROAD, NULL, (LPARAM)packet);
		//ret = HandleKeyBoard(packet);
		break;
		//测试命令
	case CMD_TESTCONNECT:
		ret = HandleTestConnect(packet);
		break;

	default:
		break;
	}
	return ret;
}

int HandleScreen(Packet* packet) {

	//获取屏幕并通过网络发送
	CImage image;
	//拿到屏幕的上下文（DC:device context）
	HDC hScreen = GetDC(NULL);   //GetDC(_In_opt_ HWND hWnd);
	//拿到屏幕像素位宽
	int bitWidth = GetDeviceCaps(hScreen, BITSPIXEL);  //WINGDIAPI int   WINAPI GetDeviceCaps( _In_opt_ HDC hdc, _In_ int index);
	printf("bitWidth:%d\r\n", bitWidth);

	//开启DPI动态感知————获取正确的屏幕宽高
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);//SetProcessDpiAwarenessContext(_In_ DPI_AWARENESS_CONTEXT value);
	//拿到屏幕的宽高
	int sWidth = GetSystemMetrics(SM_CXSCREEN);  //GetSystemMetrics(_In_ int nIndex);
	int sHight = GetSystemMetrics(SM_CYSCREEN);   //这里不要填写成高度
	printf("sWidth:%d,sHight:%d\r\n", sWidth, sHight);

	image.Create(sWidth, sHight, bitWidth);//inline BOOL CImage::Create(_In_ int nWidth,_In_ int nHeight,_In_ int nBPP,_In_ DWORD dwFlags)
	// 把屏幕数据复制到image里面
	BitBlt(image.GetDC(), 0, 0, sWidth, sHight, hScreen, 0, 0, SRCCOPY);// WINGDIAPI BOOL  WINAPI BitBlt( _In_ HDC hdc, _In_ int x, _In_ int y, _In_ int cx, _In_ int cy, _In_opt_ HDC hdcSrc, _In_ int x1, _In_ int y1, _In_ DWORD rop);

	//释放屏幕上下文
	ReleaseDC(NULL, hScreen);

	//转为网络流.
	//image.Save("test.png", ::Gdiplus::ImageFormatPNG);//将截取的图片保存为本地文件
	//在堆上申请一块可变化的内存空间
	HGLOBAL hMen = GlobalAlloc(GMEM_MOVEABLE, 0);
	if (hMen == NULL)
	{
		return -1;
	}
	//创建一个内存流
	IStream* pStream = NULL;
	HRESULT ret = CreateStreamOnHGlobal(hMen, true, &pStream);

	if (ret == S_OK)
	{
		image.Save(pStream, ::Gdiplus::ImageFormatPNG);//将截图文件传入内存流

		//将流指针放在开头 pSream= schar    STREAM_SEEK_SET:开头
		LARGE_INTEGER lg = { 0 };
		pStream->Seek(lg, STREAM_SEEK_SET, NULL);
		//获取流指针
		char* pdata = (char*)GlobalLock(hMen);
		//获取流长度
		int len = GlobalSize(hMen);
		//发送数据
		Packet* packet = PackPacket(CMD_SCREEN, pdata, len);
		int send_len = send(g_client_socket, (char*)&packet->header.magic, sizeof(PacketHeader) + len, 0);
		if (send_len > 0)
		{
			printf("发送成功：%d\r\n", send_len);
		}
		else if (send_len < 0) {
			printf("发送失败：%d\r\n", send_len);
		}
		//释放/解锁内存
		free(packet);
		GlobalUnlock(hMen);
	}
	//释放流指针
	pStream->Release();
	//释放全局内存
	GlobalFree(hMen);
	//释放DC
	image.ReleaseDC();
	return 0;
}

int HandleMouse(Packet* packet) {
	//通过结构体提取出鼠标的各种行为
	Mouse mouse;
	memcpy(&mouse.action, packet->body, packet->header.body_len);
	printf("x=%d y=%d\r\n", mouse.ptXY.x, mouse.ptXY.y);
	//模拟执行鼠标行为
	//设置鼠标位置
	SetCursorPos(mouse.ptXY.x, mouse.ptXY.y);

	//enum ENUM_MOUSE
	//{
	//	MOUSE_MOVE = 1,		//鼠标移动
	//	MOUSE_LDOWN = 2,	//鼠标左键按下
	//	MOUSE_LUP = 3,		//鼠标左键抬起
	//	MOUSE_RDOWN = 4,	//鼠标右键按下
	//	MOUSE_RUP = 5,		//鼠标右键抬起
	//	MOUSE_MDOWN = 6,		//鼠标中键按下
	//	MOUSE_MUP = 7,		//鼠标中键抬起
	//	MOUSE_LDLICK = 8,		//鼠标左键双击
	//	MOUSE_RDLICK = 9,		//鼠标右键双击
	//	MOUSE_MDLICK = 10,	//鼠标中键双击
	//};
//#define MOUSEEVENTF_MOVE        0x0001 /* mouse move */
//#define MOUSEEVENTF_LEFTDOWN    0x0002 /* left button down */
//#define MOUSEEVENTF_LEFTUP      0x0004 /* left button up */
//#define MOUSEEVENTF_RIGHTDOWN   0x0008 /* right button down */
//#define MOUSEEVENTF_RIGHTUP     0x0010 /* right button up */
//#define MOUSEEVENTF_MIDDLEDOWN  0x0020 /* middle button down */
//#define MOUSEEVENTF_MIDDLEUP    0x0040 /* middle button up */
//#define MOUSEEVENTF_XDOWN       0x0080 /* x button down */
//#define MOUSEEVENTF_XUP         0x0100 /* x button down */


	switch (mouse.action) {
	case MOUSE_MOVE:
		SetCursorPos(mouse.ptXY.x, mouse.ptXY.y);
		break;
	case MOUSE_LDOWN:
		mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
		break;
	case MOUSE_LUP:
		mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
		break;
	case MOUSE_RDOWN:
		mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
		break;
	case MOUSE_RUP:
		mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
		break;
	case MOUSE_MDOWN:
		mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
		break;
	case MOUSE_MUP:
		mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
		break;
	case MOUSE_LCLICK:
		mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
		break;
	case MOUSE_RCLICK:
		mouse_event(MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
		break;
	case MOUSE_MCLICK:
		mouse_event(MOUSEEVENTF_MIDDLEDOWN | MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
		break;
	case MOUSE_LDCLICK:
		mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
		mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
		break;
	case MOUSE_RDCLICK:
		mouse_event(MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
		mouse_event(MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
		break;
	case MOUSE_MDCLICK:
		mouse_event(MOUSEEVENTF_MIDDLEDOWN | MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
		mouse_event(MOUSEEVENTF_MIDDLEDOWN | MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
		break;
	default:
		printf("未知鼠标操作：%d\r\n", mouse.action);
		break;
	}


	return 0;
}
int HandleKeyBoard(Packet* packet) {
	KeyBoard key_board;
	memcpy(&key_board.virtual_code, packet->body, packet->header.body_len);
	INPUT input = { 0 };
	input.type = INPUT_KEYBOARD;
	input.ki.wVk = key_board.virtual_code;
	input.ki.wScan = 0;
	input.ki.time = 0;
	input.ki.dwFlags = key_board.key_status;
	input.ki.dwExtraInfo = 0;

	int ret = SendInput(1, &input, sizeof(INPUT));
	if (ret > 0)
	{
		printf("成功执行键盘事件:%d\r\n", key_board.virtual_code);
	}
	return 0;
}
int HandleTestConnect(Packet* packet) {


	return 0;
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

	//客户端单纯发出命令，没有用户数据的时候也要处理
	//获取数据
	if (pck.header.body_len == 0)//单纯的命令，没有数据
	{
		ppck = (Packet*)malloc(sizeof(PacketHeader));
		//拷贝包头
		memcpy(&ppck->header, &pck.header, sizeof(PacketHeader));
		return ppck;
	}
	if (pck.header.body_len > 0) {
		ppck = (Packet*)malloc(sizeof(PacketHeader) + pck.header.body_len);//创建缓冲区
		//拷贝数据
		memcpy(ppck->body, buffer + index, pck.header.body_len);
		//拷贝包头
		memcpy(&ppck->header, &pck.header, sizeof(PacketHeader));
		return ppck;
	}
	return NULL;
}

Packet* PackPacket(int cmd, char* buffer, int buffer_len) {
	Packet* pck = (Packet*)malloc(sizeof(PacketHeader) + buffer_len);
	pck->header.magic = 0x55AA77CC;
	pck->header.cmd = cmd;
	pck->header.body_len = buffer_len;
	if (buffer != NULL && buffer_len > 0)
	{
		memcpy(pck->body, buffer, buffer_len);
	}

	return pck;
}

int InitServer() {
	//  1.  初始化网络环境  --加载#pragma commment(lib,"ws2_32.lib"）

	WSADATA wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata);	//int PASCAL FAR WSAStartup(_In_ WORD wVersionRequired,_Out_ LPWSADATA lpWSAData);参数

	// 2. 创建服务器
	g_server_socket = socket(AF_INET, SOCK_STREAM, 0);     //SOCKET PASCAL FAR socket(_In_ int af,_In_ int type,_In_ int protocol);
	//      IPv4  , TCP 

	if (g_server_socket == INVALID_SOCKET) {
		printf("创建服务器socket失败\r\n");
		return -1;
	}
	//判断服务器的创建

	//3. 给服务器socket绑定地址

	SOCKADDR_IN server_addr;   // 参数参考 struct sockaddr_in {short   sin_family;    u_short sin_port;   struct  in_addr sin_addr;  char    sin_zero[8];  };

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(9999);  //监听的端口 范围为0——65535（short）0——1024
	server_addr.sin_addr.S_un.S_addr = inet_addr("192.168.229.128"); //0.0.0.0  监听服务器上所有的IP  192.168.1.10

	if (bind(g_server_socket, (sockaddr*)&server_addr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)   //int PASCAL FAR bind (_In_ SOCKET s,_In_reads_bytes_(namelen) const struct sockaddr FAR* addr,_In_ int namelen);
	{
		printf("服务器socket绑定失败\r\n");
		return -2;
	}

	// 4.开启服务器socket监听 , backlog:允许完成三次握手的客户端数量。

	if (listen(g_server_socket, 1) == SOCKET_ERROR)
	{
		printf("开启监听失败\r\n");
		return -3;

	}
	//int PASCAL FAR listen(_In_ SOCKET s,_In_ int backlog);
	return 0;
}