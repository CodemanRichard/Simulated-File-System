//定义一个整型变量sockfd，用来存储套接字描述符
int sockfd;
//调用socket函数创建一个套接字，使用IPv4协议族，面向连接的传输协议，协议号为0
sockfd = socket(AF_INET, SOCK_STREAM, 0);
//定义一个sockaddr_in结构体变量serv_addr，用来存储服务器的地址信息
struct sockaddr_in serv_addr;
//定义一个指向hostent结构体的指针变量host，用来存储主机信息
struct hostent *host;
//设置服务器地址的协议族为IPv4
serv_addr.sin_family = AF_INET;
//调用gethostbyname函数获取主机信息，参数为命令行参数中的第二个参数（即服务器的主机名）
host = gethostbyname(argv[1]);
//将主机信息中的IP地址复制到服务器地址结构体中
memcpy(&serv_addr.sin_addr.s_addr, host->h_addr, host->h_length);
//设置服务器地址的端口号，使用htons函数将主机字节序转换为网络字节序
serv_addr.sin_port = htons(BASIC_SERVER_PORT);
//调用connect函数连接到服务器
connect(sockfd, (sockaddr *) &serv_addr, sizeof(serv_addr);