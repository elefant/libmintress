/* 
 * File:   dshlsbw.c
 * Author: Varadhan Venkataseshan 
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <endian.h>  //man 3 endian
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <math.h>
#include "dshlsbw.h"
#include "utl/LoggerC.h"

#ifdef __cplusplus
extern "C"
{
#endif
typedef char S8;
typedef unsigned char U8;
typedef short int S16;
typedef unsigned short int U16;
typedef int S32;
typedef unsigned int U32;
typedef long long int S64;
typedef unsigned long long int U64;

#define NUM_BITRATES 16
static unsigned int g_sorted_allowed_bitrates[NUM_BITRATES+1] =
                    {128000,256000,512000,1024000,1024000,1024000,1024000,0} ;

static U32 g_minBps ;
static U32 g_maxBps ;
static U32 g_optBps ;

static S32 g_SegmentLenSecs = 0 ;
static U8 g_num_bitrates = 4 ;

typedef struct HlsClientInfo
{
 volatile U32 key ;
 volatile int tsaccess ;
 volatile int tstart ;

 U32 BaseRtt;
 U32 i_snd_mss ;
 U32 i_unacked ;
 U32 i_lost ;
 U32 i_retrans ;
 U32 i_total_retrans ;
 U32 i_rtt_ms ;
 U32 p_rtt_ms ;
 U32 f_rtt_ms ;
 U32 lf_rtt_ms ;
 U32 i_snd_ssthresh ;
 U32 i_snd_cwnd ;
 U32 advmss; 

 U32 initialBwe;
 U32 first_Bwe;
 U32 first_rtt_ms;

 U32 i_Txbps;
 U32 i_tdiff;
 U32 i_aCwndBwe;
 U32 i_eCwndBwe;
 U32 i_CSFQCwndBwe;
 U32 p_aCwndBwe;

 U32 p_ts ;
 U32 i_ts ;

 U32 pendingOutq;
 U32 large_samples;
 U32 msSegStart;
 U32 incReq;
 U32 decReq;
 int video_rate_bps; 
 int cursd;
 int bpsSwitched ;
 int prevbpsSwitched ;
 U32 timeSwitched ;
 U32 trendts ;
 U32 trendMinBps ;
 U32 trendMaxBps ;
 U32 log_once;
 #define TREND_LOW_TO_HIGH 1
 #define TREND_HIGH_TO_LOW 2
 U32 curTrend ; 
 U32 lossType;
 U32 LowToHigh ;
 U32 HighToLow ;
 U32 inOptBpsTrend;
 U32 sub_HighToLow ;
 U32 sub_LowToHigh ;
 U32 lvl_tried ;
 float diffV ; 
 struct sockaddr_in ac ;
 S16 idx;

} HlsClientInfo;

typedef struct HLSInfo
{
  #define MAX_STREAMS_SLOT 5000
  HlsClientInfo cls[MAX_STREAMS_SLOT] ;
  struct timeval StartTime ;

}HLSInfo;

#define GET_CSI_SLOT_SD_IP(sd,tnow,ci,rc)                         \
        {                                                         \
           struct sockaddr_in ac ;                                \
           socklen_t ac_len = sizeof(struct sockaddr);            \
           rc = getpeername(sd, (struct sockaddr *)&ac, &ac_len); \
           if(rc == 0)   {                                        \
              tnow = hs_getmstime(&g_hls.StartTime) ;             \
              ci = hs_get_client_slot(&g_hls, ac.sin_addr.s_addr, tnow) ;        \
              if(ci && (ci->key == ac.sin_addr.s_addr)) {         \
                ci->ac.sin_port = ac.sin_port ;                   \
                ci->ac.sin_addr.s_addr = ac.sin_addr.s_addr; }    \
              else {  rc = -1 ; }   }                             \
        }

#define COPY_SD_IP_PORT(sd, ci)                                       \
        {                                                             \
          struct sockaddr_in ac ;                                     \
          socklen_t ac_len = sizeof(struct sockaddr);                 \
          if(!getpeername(sd, (struct sockaddr *)&ac, &ac_len))  {    \
              ci->cursd = sd ;                                        \
              ci->ac.sin_port = ac.sin_port ;                         \
              ci->ac.sin_addr.s_addr = ac.sin_addr.s_addr ; }         \
        }

#define MAX_CHANNEL_ID 65535
#define GET_CSI_SLOT_CHANNEL(channel,tnow,ci,rc)                  \
        {                                                         \
           if((channel > 0) && (channel < MAX_CHANNEL_ID))  {     \
              tnow = hs_getmstime(&g_hls.StartTime) ;             \
              ci = hs_get_client_slot(&g_hls, channel, tnow) ;    \
              rc = ((!ci) || (ci->key != channel)) ? -1 : 0 ; }   \
           else { rc = -1 ; }                                     \
        }


//GetTimeMS() gives time in millisecond since the first call
//to it  with pStartTime. It basically reduces the Epoch, so that,
//millisecond fits into 32 bit variable
static int hs_getmstime(struct timeval *pStartTime);
static int hs_get_socket_outq_bytes(int sd);
static HlsClientInfo * hs_get_client_slot(HLSInfo * hs, U32 key, int tnow);
static int hs_get_tcp_info(struct tcp_info *ti , int sd);
static int hs_tcp_send(int sd, char *TcpBuff, int Size, int Opts);
#define SOCK_BUF_SET_IF_LARGE 1
static int hs_set_snd_buff(int sd, int val, int *pCurSz, U8 flag);
#define BPS_SAME_OR_LESS 0
#define BPS_SAME_OR_MORE 1
static U32 hs_get_config_allowed_bps(U32 *allowed_video_bitrates, int tnum,
                                  U32 reqbps, U8 flag, U32 *cfgIndex);
static U32 get_core_stateless_fair_queue_estimation(HlsClientInfo * ci);

static int get_enhanced_vegas_lod_predictor(HlsClientInfo * ci);

static int get_trend_bps(HlsClientInfo * ci, U32 tnow, U32 bps,
                                         U32 freq, U32 min, U32 max);

static int cmp_int(const void *p1, const void *p2);

static int copy_cfg_bitrates(int *allowed_video_bitrates, 
                               int allowed_video_bitrates_num);
#define MEDIA_LVL_SEC_NONE 0
#define MEDIA_LVL_SEC_MIN  3
#define MEDIA_LVL_SEC_LOW  6
#define MEDIA_LVL_SEC_OPT  12
#define MEDIA_LVL_SEC_MAX  20

static int check_media_lvl(HlsClientInfo * ci, U32 tnow, U32 mlvl);
//static and global variables
static HLSInfo g_hls ;


static int hs_getmstime(struct timeval *pStartTime)
{
   int tsms = 0;
   struct timeval t;

   gettimeofday(&t, NULL);
   if(!pStartTime->tv_sec)
   {
      pStartTime->tv_sec = t.tv_sec ;
      pStartTime->tv_usec = t.tv_usec ; 
   }

   t.tv_sec = t.tv_sec - pStartTime->tv_sec;

   tsms  = (t.tv_sec * 1000L) ;
   tsms += (t.tv_usec / 1000L) ;

   return tsms ;
}

static int hs_set_snd_buff(int sd, int val, int *pCurSz, U8 flag)
{
     int bufsize = 0 ;
     socklen_t len = sizeof(bufsize);

     if ( val <= 0)
      return -1 ;

     getsockopt(sd, SOL_SOCKET, SO_SNDBUF, &bufsize, &len);

     //printf("current value: socket sd => %d , "
     //                   "SO_SNDBUF size => %d bytes\n",sd,bufsize);

     if((flag == SOCK_BUF_SET_IF_LARGE) && (bufsize > val))
       return 0 ;

     if(pCurSz)
       *pCurSz = bufsize ;

     if( setsockopt(sd, SOL_SOCKET, SO_SNDBUF, &val, len) < 0 )
     {
       utl_error("%s() setsockopt SO_SNDBUF failed "
                   "(sd = %d , buff = %d ): err=%d, %s\n\n",
                  __FUNCTION__,sd,val, errno, strerror(errno));
       return -1 ;
     }
     getsockopt(sd, SOL_SOCKET, SO_SNDBUF, &bufsize, &len);

     if(pCurSz)
       *pCurSz = bufsize ;

     //printf("%s() new value: socket[%d] Requested[%d] , "
     //        "SO_SNDBUF size => %d bytes\n\n",__FUNCTION__,sd,val,bufsize);

     return 0 ;
}

static int hs_get_socket_outq_bytes(int sd)
{
   int pbytes = 0 ; 
   int rc = 0;
   rc = ioctl(sd, TIOCOUTQ, &pbytes);
   if(rc == 0)
    return pbytes;
   else
    return -1;
}

static int hs_get_tcp_info(struct tcp_info *ti , int sd)
{
   int rc = 0 ;
   int ilen = sizeof(struct tcp_info);

#if 0
struct tcp_info
{
  u_int8_t	tcpi_state;
  u_int8_t	tcpi_ca_state;
  u_int8_t	tcpi_retransmits;
  u_int8_t	tcpi_probes;
  u_int8_t	tcpi_backoff;
  u_int8_t	tcpi_options;
  u_int8_t	tcpi_snd_wscale : 4, tcpi_rcv_wscale : 4;

  u_int32_t	tcpi_rto;
  u_int32_t	tcpi_ato;
  u_int32_t	tcpi_snd_mss;
  u_int32_t	tcpi_rcv_mss;

  u_int32_t	tcpi_unacked;
  u_int32_t	tcpi_sacked;
  u_int32_t	tcpi_lost;
  u_int32_t	tcpi_retrans;
  u_int32_t	tcpi_fackets;

  /* Times. */
  u_int32_t	tcpi_last_data_sent;
  u_int32_t	tcpi_last_ack_sent;	/* Not remembered, sorry.  */
  u_int32_t	tcpi_last_data_recv;
  u_int32_t	tcpi_last_ack_recv;

  /* Metrics. */
  u_int32_t	tcpi_pmtu;
  u_int32_t	tcpi_rcv_ssthresh;
  u_int32_t	tcpi_rtt;
  u_int32_t	tcpi_rttvar;
  u_int32_t	tcpi_snd_ssthresh;
  u_int32_t	tcpi_snd_cwnd;
  u_int32_t	tcpi_advmss;
  u_int32_t	tcpi_reordering;

  u_int32_t	tcpi_rcv_rtt;
  u_int32_t	tcpi_rcv_space;

  u_int32_t	tcpi_total_retrans;
};
#endif
   rc =  getsockopt(sd, SOL_TCP, TCP_INFO,
                      (void *)ti, (socklen_t *)&ilen);
   if(rc < 0)
     return -1 ;


