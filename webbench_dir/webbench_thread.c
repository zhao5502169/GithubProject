#include<stdio.h>
#include<unistd.h>
#include<sys/param.h>
#include<sys/wait.h>
#include<rpc/types.h>
#include<getopt.h>
#include<strings.h>
#include<time.h>
#include<signal.h>
#include<string.h>
#include<error.h>
#include<pthread.h>
#include"socket.c"
static void Usage(void){
    char useinfo[] =
        "webbench [选项参数]...URL\n"
        "-f\t --force\t\t\t不用等待服务器响应\n"
        "-r\t --reload\t\t\t重新请求加载(无缓存)\n"
        "-t\t --time <sec>\t\t\t运行时间，单位：秒，默认为30秒\n"
        "-p\t --proxy <IP:Port>\t\t使用代理服务器发送请求\n"
        "-c\t --clients <num>\t\t创建客户端个数,默认为1\n"
        "-9\t --http09\t\t\t使用http0.9协议\n"
        "-1\t --http10\t\t\t使用http1.0协议\n" 
        "-2\t --http11\t\t\t使用http1.1协议\n"
        "\t --get\t\t\t\tGET请求方式\n"
        "\t --head\t\t\t\tHEAD请求方式\n"
        "\t --options\t\t\tOPTIONS请求方式\n"
        "\t --trace\t\t\tTRACE请求方式\n"
        "-?|-h\t --help\t\t\t\t显示帮助信息\n"
        "-v\t --version\t\t\t显示版本信息\n"
    ;
    fprintf(stderr,"%s",useinfo);
}
/*http请求方法的相关宏定义*/
#define METHOD_GET (0)
#define METHOD_HEAD (1)
#define METHOD_OPTIONS (2)
#define METHOD_TRACE (3)

/*代码相关的宏定义*/
#define bool int
#define true (1)
#define false (0)
#define PROGRAM_VERSION "1.5"
/*缓冲流大小相关的宏定义*/
#define REQUEST_SIZE (2048)
#define BUF_SIZE (1024)
#define THREAD_SIZE (100010)
/*选项相关变量与其默认值*/
int method = METHOD_GET;
int clients = 1;
bool force = false;
bool reload = false;
int proxy_port = 80;
char* proxyhost = NULL;//默认不使用代理
int bench_time = 30;
int http = 1;//http协议版本，0:http0.9 1:http1.0 2:http1.1
/*结果统计相关的变量*/
int speed = 0;
int failed = 0;
long long int byte_counts = 0;
volatile int timeout = 0;

int thread_count = 0;
/*进程管道*/
int pipe_buf[2];
/*服务器网络地址*/
char host[MAXHOSTNAMELEN];//这个宏在rpc/types.h下

/*请求消息*/
char request[REQUEST_SIZE];

