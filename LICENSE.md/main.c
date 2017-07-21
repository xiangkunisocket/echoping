/*************************************************************************
  > File Name: main.c
  > Author: Echo
  > Mail: 1406451659@qq.com 
  > Created Time: 2015年07月15日 星期三 10时17分53秒
 ************************************************************************/

#include"congrobot.h"
#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<string.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netinet/in.h>
#include<netinet/ip.h>
#include<netinet/ip_icmp.h>
#include<errno.h>
#include<netdb.h>
#include<setjmp.h>
#include<sys/time.h> 
#include<pthread.h>
#include<getopt.h>

#define PACKET_SIZE 1024					/***数据报缓冲区大小****/
#define MAX_WAIT_TIME 5						/***阻塞等待时间****/
#define MAX_SEND_PACKETS 10000					/***发送数据报的数量****/

static float nMax = 0;
static float nMin = 0;
static float nAvg = 0;								/***平均值***/
static float nDev = 0;
static float nTime = 0;
static float  sTv_RTT[MAX_SEND_PACKETS];					/***每个数据报的时间差***/
static float nAll_Time = 0;
static INT32 nSend = 0;								/***发送报文个数***/
static INT32 nRecv = 0;								/***接受报文个数***/
static INT32 nDatelen = 56;						 	/***数据报的长度***/
pid_t pid;

static int sockfd;									/***套接字****/
int nCurrent_TTL;							/***获取系统TTL时间***/
int nSet_TTL;								/***设置系统TTL时间***/
static int isLive = 1;							
static int g_nTime_out = 10;					/***设置默认超时时间是10秒***/
static int g_nCount = 128;					/***默认发送包的大小是56个***/
static int nflag = 0;						/***标记是否设置了count参数***/

CHAR aSendBuf[PACKET_SIZE];					/***发送数据缓冲区***/
CHAR aRecvBuf[PACKET_SIZE];					/***接受缓冲区大小***/
CHAR *Addr;

struct sockaddr_in sFrom_Addr;			
struct sockaddr_in sDest_Addr;			    		 /***目的ip地址***/
struct timeval sTv_Begin;					 /***储存数据报发送时间***/
struct timeval sTv_End;			   			 /***储存结束时间**/
struct timeval sTv_Recv;


/****保存已经发送包的状态值***/
typedef struct sPingm_packet
{
	struct timeval sTv_begin;				/***发送的时间***/
	struct timeval sTv_end;					/***接受时间***/
	short seq;								/***记录序列号***/
	int flag;								/***1,表示已经发送但没有被接受
											   0，表示接收到回应报***/
}sPingm_packet;


/*****接口函数声明****/
static void ping_Prompt(void);						/***打印提示信息***/
static void ping_Statiscs(void);					/***统计结果****/
static void Computer_RTT(void);					/***统计时间****/
static UINT16 CheckSum(UINT16 *aAddr,int nLen);	/***计算效验和***/
static int SetIpHead(int seq);			/***设置icmp头部**/
static void *Send_Packet(void *temp);						/***发送数据报***/
static void *Recv_Packet(void *temp);						/***接受报文***/
int Unpack(char *aBuf,int nLen);				
/***解包***/
static struct timeval SubTime(struct timeval *sRecvTime,struct timeval *sSendTime);									/***计算时间差***/
static sPingm_packet *icmp_Find_Packet(int seq);
static sPingm_packet sPing_packet[MAX_SEND_PACKETS];	


/**************
 *
 *Auther: Echo
 *
 *function:查找合适的包的位置
 *
 *parameter:整数seq当seq为-1时 表示查找空包
 *					其他值表示查找seq对应的包
 *
 *return value:  返回结构体指针
 **************/
