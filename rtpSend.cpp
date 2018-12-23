/*
 * rtpSend.cpp
 *
 *  Created on: 2016年1月15日
 *      Author: Westlor
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include "h264.h"

RtpSend::RtpSend() {

	seq_num = 0;
	timestamp_increse = 0;
	ts_current = 0;
	framerate = 20;
	timestamp_increse=(unsigned int)(90000.0 / framerate); //+0.5);

	nalu_t  = (NALU_t*)calloc(1, sizeof (NALU_t));
	sendBuf = (char*)calloc(1, MAX_RTP_PKT_LENGTH+50);

	struct sockaddr_in server;
    int len =sizeof(server);

	server.sin_family 		= AF_INET;
    server.sin_port			= htons(DEST_PORT);
    server.sin_addr.s_addr	= inet_addr(DEST_IP);
    socketFd = socket(AF_INET,SOCK_DGRAM,0); //使用UDP
    connect(socketFd, (const sockaddr *)&server, len);
}

RtpSend::~RtpSend() {
	free(sendBuf);
	free(nalu_t);
}

int RtpSend::Send(char* buf, int len) {

	int dataLen = 0;
	int leftLen = len;
	naluBuf = buf;
	while(1){
		dataLen = FindNalu(naluBuf+dataLen, leftLen);
		leftLen = leftLen-(naluIndex+dataLen);

		if(dataLen > 0){
			SendNalu(naluBuf, dataLen);
		}
		if(leftLen <= 0)
			break;
	}
}

int RtpSend::FindNalu(char* buf, int len) {

	int index = 0;
	int headerCount = 0;
	naluBuf = buf;
	naluIndex = 0;

	while(1){

		if(index == 0)
			index+=2;
		else{
			index++;
		}

		if(index == len){			//The end
			if(headerCount == 0)
				return -1;
			else
				return len-naluIndex;
		}

		if(buf[index] == 0x01){		//find  00 00 01
			if(buf[index-1] || buf[index-2]);
			else{

				headerCount++;
				if(headerCount == 1){			//The first find
					naluIndex = index+1;
					if(naluIndex == len)		//No data Left
						return -1;
					naluBuf = buf+naluIndex;
				}else{							//The second find
					if(buf[index-3]  == 0)		//find 00 00 00 01
						return index-3-naluIndex;
					else
						return index-2-naluIndex;
				}
			}
		}
	}
}

void RtpSend::SendNalu(char* buf, int len) {

	char* nalu_payload;
	int	bytes=0;

	nalu_t->buf = buf;
	nalu_t->len = len;
	nalu_t->forbidden_bit 		= buf[0] & 0x80; 	//1 bit
    nalu_t->nal_reference_idc 	= buf[0] & 0x60; 	//2 bit
    nalu_t->nal_unit_type 		= buf[0] & 0x1f;	//5 bit

	printf("Nalu->type: %d\tNalu->len: %d\n", nalu_t->nal_unit_type, nalu_t->len);

	memset(sendBuf, 0, 14);

	rtp_hdr =(RTP_FIXED_HEADER*)&sendBuf[0];
	rtp_hdr->payload   	= H264;
	rtp_hdr->version  	= 2;
	rtp_hdr->marker    	= 0;
	rtp_hdr->ssrc      	= htonl(10);

	if(nalu_t->len<=MAX_RTP_PKT_LENGTH){

		rtp_hdr->marker=1;
		rtp_hdr->seq_no     = htons(seq_num ++);

		nalu_payload	= &sendBuf[12];
		memcpy(nalu_payload, nalu_t->buf, nalu_t->len);

		ts_current=ts_current+timestamp_increse;
		rtp_hdr->timestamp	= htonl(ts_current);
		bytes = nalu_t->len + 12 ;

		::send(socketFd, sendBuf, bytes, 0);
	}else if(nalu_t->len>MAX_RTP_PKT_LENGTH){

		int k=0,l=0;
		k = nalu_t->len/MAX_RTP_PKT_LENGTH;
		l = nalu_t->len%MAX_RTP_PKT_LENGTH;
		int t=0;
		ts_current = ts_current+timestamp_increse;
		rtp_hdr->timestamp = htonl(ts_current);
		while(t<=k){

			rtp_hdr->seq_no = htons(seq_num ++);
			if(!t){
				rtp_hdr->marker=0;

				fu_ind =(FU_INDICATOR*)&sendBuf[12];
				fu_ind->F=nalu_t->forbidden_bit;
				fu_ind->NRI=nalu_t->nal_reference_idc>>5;
				fu_ind->TYPE=28;

				fu_hdr =(FU_HEADER*)&sendBuf[13];
				fu_hdr->E=0;
				fu_hdr->R=0;
				fu_hdr->S=1;
				fu_hdr->TYPE=nalu_t->nal_unit_type;

				nalu_payload=&sendBuf[14];
				memcpy(nalu_payload,nalu_t->buf+1,MAX_RTP_PKT_LENGTH);

				bytes=MAX_RTP_PKT_LENGTH+14;
				::send( socketFd, sendBuf, bytes, 0 );
				t++;

			}else if(k==t){

				rtp_hdr->marker=1;

				fu_ind =(FU_INDICATOR*)&sendBuf[12];
				fu_ind->F=nalu_t->forbidden_bit;
				fu_ind->NRI=nalu_t->nal_reference_idc>>5;
				fu_ind->TYPE=28;

				fu_hdr =(FU_HEADER*)&sendBuf[13];
				fu_hdr->R=0;
				fu_hdr->S=0;
				fu_hdr->TYPE=nalu_t->nal_unit_type;
				fu_hdr->E=1;

				nalu_payload=&sendBuf[14];
				memcpy(nalu_payload,nalu_t->buf+t*MAX_RTP_PKT_LENGTH+1,l-1);
				bytes=l-1+14;
				::send( socketFd, sendBuf, bytes, 0 );
				t++;
			}else if(t<k&&0!=t){

				rtp_hdr->marker=0;

				fu_ind =(FU_INDICATOR*)&sendBuf[12];
				fu_ind->F=nalu_t->forbidden_bit;
				fu_ind->NRI=nalu_t->nal_reference_idc>>5;
				fu_ind->TYPE=28;

				fu_hdr =(FU_HEADER*)&sendBuf[13];
				fu_hdr->R=0;
				fu_hdr->S=0;
				fu_hdr->E=0;
				fu_hdr->TYPE=nalu_t->nal_unit_type;

				nalu_payload=&sendBuf[14];
				memcpy(nalu_payload,nalu_t->buf+t*MAX_RTP_PKT_LENGTH+1,MAX_RTP_PKT_LENGTH);
				bytes=MAX_RTP_PKT_LENGTH+14;
				::send( socketFd, sendBuf, bytes, 0 );
				t++;
			}
		}
	}
}