/*线程ID*/
pthread_t tid[THREAD_SIZE];
/*长选项数组*/
static const struct option long_options[] = {
    {"force",no_argument,&force,true},
    {"reload",no_argument,&reload,true},
    {"time",required_argument,NULL,'t'},
    {"help",no_argument,NULL,'h'},
    {"http09",no_argument,&http,0},
    {"http10",no_argument,&http,1},
    {"http11",no_argument,&http,2},
    {"get",no_argument,&method,METHOD_GET},
    {"head",no_argument,&method,METHOD_HEAD},
    {"options",no_argument,&method,METHOD_OPTIONS},
    {"trace",no_argument,&method,METHOD_TRACE},
    {"version",no_argument,NULL,'v'},
    {"proxy",required_argument,NULL,'p'},
    {"clients",required_argument,NULL,'c'},
    {NULL,0,NULL,0}
};
/*相关函数*/
void build_request(const char*url);//构造请求消息
static int bench();/*压力测试*/
static void alarm_handler(int signal);/*闹钟信号处理*/
void benchcore(void* arg);
void error_handling(const char* message);
int main(int argc,char* argv[]){
    int opt = 0;
    int options_index = 0;
    char *temp = NULL;

    if(argc == 1){
        Usage();
        exit(1);
    }
    /*短参数选项*/
    const char optstring[] = "frt:h?912vp:c:";
    
    while((opt = getopt_long(argc,argv,optstring,long_options,&options_index)) != EOF){
        switch(opt){
            case 0:
                break;//参数值被放入对应变量，返回
            case 'f':
                force = true;
                break;
            case 'r':
                reload = true;
                break;
            case 't':
                bench_time = atoi(optarg);
                break;
            case 'h':
            case '?':
                Usage();
                exit(0);//带有该参数，程序会直接结束
            case '9':
                http = 9;break;
            case '1':
                http = 1;break;
            case '2':
                http = 2;break;
            case 'v':
                fprintf(stdout,PROGRAM_VERSION "\n");
                exit(0);
            case 'p':
                /*代理服务器格式 server:port*/
                temp = strrchr(optarg,':');/*返回最后一个':',的位置*/
                proxyhost = optarg;
                if(temp == NULL)    break;
                if(temp == optarg){
                    fprintf(stderr,"Error in option --proxy %s:缺少主机名",optarg);
                    exit(1);
                }
                if(temp == optarg+strlen(optarg)-1){
                    fprintf(stderr,"Error in option --proxy %s:缺少端口号",optarg);
                    exit(1);
                }
                *temp = '\0';
                proxy_port = atoi(temp+1);
                break;
            case 'c':
                clients = atoi(optarg);
                break;
        }
    }
    //测试getopt函数
    fprintf(stdout,"force = %d,reload = %d,bench_time = %d,http = %d,proxyhost = %s,proxy_port = %d,clients = %d\n",force,reload,bench_time,http,proxyhost,proxy_port,clients);
    if(optind == argc){
        fprintf(stderr,"webbench:没有链接URL\n");
        Usage();
        exit(0);
    }
    /*考虑用户输入错误的情况,使用默认值*/
    if(clients <= 0)
        clients = 1;
    if(bench_time <= 0)
        bench_time = 30;
    /*构造请求消息*/ 
    build_request(argv[optind]);
    /**/
    fprintf(stdout,"Webbench --一款网站压力测试程序\n");
    fprintf(stdout,"测试链接:%s\n",argv[optind]);
    fprintf(stdout,"运行信息:");
    fprintf(stdout,"%d 客户端, 运行 %d 秒",clients,bench_time);
    if(force)
        fprintf(stdout,",提前关闭连接");
    if(proxyhost != NULL)
        fprintf(stdout,"，使用代理服务器 %s:%d",proxyhost,proxy_port);
    if(reload)
        fprintf(stdout,",无缓存连接");
    fprintf(stdout,".\n");

    return bench();
}

void build_request(const char *url){
    char temp[10];
    /**/
    memset(host,0,MAXHOSTNAMELEN);
    memset(request,0,REQUEST_SIZE);
   
    /*
        http0.9只有get这一种方法,不支持缓存机制.
        http1.0有get,post,head三种方法，支持长连接，缓存机制
        http1.1 四种方法都有.
    */
    if(reload && proxyhost != NULL && http == 0)
        http = 1;
    if(method == METHOD_HEAD && http == 0)
        http = 1;
    if(method == METHOD_OPTIONS && http < 2)
        http = 2;
   
    switch(method){
        default:
        case METHOD_GET:
            strcpy(request,"GET ");
            break;
        case METHOD_HEAD:
            strcpy(request,"HEAD ");
            break;
        case METHOD_OPTIONS:
            strcpy(request,"OPTIONS ");
            break;
        case METHOD_TRACE:
            strcpy(request,"TRACE ");
            break;
    }
    
    if(strstr(url,"://") == NULL){
        fprintf(stderr,"\n%s 不是一个合法的URL",url);
        exit(2);
    }
    if(strlen(url) > 1000){
        fprintf(stderr,"URL 长度超过合法值");
        exit(2);
    }
    if(strncasecmp("http://",url,7) != 0){
        fprintf(stderr,"只有http协议是被支持的.");
        exit(2);
    }
    /*主机名起始位置*/
    int pos = strstr(url,"://")-url+3;
    
    if(strchr(url+pos,'/') == NULL){
        fprintf(stderr,"url不合法，地址结束位置必须是 '/'.\n");
        exit(2);
    }
    /*未使用代理服务器时*/
    if(proxyhost == NULL){
        /*获取域名和端口*/
        if(index(url+pos,':') != NULL && index(url+pos,':') < index(url+pos,'/')){
            strncpy(host,url+pos,strchr(url+pos,':')-url-pos);

            memset(temp,0,sizeof(temp));
            /*这里主要要只获得数字部分,否则会有段错误*/
            strncpy(temp,index(url+pos,':')+1,strchr(url+pos,'/')-index(url+pos,':')-1);
            proxy_port = atoi(temp);
            if(proxy_port == 0)
                proxy_port = 80;
        }
        else{
             strncpy(host,url+pos,strchr(url+pos,'/')-url-pos);
//            strncpy(host,url+pos,strcspn(url+pos,"/"));
        }
        /*向request写入请求页,如/index.html */
        strcat(request,strchr(url+pos,'/'));
  //      strcat(request+strlen(request),url+pos+strcspn(url+pos,"/"));
    }
    else{
        strcat(request,url);
    }
    if(http == 1)
        strcat(request," HTTP/1.0");
    else if(http == 2)
        strcat(request," HTTP/1.1");
    strcat(request,"\r\n");
    /*请求消息的请求行，到这里已经填写完成*/

    if(http > 0){
        strcat(request,"User-Agent:webbench " PROGRAM_VERSION "\r\n");
    }
    if(proxyhost == NULL && http > 0){
        strcat(request,"Host: ");
        strcat(request,host);
        strcat(request,"\r\n");
    }
    if(reload && proxyhost == NULL){
        strcat(request,"Pragma: no-cache\r\n");
    }
    if(http > 1){
        strcat(request,"Connection: close\r\n");
    }
    /*消息头填写完毕*/
    if(http > 0)
        strcat(request,"\r\n");
    fprintf(stdout,"\nrequest:\n%s\n",request);
    return;
}