#if 0
printf(
  "tcpi_state\t=>\t%u \n"
  "tcpi_retransmits\t=>\t%u \n"
  "tcpi_snd_wscale\t=>\t%u \n"

  "tcpi_rto\t=>\t%u \n"
  "tcpi_ato\t=>\t%u \n"
  "tcpi_snd_mss\t=>\t%u \n"

  "tcpi_unacked\t=>\t%u \n"
  "tcpi_sacked\t=>\t%u \n"
  "tcpi_lost\t=>\t%u \n"
  "tcpi_retrans\t=>\t%u \n"
  "tcpi_fackets\t=>\t%u \n"

  /* Times. */
  "tcpi_last_data_sent\t=>\t%u \n"
  "tcpi_last_data_recv\t=>\t%u \n"
  "tcpi_last_ack_recv\t=>\t%u \n"

  /* Metrics. */
  "tcpi_pmtu\t=>\t%u \n"
  "tcpi_rcv_ssthresh\t=>\t%u \n"
  "tcpi_rtt\t=>\t%u \n"
  "tcpi_rttvar\t=>\t%u \n"
  "tcpi_snd_ssthresh\t=>\t%u \n"
  "tcpi_snd_cwnd\t=>\t%u \n"
  "tcpi_advmss\t=>\t%u \n"
  "tcpi_reordering\t=>\t%u \n"
  "tcpi_rcv_rtt\t=>\t%u \n"
  "tcpi_rcv_space\t=>\t%u \n"
  "tcpi_total_retrans\t=> \t%u\n\n",
  ti.tcpi_state,  ti.tcpi_retransmits,  ti.tcpi_snd_wscale, ti.tcpi_rto, ti.tcpi_ato,
  ti.tcpi_snd_mss, ti.tcpi_unacked, ti.tcpi_sacked, ti.tcpi_lost,
  ti.tcpi_retrans, ti.tcpi_fackets, ti.tcpi_last_data_sent, 
  ti.tcpi_last_data_recv,  ti.tcpi_last_ack_recv, ti.tcpi_pmtu, 
  ti.tcpi_rcv_ssthresh, ti.tcpi_rtt, ti.tcpi_rttvar, ti.tcpi_snd_ssthresh,
  ti.tcpi_snd_cwnd, ti.tcpi_advmss, ti.tcpi_reordering,ti.tcpi_rcv_rtt,ti.tcpi_rcv_space,
  ti.tcpi_total_retrans);
#endif

  return 0 ;
}

#define HLS_MIN(x,y) (((x) < (y)) ? (x) : (y))

#define OUT_OF_BOUND_RATE 20480000

#define HS_MEDIA_BEST_RATE  2048000
#define HS_RTT_BEST(hsrtt) ((hsrtt) <= 10)

#define HS_MEDIA_BETTER_RATE  1024000
#define HS_RTT_BETTER(hsrtt) (((hsrtt) > 10) && (hsrtt) <= 30)

#define HS_MEDIA_GOOD_RATE  768000
#define HS_RTT_GOOD(hsrtt) (((hsrtt) > 30) && (hsrtt) <= 50)

#define HS_MEDIA_REASONABLE_RATE  512000
#define HS_RTT_REASONABLE(hsrtt)   (((hsrtt) > 50) && (hsrtt) <= 70)
#define HS_RTT_REASONABLE_ABOVE(hsrtt)   ((hsrtt) <= 70)

#define HS_MEDIA_OK_RATE  256000
#define HS_RTT_OK(hsrtt)   (((hsrtt) > 70) && (hsrtt) <= 120)

#define HS_MEDIA_POOR_RATE  128000
#define HS_RTT_POOR(hsrtt) (((hsrtt) > 120) && (hsrtt) <= 320)

#define HS_MEDIA_BAD_RATE  96000
#define HS_RTT_BAD(hsrtt) ((hsrtt) > 320)

