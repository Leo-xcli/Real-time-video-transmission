#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include "mfc.h"
MFC::MFC()
{    
}
SSBSIP_MFC_ERROR_CODE MFC::initMFC_ENC(int w, int h, int qb)
{
    SSBSIP_MFC_ERROR_CODE ret;
    enc_width=w;
    enc_height=h;
    enc_param = (SSBSIP_MFC_ENC_H264_PARAM*)malloc(sizeof(SSBSIP_MFC_ENC_H264_PARAM));
    memset(enc_param, 0 , sizeof(SSBSIP_MFC_ENC_H264_PARAM));
    enc_param->codecType=H264_ENC;
    enc_param->SourceWidth=enc_width;
    enc_param->SourceHeight=enc_height;
 //   enc_param->ProfileIDC=1;  
  //  enc_param->LevelIDC=40;
  //  enc_param->IDRPeriod=2;//3
   // enc_param->NumberReferenceFrames=1;
  //  enc_param->NumberRefForPframes=2;
  //  enc_param->SliceMode=0;//4//2
  //  enc_param->SliceArgument=0;
   // enc_param->NumberBFrames=0;
  //  enc_param->LoopFilterDisable=0;
   // enc_param->LoopFilterAlphaC0Offset=2;
  //  enc_param->LoopFilterBetaOffset=0;//1
  //  enc_param->SymbolMode=1;//0
   // enc_param->PictureInterlace=0;//0
  //  enc_param->Transform8x8Mode=1;//0
  //  enc_param->RandomIntraMBRefresh=0;
  //  enc_param->PadControlOn=0;
  //  enc_param->LumaPadVal=0;
  //  enc_param->CbPadVal=0;
   // enc_param->CrPadVal=0;
  //  enc_param->EnableFRMRateControl=0; //0 this must be 1 otherwise init error
 //   enc_param->EnableMBRateControl=0;
    enc_param->FrameRate=20;//30//18
 //   enc_param->Bitrate=9216000;//9216000
    enc_param->FrameQp=qb;
 //   enc_param->QSCodeMax=51;//0
  //  enc_param->QSCodeMin=10;//0
  //  enc_param->CBRPeriodRf=120;//20
  //  enc_param->DarkDisable=0;//1
  //  enc_param->SmoothDisable=0;//1
  //  enc_param->StaticDisable=0;//1
   // enc_param->ActivityDisable=0;//1
   enc_param->FrameQp_P = enc_param->FrameQp+1;
   enc_param->FrameQp_B = enc_param->FrameQp+3;
    hOpen = SsbSipMfcEncOpen();
    if(hOpen == NULL)
    {
        printf("SsbSipMfcEncOpen Failed\n");
        ret = MFC_RET_FAIL;
        return ret;
    }
    if(SsbSipMfcEncInit(hOpen, enc_param) != MFC_RET_OK)
    {
        printf("SsbSipMfcEncInit Failed\n");
        ret = MFC_RET_FAIL;
        goto out;
    }
    if(SsbSipMfcEncGetInBuf(hOpen, &input_info_enc) != MFC_RET_OK)
    {
        printf("SsbSipMfcEncGetInBuf Failed\n");
        ret = MFC_RET_FAIL;
        goto out;
    }
    ret=SsbSipMfcEncGetOutBuf(hOpen, &output_info_enc);
    if(output_info_enc.headerSize <= 0)
    {
        printf("Header Encoding Failed\n");
        ret = MFC_RET_FAIL;
        goto out;
    }
    headerSize=output_info_enc.headerSize;
    memcpy(header,output_info_enc.StrmVirAddr,headerSize);
    printf("MFC init success:: Yphy(0x%08x) Cphy(0x%08x)\n",
           input_info_enc.YPhyAddr, input_info_enc.CPhyAddr);
    return ret;
out:
    SsbSipMfcEncClose(hOpen);
    return ret;
}
int MFC::getHeader(unsigned char **p)
{
    //memcpy(*p,header,headerSize);
    *p=header;
    return headerSize;
}
void MFC::getInputBuf(void **Y,void **UV)
{
    *Y=input_info_enc.YVirAddr;
    *UV=input_info_enc.CVirAddr;
}
int MFC::encode(void *input_buf,void **output_buf)
{
   SSBSIP_MFC_ERROR_CODE err;
   int retv = 0;
   int ouputbuffer_size;
      printf("::MFC::encode start !!!\n");
      memcpy(input_info_enc.YVirAddr, (void*)input_buf, enc_width*enc_height);//¿camera¿¿¿Y¿¿
      memcpy(input_info_enc.CVirAddr,(void*)input_buf+(enc_width*enc_height), enc_width*enc_height/2);//+(CAMERA_WIDTH*CAMERA_HEIGHT)	
      //  printf("::MFC::SsbSipMfcEncSetInBuf !!!\n");
        err = SsbSipMfcEncSetInBuf(hOpen,&input_info_enc);
	        if(err<0)
            {
	            fprintf(stderr,"Error: SsbSipMfcEncSetInBuf. Code %d\n",err);
	            return 0;
	        }
    if(SsbSipMfcEncExe(hOpen) != MFC_RET_OK){
        printf("Encoding Failed\n");
        return 0;
    }
    SsbSipMfcEncGetOutBuf(hOpen, &output_info_enc);
    if(output_info_enc.StrmVirAddr == NULL)
    {
        printf("SsbSipMfcEncGetOutBuf Failed\n");
        return 0;
    }
     // memcpy(output_buf, output_info_enc.StrmVirAddr,  output_info_enc.dataSize);
   *output_buf=output_info_enc.StrmVirAddr;
    return output_info_enc.dataSize;
}
void MFC::closeMFC()
{
    SsbSipMfcEncClose(hOpen);
}
