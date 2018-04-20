#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <time.h>


/*
* 函数声明
*/
static inline void exiterr(const char* reason = NULL, char status = 1);
static int writen(int fd, const char* str, int len);
static void startserv(int clinfd);
static void tmout(int sig);

/*
* 全局数据声明和初始化
*/
char** blacklist = NULL;//黑名单
int blsize = 0;//黑名单长度

const int bufsize = 1024, wbufsize = 10240;//规定缓冲区大小
char buf[bufsize], wbuf[wbufsize];//用于客户请求的读缓冲区和写缓冲区

//日志文件
const char* logfile = "proxy.log";

//超时
const int TMOUT = 5;

//400错误
const char* emsg_badreq ="\
HTTP/1.1 400 Bad Request\r\n\
\r\n\
<html>ProxyServer: Bad request</html>\r\n";

//405错误
const char* emsg_umethod = "\
HTTP/1.1 405 Method Not Allowed\r\n\
\r\n\
<html>ProxyServer: Method not allowed</html>\r\n";

//403错误
const char* emsg_forbidden = "\
HTTP/1.1 403 Forbidden\r\n\
\r\n\
<html>ProxyServer: This website is forbidden</html>\r\n";

//500错误
const char* emsg_internal = "\
HTTP/1.1 500 Internal Server Exception\r\n\
\r\n\
<html>ProxyServer: Internal server exception</html>\r\n";

//506错误
const char* emsg_uhost = "\
HTTP/1.1 506 Connect to Host Failed\r\n\
\r\n\
<html>ProxyServer: Can't connect to web host</html>\r\n";