#define HS_MATCH_BPSRTT(bps , rtt) {\
 if(HS_RTT_BEST(rtt)) \
   bps = HS_MEDIA_BEST_RATE; \
 else if(HS_RTT_BETTER(rtt)) \
   bps = HS_MEDIA_BETTER_RATE; \
 else if(HS_RTT_GOOD(rtt)) \
   bps = HS_MEDIA_GOOD_RATE; \
 else if(HS_RTT_REASONABLE(rtt)) \
   bps = HS_MEDIA_REASONABLE_RATE; \
 else if(HS_RTT_OK(rtt)) \
   bps = HS_MEDIA_OK_RATE; \
 else if(HS_RTT_POOR(rtt)) \
    bps = HS_MEDIA_POOR_RATE; \
 else if(HS_RTT_BAD(rtt)) \
    bps = HS_MEDIA_BAD_RATE; \
 else \
   { bps = 0; } }\

//stale location
#define IDLE_TIME_MS 600000 //600 seconds

#define OCCUPY_STALE_SLOT(ci, key , tnow) \
{ \
   if((ci->tsaccess < tnow) && \
       ((tnow - ci->tsaccess) > IDLE_TIME_MS)) { \
           utl_trace("%s() using idle slot[%d] \n",__FUNCTION__,ci->idx); \
           memset(ci,0,sizeof(HlsClientInfo)); \
           ci->key = key ; \
           ci->tstart = tnow ; \
           ci->tsaccess = tnow ; } \
}

static HlsClientInfo * hs_get_client_slot(HLSInfo * hs, U32 key, int tnow)
{
   HlsClientInfo *ci = NULL;
   HlsClientInfo *found_ci = NULL;
   S32 idx = 0 ;
   S32 i = 0 ;
   U8 free_available = 0;

   //TODO implement faster and better data structure for client slot
   if((!hs) || (!key) || (!tnow))
     return NULL ;

   //For now just use simple modulo to get slot
   idx = key % MAX_STREAMS_SLOT;

   ci = &hs->cls[idx];
   if(!ci->key)
   {
     //new location
      memset(ci,0,sizeof(HlsClientInfo));
      ci->key = key ;
      ci->tstart = tnow ;
      ci->idx = idx ;
      ci->tsaccess = tnow ;
      found_ci = ci ;
      goto found;
   }

   if(ci->key == key)
   {
     //Existing location
     OCCUPY_STALE_SLOT(ci, key , tnow)
     found_ci = ci ;
     ci->idx = idx ;
     ci->tsaccess = tnow ;
     goto found;
   }

   //Do a linear search and find the slot
   for(i = 0 ; i < MAX_STREAMS_SLOT; i++)
   {
       ci = &hs->cls[i];
       if(ci->key == key)
       {
          printf("%s() Returning insue slot[i=%u]after searching",
            __FUNCTION__,i);
          OCCUPY_STALE_SLOT(ci, key , tnow)
          ci->idx = i ;
          ci->tsaccess = tnow ;
          found_ci = ci ;
          goto found;
       }

       if(!free_available)
       {
          if((!ci->key) ||
             ((ci->tsaccess < tnow) &&
             ((tnow - ci->tsaccess) > IDLE_TIME_MS)))
          {
              free_available = 1;
              idx = i ; //note down first free
          }
       }

    }//end of for search loop

    //use free slot if available
    if(free_available)
    {
        ci = &hs->cls[idx];
        memset(ci,0,sizeof(HlsClientInfo));
        ci->key = key ;
        ci->tsaccess = tnow ;
        ci->idx = idx ;
        printf("%s() Returning free slot[idx=%u]after searching",
            __FUNCTION__,idx);
        found_ci = ci ;
        goto found;
    }

    found:

      return found_ci;
}

static int hs_tcp_send(int sd, char *TcpBuff, int Size, int Opts)
{
   int rc2 = 0;

   if((!TcpBuff) || (!Size))
   {
      return -1;
   }
   while((Size > 0) && (rc2 < Size))
   {
      //tcp send responce
      rc2 = send(sd, TcpBuff, Size, Opts);
      if(rc2 <= 0)
      {
         if((errno == EAGAIN) || (errno == EINTR) || (errno == EWOULDBLOCK))
         {
            struct timespec tslp = {0, 5000000}; //5 ms sleep when interrupted
            utl_trace("%s() send err %d, %s, retrying after 5ms sleep\n", 
                       __FUNCTION__,errno, strerror(errno));
            nanosleep(&tslp, NULL);
            errno = 0 ;
            continue;
         }
         else
         {
            //close down the connection and update flags
            if(rc2 == 0)
               utl_trace("send err, rc = 0 \n");
            else
               utl_trace("send err %d, %s\n", errno, strerror(errno));

            return rc2;
         }
      }
      else if(rc2 < Size)
      {
         //printf("sent less bytes req=%d , sent=%d \n", Size, rc2);
         TcpBuff += rc2;
         Size -= rc2;
         rc2 = 0;
         continue;
      }

      break;                    //rc2 == Size , sent case
   }
   return rc2;
}

static U32 hs_get_config_allowed_bps(U32 *allowed_video_bitrates, int tnum,
                                  U32 reqbps, U8 flag, U32 *cfgIndex)
{
   U32 rc = 0  ;
   S32 Index = -1 ;
   int b = 0 ;
   if(tnum  && (tnum <= NUM_BITRATES))
   {
      //quick checks
      if(reqbps <= allowed_video_bitrates[0])
      {
        Index = 0 ;
        rc = allowed_video_bitrates[0] ;
      }
      else if(reqbps >= allowed_video_bitrates[tnum -1])
      {
        rc = allowed_video_bitrates[tnum -1] ;
        Index = tnum - 1 ;
      }
      else
      {
        for(b = 0 ; b < tnum ; b++)
        {
           //printf("%d \n",allowed_video_bitrates[b]);
           //check if reqbps, is in available list from transcoder
           rc = allowed_video_bitrates[b] ;
           if(reqbps == allowed_video_bitrates[b])
           {
              Index = b ;
              break ;
           }
           else if(reqbps < allowed_video_bitrates[b])
           {
              if(flag == BPS_SAME_OR_LESS)
              {
                if(b)
                {
                  rc = allowed_video_bitrates[b-1] ;
                  Index = b - 1;
                }
                else
                {
                  rc = allowed_video_bitrates[0] ;
                  Index = 0;
                }
                break ;
              }
              if(flag == BPS_SAME_OR_MORE)
              {
                Index = b ;
                break ;
              }
           }
         }
       }
       //printf("%s() reqbps=%u , altbps=%u flag=%u\n", __FUNCTION__,reqbps,rc,flag);
    }

    if(cfgIndex && rc && (Index >= 0) && (allowed_video_bitrates[Index] == rc))
    {
       *cfgIndex = Index ;
    }
    return rc ;
}

int hls_get_time(void)
{
  return hs_getmstime(&g_hls.StartTime) ;
}

