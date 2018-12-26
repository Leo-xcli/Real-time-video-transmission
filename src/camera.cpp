#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>             /* getopt_long() */
#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>          /* for videodev2.h */
#include "videodev2.h"
#include "camera.h"
#include <stdarg.h>
#include <sys/mman.h>
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#define CLEAR(x) memset (&(x), 0, sizeof (x))
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#define ERRSTR strerror(errno)
#define ERR(...) __info("Error", __FILE__, __LINE__, __VA_ARGS__)
#define ERR_ON(cond, ...) ((cond) ? ERR(__VA_ARGS__) : 0)
#define CRIT(...) \
	do { \
		__info("Critical", __FILE__, __LINE__, __VA_ARGS__); \
		exit(EXIT_FAILURE); \
	} while(0)
#define CRIT_ON(cond, ...) do { if (cond) CRIT(__VA_ARGS__); } while(0)
extern  BUFTYPE* fimc0_out_buf;
extern  BUFTYPE* cam_buffers;
extern  BUFTYPE* fimc0_cap_buf;
extern  int fimc0_out_buf_length;
extern  int fimc0_cap_buf_length;
extern  char * image;
Camera::Camera(char *DEV_NAME,char *FIMC_NAME,int w, int h)//构造函数
{
    memcpy(dev_name,DEV_NAME,strlen(DEV_NAME));
    memcpy(fimc_name,FIMC_NAME,strlen(DEV_NAME));
    io = IO_METHOD_MMAP;//IO_METHOD_READ;//IO_METHOD_MMAP;
    cap_image_size=0;
    width=w;
    height=h;
}
Camera::~Camera()
{
}
static inline int __info(const char *prefix, const char *file, int line,
                         const char *fmt, ...)
{
    int errsv = errno;
    va_list va;
    va_start(va, fmt);
    fprintf(stderr, "%s(%s:%d): ", prefix, file, line);
    vfprintf(stderr, fmt, va);
    va_end(va);
    errno = errsv;
    return 1;
}
unsigned int Camera::getImageSize()
{
    return cap_image_size;
}
void Camera::CloseDevice()
{
    stop_capturing();
    // uninit_device();
    close_device();
}
void Camera::dump_format(char *str, struct v4l2_format *fmt)
{
    if (V4L2_TYPE_IS_MULTIPLANAR(fmt->type))
    {
        struct v4l2_pix_format_mplane *pix = &fmt->fmt.pix_mp;
        LOG("%s: width=%u height=%u format=%.4s bpl=%u\n", str,
            pix->width, pix->height, (char*)&pix->pixelformat,
            pix->plane_fmt[0].bytesperline);
    }
    else
    {
        struct v4l2_pix_format *pix = &fmt->fmt.pix;
        LOG("%s: width=%u height=%u format=%.4s bpl=%u\n", str,
            pix->width, pix->height, (char*)&pix->pixelformat,
            pix->bytesperline);
    }
}
void Camera::errno_exit(const char * s)
{
    fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
    exit(EXIT_FAILURE);
}
int Camera::xioctl(int fd, int request, void * arg)
{
    int r;
    do
        r = ioctl(fd, request, arg);
    while (-1 == r && EINTR == errno);
    return r;
}
void Camera::stop_capturing(void)
{
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == ioctl(fd,VIDIOC_STREAMOFF,&type))
    {
        perror("Fail to ioctl 'VIDIOC_STREAMOFF'");
        exit(EXIT_FAILURE);
    }
    return;
}
void Camera::close_camer_device()
{
    if(-1 == close(fd))
    {
        perror("Fail to close lcd_fd");
        exit(EXIT_FAILURE);
    }
    if(-1 == close(fimc0_fd))
    {
        perror("Fail to close cam_fd");
        exit(EXIT_FAILURE);
    }
    return;
}
bool Camera::start_capturing()
{
    unsigned int i;
    enum v4l2_buf_type type;
    int ret;
    struct v4l2_buffer b;
    struct v4l2_plane plane;
    printf("%s +\n", __func__);
    for(i = 0; i < CAPTURE_BUFFER_NUMBER; i ++)
    {
        struct v4l2_buffer buf;
        CLEAR(buf);
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if(-1 == ioctl(fd,VIDIOC_QBUF,&buf))
        {
            perror("cam Fail to ioctl 'VIDIOC_QBUF'");
            exit(EXIT_FAILURE);
        }
    }
    for(i = 0; i < CAPTURE_BUFFER_NUMBER; i++)
    {
        CLEAR(plane);
        CLEAR(b);
        b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        b.memory = V4L2_MEMORY_MMAP;
        b.index = i;
        b.m.planes = &plane;
        b.length = 1;
        if(b.memory == V4L2_MEMORY_USERPTR)
        {
            plane.m.userptr = (unsigned long)fimc0_cap_buf[i].start;
            plane.length = (unsigned long)fimc0_cap_buf_length;
            plane.bytesused = fimc0_cap_buf_length;
        }
        ret = ioctl(fimc0_fd, VIDIOC_QBUF, &b);
        if (ERR_ON(ret < 0, "fimc0: VIDIOC_QBUF: %s\n", ERRSTR))
            return -errno;
        /*
        b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        b.memory = V4L2_MEMORY_USERPTR;
        b.index = i;
        b.m.planes = &plane;
        b.length = 1;
        if(b.memory == V4L2_MEMORY_USERPTR)
        {
        	plane.m.userptr = (unsigned long)fimc0_cap_buf[i].start;
        	plane.length = (unsigned long)fimc0_cap_buf_length;
        	plane.bytesused = fimc0_cap_buf_length;
        }
        ret = ioctl(fimc0_fd, VIDIOC_QBUF, &b);
        if (ERR_ON(ret < 0, "fimc0: VIDIOC_QBUF: %s\n", ERRSTR))
        	return -errno;	*/
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == ioctl(fd,VIDIOC_STREAMON,&type))
    {
        printf("i = %d.\n",i);
        perror("cam_fd Fail to ioctl 'VIDIOC_STREAMON'");
        exit(EXIT_FAILURE);
    }
    /* start processing */
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    ret = ioctl(fimc0_fd, VIDIOC_STREAMON, &type);
    if (ERR_ON(ret < 0, "fimc0: VIDIOC_STREAMON: %s\n", ERRSTR))
        return -errno;
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    ret = ioctl(fimc0_fd, VIDIOC_STREAMON, &type);
    if (ERR_ON(ret < 0, "fimc0: VIDIOC_STREAMON: %s\n", ERRSTR))
        return -errno;
    printf("%s -\n", __func__);
    return 0;
}
/*
void Camera::uninit_device(BUFTYPE * fimc0_out_buf) {
  unsigned int i;
	for(i = 0;i < CAPTURE_BUFFER_NUMBER;i ++)
	{
		if(-1 == munmap(fimc0_out_buf[i].start, fimc0_out_buf[i].length))
		{
			exit(EXIT_FAILURE);
		}
	}
	free(fimc0_out_buf);
} */
bool Camera::cam_reqbufs()
{
    struct v4l2_requestbuffers req;
    int i;
    printf("%s: +\n", __func__);
    CLEAR(req);
    req.count  = CAPTURE_BUFFER_NUMBER;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (-1 == ioctl(fd, VIDIOC_REQBUFS, &req))
    {
        if (EINVAL == errno)
        {
            fprintf(stderr, "%s does not support "
                    "user pointer i/o\n", "campture");
            exit(EXIT_FAILURE);
        }
        else
        {
            printf("VIDIOC_REQBUFS");
            exit(EXIT_FAILURE);
        }
    }
    cam_buffers = (BUFTYPE *)calloc(CAPTURE_BUFFER_NUMBER, sizeof(BUFTYPE));
    if (!cam_buffers)
    {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }
    for (i = 0; i < CAPTURE_BUFFER_NUMBER; ++i)
    {
        struct v4l2_buffer buf;
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if(-1 == ioctl(fd, VIDIOC_QUERYBUF, &buf))
        {
            perror("Fail to ioctl : VIDIOC_QUERYBUF");
            exit(EXIT_FAILURE);
        }
        cam_buffers[i].length = buf.length;
        cam_buffers[i].start =
            mmap(
                NULL,/*start anywhere*/
                buf.length,
                PROT_READ | PROT_WRITE,
                MAP_SHARED,
                fd,buf.m.offset
            );
        if(MAP_FAILED == cam_buffers[i].start)
        {
            perror("Fail to mmap\n");
            printf("%d\n",i);
            exit(EXIT_FAILURE);
        }
        printf("cam_capture rebuf::08%lx\n", cam_buffers[i].start);
    }
    printf("%s: -\n", __func__);
}
int  Camera::cam_setrate()
{
    int err;
    int ret;
    struct v4l2_streamparm stream;
    CLEAR(stream);
    stream.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    stream.parm.capture.capturemode = 0;
    stream.parm.capture.timeperframe.numerator = 1;
    stream.parm.capture.timeperframe.denominator = FPS;
    err = xioctl(fd, VIDIOC_S_PARM, &stream);
    if(err < 0)
        printf("FimcV4l2 start: error %d, VIDIOC_S_PARM", err);
    return 0;
}
bool Camera::cam_setfmt(void)
{
    v4l2_input input;
    memset(&input, 0, sizeof(struct v4l2_input));
    input.index = 0;
    if (xioctl(fd, VIDIOC_ENUMINPUT, &input) != 0)
    {
        fprintf(stderr, "No matching index found\n");
        return false;
    }
    if (!input.name)
    {
        fprintf(stderr, "No matching index found\n");
        return false;
    }
    if (xioctl(fd, VIDIOC_S_INPUT, &input) < 0)
    {
        fprintf(stderr, "VIDIOC_S_INPUT failed\n");
        return false;
    }
    /*******************查询当前摄像头所支持的输出格式************/
    struct v4l2_fmtdesc fmt_aviable; //v4l2_fmtdesc : 帧格式结构体
    int ret;
    memset(&fmt_aviable, 0, sizeof(fmt_aviable));//将fmt1结构体填充为0
    fmt_aviable.index = 0;            //初始化为0，要查询的格式序号
    fmt_aviable.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; // 数据流类型，必须永远是V4L2_BUF_TYPE_VIDEO_CAPTURE
    while ((ret = xioctl(fd, VIDIOC_ENUM_FMT, &fmt_aviable)) == 0) //显示所有支持的格式
    {
        fmt_aviable.index++;
        printf("{ pixelformat = '%c%c%c%c', description = '%s' }\n",fmt_aviable.pixelformat & 0xFF,
               (fmt_aviable.pixelformat >> 8) & 0xFF,(fmt_aviable.pixelformat >> 16) & 0xFF,
               (fmt_aviable.pixelformat >> 24) & 0xFF,fmt_aviable.description); //  pixelformat;格式32位   description[32];// 格式名称8位
    }
    /*******************查询驱动功能************************/
    struct v4l2_capability cap;
    ret = xioctl(fd,VIDIOC_QUERYCAP,&cap);
    if(ret < 0)
    {
        perror("FAIL to ioctl VIDIOC_QUERYCAP");
        exit(EXIT_FAILURE);
    }
    if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        printf("The Current device is not a video capture device\n");
        exit(EXIT_FAILURE);
    }
    if(!(cap.capabilities & V4L2_CAP_STREAMING))
    {
        printf("The Current device does not support streaming i/o\n");
        exit(EXIT_FAILURE);
    }
    /**************************设置当前帧格式************************/
    struct v4l2_format fmt; //设置当前格式结构体(v4l2_format包含v4l2_fmtdesc  fmt为共用体)
    CLEAR (fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;//数据流类型
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;//V4L2_PIX_FMT_NV12;//视频数据存储类型
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))//设置图像格式
    {
        printf("Can't set the fmt\n");
        perror("Fail to ioctl\n");
        exit(EXIT_FAILURE);
        return false;
    }
    /**************************读取当前帧格式*************************/
    if (-1 == xioctl(fd, VIDIOC_G_FMT, &fmt))
        return false;
    else
    {
        printf("\nCurrent data format information:\n twidth:%d \n theight:%d \n",
               fmt.fmt.pix.width,fmt.fmt.pix.height);
        printf(" pixelformat = '%c%c%c%c'\n",fmt.fmt.pix.pixelformat & 0xFF,
               (fmt.fmt.pix.pixelformat >> 8) & 0xFF,(fmt.fmt.pix.pixelformat >> 16) & 0xFF,
               (fmt.fmt.pix.pixelformat >> 24) & 0xFF); //  pixelformat;格式32位
    }
    printf("VIDIOC_S_FMT successfully\n");//设置帧格式成功
    //原始摄像头数据每帧的大小
    cap_image_size = fmt.fmt.pix.sizeimage;
    return true;
}
int  Camera::fimc0_setfmt()
{
    int ret;
    struct v4l2_format stream_fmt;
    printf("%s: +\n", __func__);
    CLEAR(stream_fmt);
    stream_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    stream_fmt.fmt.pix.width = width;
    stream_fmt.fmt.pix.height = height;
    stream_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    stream_fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    /* get format from VIVI */
    ret = ioctl(fd, VIDIOC_G_FMT, &stream_fmt);
    if (ERR_ON(ret < 0, "vivi: VIDIOC_G_FMT: %s\n", ERRSTR))
        return -errno;
    dump_format("cam_fd-capture", &stream_fmt);
    /* setup format for FIMC 0 */
    /* keep copy of format for to-mplane conversion */
    struct v4l2_pix_format pix = stream_fmt.fmt.pix; //在stream_fmt被格式化前将帧信息保存至pix
    CLEAR(stream_fmt);
    stream_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    struct v4l2_pix_format_mplane *pix_mp = &stream_fmt.fmt.pix_mp;
    pix_mp->width = pix.width;  //640
    pix_mp->height = pix.height;  //480
    pix_mp->pixelformat = pix.pixelformat;//stream_fmt.fmt.pix.pixelformat=V4L2_PIX_FMT_YUYV  因为pix = stream_fmt.fmt.pix
    pix_mp->num_planes = 1;
    pix_mp->plane_fmt[0].bytesperline = pix.bytesperline;
    dump_format("fimc0-output", &stream_fmt);
    ret = ioctl(fimc0_fd, VIDIOC_S_FMT, &stream_fmt);
    if (ERR_ON(ret < 0, "fimc0: VIDIOC_S_FMT: %s\n", ERRSTR))
        return -errno;
    dump_format("fimc0-output", &stream_fmt);
    /* set format on fimc0 capture */
    stream_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    /* try cmdline format, or use fimc0-output instead */
    struct v4l2_pix_format_mplane *pix_mp_f = &stream_fmt.fmt.pix_mp;
    CLEAR(*pix_mp_f);
    pix_mp_f->pixelformat = V4L2_PIX_FMT_NV12;
    pix_mp_f->width = 640;
    pix_mp_f->height = 480;
    pix_mp_f->plane_fmt[0].bytesperline = 0;
    dump_format("pre-fimc0-capture", &stream_fmt);
    ret = ioctl(fimc0_fd, VIDIOC_S_FMT, &stream_fmt);
    if (ERR_ON(ret < 0, "fimc0: VIDIOC_S_FMT: %s\n", ERRSTR))
        return -errno;
    printf("%s -\n", __func__);
}
int Camera::fimc0_reqbufs()
{
    int ret;
    struct v4l2_requestbuffers rb;
    CLEAR(rb);
    /* enqueue the dmabuf to vivi */
    struct v4l2_buffer b;
    CLEAR(b);
    printf("%s: +\n", __func__);
    /* request buffers for FIMC0 */
    rb.count = CAPTURE_BUFFER_NUMBER;
    rb.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    rb.memory = V4L2_MEMORY_MMAP;
    ret = ioctl(fimc0_fd, VIDIOC_REQBUFS, &rb);
    if (ERR_ON(ret < 0, "fimc0: VIDIOC_REQBUFS: %s\n", ERRSTR))
        return -errno;
    printf("fimc0 output_buf_num:%d\n",rb.count);
    int n;
    fimc0_out_buf = (BUFTYPE*)calloc(rb.count,sizeof(BUFTYPE));
    if(fimc0_out_buf == NULL)
    {
        fprintf(stderr,"Out of memory\n");
        exit(EXIT_FAILURE);
    }
    printf("%s, fimc0_out_buf request successfully\n",__func__);
    /* mmap DMABUF */
    struct v4l2_plane plane;
    for (n = 0; n < CAPTURE_BUFFER_NUMBER; ++n)
    {
        b.index = n;
        b.length = 1;
        b.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        b.memory = V4L2_MEMORY_MMAP;
        b.m.planes = &plane;
        ret = ioctl(fimc0_fd, VIDIOC_QUERYBUF, &b);
        if (ERR_ON(ret < 0, "fimc0: VIDIOC_REQBUFS: %s\n", ERRSTR))
            exit(EXIT_FAILURE);
        fimc0_out_buf[n].start = mmap(NULL,
                                      b.m.planes->length,
                                      PROT_READ | PROT_WRITE,
                                      MAP_SHARED, fimc0_fd,
                                      b.m.planes->m.mem_offset);
        fimc0_out_buf[n].length = b.m.planes->length;
        if (fimc0_out_buf[n].start == MAP_FAILED)
        {
            printf("Failed mmap buffer %d for %d\n", n,
                   fimc0_fd);
            return -1;
        }
        fimc0_out_buf_length = b.m.planes->length;
        printf("fimc0 querybuf:0x%08lx,%d,%d\n", fimc0_out_buf[n], fimc0_out_buf_length, b.m.planes->m.mem_offset);
    }
    CLEAR(plane);
    CLEAR(b);
    CLEAR(rb);
    rb.count = CAPTURE_BUFFER_NUMBER;
    rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    rb.memory = V4L2_MEMORY_MMAP;
    ret = ioctl(fimc0_fd, VIDIOC_REQBUFS, &rb);
    if (ERR_ON(ret < 0, "fimc0: VIDIOC_REQBUFS: %s\n", ERRSTR))
        return -errno;
    printf("fimc0 CAPTURE_buf_num:%d\n",rb.count);
    fimc0_cap_buf = (BUFTYPE*)calloc(CAPTURE_BUFFER_NUMBER, sizeof(BUFTYPE));
    if (ERR_ON(fimc0_cap_buf == NULL, "fimc0_cap_buf: VIDIOC_QUERYBUF: %s\n", ERRSTR))
        return -errno;
    for (n = 0; n < CAPTURE_BUFFER_NUMBER; ++n)
    {
        b.index = n;
        b.length = 1;
        b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        b.memory = V4L2_MEMORY_MMAP;
        b.m.planes = &plane;
        ret = ioctl(fimc0_fd, VIDIOC_QUERYBUF, &b);
        if (ERR_ON(ret < 0, "fimc0: VIDIOC_REQBUFS: %s\n", ERRSTR))
            exit(EXIT_FAILURE);
        fimc0_cap_buf[n].start = mmap(NULL,
                                      b.m.planes->length,
                                      PROT_READ | PROT_WRITE,
                                      MAP_SHARED, fimc0_fd,
                                      b.m.planes->m.mem_offset);
        fimc0_cap_buf[n].length = b.m.planes->length;
        if (fimc0_cap_buf[n].start == MAP_FAILED)
        {
            printf("Failed mmap buffer %d for %d\n", n,
                   fimc0_fd);
            return -1;
        }
        fimc0_cap_buf_length = b.m.planes->length;
        printf("fimc0 querybuf:0x%08lx,%d,%d\n", fimc0_cap_buf[n], fimc0_cap_buf_length, b.m.planes->m.mem_offset);
    }
    /*
    rb.count = CAPTURE_BUFFER_NUMBER;
    rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    rb.memory = V4L2_MEMORY_USERPTR;
    ret = ioctl(fimc0_fd, VIDIOC_REQBUFS, &rb);
    if (ERR_ON(ret < 0, "fimc0: VIDIOC_REQBUFS: %s\n", ERRSTR))
    	return -errno;
    for (n = 0; n < CAPTURE_BUFFER_NUMBER; ++n)
    {
    	fimc0_cap_buf[n].start = image;
    	if (!fimc0_cap_buf[n].start) {
    			printf("Failed mmap buffer %d for %d\n", n,
    						fimc0_fd);
    			return -1;
    	}
    }
    	fimc0_cap_buf_length = 640*480*3/2;*/
    printf("fimc0 capture:plane.length:%d\n",fimc0_cap_buf_length);
    printf("%s -\n", __func__);
    return 0;
}
void Camera::process_cam_to_fimc0()
{
    int index;//摄像投缓存中的某一帧
    cam_cap_dbuf(&index);//返回捕获到的是第几真
    memcpy(fimc0_out_buf[index].start, cam_buffers[index].start, fimc0_out_buf_length);
    fimc0_out_qbuf(index);
}
int Camera::cam_cap_dbuf(int * index)
{
    struct v4l2_buffer buf;
    bzero(&buf,sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bzero(&buf,sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if(-1 == ioctl(fd,VIDIOC_DQBUF,&buf))//获取一帧图
    {
        perror("cam Fail to ioctl 'VIDIOC_DQBUF'");
        exit(EXIT_FAILURE);
    }
    cam_buffers[buf.index].bytesused = buf.bytesused;//一帧大小
    *index = buf.index;//具体缓存中的第几帧
    return 0;
}
int Camera::fimc0_out_qbuf(int index)
{
    int ret;
    struct v4l2_buffer b;
    struct v4l2_plane plane;
    /* enqueue buffer to fimc0 output */
    CLEAR(plane);
    CLEAR(b);
    b.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    b.memory = V4L2_MEMORY_MMAP;
    b.index = index;
    b.m.planes = &plane;
    b.length = 1;
    plane.m.userptr = (unsigned long)fimc0_out_buf[index].start;//指向具体某一帧原始图像起始地址
    plane.length = (unsigned long)fimc0_out_buf[index].length;
    plane.bytesused = fimc0_out_buf[index].length;
    ret = ioctl(fimc0_fd, VIDIOC_QBUF, &b);//缓存入列
    if (ERR_ON(ret < 0, "fimc0: VIDIOC_QBUF: %s\n", ERRSTR))
        return -errno;
    return true;
}
void Camera::process_fimc0_to_cam()
{
    int index;
    fimc0_out_dbuf(&index);
    cam_cap_qbuf(index);
}
int Camera::fimc0_out_dbuf(int *index)
{
    int ret;
    struct v4l2_buffer b;
    struct v4l2_plane plane[3];
    /* enqueue buffer to fimc0 output */
    CLEAR(plane);
    CLEAR(b);
    b.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    b.memory = V4L2_MEMORY_USERPTR;
    b.m.planes = plane;
    b.length = 1;
    ret = ioctl(fimc0_fd, VIDIOC_DQBUF, &b);
    *index = b.index;
    if (ERR_ON(ret < 0, "fimc0: VIDIOC_DQBUF: %s\n", ERRSTR))
        return -errno;
    return true;
}
void Camera::cam_cap_qbuf(int index)
{
    struct v4l2_buffer buf;
    bzero(&buf,sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = index;
    if(-1 == ioctl(fd,VIDIOC_QBUF,&buf)) //再将其入列，把数据放回缓存队列
    {
        perror("Fail to ioctl 'VIDIOC_QBUF'");
        exit(EXIT_FAILURE);
    }
}
int Camera::fimc0_cap_dbuf(int *index)
{
    int ret;
    struct v4l2_buffer b;
    struct v4l2_plane plane[3];
    static int count = 0;
    /* enqueue buffer to fimc0 output */
    CLEAR(plane);
    CLEAR(b);
    /*
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    b.memory = V4L2_MEMORY_MMAP;
    b.m.planes = plane;
    b.length = 1;*/
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    b.memory = V4L2_MEMORY_USERPTR;
    b.m.planes = plane;
    b.length = 1;
    ret = ioctl(fimc0_fd, VIDIOC_DQBUF, &b);//读取缓存中的数据，放到b
    *index = b.index;//缓存帧序号
    //fwrite(image,1, fimc0_cap_buf_length,out_NV12);
    //fwrite(fimc0_cap_buf[b.index].start,1, fimc0_cap_buf_length,out_NV12);
    if (ERR_ON(ret < 0, "fimc0: VIDIOC_DQBUF: %s\n", ERRSTR))
        return -errno;
    count ++;
    return 0;
}
int Camera::fimc0_cap_qbuf(int index)
{
    int ret;
    struct v4l2_buffer b;
    struct v4l2_plane plane;
    CLEAR(plane);
    CLEAR(b);
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    b.memory = V4L2_MEMORY_MMAP;
    b.m.planes = &plane;
    b.length = 1;
    b.index = index;
    plane.m.userptr = (unsigned long)fimc0_cap_buf[index].start;
    plane.length = (unsigned long)fimc0_cap_buf_length;
    plane.bytesused = fimc0_cap_buf_length;
    /*
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    b.memory = V4L2_MEMORY_USERPTR;
    b.m.planes = &plane;
    b.length = 1;
    b.index = index;
    plane.m.userptr = (unsigned long)fimc0_cap_buf[index].start;
    plane.length = (unsigned long)fimc0_cap_buf_length;
    plane.bytesused = fimc0_cap_buf_length;*/
    ret = ioctl(fimc0_fd, VIDIOC_QBUF, &b); //再将其入列，把数据放回缓存队列
    if (ERR_ON(ret < 0, "fimc0: VIDIOC_QBUF: %s\n", ERRSTR))
        return -errno;
    return true;
}
void Camera::close_device(void)
{
    if (-1 == close(fd))
        errno_exit("close");
    fd = -1;
}
bool Camera::open_device(int *cam_fd,int * fimc_fd)
{
    struct stat st;
    if ((-1 == stat(dev_name, &st))||(-1 == stat(fimc_name, &st))) //通过文件名filename获取文件信息，并保存在buf所指的结构体st中
    {
        fprintf(stderr, "Cannot identify '%s': %d, %s\n", dev_name, errno,
                strerror(errno));
        return false;
    }
    if (!S_ISCHR(st.st_mode))
    {
        fprintf(stderr, "%s is no device\n", dev_name);
        return false;
    }
    fd = ::open(dev_name, O_RDWR | O_NONBLOCK);
    if (-1 == fd)
    {
        fprintf(stderr, "Cannot open '%s': %d, %s\n", dev_name, errno,
                strerror(errno));
        return false;
    }
    fimc0_fd = ::open(fimc_name, O_RDWR | O_NONBLOCK);
    if (-1 == fd)
    {
        fprintf(stderr, "Cannot open '%s': %d, %s\n", dev_name, errno,
                strerror(errno));
        return false;
    }
    *cam_fd = fd;
    *fimc_fd = fimc0_fd;
    printf("open cam success %d  fimc success %d \n",*cam_fd,*fimc_fd);
    return true;
}
bool Camera::InitDevice()
{
    cam_setfmt();//为Camera设置帧格式
    fimc0_setfmt();//为FIMC设置帧格式
    fimc0_reqbufs( );
    cam_reqbufs();//位Camera申请缓存
    cam_setrate();//Camera设置帧率
    return true;
}