static int bench(){
    
    int sock = my_socket(proxyhost == NULL?host:proxyhost,proxy_port);
    if(sock < 0){
        fprintf(stderr,"\n连接server失败.");
        return 1;
    }
    close(sock);
    for(int i=0;i<clients;++i){
        if(pthread_create(&tid[i],NULL,(void*)benchcore,NULL) != 0){
            fprintf(stderr,"\n创建线程失败");
            return 1;
        }    
    }
    for(int i=0;i<clients;++i){
        if(pthread_join(tid[i],NULL) != 0){
            fprintf(stderr,"\njoin thread 错误");
            return 1;
        }
    }
    fprintf(stdout,"速度: %d 请求/分, %d 字节/秒.\n请求: %d 成功, %d 失败.\n",
           (int)((speed+failed)/(bench_time/60.0f)),
            (int)((byte_counts*1.0)/bench_time),
            speed,
            failed);
    fprintf(stdout,"%d 成功,%d 失败\n",speed,failed);
    return 0;
}

void benchcore(void* arg){
    const int port = proxy_port;
    const char* req = request;
    int rlen;
    char buf[BUF_SIZE];
    struct sigaction act;
    
    act.sa_handler = &alarm_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    if(sigaction(SIGALRM,&act,0) != 0){
        exit(3);
    }
    alarm(bench_time);

    rlen = strlen(req);
    
    while(true){
        bool have_close = false;//用来判断套接字是否已经关闭.
        if(timeout){
            if(failed > 0)
                failed--;
            return;
        }
        int sock =my_socket(host,port);
        if(sock < 0){
            failed++;
            continue;
        }
        if(write(sock,req,rlen) != rlen){
            failed++;
            close(sock);
            continue;
        }
        /*http0.9在受到服务器回复后会自动断开，没有keep-alive
        * 我们可以用shutdown把套接字的输出流关闭，如果关闭失败，那么证明这是一个失败的套接字
        * */
        if(http == 0 && shutdown(sock,SHUT_WR) == -1){
            failed++;
            close(sock);
            continue;
        }
        /*接收服务器的回应.*/
        if(force == false){
            while(true){
                if(timeout)
                    break;
                int read_len = read(sock,buf,BUF_SIZE);

                if(read_len < 0){
                    failed++;
                    close(sock);
                    have_close = true;
                    break;
                }
                else if(read_len == 0){
                    break;
                }
                else{
                    byte_counts += read_len;
                }
            }
        }
        if(have_close == false && close(sock) != 0){
            failed++;
            continue;
        }
        speed++;
    }
    return;
}
static void alarm_handler(int signal){
    if(signal == SIGALRM)
        timeout = 1;
    else if(signal == SIGCHLD){
        int status;
        pid_t pid = waitpid(-1,&status,WNOHANG);
    }
}
