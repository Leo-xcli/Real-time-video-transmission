#ifndef CAMERA_H
#define CAMERA_H
#include <sys/types.h>
#define CAPTURE_BUFFER_NUMBER	3
#define FPS              20
typedef enum
{
    IO_METHOD_READ, IO_METHOD_MMAP, IO_METHOD_USERPTR,
} io_method;
typedef struct
{
    void * start;
    int length;
    int bytesused;
} BUFTYPE;
class Camera
{
public:
    Camera(char *DEV_NAME,char *FIMC_NAME,int w,int h);//构造函数
    ~Camera();
    bool InitDevice();//打开设备
    bool open_device(int *,int *);
    void CloseDevice();//关闭设备
    bool start_capturing();//开始捕捉
    unsigned int getImageSize();   //图像大小
    void  process_cam_to_fimc0();
    void  process_fimc0_to_cam(void);
    int  fimc0_cap_dbuf(int *index);
    int  fimc0_cap_qbuf(int index);
    void close_camer_device();
    //void uninit_device(BUFTYPE *);
private:
    char dev_name[50];//摄像头设备的名字
    char fimc_name[50];//FIMC设备名字
    io_method io;
    int fd;
    int fimc0_fd;
    bool  sign3;
    int width;
    int height;
    unsigned int cap_image_size ;
    bool  cam_setfmt(void);
    bool  cam_reqbufs();
    void  stop_capturing(void);//停止捕捉
    int   fimc0_setfmt(void);
    int   fimc0_reqbufs();
    int   cam_setrate(void);//设置帧率
    void  close_device(void);
    int  cam_cap_dbuf(int * );
    void  cam_cap_qbuf(int );
    int   fimc0_out_qbuf(int);
    int   fimc0_out_dbuf(int *);
    void  errno_exit(const char * s);
    int   xioctl(int fd, int request, void * arg);
    void  dump_format(char *str, struct v4l2_format *fmt);
};
#endif // CAMERA_H