static sPingm_packet *icmp_Find_Packet(int seq)
{
	int i = 0;
	sPingm_packet *sFound = NULL;				
	if(seq == -1)    /***查找包的位置***/
	{
		for(i = 0;i < MAX_SEND_PACKETS; i++)
		{
			if(sPing_packet[i].flag == 0)
			{
				sFound = &sPing_packet[i];
				break;
			}
		}
	}
	else if(seq >= 0)     /**查找空包***/
	{
		for(i = 0;i < MAX_SEND_PACKETS; i++)
		{
			if(sPing_packet[i].seq == seq)
			{
				sFound = &sPing_packet[i];
				break;
			}
		}
	}
	return sFound;
}



/*********************
 *
 * auther: ECHO
 *
 * function:计算rtt的最大，最小，平均，算术平均数差 
 *
 * parameter: 无
 *
 * return value: 无
 * **********************/

static void Computer_Rtt(void)
{
	float nSum_Avg = 0;
	int i = 0;
	nMin = nMax = sTv_RTT[0];
	nAvg = nTime / nRecv;
	for(i = 0; i < nRecv; i++)
	{
		if(sTv_RTT[i] < nMin)
		{
			nMin = sTv_RTT[i];
		}
		else if(sTv_RTT[i] > nMax)
		{
			nMax = sTv_RTT[i];
		}

		if((sTv_RTT[i] - nAvg) < 0)
		  nSum_Avg += nAvg - sTv_RTT[i];
		else
		  nSum_Avg += sTv_RTT[i] - nAvg;
	}
	nDev = nSum_Avg/nRecv;
}



/****************************
 *
 * author:Echo
 *
 * fuction:校验和算法
 *
 * parameter:无符号短整型数组和整型长度
 *
 * retrun value: 返回效验和
 *
 * ***************************/
static UINT16 CheckSum(UINT16 *aAddr,int nLen)
{
	INT32 nLeft = nLen;
	INT16 nSum = 0;
	UINT16 *nW = aAddr;
	UINT16 *nCheck_Sum = 0;

	while(nLeft > 1)    
	{
		nSum += *nW++;
		nLeft -= 2;        
	}

	if(nLeft == 1)		/***icmp为奇数时，转换最后一个字节继续累加***/
	{
		*(UCHAR *)(&nCheck_Sum) = *(UCHAR *)nW;
		nSum += *nCheck_Sum;	
	}
	nSum = (nSum >> 16) + (nSum & 0xFFFF);
	nSum += (nSum >> 16);
	*nCheck_Sum = ~nSum;			/***取反得到效验和***/
	return *nCheck_Sum;			/***返回值***/
}

/***********************
 *
 * auther:Echo
 *
 * fuction:设置iP报文头部
 *
 * parameter: 报文个数
 *
 * return value:返回ip数据报的大小
 * *********************/
static int SetIpHead(int seq)
{
	int i = 0,nPacketSize = 0;
	struct icmp *icmp;     /***icmp报****/
	icmp = (struct icmp *)aSendBuf;	
	icmp->icmp_type = ICMP_ECHO;	/***设置icmp的类型是****/
	icmp->icmp_code = 0;
	icmp->icmp_cksum = 0;			/***将校验和用0填充***/
	icmp->icmp_seq = seq;		/***发送数据的编号***/
	
	icmp->icmp_id = pid;
	nPacketSize = 8 + nDatelen;		/**×数据的长度是64位***/
	/***填充数据部分***/
	for(i = 0; i < nDatelen; i++)
	  {
		icmp->icmp_data[i] = i;
	  }
	/****设置校验算法***/
	icmp->icmp_cksum = CheckSum((UINT16 *)icmp,nPacketSize);
	return nPacketSize;
}





/************************
 *
 *	auther:Echo
 *
 *  fuction:发送数据报
 *
 *  parameter:无
 *
 *  return value:返回icmp数据报的个数
 *
 * **************************/