int hls_estimate_bw(int sd, unsigned int channel, int txbytes, int txtime)
{
     int rc = 0 ;
     int i = 0 ;
     int tnow = 0 ;
     U32 tmpu32 = 0 ;
     int outq = 0 ;
     int i_Tx = 0 ;
     U32 i_Txbps = 0 ;
     int tdiff = 0 ;
     HlsClientInfo * ci = NULL ;
     struct tcp_info tcpi ;
     struct timespec tslp = {0, 0}; 

     GET_CSI_SLOT_CHANNEL(channel,tnow,ci,rc)

     if((rc < 0) || (!ci))
       return -1 ;

     if(!ci->log_once)
     { 
        ci->log_once = 1 ;
        COPY_SD_IP_PORT(sd, ci)
        utl_trace("%s() ci[%d]: channel=%u sd=%d [port=%u (%s) ] SegmentLenSecs=%d\n",
                  __FUNCTION__,  ci->idx, channel, ci->cursd, 
                  ci->ac.sin_port, inet_ntoa(ci->ac.sin_addr),g_SegmentLenSecs);
     }

     if(ci->BaseRtt && ci->advmss && (txbytes < ((int)ci->advmss << 2)))
     {
        //printf("%s() returning BaseRtt[%d] advmss[%u] txbytes[%u] \n",
        //         __FUNCTION__,ci->BaseRtt,ci->advmss,txbytes);
        return 0 ;
     }
    
     if(txbytes > ((int)ci->advmss << 3))
     {
        tmpu32 = ci->large_samples + 1 ;
        ci->large_samples = tmpu32 ;
     }


     rc = hs_get_tcp_info(&tcpi, sd);
     if(rc < 0)
        return -1 ;

     #define MAX_OUTQ_ITER 30
     do
     {
        outq = hs_get_socket_outq_bytes(sd) ;
        if(outq < 0)
           return -1 ;

        if(outq)
        {
            tslp.tv_sec = 0 ;
            tslp.tv_nsec = tcpi.tcpi_rtt * 1000 ; //micro second to nanosecond
            if((!tslp.tv_nsec) || ( tslp.tv_nsec < 10000000))
               tslp.tv_nsec = 10000000 ;
            else if(tslp.tv_nsec > 500000000)
               tslp.tv_nsec = 500000000 ;
   
            // printf("%s() i=%d outq=%u txbytes=%u tv_nsec=%lu\n",
            //          __FUNCTION__,i,outq,txbytes,tslp.tv_nsec);

            nanosleep(&tslp, NULL);
        }
        i++;
     }
     while(outq  && (i < MAX_OUTQ_ITER));

     tnow = hs_getmstime(&g_hls.StartTime) ;

     //Get information about state of tcp endpoint
     memset(&tcpi,0,sizeof(tcpi));

     rc = hs_get_tcp_info(&tcpi, sd);
     if(rc < 0)
        return -1 ;
  
     ci->pendingOutq = outq;
     ci->i_snd_mss = tcpi.tcpi_snd_mss;
     ci->advmss = tcpi.tcpi_advmss;
     ci->i_unacked = tcpi.tcpi_unacked;
     ci->i_lost = tcpi.tcpi_lost;
     ci->i_retrans = tcpi.tcpi_retrans;
     ci->i_total_retrans = tcpi.tcpi_total_retrans;
     ci->p_rtt_ms = ci->i_rtt_ms ;
     ci->i_rtt_ms = tcpi.tcpi_rtt>1000?tcpi.tcpi_rtt/1000:1;
     ci->i_snd_ssthresh = tcpi.tcpi_snd_ssthresh;
     ci->i_snd_cwnd = tcpi.tcpi_snd_cwnd;

     if(!ci->BaseRtt)
         ci->BaseRtt = ci->i_rtt_ms;
     else
         ci->BaseRtt = HLS_MIN(ci->BaseRtt, ci->i_rtt_ms);
   
     if(outq && (i == MAX_OUTQ_ITER))
     {
       //get once again
       outq = hs_get_socket_outq_bytes(sd) ;
     }

     if((txbytes >= outq) && (tnow > txtime))
     {
        //throughput of current stream
        i_Tx  = txbytes - outq ;
        tdiff = tnow - txtime ;
        i_Txbps = (i_Tx/tdiff)*8000 ;
     }

     if((ci->large_samples == 1) || (!ci->msSegStart))
     {
        ci->msSegStart = tnow ;
        utl_trace("%s() first Large segment @ %d ms \n",__FUNCTION__,ci->msSegStart);
     }

     ci->p_aCwndBwe = ci->i_aCwndBwe ;

     //ci->i_aCwndBwe = ((ci->i_snd_cwnd * ci->i_snd_mss)/ci->i_rtt_ms) * 8000;
     //ci->i_eCwndBwe = ((ci->i_snd_cwnd * ci->i_snd_mss)/ci->BaseRtt) * 8000;

     tmpu32 = HLS_MIN(ci->i_snd_cwnd , ci->i_snd_ssthresh);
     tmpu32 = (tmpu32 * ci->i_snd_mss * 8000) ;
     ci->i_aCwndBwe = tmpu32/ci->i_rtt_ms;
     ci->i_eCwndBwe = tmpu32/ci->BaseRtt;
     ci->i_Txbps = i_Txbps;
     ci->i_tdiff = tdiff;

     get_core_stateless_fair_queue_estimation(ci);

     get_enhanced_vegas_lod_predictor(ci);
     
#if 0
    if(txbytes > 4096)
    {
     utl_trace("%s(pid=%d tid=%ld time=%f) ci[%d] channel=%d sd=%u \n"
           "tdiff=%d msec {txbytes[%d] - outq[%d] = %d} i_Txbps=%u \n"
           "cwnd=%u ssthres=%u CwndBwe[ia=%d ie=%d ie-ia=%d iCSFQ=%d pa=%d] \n"
           "basertt=%u diffV = %f, lossType = %d \n"
           "tcp: unacked=%u lost=%u retrans=%u t_retrans=%u \n"
           "bpsSwitched=%u timeSwitched=%u first[bwe=%u rtt_ms=%u] \n"
           "i=%d i_rtt_ms=%d tv_nsec=%lu large_samples=%d \n"
           "Mdata(%u , tElapsed[%u - %u] = %u)\n",
            __FUNCTION__,
            getpid(),syscall(SYS_gettid),(float)tnow/1000.0,
            ci->idx, channel, ci->cursd,
            tdiff, txbytes, outq, i_Tx, i_Txbps, ci->i_snd_cwnd ,ci->i_snd_ssthresh,
            ci->i_aCwndBwe, ci->i_eCwndBwe, ci->i_eCwndBwe - ci->i_aCwndBwe,
            ci->i_CSFQCwndBwe, ci->p_aCwndBwe, ci->BaseRtt, ci->diffV, ci->lossType,
            ci->i_unacked, ci->i_lost, ci->i_retrans, ci->i_total_retrans,
            ci->bpsSwitched, ci->timeSwitched,ci->first_Bwe,ci->first_rtt_ms,
            i, ci->i_rtt_ms, tslp.tv_nsec,ci->large_samples,
            ci->large_samples * g_SegmentLenSecs,
            tnow,ci->msSegStart,(tnow-ci->msSegStart)/1000);
     utl_trace("\n----------------------------------------------------------------\n");
    }
#endif
    
     return 0 ;
}

