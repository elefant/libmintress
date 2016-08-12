/* 
 * File:   dshlsbw.h
 * Author: Varadhan Venkataseshan 
*/

#ifndef __DS_HLSBW_H__
#define __DS_HLSBW_H__

#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

//returns elapsed time in milliseconds since epoc maintained by hls code
int hls_get_time(void);

//hls_int() : Initialization function and should be called when
//process is started
int hls_init(int *allowed_video_bitrates, int allowed_video_bitrates_num, int segmentLen);

//hls_destroy() : destroy function and should be called when
//process is terminated
int hls_destroy() ;

//hls_destroy() : destroy function and should be called when
//process is terminated
int hls_delete_channel(unsigned int channel) ;

//hls_estimate_bw() estimates new BW
int hls_estimate_bw(int sd, unsigned int channel, int txbytes, int txtime);

//hls_send_and_estimate_bw() sends data over sd and estimates new BW
int hls_send_and_estimate_bw(int sd, char *txbuf, int txbytes, int sndflag,
                                        unsigned int channel );

//hls_update_switched( ) is called after switching bit rate, with
//switched bit rate and switched time
int hls_update_switched(int sd, unsigned int channel, int bpsSwitched);

//hls_get_predicted_bps_advice returns rate in bits/sec if available
//else returns 0 . if it returns 0 , just ignore
int hls_get_predicted_bps_advice(int sd, unsigned int channel, int curbps);

int hls_get_initial_probe_bw(int sd, unsigned int channel);

#ifdef __cplusplus
}
#endif

//end __DS_HLSBW_H__
#endif