static void *Send_Packet(void *temp)
{
	
	pthread_detach(pthread_self());
	INT32 nPacketSize;	
	gettimeofday(&sTv_Begin,NULL);		/**获取开始发送数据的时间***/
	
	while(isLive)
	{
	printf("----------------------------------------------------------------\n");
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 200;
		gettimeofday(&tv,NULL);
		nPacketSize =  SetIpHead(nSend);	/***设置将要发送的ip报文****/
		if(sendto(sockfd,aSendBuf,nPacketSize,0,(struct  sockaddr *)&sDest_Addr,sizeof(sDest_Addr)) < 0)				/***调用sendto函数****/
		{

			perror("sento error");
			continue;						
		}
		else
		{
			/***将发送的报暂时保存***/
			sPingm_packet *sPacket = icmp_Find_Packet(-1);
			if(sPacket)
			{
				sPacket->seq = nSend;
				sPacket->flag = 1;		/***标记为已经使用***/
				/***记录发送数据包的时间***/
				sPacket->sTv_begin.tv_sec = tv.tv_sec;
				sPacket->sTv_begin.tv_usec = tv.tv_usec;
				nSend++;				/***计数器****/
			}
		}
		sleep(1);
	}
	//pthread_exit(0);			/**线程退出***/
	return;
}


/**********************
 *	auther:Echo
 *
 *	fuction:接受所有数据报文
 *
 *	parameter:无
 *
 *	return value : 无
 * ***********************/
static void *Recv_Packet(void *temp)
{
	struct timeval  temp_tv;
	temp_tv.tv_sec = 0;
	temp_tv.tv_usec = 100;
	pthread_detach(pthread_self());
	int n = 0,nFromlen = 0;	
	int ret;
	nFromlen = sizeof(sFrom_Addr);
	while(isLive)        /****判断发送的数据和接受的数据是否相等**/
	{
		int reHAR;

	
		alarm( g_nTime_out );		/***设置超时信号***/
		
		
	
		if((n = recvfrom(sockfd,aRecvBuf,sizeof(aRecvBuf),0,(struct sockaddr *)&sFrom_Addr,&nFromlen)) < 0)
		{
			continue;
		}
		
		
       	    	ret = Unpack(aRecvBuf,n);			/***剥去icmp报头***/
		if(ret == -1)
		{
			continue;
		}
	}
	//pthread_exit(0);			/**线程退出***/
	return;
}


/**********************
 *
 * auther:Echo
 *
 * fuction:剥除icmp报文
 *
 * parameter: 
 *
 *return value;
 *
 * ***********************/

int Unpack(char *aBuf,int nLen)
{
	int nIphdrlen = 0;	/***获取iP头部长度***/
	struct ip *ip = NULL;
	struct icmp *icmp = NULL;
	float rtt = 0;			/**生存时间****/
	
	ip = (struct ip *)aBuf;
	nIphdrlen = ip->ip_hl << 2;		/***求iP报文头部的长度 即iP报文长度乘以4***/
	icmp = (struct icmp *)(aBuf + nIphdrlen);	/***越过ip头，指向icmp包头***/
	nLen -= nIphdrlen;

	if(nLen < 8)
	{
		printf("ICMP packet's length is less than 8\n");
		return -1;
	}
	/*********确保所接受的是所发的icmp报***********/
	if((icmp->icmp_type == ICMP_ECHOREPLY) && (icmp->icmp_id == pid))
	{
		struct timeval sTvsend;
		struct timeval  sTv_internel,sTv_Recv;
		
		/***在发送包的数组中查找已经发送的包***/
		sPingm_packet * sPacket = icmp_Find_Packet(icmp->icmp_seq);
		if(sPacket == NULL)
		{
			return -1;
		}
		
		/**取消标志**/
		sPacket->flag = 0;
		/***获取本包的发送时间***/
		sTvsend = sPacket->sTv_begin;
		/***读取此时时间，计算时间差***/
		gettimeofday(&sTv_Recv,NULL);
		sTv_internel = SubTime(&sTv_Recv,&sTvsend); /***接受和发送的时间差****/
		
	
		/*****以毫秒为单位计算ttl时间×******/
		rtt = sTv_internel.tv_sec*1000 + (float)sTv_internel.tv_usec/1000;
	
		printf("------------rtt=%f-------------\n",rtt);

		nTime += rtt;
		sTv_RTT[nRecv] = rtt;
		printf("%d bytes from : icmp_seq = %d ttl = %d time = %.3f ms\n",nLen,icmp->icmp_seq,ip->ip_ttl,rtt);
		nRecv++;				/***数据包的数量递增***/
		if(nRecv == g_nCount)
			{
				ping_Statiscs();
				exit(1);
			}
	}
	else
	{
		return -1;
	}
}