int hls_get_predicted_bps_advice(int sd, unsigned int channel, int curbps)
{
    volatile U32 bps = 0 ;
    U32 trendbps = 0 ;
    U32 rt = 0 ;
    int rc = 0 ;
    U32 tnow = 0 ;
    U32 Index = 0 ;
    HlsClientInfo *ci = NULL;
    U32 tdmin = 0 , tdmax = 0 , tdfreq = 0 ;

    GET_CSI_SLOT_CHANNEL(channel,tnow,ci,rc)

    if((rc < 0) || (!ci))
      return -1 ;

    if(ci->large_samples < 5)
    {
       //printf("%s() no advice until 5 samples . analyzed[%d]\n",
       //        __FUNCTION__,ci->large_samples);
       return 0 ;
    }

    ci->video_rate_bps = curbps ;

     rt = ci->i_rtt_ms ;

#if 0
     if(rt < ci->f_rtt_ms)
        rt = ci->f_rtt_ms ;

     if(rt < ci->lf_rtt_ms)
       rt = ci->lf_rtt_ms ;
#endif

    if(!rt)
       return 0 ;

    HS_MATCH_BPSRTT(bps , rt)

    if(ci->bpsSwitched)
    {
       volatile U32 tmp32 = 0 ;
       #define SHIFT_FACTOR 1
       //Very conservative approach, the heuristics is to use the minimum of
       //of Bandwidth estimated from multiple ways
       //printf("%s() bps=%u i_aCwndBwe=%u i_CSFQCwndBwe=%u i_Txbps=%u curbps=%u\n",
       //     __FUNCTION__,bps,ci->i_aCwndBwe,ci->i_CSFQCwndBwe,ci->i_Txbps,curbps);
       tmp32 = ci->i_aCwndBwe;
       if(tmp32 && (tmp32 < bps))
          bps = tmp32;

       tmp32 = ci->i_CSFQCwndBwe >> SHIFT_FACTOR ;
       if(tmp32 && (tmp32 < bps))
          bps = tmp32;

       tmp32 = ci->i_Txbps >> SHIFT_FACTOR ;
       if(tmp32 && (tmp32 < bps))
          bps = tmp32;
    }
    else
    {
       if((ci->first_Bwe > 0) && (ci->first_Bwe < bps))
       {
           bps = (bps + ci->first_Bwe) >> 1 ;
           utl_trace("%s() channel=%d new bps=%u First[Bwe=%u rtt_ms=%u] rt=%u \n",
                        __FUNCTION__,channel,bps,ci->first_Bwe,ci->first_rtt_ms,rt);
       }
    }

   // printf("%s(), sd=%d curbps=%d newbps=%d rt=%u bpsSwitched=%u diff=%dmsec\n",
   //                __FUNCTION__,sd,curbps,bps,rt,ci->bpsSwitched,
   //                ci->tsaccess - ci->tstart);
   
    bps = hs_get_config_allowed_bps(g_sorted_allowed_bitrates, g_num_bitrates,
                                      bps, BPS_SAME_OR_LESS , &Index);
    if(curbps != bps)
    {
       if(!ci->bpsSwitched)
       {
          if(bps > curbps)
          {
             //Initial Media Level Check when going low to high
             rc = check_media_lvl(ci, tnow, MEDIA_LVL_SEC_LOW);
             if(rc < 0)
             {
                ci->lvl_tried ++ ;
                utl_trace("%s() channel=%d Initital Switch bps=%u "
                       "check_media_lvl not ok mlvl=%u lvl_tried=%d\n",
                      __FUNCTION__,channel,bps,MEDIA_LVL_SEC_LOW,ci->lvl_tried);
                return 0 ;
             }
          }
            
          utl_trace("%s() channel=%d Initital Switch bps=%u \n",
                           __FUNCTION__,channel,bps);
          return bps ;
       }
       else
       {
          if(bps < curbps) //High to Low
          {
              ci->incReq = 0;
             #define RETRANSLIMIT_CHK 10
             if(ci->pendingOutq || ( ci->i_total_retrans > RETRANSLIMIT_CHK )|| ci->i_unacked)
             {
                 #define MIN_LARGE_DEC_SAMPLES 2
                 U8 decSwing = MIN_LARGE_DEC_SAMPLES;
                 if((ci->curTrend == TREND_LOW_TO_HIGH) && (g_num_bitrates >= 4))
                 {
                     //reverse in trend compared to previous trend
                     //check neighbour rule
                     if(((Index+1)< g_num_bitrates) &&
                         (g_sorted_allowed_bitrates[Index+1] == curbps))
                     {
                        decSwing = MIN_LARGE_DEC_SAMPLES + 1 ;
                        utl_trace("%s() Neighbour swing rule lHl... decSwing=%u "
                                   "large_sampe=%u decReq=%u tobps=%u curbps=%u\n",
                                   __FUNCTION__,decSwing,ci->large_samples,ci->decReq,bps,curbps);
                     }
                 }

                 if((!ci->decReq) || (ci->large_samples < ci->decReq))
                 {
                    ci->decReq = ci->large_samples;
                    return 0 ;
                 }
                 else if((ci->large_samples - ci->decReq) >= decSwing)
                 {
                    utl_trace("%s(HtoL1) channel=%d, bps=%u  i_aCwndBwe=%u i_CSFQCwndBwe=%u i_Txbps=%u "
                           "curbps=%u pendingOutq=%u i_total_retrans=%u i_unacked=%u "
                           "rtt_ms[i=%u p=%u] i_tdiff=%u \n",
                          __FUNCTION__,channel,bps,ci->i_aCwndBwe,ci->i_CSFQCwndBwe,ci->i_Txbps,curbps,
                          ci->pendingOutq,ci->i_total_retrans,ci->i_unacked,
                          ci->i_rtt_ms,ci->p_rtt_ms,ci->i_tdiff);
                    ci->decReq = 0;

                    trendbps = get_trend_bps(ci, tnow, bps,tdfreq,tdmin,tdmax);
                    if(trendbps)
                    {
                       utl_trace("%s() using trend analysis : trendbps=%u bps=%u\n",
                               __FUNCTION__,trendbps,bps);
                       bps = trendbps ;

                       if(trendbps == curbps)
                         return 0 ;
                    }
                    return bps ;
                 }
             }
          }
          else if(bps > curbps) //Low to High
          {
             ci->decReq = 0;

             rc = check_media_lvl(ci, tnow, MEDIA_LVL_SEC_LOW);
             if(rc < 0)
             {
                 ci->lvl_tried ++ ;
                 utl_trace("%s() channel=%d Low to High , bps=%u curbps=%u"
                           "check_media_lvl not ok %d lvl_tried=%d\n",
                           __FUNCTION__,channel,bps,curbps,MEDIA_LVL_SEC_LOW,ci->lvl_tried);
                 return 0 ;
             }

             if((!ci->pendingOutq) && (!ci->i_total_retrans) && (!ci->i_unacked))
             {
                 #define MIN_LARGE_INC_SAMPLES 8
                 U8 incSwing = MIN_LARGE_INC_SAMPLES ;
                 if((ci->curTrend == TREND_HIGH_TO_LOW) && (g_num_bitrates >= 4))
                 {
                     //reverse in trend compared to previous trend
                     //check neighbour rule
                     if((Index > 0) && (g_sorted_allowed_bitrates[Index-1] == curbps))
                     {
                        if(ci->HighToLow > 2)
                           incSwing = MIN_LARGE_INC_SAMPLES * 4 ;
                        else
                           incSwing = MIN_LARGE_INC_SAMPLES + 4 ;
                        tdmin = curbps ; 
                        tdmax = bps ; 
                        tdfreq = 1 ;
                        utl_trace("%s() Neighbour swing rule HlH... incSwing=%u "
                                   "large_sampe=%u incReq=%u tobps=%d curbps=%d\n",
                                   __FUNCTION__,incSwing,ci->large_samples,ci->incReq,bps,curbps);
                     }
                 }

                 if((!ci->incReq) || (ci->large_samples < ci->incReq))
                 {
                    ci->incReq = ci->large_samples;
                    return 0 ;
                 }
                 else if((ci->large_samples - ci->incReq) >= incSwing)
                 {
                    utl_trace("%s(LtoH1) channel=%d large_samples=%u incReq=%u, bps=%u "
                            "rtt_ms[i=%u p=%u] i_tdiff=%u\n",
                             __FUNCTION__,channel,ci->large_samples,ci->incReq,bps,
                             ci->i_rtt_ms,ci->p_rtt_ms,ci->i_tdiff);
                    ci->incReq = 0;


                    trendbps = get_trend_bps(ci, tnow, bps,tdfreq,tdmin,tdmax);
                    if(trendbps)
                    {
                       utl_trace("%s() using trend analysis : trendbps=%u bps=%u\n",
                               __FUNCTION__,trendbps,bps);
                       bps = trendbps ;
                       if(trendbps == curbps)
                         return 0 ;
                    }
                    return bps ;
                 }
             }
          }

          if((bps <= HS_MEDIA_POOR_RATE) &&
                (ci->bpsSwitched >= HS_MEDIA_REASONABLE_RATE))
          {
              utl_trace("%s(HtoL2) channel=%d bps=%u  i_aCwndBwe=%u i_CSFQCwndBwe=%u i_Txbps=%u "
                        "curbps=%u pendingOutq=%u i_total_retrans=%u i_unacked=%u "
                        "rtt_ms[i=%u p=%u] i_tdiff=%u\n",
                       __FUNCTION__,channel,bps,ci->i_aCwndBwe,ci->i_CSFQCwndBwe,
                       ci->i_Txbps,curbps, ci->pendingOutq,ci->i_total_retrans,
                       ci->i_unacked,ci->i_rtt_ms,ci->p_rtt_ms,ci->i_tdiff);

              trendbps = get_trend_bps(ci, tnow, bps,tdfreq,tdmin,tdmax);
              if(trendbps)
              {
                  utl_trace("%s() using trend analysis : trendbps=%u bps=%u\n",
                               __FUNCTION__,trendbps,bps);
                  bps = trendbps ;
                  if(trendbps == curbps)
                     return 0 ;
              }

              return bps ;
          }
          else if((ci->bpsSwitched <= HS_MEDIA_POOR_RATE) && 
                  (bps >= HS_MEDIA_REASONABLE_RATE))
          {
              ci->incReq = 0;
              utl_trace("%s(LtoH2) channel=%d bps=%u curbps=%u rtt_ms[i=%u p=%u] i_tdiff=%u\n",
               __FUNCTION__,channel,bps,curbps,ci->i_rtt_ms,ci->p_rtt_ms,ci->i_tdiff);

              trendbps = get_trend_bps(ci, tnow, bps,tdfreq,tdmin,tdmax);
              if(trendbps)
              {
                  utl_trace("%s() using trend analysis : trendbps=%u bps=%u\n",
                               __FUNCTION__,trendbps,bps);
                  bps = trendbps ;
                  if(trendbps == curbps)
                     return 0 ;
              }

              if((bps > HS_MEDIA_OK_RATE) && (curbps <= HS_MEDIA_POOR_RATE))
              {
                  utl_trace("%s() step rule: trendbps=%u bps=%u curbps=%d tobps=%d\n",
                               __FUNCTION__,trendbps,bps,curbps,HS_MEDIA_OK_RATE);
                  bps = HS_MEDIA_OK_RATE ;
              }
              
              return bps ;
          }
          else
          {
              return 0 ;
          }
       }
    }

    //TODO : Add Bit error adjustment algorithm after testing

    return 0 ;
}