/*
* main 函数
*/
int main(int argc, char*argv[])
{
  unsigned short lsnport = 11111;//监听的端口号
  int backlog = 50;//连接队列的最大长度

  //监听套接字, 客户连接套接字和日志文件描述字
  int lsnfd, clinfd, logfd;
  sockaddr_in lsnaddr;

  //分析命令行参数, 获取端口和黑名单
  if(argc>1)
  {
    int port = atoi(argv[1]);

    if(port > 0)
    {
      lsnport = port;
      blacklist = argv+2;
      blsize = argc - 2;
    }
    else
    {
      blacklist = argv+1;
      blsize = argc - 1;
    }
  }

  //服务器完成监听的一般性步骤
  memset(&lsnaddr, 0, sizeof(lsnaddr));
  lsnaddr.sin_family = AF_INET;
  lsnaddr.sin_port = htons(lsnport);
  lsnaddr.sin_addr.s_addr = htonl(INADDR_ANY);

  if((lsnfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    exiterr("socket error");

  if(bind(lsnfd, (sockaddr*)&lsnaddr, sizeof(lsnaddr)) < 0)
    exiterr("bind error");
  
  if(listen(lsnfd, backlog) < 0)
    exiterr("listen error");

  //忽略子进程中止信号和会晤期主席退出信号
  //由于Linux中signal函数是可靠的,
  //所以就不麻烦sigaction了
  signal(SIGCHLD, SIG_IGN);
  signal(SIGHUP, SIG_IGN);

  //转为Deamon进程
  if(fork() != 0)
    exit(0);

  setsid();

  if(fork() != 0)
    exit(0);

  //打开日志
  if((logfd = open(logfile, O_WRONLY|O_APPEND|O_CREAT, 0666)) < 0)
    exiterr("can't open logfile");

  //重定向输出
  dup2(logfd, 1);
  dup2(logfd, 2);

  //循环等待连接, 并启动子服务器
  while(true)
  {
    //接受客户连接
    if((clinfd = accept(lsnfd, NULL, NULL)) < 0)
    {
      if(errno == EINTR)
        continue;
      else
        exiterr("accept error");
    }
    //生成子进程
    if(fork() == 0)
    {
      close(lsnfd);
      startserv(clinfd);
      exit(0);
    }
    close(clinfd);
  }

  return 0;
}


/*
* 错误退出的包装函数
*
* 参数:
*     const char* reason: 要显示的消息
*     char status: 退出码
*/
static inline void exiterr(const char* reason/* = NULL*/, char status/* = 1*/)
{
  perror(reason);
  exit(status);
}


/*
* 写n个字节到fd
*
* 参数:
*     int fd: 描述字
*     const char* str: 要写的字符串
*     int len: 要写的长度
*
* 返回:
*     int: 成功返回实际写入的字节数, 失败返回-1
*
*/
static int writen(int fd, const char* str, int len)
{
  char* wstr = (char*)str;
  int nwrite = 0;
  int left = len;

  while(left > 0)
  {
    if((nwrite = write(fd, wstr, left)) <= 0)
    {
      if(errno == EINTR)
        nwrite = 0;
      else
        return -1;
    }
    left -= nwrite;
    wstr += nwrite;
  }

  return len;
}


/*
* 处理SIGALRM信号的句柄
* 该函数用于处理超时, 由于标准IO会自动重启
* 返回EINTR的IO系统调用, 所以只能在信号句柄里退出
*
* 参数:
*     int sig: 应该是SIGALRM信号
*/
static void tmout(int sig)
{
  puts("***Timed out***");
  exit(0);
}


/*
* 启动子服务器
*
* 参数:
*     int clinfd: 已连接的客户套接字
*/
static void startserv(int clinfd)
{
  int servfd = 0;//服务器连接套接字
  unsigned short servport = 80;//服务器端口
  sockaddr_in clinaddr, servaddr;//客户和服务器的IP地址结构
  socklen_t clinlen = sizeof(clinaddr);//客户IP地址结构的长度
  bool isPost = false;//标记是否是POST请求
  char* ptype = NULL, *paddr = NULL;//指向请求行中类型和URL字段的指针
  time_t tm= time(NULL); //时间

  strftime(buf, bufsize, "%Y-%m-%d %R", localtime(&tm));
  //得到客户的IP
  if(getpeername(clinfd, (sockaddr*)&clinaddr, &clinlen) < 0)
  {
    perror("getpeername error");
    //返回500错误
    writen(clinfd, emsg_internal, strlen(emsg_internal));
    exit(1);
  }
  //输出时间和IP
  printf("%s FROM <%s>:  ", buf, inet_ntoa(clinaddr.sin_addr));

  //为了便于每次读取一行,
  //将客户套接字关联到只读流
  FILE* fclin = fdopen(clinfd, "r");
  if(fclin == NULL)
  {
    perror("can't create stream for clinfd");
    //返回500错误
    writen(clinfd, emsg_internal, strlen(emsg_internal));
    exit(1);
  }

  //设置超时句柄
  signal(SIGALRM, tmout);
  alarm(TMOUT);
  //读取请求行
  if(fgets(buf, bufsize, fclin) == NULL)
  {
    puts("");
    exit(0);
  }
  alarm(0);

  //按空格将请求行分割为三部分
  ptype = strtok(buf, " ");
  paddr = strtok(NULL, " ");
  strtok(NULL, " ");

  //分析请求类型
  if(strcmp(ptype, "POST") == 0)
    isPost = true;
  //如果是不支持的请求, 则退出
  else if(strcmp(ptype, "GET") != 0 && strcmp(ptype, "HEAD") != 0)
  {
    //输出方法和地址
    if(paddr != NULL)
      printf("%s %s\n", ptype, paddr);
    else
      printf("%s\n", ptype);

    //返回405错误
    writen(clinfd, emsg_umethod, strlen(emsg_umethod));
    return;
  }

  //如果没有URL字段, 则退出
  if(paddr == NULL)
  {
    printf("%s \n", ptype);
    //返回400错误
    writen(clinfd, emsg_badreq, strlen(emsg_badreq));
    return;
  }

  //URL必须是如下格式: http://foo, 否则退出
  if(strlen(paddr) < 8 || strncmp(paddr, "http://", 7) != 0)
  {
    printf("%s %s\n", ptype, paddr);
    //返回400错误
    writen(clinfd, emsg_badreq, strlen(emsg_badreq));
    return;
  }

  //指向服务器域名的指针("http://"后的第一个位置)
  char* phost = paddr+7;

  //检查黑名单, 找到则返回403错误
  for(int i = 0; i<blsize; ++i)
    if(strncmp(phost, blacklist[i], strlen(blacklist[i])) == 0)
    {
      printf("FILTERED %s %s\n", ptype, paddr);
      writen(clinfd, emsg_forbidden, strlen(emsg_forbidden));
      return;
    }

  //输出客户请求
  printf("%s %s\n", ptype, paddr);

  //把服务器地址分割为主机域名和文件路径
  phost = strtok(phost, "/");
  char* ppath = strtok(NULL, "\0");

  //尝试找出指定的端口
  phost = strtok(phost, ":");
  char* pport = strtok(NULL, "\0");
  if(pport != NULL)
  {
    int port = 0;
    //端口格式错误则退出
    if((port = atoi(pport)) <= 0)
    {
      //返回400错误
      writen(clinfd, emsg_badreq, strlen(emsg_badreq));
      return;
    }
    servport = port;
  }

  //查询服务器的IP
  hostent* phe = gethostbyname(phost);
  if(phe == NULL || phe->h_addrtype != AF_INET)
  {
    //返回506错误
    writen(clinfd, emsg_uhost, strlen(emsg_uhost));
    return;
  }

  //初始化服务器的IP地址结构
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(servport);

  //尝试每个服务器IP直到连接成功
  for(int i = 0; true; ++i)
  {
    //无法连接则退出
    if(phe->h_addr_list[i] == NULL)
    {
      //返回506错误
      writen(clinfd, emsg_uhost, strlen(emsg_uhost));
      return;
    }

    //填充服务器IP地址结构的地址字段
    servaddr.sin_addr = *(in_addr*)phe->h_addr_list[i];

    //创建套接字, 失败则返回500错误并退出
    if((servfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
      perror("socket error");
      //返回500错误
      writen(clinfd, emsg_internal, strlen(emsg_internal));
      exit(1);
    }

    //连接成功则跳出循环
    if(connect(servfd, (sockaddr*)&servaddr, sizeof(servaddr)) == 0)
      break;
  }

  //输出请求行和Close标记到wbuf里
  if(ppath != NULL)
    sprintf(wbuf, "%s /%s HTTP/1.1\r\nConnection: Close\r\n", ptype, ppath);
  else
    sprintf(wbuf, "%s / HTTP/1.1\r\nConnection: Close\r\n", ptype);

  //逐行读取并处理客户请求直到遇到空行.
  //这里如果直接读到wbuf里效率显然要更高一些, 
  //但考虑到代码的可读性, 加之程序的瓶颈主要
  //在网络IO上,所以采用了两次缓冲方式
  alarm(TMOUT*6);
  int ctlen = 0;
  while(strcmp(buf, "\r\n") != 0)
  {
    //读一行
    if(fgets(buf, bufsize, fclin) == NULL)
      exiterr("fgets error");

    //如果没能读到一行则退出
    if(buf[strlen(buf)-1] != '\n')
    {
      //返回500错误
      writen(clinfd, emsg_internal, strlen(emsg_internal));
      return;
    }

    //忽略支持持续连接的行
    if(strncmp(buf, "Connection:", 11) == 0 ||
      strncmp(buf, "Proxy-Connection:", 17) == 0 ||
      strncmp(buf, "Keep-Alive:", 11) == 0)
      continue;

    //查找Connent-Length行, 读取实体的长度, 错误则退出
    if(isPost && strncmp(buf, "Content-Length:", 15) == 0 &&
      (ctlen = atoi(buf+15)) <= 0)
    {
      //返回400错误
      writen(clinfd, emsg_badreq, strlen(emsg_badreq));
      return;
    }

    //如果写缓冲区将要溢出, 则返回500错误
    if(strlen(buf)+strlen(wbuf) > (unsigned)wbufsize)
    {
      //返回500错误
      writen(clinfd, emsg_internal, strlen(emsg_internal));
      return;
    }

    //将刚读到的行添加到wbuf尾部
    strcat(wbuf, buf);
  }

  alarm(0);

  //处理POST请求
  if(isPost)
  {
    //记下当前wbuf内容的长度
    int wlen = strlen(wbuf);

    //如果写缓冲区将要溢出, 则返回500错误并退出
    if(wlen+ctlen+1 > wbufsize)
    {
      //返回500错误
      writen(clinfd, emsg_internal, strlen(emsg_internal));
      return;
    }

    alarm(TMOUT);
    //读取POST请求主部
    if(fread(wbuf + wlen, ctlen, 1, fclin) != 1)
    {
      //返回400错误
      writen(clinfd, emsg_badreq, strlen(emsg_badreq));
      return;
    }
    alarm(0);

    //构造字符串
    wbuf[wlen+ctlen] = '\0';
  }

  //发送请求到服务器
  if(writen(servfd, wbuf, strlen(wbuf)) < 0)
  {
    //返回500错误
    writen(clinfd, emsg_internal, strlen(emsg_internal));
    return;
  }

  //接收服务器应答, 并传送给客户.
  //我的策略是不进行存储转发(即等到缓冲区内数据达到足够量时再转发),
  //而是直接转发每次读到的服务器应答.
  //理由是, 如果服务器响应较快的话, 二者没什么区别. 
  //而如果服务器响应较慢的话, 等待数据到达将消耗大量的时间,
  //进行存储会使代理服务器长期阻塞在read上, 这样还不如利用
  //数据到达的这段时间向客户转发响应.
  int nread = 0;
  while(true)
  {
    if((nread = read(servfd, wbuf, wbufsize)) <= 0)
      return;

    if(writen(clinfd, wbuf, nread) < 0)
      return;
  }
}