/**
 *
 *Auther:Echo
 *
 *Function:计算时间
 *
 * parameter:发送时间和接收时间
 *
 * return value：时间差
 ***/

static struct timeval SubTime(struct timeval *sRecvTime,struct timeval *sSendTime)
{
	struct timeval tv;
	/**计算差值***/
	tv.tv_sec = sRecvTime->tv_sec - sSendTime->tv_sec;
	tv.tv_usec = sRecvTime->tv_usec - sSendTime->tv_usec;
	if(tv.tv_usec < 0)
	{
		tv.tv_sec--;
		tv.tv_usec += 1000000;
	}
	return tv;
}


/**
 *Auther: EHCO
 *
 *Fuction:统计数据
 *
 *parameter:无
 *
 *return value: 无
 * **/
static void ping_Prompt(void)
{
	printf("cong_ping [distination] [-h 显示帮助信息] [-t 设置生存时间] [-c 限定count数目] [-w 设置超时时间]\n");
}

/**
 *	Auther:Echo
 *
 *	function:统计数目
 *	
 *	parameter:无
 *
 *	return value:无
 *
 * **/

static void ping_Statiscs(void)
{
	struct timeval stv_interval1;
	Computer_Rtt();								/***调用统计函数统计信息****/
	gettimeofday(&sTv_End,NULL);
	stv_interval1 = SubTime(&sTv_End,&sTv_Begin);
	nAll_Time = ( stv_interval1.tv_sec * 1000 ) + (stv_interval1.tv_usec/1000);
	printf("\n--------%s ping statiscs--------\n",(CHAR*)Addr);
	printf("%ld packets transmitted,%ld received,%ld%% packet loss,time%.f ms\n",nSend,nRecv,(nSend - nRecv)/nSend*100,nAll_Time);
	printf("rtt min/max/avg/mdev = %.3f/%.3f/%.3f/%.3f ms \n",nMin,nMax,nAvg,nDev);
}

/********
 *Auther :Echo
 *
 *function:信号处理函数
 *
 *parameter:无
 *
 * return value:无
 * **********/

static void int_handle(int signo)
{
	isLive = 0;
	printf("-----in_handle--islive=%d-----\n",isLive);
}

static void alarm_handle(int signo)
{
	isLive = 0;
	printf("-----ararm_handle--islive=%d-----\n",isLive);
}

struct option long_options[] = {
		{"ip",required_argument,NULL,'i'},
		{"count",required_argument,NULL,'c'},
		{"timeout",required_argument,NULL,'w'},
		{0,0,0}
	};