int hls_get_initial_probe_bw(int sd, unsigned int channel)
{
     int rc = 0 ;
     int tnow = 0 ;
     U32 tmpu32 = 0 ;
     U32 bps = 0 ;
     HlsClientInfo * ci = NULL ;
     struct tcp_info tcpi ;
     U8 useChannel = 0 ; //Does not use it for now

     if((channel > 0) && (channel < MAX_CHANNEL_ID))
     {
       GET_CSI_SLOT_CHANNEL(channel,tnow,ci,rc)

       if(rc < 0)
         return -1 ;
       useChannel = 0 ; //do not update channel now
     }

     //Get information about state of tcp endpoint
     memset(&tcpi,0,sizeof(tcpi));

     rc = hs_get_tcp_info(&tcpi, sd);
     if(rc < 0)
        return -1 ;
    if(ci && useChannel && (!ci->initialBwe))
    {
#if 0
     COPY_SD_IP_PORT(sd, ci)
     utl_trace("%s() ci[%d]: channel=%u sd=%d [port=%u (%s) ] \n",
            __FUNCTION__,  ci->idx, channel, ci->cursd, 
           ci->ac.sin_port, inet_ntoa(ci->ac.sin_addr));
#endif

     ci->i_snd_mss = tcpi.tcpi_snd_mss;
     ci->advmss = tcpi.tcpi_advmss;
     ci->i_unacked = tcpi.tcpi_unacked;
     ci->i_lost = tcpi.tcpi_lost;
     ci->i_retrans = tcpi.tcpi_retrans;
     ci->i_total_retrans = tcpi.tcpi_total_retrans;
     ci->p_rtt_ms = ci->i_rtt_ms ;
     ci->i_rtt_ms = tcpi.tcpi_rtt>1000?tcpi.tcpi_rtt/1000:1;
     ci->i_snd_ssthresh = tcpi.tcpi_snd_ssthresh;
     ci->i_snd_cwnd = tcpi.tcpi_snd_cwnd;
#if 0 
     if(!ci->BaseRtt)
         ci->BaseRtt = ci->i_rtt_ms;
     else
         ci->BaseRtt = HLS_MIN(ci->BaseRtt, ci->i_rtt_ms);
#endif
   
     ci->p_aCwndBwe = ci->i_aCwndBwe ;
     tmpu32 = HLS_MIN(ci->i_snd_cwnd , ci->i_snd_ssthresh);
     if(ci->i_snd_mss)
       tmpu32 = (tmpu32 * ci->i_snd_mss * 8000) ;
     else if ( ci->advmss )
       tmpu32 = (tmpu32 * ci->advmss * 8000) ;
     if(tmpu32 && ci->i_rtt_ms)
        ci->i_aCwndBwe = tmpu32/ci->i_rtt_ms;

     HS_MATCH_BPSRTT(bps , ci->i_rtt_ms)

     utl_trace("%s(Initial) channel=%u updated i_aCwndBwe=%u bps=%u i_rtt_ms=%u\n",
             __FUNCTION__,channel,ci->i_aCwndBwe,bps,ci->i_rtt_ms);

     if(ci->i_aCwndBwe && (ci->i_aCwndBwe < bps)) //use smaller of the two
        bps = ci->i_aCwndBwe;
    }
    else
    {
       U32 i_aCwndBwe = 0 ;
       U32 i_rtt_ms = tcpi.tcpi_rtt>1000?tcpi.tcpi_rtt/1000:1;
       tmpu32 = HLS_MIN(tcpi.tcpi_snd_cwnd , tcpi.tcpi_snd_ssthresh);
       if(tcpi.tcpi_snd_mss)
          tmpu32 = (tmpu32 * tcpi.tcpi_snd_mss * 8000) ;
       else if ( tcpi.tcpi_advmss )
          tmpu32 = (tmpu32 * tcpi.tcpi_advmss * 8000) ;

       if(tmpu32 && i_rtt_ms)
          i_aCwndBwe = tmpu32/i_rtt_ms;

       HS_MATCH_BPSRTT(bps , i_rtt_ms)

       utl_trace("%s(Initial) channel=%d i_aCwndBwe=%u bps=%u i_rtt_ms=%u\n",
             __FUNCTION__,channel,i_aCwndBwe,bps,i_rtt_ms);

       if(i_aCwndBwe && (i_aCwndBwe < bps)) //use smaller of the two
           bps = i_aCwndBwe;
    }

     bps = hs_get_config_allowed_bps(g_sorted_allowed_bitrates, g_num_bitrates,
                                      bps, BPS_SAME_OR_LESS , NULL);
     if(bps > 0)
     {
       if(ci)
       {
          if(useChannel)
            ci->initialBwe  = bps;

          ci->first_Bwe = bps;
          ci->first_rtt_ms = tcpi.tcpi_rtt/1000;
       }
       return bps ;
     }

     return 0 ;
}

