#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <assert.h>
#include <linux/videodev2.h>
#include <linux/fb.h>
#include <pthread.h>
#include <poll.h>
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>
#include "camera.h"
#include "mfc.h"
#include "h264.h"
#define TimeOut 5
#define FIMC         "/dev/video0"
#define CAM_DEV		"/dev/video15"
BUFTYPE *fimc0_out_buf;//未转码前的数据
BUFTYPE *cam_buffers;   //摄像头采集的数据
BUFTYPE *fimc0_cap_buf;  //FIMC 转码后的数据
int  fimc0_cap_index;
int fimc0_out_buf_length; //转码前的数据长度
int fimc0_cap_buf_length;//转码后的数据长度
Camera *camera;
int cam_fd;    //Camera文件描述符
int fimc_fd;   //fimc文件描述符
int ouput_buf_size;//编码后一帧的大笑
int writesize;   //写入文件的
void sign_func(int sign_num)
{
    switch(sign_num)
    {
    case SIGINT:
        printf("I have get SIGINT<Ctrl+c>, I'm going now..\n");
        camera->CloseDevice();
        exit(0);
        break;
    }
}
int main()
{
    int width=640;
    int height=480;
    FILE * out_h264 = 0;
    out_h264 = fopen("out.h264", "wb");
    camera=new Camera(CAM_DEV, FIMC,width, height);
    camera->open_device(&cam_fd,&fimc_fd);
    if(!camera->InitDevice())
    {
        printf("Cam Open error\n");
        return -1;
    }
    camera->start_capturing();
    /*MFC encode */
    MFC *mfc=new MFC();
    SSBSIP_MFC_ERROR_CODE ret;
    unsigned char *header;
    ret=mfc->initMFC_ENC(width,height,28);
    if(ret<0)
    {
        printf("init mfc failed !!\n");
    }
    // 打开rtp
    RtpSend* rtp = new RtpSend();
    int headerSize=mfc->getHeader(&header);
    fwrite(header,1,headerSize,out_h264);
    rtp->Send((char*)header, headerSize);
    printf("Waiting for signal SIGINT..\n");
    signal(SIGINT, sign_func);
    int count = 1;//CapNum;
    struct pollfd fds[2];
    void *ouput_buf;
    while(count< 6000)
    {
        int r;
        struct timeval start;
        struct timeval end;
        double time_use=0;
        gettimeofday(&start,NULL);
        fds[0].events |= POLLIN | POLLPRI;//读事件
        fds[0].fd = cam_fd;//摄像头
        fds[1].events |= POLLIN | POLLPRI | POLLOUT;//写事件
        fds[1].fd = fimc_fd;//FIMC
        r = poll(fds, 2, -1);/* */
        if(-1 == r)
        {
            if(EINTR == errno)
                continue;
            perror("Fail to select");
            exit(EXIT_FAILURE);
        }
        if(0 == r)
        {
            fprintf(stderr,"select Timeout\n");
            exit(EXIT_FAILURE);
        }
        if (fds[0].revents & POLLIN)//摄像头可读
        {
            camera->process_cam_to_fimc0();
            gettimeofday(&end,NULL);
            time_use=(end.tv_sec-start.tv_sec)*1000000+(end.tv_usec-start.tv_usec);
            //printf("-------------------use time_use is----------------%f ms-------------",time_use/1000);
        }
        if (fds[1].revents & POLLIN)//FIMC可读
        {
            int index;
            camera->fimc0_cap_dbuf(&index);
            ouput_buf_size=mfc->encode(fimc0_cap_buf[index].start,&ouput_buf);
            if(ouput_buf_size == 0)
            {
                printf("framd_%d error!!!\n",count);
            }
            else
            {
                rtp->Send((char*)ouput_buf, ouput_buf_size);
                if(count%10 == 0) printf("enc %d\n", count);
            }
            //writesize=fwrite(ouput_buf,1, ouput_buf_size,out_h264);
            count++;
            //printf("frame:%d,writesize:%d\n", index,writesize);
            camera->fimc0_cap_qbuf(index);
        }
        if (fds[1].revents & POLLOUT)//FIMC可写
        {
            camera->process_fimc0_to_cam();
        }
    }
    return 0;
}