int main(int argc,char *argv[])
{
	int nParameter = 1;
	int err = 0;
	int nLength_TTL = sizeof(int);

	INT32 nSize = 50 * 1024;						/**扩展缓冲区的大小***/

	struct hostent *sHost;						
	struct protoent *sProtocol;
	Addr =argv[1];
	if(argc < 2)
	{
		printf("请输入参数\n");
		ping_Prompt();
		exit(1);			
	}

	int rn,nCh = 1; 
		


	if((sProtocol = getprotobyname("icmp")) == NULL)
	{
		perror("socket error");
		exit(1);
	}

	/***生成socket套接字***/
	if((sockfd = socket(AF_INET,SOCK_RAW,sProtocol->p_proto)) < 0)
	{
		perror("socket error");
		exit(1);
	}
	/***设置权限***/
//	setuid(getuid);
	setsockopt(sockfd,SOL_SOCKET,SO_RCVBUF,&nSize,sizeof(nSize));
	/***初始化套接字***/
	bzero(&sDest_Addr,sizeof(sDest_Addr));
	/***设置套接字***/
	sDest_Addr.sin_family = AF_INET;

if( argc == 2){
    nCh = 0;  
	if(argv[1][0] == '-')
	  {
		if(argv[1][1] == 'h' || argv[1][1] == 'c' || argv[1][1] == 't' || argv[1][1] == 'w')
		{
			ping_Prompt();
			return;
		}
	  }else{
		/***判断主机是否是ip地址****/
		if(inet_addr(argv[1]) == INADDR_NONE)
		{
			if((sHost = gethostbyname(argv[1])) == NULL)
			{
				perror("gethostbyname");
				exit(1);
			}
			memcpy((char*)&sDest_Addr.sin_addr,sHost->h_addr,sHost->h_length);
		}else
		{
			sDest_Addr.sin_addr.s_addr = inet_addr(argv[1]);
		}
	  }
	}
	else if(argc >= 3){
		nParameter = 3;
		while((rn = getopt_long(argc,argv,"w:c:t:h",long_options,NULL)) != -1)
		{
			switch(rn)
			{
				case 'w':
					{
						g_nTime_out = atoi( optarg );
				        if(g_nTime_out < 0 || g_nTime_out > 1024)
						{
							printf("请正确输入时间\n");
							return 0;
						}
						break;
					}
				case 'c':
					{
						g_nCount = atoi( optarg );
						if( g_nCount < 0)
						{
							printf("数据包的个数大于0\n");
						}
						break;
					}
				case 't':
					{
					
						int nflag = 1;
						nSet_TTL = atoi( optarg );
						getsockopt(sockfd,IPPROTO_IP,IP_TTL,&nCurrent_TTL,&nLength_TTL);		/***获取系统默认TTL***/
						
						setsockopt(sockfd,IPPROTO_IP,IP_TTL,&nSet_TTL,nLength_TTL);
						
						getsockopt(sockfd,IPPROTO_IP,IP_TTL,&nCurrent_TTL,&nLength_TTL);		/***获取系统默认TTL***/
						break;
					}
				default:
					{
						printf("请正确输入参数\n");
						break;
					}
				}
			}
	}

	if(nCh)
	{
		/***判断主机是否是ip地址****/
		if(inet_addr(argv[3]) == INADDR_NONE)
		{
			if((sHost = gethostbyname(argv[3])) == NULL)
			{
				perror("gethostbyname");
				exit(1);
			}
			memcpy((char*)&sDest_Addr.sin_addr,sHost->h_addr,sHost->h_length);
		}else
		{
			sDest_Addr.sin_addr.s_addr = inet_addr(argv[3]);
		}
	}
	
	pid = getpid();							/***获取当前进程pid号***/
	

	printf("PING %s(%s):%ld byte of data,\n",argv[nParameter],inet_ntoa(sDest_Addr.sin_addr),nDatelen);


	/****注册信号****/
	signal(SIGINT,int_handle);
	signal(SIGALRM,alarm_handle);		

	pthread_t nSend_id,nRecv_id;					/***创建两个线程***/
	/****多线程处理数据报的接收和发送****/
	printf("-------------------线程执行----------------------\n");
	err = pthread_create(&nSend_id,NULL,Send_Packet,NULL);
	if(err < 0)
	{
		printf("------------------错误发生-----------------------\n");
		return -1;
	}
	
	err = pthread_create(&nRecv_id,NULL,Recv_Packet,NULL);
	printf("------------------------线程2执行-------------------\n");
	if(err < 0)
	{
		printf("--------------------错误2发生---------------------\n");
		return -1;
	}
	/****等待线程结束****/


	pthread_join(nSend_id,NULL);
	printf("----------------------是否结束-------------------------\n");
	pthread_join(nRecv_id,NULL);



	if( nflag == 1 )
		setsockopt(sockfd,IPPROTO_IP,IP_TTL,&nSet_TTL,nLength_TTL);
		/***关闭套接字***/
	close(sockfd);
	ping_Statiscs();
	return 0;
}