int hls_update_switched(int sd, U32 channel, int bpsSwitched)
{
    int rc = 0 ;
    U32 tnow = 0 ;
    HlsClientInfo *ci = NULL;
    U32 tmpu32 = 0 ;

    GET_CSI_SLOT_CHANNEL(channel,tnow,ci,rc)

    if((rc < 0) || (!ci))
      return -1 ;
 
    if(!ci->trendts)
    {
      ci->LowToHigh = 0 ;
      ci->HighToLow = 0 ;
      ci->trendMaxBps =  0 ;
      ci->trendMinBps = 0;
      ci->sub_HighToLow = 0  ;
      ci->sub_LowToHigh  = 0 ;
      ci->inOptBpsTrend = 0 ;
      ci->trendts = tnow;
    }
    
    if(bpsSwitched > ci->bpsSwitched)
    {
       tmpu32 = ci->LowToHigh + 1 ;
       ci->LowToHigh = tmpu32 ;
       if((!ci->trendMaxBps) || (bpsSwitched > ci->trendMaxBps))
       {
         ci->trendMaxBps = bpsSwitched;
       }
       ci->curTrend = TREND_LOW_TO_HIGH ;
    }
    else
    {
       tmpu32 = ci->HighToLow + 1;
       ci->HighToLow = tmpu32 ;

       if((!ci->trendMinBps) || (bpsSwitched < ci->trendMinBps))
       { 
         ci->trendMinBps = bpsSwitched;
       }
       ci->curTrend = TREND_HIGH_TO_LOW ;
    }

    ci->prevbpsSwitched = ci->bpsSwitched ;
    ci->bpsSwitched = bpsSwitched;
    ci->timeSwitched = tnow;

    utl_trace("%s(), sd=%d channel=%u bpsSwitched=%d oldbps=%u"
           " tnow=%f secs HtoL=%u LtoH=%u\n",
           __FUNCTION__,sd,channel,ci->bpsSwitched,ci->prevbpsSwitched,
           (float)ci->timeSwitched/1000.0,ci->HighToLow,ci->LowToHigh);

    return 0 ;
}

int hls_send_and_estimate_bw(int sd, char *txbuf, int txbytes, int sndflag,
                                 unsigned int channel)
{
    int rc = 0 ;
    int t2 = hs_getmstime(&g_hls.StartTime) ;

    //TODO : Add channel aware Transmit scheduler along with sub interval estimation

    if(txbytes > 4096)
    {
       int newsz = (txbytes << 1) ;
       if(newsz > 4096000)
          newsz = 4096000 ;
       hs_set_snd_buff(sd, newsz, NULL, SOCK_BUF_SET_IF_LARGE);
    }

    //rc = send(sd, txbuf, txbytes, sndflag);
    rc = hs_tcp_send(sd, txbuf, txbytes, sndflag);
    if(rc < 0)
    {
       utl_error("send err(%d) = %s\n", errno, strerror(errno));
       return -1;
    }
    
    #define MIN_TX_ESTIMATE_SZ 1024
    if((rc > 0) && (txbytes > MIN_TX_ESTIMATE_SZ))
    {
       // printf("%s() sd=%d , rc=%d , txbytes=%d \n",
       //            __FUNCTION__,sd,rc,txbytes);

        hls_estimate_bw(sd, channel, rc, t2);
    }

    return 0 ;
}

int hls_delete_channel(unsigned int channel)
{
   int rc = 0 ;
   int tnow = 0 ;
   HlsClientInfo * ci = NULL ;
   GET_CSI_SLOT_CHANNEL(channel,tnow,ci,rc)

   if((rc < 0) || (!ci))
       return -1 ;

   ci->key = 0; //deletes it, makes it reusable

   utl_trace("%s() : deleting channel %u \n",__FUNCTION__,channel);

   return 0 ;
}

int hls_init(int *allowed_video_bitrates, int allowed_video_bitrates_num, int segmentLen)
{
   if(segmentLen > 0)
   {
      g_SegmentLenSecs = segmentLen ;
      //printf("%s() Segment length is %d secs \n",__FUNCTION__,segmentLen);
   }
   copy_cfg_bitrates(allowed_video_bitrates, 
                                 allowed_video_bitrates_num);
   return  0 ;
}

int hls_destroy()
{
   return 0 ;
}

static int copy_cfg_bitrates(int *allowed_video_bitrates, 
                                  int allowed_video_bitrates_num)
{
   if((allowed_video_bitrates && allowed_video_bitrates_num))
   {
       int i = 0 , j = 0 ;
       int num = HLS_MIN(allowed_video_bitrates_num, NUM_BITRATES);

       memset(g_sorted_allowed_bitrates,0,NUM_BITRATES*sizeof(int));

       while((i < num) && (j < num))
       {
           if(allowed_video_bitrates[j] > 0)
           {
              g_sorted_allowed_bitrates[i] = allowed_video_bitrates[j] ;
              i++;
           }
           j++;
       }

       //printf("%s() : i=%d , j=%d , allowed_video_bitrates_num=%d NUM_BITRATES=%d\n",
       //       __FUNCTION__,i,j,allowed_video_bitrates_num,NUM_BITRATES);

       if(i > 0)
       {
          //int b = 0 ;
          g_num_bitrates = i ;

          //sort incase if they are not in order
          qsort(g_sorted_allowed_bitrates,i,sizeof(int),cmp_int);

          g_minBps = g_sorted_allowed_bitrates[0];
          g_maxBps = g_sorted_allowed_bitrates[i-1];
          g_optBps = (i > 1)? g_sorted_allowed_bitrates[1] : g_minBps ;

          utl_trace("%s() Available bit rates[%d] provided by transcoder:"
                 " Bps[min=%u Opt=%u Max=%u]\n",
	       __FUNCTION__,g_num_bitrates,g_minBps,g_optBps,g_maxBps);
#if 0
          for(b = 0 ; b < g_num_bitrates ; b++)
          {
	     printf("%d \n",g_sorted_allowed_bitrates[b]);
          }
#endif
       }
   }
   return  0 ;
}


static U32 get_core_stateless_fair_queue_estimation(HlsClientInfo * ci)
{

#define T0_CONST 1.0
   float tk = 0.0 ;
   float i_CwndBwe = 0 ;
   float p_CwndBwe = 0 ;
   float expTkTo = 0 ;
   //int a = 0 ;
   U32 Bwe ;
   //U32 f_rtt_ms;
 
   if(!ci->i_CSFQCwndBwe)
   {
     ci->i_CSFQCwndBwe = ci->i_aCwndBwe ;
   }

   if(ci->p_ts < ci->i_ts)
     tk = (float)(ci->i_ts - ci->p_ts)/1000.0 ;

   if(tk < 0.1)
     tk = 0.1 ;
   else if(tk > 0.5)
     tk = 0.5 ;

   i_CwndBwe = ci->i_aCwndBwe ;

   //p_CwndBwe = ci->p_aCwndBwe ; //Use just the previous sample
   p_CwndBwe = ci->i_CSFQCwndBwe ; //Use sample since first

   expTkTo = expf(-tk/T0_CONST) ;

   Bwe = ((1 - expTkTo)*i_CwndBwe) + (expTkTo * p_CwndBwe) ;

   if(Bwe > 0)
   {
      ci->i_CSFQCwndBwe = Bwe ;
   }

#if 0
   if(!ci->p_rtt_ms)
   {
      ci->p_rtt_ms = ci->i_rtt_ms ;
      ci->f_rtt_ms = ci->i_rtt_ms;
      ci->lf_rtt_ms = ci->i_rtt_ms;
   }
   else
   {
      f_rtt_ms = ((1 - expTkTo)*ci->i_rtt_ms) + (expTkTo * ci->p_rtt_ms) ;
      if(f_rtt_ms > 0)
      {
         ci->f_rtt_ms = f_rtt_ms;
      }
      a  = (ci->i_rtt_ms + ci->lf_rtt_ms) >> 1 ;
      ci->lf_rtt_ms = (ci->lf_rtt_ms >> 2) + a - (a >> 2);
   }
#endif

   return Bwe;
}

//Enhanced Vegas Loss Predictor:
//Loss Differentiation
#define LOSS_TYPE_NONE       0
#define LOSS_TYPE_CONGESTION 1
#define LOSS_TYPE_WIRELESS   2

#define VLP_ALPHA 1
#define VLP_BETA  3
static int get_enhanced_vegas_lod_predictor(HlsClientInfo * ci)
{
  //float diffV  = 0 ;
  //int lossType = 0 ;

  ci->diffV = (float)ci->i_snd_cwnd * ( 1.0 - (float)ci->BaseRtt/(float)ci->i_rtt_ms) ;

  if(ci->diffV <= VLP_ALPHA)
     ci->lossType = LOSS_TYPE_WIRELESS ;
  else if(ci->diffV >= VLP_BETA)
     ci->lossType = LOSS_TYPE_CONGESTION;
  else 
     ci->lossType = LOSS_TYPE_NONE;
  
  return ci->lossType;
}

static int get_trend_bps(HlsClientInfo * ci, U32 tnow, U32 bps,
                                         U32 freq, U32 min, U32 max)
{
   U32 tmpu32 = 0 ;
   S32 tsd = tnow > ci->trendts ? tnow - ci->trendts : 0 ;
   U32 optbps = g_optBps ;
   //5 mins trend
   #define NW_TREND_MS 300000
   if((!ci->trendts) || (tsd  > NW_TREND_MS))
   {
      ci->trendts = tnow;
      ci->LowToHigh = 0 ;
      ci->HighToLow = 0 ;
      ci->trendMaxBps =  0 ;
      ci->trendMinBps = 0;
      ci->sub_HighToLow = 0  ;
      ci->sub_LowToHigh  = 0 ;
      ci->inOptBpsTrend = 0 ;
      return 0 ;
   }

   if(!min) 
   { 
     min = g_minBps ; 
     optbps = g_optBps ;
   }
   else  
   {
     optbps = min ;
   } 

   if(!max) { max = g_maxBps ; }
   if(!freq) { freq = 3 ; } 

   if( (ci->LowToHigh >= freq)  &&
       (ci->HighToLow >= freq)  &&
       (ci->trendMinBps == min) &&
       (ci->trendMaxBps == max) )
   {
      #if 0
      printf("%s(%u) LToH=%u HToL=%u trend[MinBps=%u MaxBps=%u OptBps=%u] "
             "sub[LToH=%u HToL=%u] min=%d max=%d freq=%d\n",
             __FUNCTION__,tnow-ci->trendts,ci->LowToHigh,ci->HighToLow,ci->trendMinBps,
             ci->trendMaxBps,optbps,ci->sub_LowToHigh,ci->sub_HighToLow,min,max,freq);
      #endif

      if(!ci->inOptBpsTrend)
      {
         ci->trendts = tnow;
         ci->inOptBpsTrend = 1 ;
      }
      else 
      {
         if(bps == ci->trendMinBps)
         {
            tmpu32 = ci->sub_LowToHigh + 1 ;
            ci->sub_LowToHigh = tmpu32 ;
         }
         else if(bps == ci->trendMaxBps)
         {
            tmpu32 = ci->sub_HighToLow + 1 ;
            ci->sub_HighToLow = tmpu32 ;
         }
         else
         {
             ci->sub_HighToLow = 0  ;
             ci->sub_LowToHigh  = 0 ;
         } 

         if((ci->sub_LowToHigh >= freq) && (ci->sub_HighToLow >= freq))
         {
             ci->trendts = tnow ;
             ci->sub_HighToLow = 0  ;
             ci->sub_LowToHigh  = 0 ;
         }
      }
      return optbps;
   }

   return 0 ;
}

static int check_media_lvl(HlsClientInfo * ci, U32 tnow, U32 mlvl)
{
   //U32 TotalMediaTxSecs = 0 ;
   U32 tMediaTxSecs = 0 ;
   U32 tElapsedSec = 0 ;

   if(!ci)
     return -1 ;

   if(g_SegmentLenSecs <= 0)
     return 0 ;

   if((!ci->large_samples) || (!ci->msSegStart) || (tnow < ci->msSegStart))
   {
     utl_trace("%s() media lvl not evaluated tnow=% msSegStart=%u u\n",__FUNCTION__,
               tnow,ci->msSegStart);
     return 0 ;
   }
   
   tMediaTxSecs = ci->large_samples * g_SegmentLenSecs ;
   tElapsedSec = (tnow - ci->msSegStart)/1000 ;

   utl_trace("%s() tMediaTxSecs=%u , large_samples=%u tElapsedSec=%u mlvl=%u\n",
             __FUNCTION__,tMediaTxSecs,ci->large_samples,tElapsedSec,mlvl);

   if(mlvl > MEDIA_LVL_SEC_MAX)
      mlvl = MEDIA_LVL_SEC_MAX ;

   if(tMediaTxSecs >= (tElapsedSec + mlvl))
   {
     utl_trace("%s() media lvl is OK \n",__FUNCTION__);
     ci->lvl_tried = 0 ; 
     return 0 ;
   }
   else
   {
     #define MAX_MEDIA_LVL_TRIED 5 
     if(ci->lvl_tried > MAX_MEDIA_LVL_TRIED)
     {
       utl_trace("%s() media lvl is below requested lvl , "
                 "but max lvl tried=%d reached\n",__FUNCTION__,ci->lvl_tried);
       ci->lvl_tried = 0 ; 
       return 0 ;
     }  
     utl_trace("%s() media lvl is below requested lvl \n",__FUNCTION__);
     return -1 ;
   }

   return 0 ;
}

static int cmp_int(const void *p1, const void *p2)
{
     int v1 = *(int *)p1 ;
     int v2 = *(int *)p2 ;
/*
returns an integer less than, equal to, or greater than zero if the first argument
is considered to be respectively less than,  equal  to, or greater than the second.
*/
     if(v1 < v2)
	return -1 ;
     else if(v1 > v2)
	return 1 ;
     else 
      return 0;
}

#ifdef __cplusplus
}
#endif
