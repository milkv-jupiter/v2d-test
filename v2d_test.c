/*
* V2D test for Spacemit
* Copyright (C) 2023 Spacemit Co., Ltd.
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sched.h>
#include <sys/mman.h>
#include <time.h>
#include <sys/cdefs.h>
#include <sys/sysinfo.h>
#include <sys/ioctl.h>
#include "v2d_api.h"
#include "v2d_type.h"
#include "dmabufheap/BufferAllocatorWrapper.h"

char *pFbcCase0Layer0H = "320_240_yuv_s0_header.fbc";
char *pFbcCase0Layer0B = "320_240_yuv_s0_payload.fbc";
char *pFbcCase0Header  = "adv_320_240_rgb888_s1_header.fbc";
char *pFbcCase0Body    = "adv_320_240_rgb888_s1_payload.fbc";
char *pFbcCase0Raw     = "adv_320_240_rgb888.raw";
char *pRawData         = "320_240_bt601_n.yuv";
char *sysfile          = "/sys/bus/platform/devices/c0100000.v2d/clkrate";

struct v2d_alloc_dma_buf {
	int fd;	/* fd */
	uint32_t flags;	/* flags to map with */
	uint64_t size; /* size */
};
BufferAllocator* bufferAllocator = NULL;

#define ALIGN_UP(size, shift) (((size+shift-1)/shift)*shift)
#define PAGESIZE (4096)

static int write_sysfile(const char *file, const char *str)
{
        int fd;

        if ((fd = open(file, O_WRONLY)) < 0) {
                printf("failed to open %s\n", file);
                return -errno;
        }
        write(fd, str, strlen(str));
        close(fd);

        return 0;
}

int readFile(char *pFileName, void* pBuff)
{
	int ret = 0;
	unsigned int filesize = 0;

	FILE* pImageFile = fopen(pFileName, "rb");
	if (!pImageFile)
	{
		printf("Error in read %s,file not found\n", pFileName);
		return -1;
	}
	fseek(pImageFile, 0, SEEK_END);
	filesize = ftell(pImageFile);
	fseek(pImageFile, 0, SEEK_SET);
	size_t elementsRead = fread(pBuff, filesize, 1, pImageFile);
	fclose(pImageFile);

	return ret;
}

int writeFile(char *pFileName, int size, void* pBuff)
{
	int ret = 0;
	unsigned int filesize = 0;

	FILE* pImageFile = fopen(pFileName, "rb");
	if (!pImageFile)
	{
		printf("Error in read %s,file not found\n", pFileName);
		return -1;
	}
	size_t elementsRead = fwrite(pBuff, size, 1, pImageFile);
	fclose(pImageFile);

	return ret;
}
void createAllocator()
{
	if (bufferAllocator == NULL) {
		bufferAllocator = CreateDmabufHeapBufferAllocator();
	}
}
void destroyAllocator()
{
	if (bufferAllocator) {
		FreeDmabufHeapBufferAllocator(bufferAllocator);
		bufferAllocator = NULL;
	}
}

int v2d_adv(void)
{
	int ret = 0;
	V2D_HANDLE hHandle;
	V2D_SURFACE_S stBackGround, stForeGround, stDst;
	V2D_AREA_S stBackGroundRect, stForeGroundRect, stDstRect;
	V2D_BLEND_CONF_S stBlendConf;
	V2D_ROTATE_ANGLE_E enForeRotate, enBackRotate;
	V2D_CSC_MODE_E enForeCSCMode, enBackCSCMode;
	V2D_DITHER_E dither;
	void *tmp = NULL;
	int i;
	unsigned int *real, *expect;
	bool cpu_access_need = true;

	struct v2d_alloc_dma_buf in, out;
	void *pLayer0, *pLayer1, *pDst;
	unsigned int mapsize0, mapsize1, mapsize2;

	V2DLOGD("v2d adv test start\n");
	in.size    = 115200+4800;
	out.size   = 320*240*3+4800;
	mapsize0   = ALIGN_UP(in.size,  PAGESIZE);
	mapsize2   = ALIGN_UP(out.size, PAGESIZE);
	createAllocator();
	in.fd = DmabufHeapAllocSystem(bufferAllocator, cpu_access_need, mapsize0, 0, 0);
	out.fd = DmabufHeapAllocSystem(bufferAllocator, cpu_access_need, mapsize2, 0, 0);
	printf("dmabuf fd:%u, %u\n", in.fd, out.fd);
	pLayer0 = mmap(NULL, mapsize0, PROT_READ | PROT_WRITE, MAP_SHARED, in.fd, 0);
	if (pLayer0 == MAP_FAILED) {
		V2DLOGD(" v2d mmap layer0 failed\n");
	}
	pDst = mmap(NULL, mapsize2, PROT_READ | PROT_WRITE, MAP_SHARED, out.fd, 0);
	if (pDst == MAP_FAILED) {
		V2DLOGD(" v2d mmap dst failed\n");
	}
	ret = readFile(pFbcCase0Layer0H, pLayer0);	
	ret = readFile(pFbcCase0Layer0B,  pLayer0+4800);
	memset(pDst, 0,  ALIGN_UP(out.size, PAGESIZE));	
	//config layer0
	enBackRotate  = V2D_ROT_MIRROR;
	enBackCSCMode = V2D_CSC_MODE_BT601NARROW_2_RGB;
	memset(&stBackGround, 0, sizeof(V2D_SURFACE_S));
	stBackGround.fbc_enable = 1;
	stBackGround.fd         = 0;
	stBackGround.offset     = 0;
	stBackGround.w          = 320;
	stBackGround.h          = 240;
	stBackGround.stride     = 320;
	stBackGround.format     = V2D_COLOR_FORMAT_NV12;
	stBackGroundRect.x      = 0;
	stBackGroundRect.y      = 0;
	stBackGroundRect.w      = 320;
	stBackGroundRect.h      = 240;
	stBackGround.fbcDecInfo.fd          = in.fd;	
	stBackGround.fbcDecInfo.bboxLeft    = 0;
	stBackGround.fbcDecInfo.bboxTop     = 0;
	stBackGround.fbcDecInfo.bboxRight   = 319;
	stBackGround.fbcDecInfo.bboxBottom  = 239;
	stBackGround.fbcDecInfo.enFbcdecFmt = FBC_DECODER_FORMAT_NV12;
	stBackGround.fbcDecInfo.is_split    = 0;
	stBackGround.fbcDecInfo.enFbcdecMode= FBC_DECODER_MODE_SCAN_LINE;
	//config layer1
	enForeRotate  = V2D_ROT_0;
	enForeCSCMode = V2D_CSC_MODE_BUTT;	
	//config output
	dither        = V2D_NO_DITHER;	
	memset(&stDst, 0, sizeof(V2D_SURFACE_S));
	stDst.fbc_enable = 0;
	stDst.fd         = out.fd;
	stDst.offset     = 0x00;
	stDst.w          = 320;
	stDst.h          = 240;
	stDst.stride     = 320*3;
	stDst.format     = V2D_COLOR_FORMAT_RGB888;
	stDstRect.x      = 0;
	stDstRect.y      = 0;
	stDstRect.w      = 320;
	stDstRect.h      = 240;
	//stDst.fbcEncInfo.fd          = out.fd;
	//stDst.fbcEncInfo.offset      = 320*240/16;
	//stDst.fbcEncInfo.bboxLeft    = 0;	
	//stDst.fbcEncInfo.bboxTop     = 0;	
	//stDst.fbcEncInfo.bboxRight   = 319;
	//stDst.fbcEncInfo.bboxBottom  = 239;
	//stDst.fbcEncInfo.enFbcencFmt = FBC_DECODER_FORMAT_RGB888;
	//stDst.fbcEncInfo.is_split    = 1;
	//config blend layer
	memset(&stBlendConf, 0, sizeof(V2D_BLEND_CONF_S));
	stBlendConf.blendlayer[0].blend_area.x = 0;
	stBlendConf.blendlayer[0].blend_area.y = 0;
	stBlendConf.blendlayer[0].blend_area.w = 320;
	stBlendConf.blendlayer[0].blend_area.h = 240;
	ret = V2D_BeginJob(&hHandle);
	if (ret) {
		V2DLOGD("V2D_BeginJob err\n");
		return ret;
	}
	
	ret = V2D_AddBlendTask(hHandle, &stBackGround, &stBackGroundRect, NULL, NULL, NULL, NULL, &stDst, 
								&stDstRect, &stBlendConf, enForeRotate, enBackRotate, enForeCSCMode, enBackCSCMode, NULL, dither);
	if (ret) {
		V2DLOGD("V2D_AddBlendTask err\n");
		return ret;
	}
	
	ret = V2D_EndJob(hHandle);
	if (ret) {
		V2DLOGD("V2D_EndJob err\n");
		return ret;
	}
	tmp = malloc(out.size);
	if (!tmp) {
		V2DLOGD("malloc fail\n");
	}
	if (stDst.fbc_enable) {
		readFile(pFbcCase0Header, tmp);
		readFile(pFbcCase0Body, tmp+4800);
	} else {
		readFile(pFbcCase0Raw, tmp);
	}
	real = (unsigned int *)tmp;
	expect = (unsigned int *)pDst;
	for (i=0; i<out.size/4; i++) {
		if (*(expect+i) != *(real+i)) {
			V2DLOGD("i:%d,exp:0x%08x,real:0x%08x\n",i,*(expect+i),*(real+i));
			ret = 1;
			break;
		}
	}
	munmap(pLayer0, mapsize0);
	munmap(pDst,    mapsize2);
	destroyAllocator();
	V2DLOGD("v2d adv %s\n", ret ? "v2d blend test case failed!":"v2d blend test case successful!");

	return ret;
}

int v2d_SimpleCase(void)
{
	int ret = 0;
	V2D_HANDLE hHandle;
	V2D_SURFACE_S stBackGround, stForeGround, stDst;
	V2D_AREA_S stBackGroundRect, stForeGroundRect, stDstRect;
	V2D_BLEND_CONF_S stBlendConf;
	V2D_ROTATE_ANGLE_E enForeRotate, enBackRotate;
	V2D_CSC_MODE_E enForeCSCMode, enBackCSCMode;
	V2D_DITHER_E dither;
	void *tmp = NULL;
	int i;
	unsigned int *real, *expect;
	bool cpu_access_need = true;

	struct v2d_alloc_dma_buf in, out;
	void *pLayer0, *pDst;
	unsigned int mapsize0, mapsize2;

	V2DLOGD("v2d simple test start\n");
	in.size    = 115200;
	out.size   = 115200;
	mapsize0   = ALIGN_UP(in.size,  PAGESIZE);
	mapsize2   = ALIGN_UP(out.size, PAGESIZE);
	createAllocator();
	in.fd = DmabufHeapAllocSystem(bufferAllocator, cpu_access_need, mapsize0, 0, 0);
	out.fd = DmabufHeapAllocSystem(bufferAllocator, cpu_access_need, mapsize2, 0, 0);
	printf("dmabuf fd:%u, %u\n", in.fd, out.fd);
	pLayer0 = mmap(NULL, mapsize0, PROT_READ | PROT_WRITE, MAP_SHARED, in.fd, 0);
	if (pLayer0 == MAP_FAILED) {
		V2DLOGD(" v2d mmap layer0 failed\n");
	}
	pDst = mmap(NULL, mapsize2, PROT_READ | PROT_WRITE, MAP_SHARED, out.fd, 0);
	if (pDst == MAP_FAILED) {
		V2DLOGD(" v2d mmap dst failed\n");
	}
	ret = readFile(pRawData, pLayer0);
	memset(pDst, 0,  ALIGN_UP(out.size, PAGESIZE));	
	//config layer0
	enBackRotate  = V2D_ROT_0;
	enBackCSCMode = V2D_CSC_MODE_BUTT;
	memset(&stBackGround, 0, sizeof(V2D_SURFACE_S));
	stBackGround.fbc_enable = 0;
	stBackGround.fd         = in.fd;
	stBackGround.offset     = 320*240;
	stBackGround.w          = 320;
	stBackGround.h          = 240;
	stBackGround.stride     = 320;
	stBackGround.format     = V2D_COLOR_FORMAT_NV12;
	stBackGroundRect.x      = 0;
	stBackGroundRect.y      = 0;
	stBackGroundRect.w      = 320;
	stBackGroundRect.h      = 240;
	//stBackGround.fbcDecInfo.fd          = in.fd;
	//stBackGround.fbcDecInfo.bboxLeft    = 0;
	//stBackGround.fbcDecInfo.bboxTop     = 0;
	//stBackGround.fbcDecInfo.bboxRight   = 319;
	//stBackGround.fbcDecInfo.bboxBottom  = 239;
	//stBackGround.fbcDecInfo.enFbcdecFmt = FBC_DECODER_FORMAT_NV12;
	//stBackGround.fbcDecInfo.is_split    = 0;
	//stBackGround.fbcDecInfo.enFbcdecMode= FBC_DECODER_MODE_SCAN_LINE;
	//config layer1
	enForeRotate  = V2D_ROT_0;
	enForeCSCMode = V2D_CSC_MODE_BUTT;
	//config output
	dither        = V2D_NO_DITHER;
	memset(&stDst, 0, sizeof(V2D_SURFACE_S));
	stDst.fbc_enable = 0;
	stDst.fd         = out.fd;
	stDst.offset     = 320*240;
	stDst.w          = 320;
	stDst.h          = 240;
	stDst.stride     = 320;
	stDst.format     = V2D_COLOR_FORMAT_NV12;
	stDstRect.x      = 0;
	stDstRect.y      = 0;
	stDstRect.w      = 320;
	stDstRect.h      = 240;
	//stDst.fbcEncInfo.fd          = out.fd;
	//stDst.fbcEncInfo.offset      = 320*240/16;
	//stDst.fbcEncInfo.bboxLeft    = 0;
	//stDst.fbcEncInfo.bboxTop     = 0;
	//stDst.fbcEncInfo.bboxRight   = 319;
	//stDst.fbcEncInfo.bboxBottom  = 239;
	//stDst.fbcEncInfo.enFbcencFmt = FBC_DECODER_FORMAT_RGB888;
	//stDst.fbcEncInfo.is_split    = 1;
	//config blend layer
	memset(&stBlendConf, 0, sizeof(V2D_BLEND_CONF_S));
	stBlendConf.blendlayer[0].blend_area.x = 0;
	stBlendConf.blendlayer[0].blend_area.y = 0;
	stBlendConf.blendlayer[0].blend_area.w = 320;
	stBlendConf.blendlayer[0].blend_area.h = 240;
	V2DLOGD("V2D_BeginJob\n");
	ret = V2D_BeginJob(&hHandle);
	if (ret) {
		V2DLOGD("V2D_BeginJob err\n");
		return ret;
	}
	
	ret = V2D_AddBlendTask(hHandle, &stBackGround, &stBackGroundRect, NULL, NULL, NULL, NULL, &stDst, 
								&stDstRect, &stBlendConf, enForeRotate, enBackRotate, enForeCSCMode, enBackCSCMode, NULL, dither);
	if (ret) {
		V2DLOGD("V2D_AddBlendTask err\n");
		return ret;
	}
	
	ret = V2D_EndJob(hHandle);
	if (ret) {
		V2DLOGD("V2D_EndJob err\n");
		return ret;
	}
	tmp = malloc(out.size);
	if (!tmp) {
		V2DLOGD("malloc fail\n");
	}
	if (stDst.fbc_enable) {
		readFile(pFbcCase0Header, tmp);
		readFile(pFbcCase0Body, tmp+4800);
	} else {
		readFile(pRawData, tmp);
	}
	real = (unsigned int *)tmp;
	expect = (unsigned int *)pDst;
	for (i=0; i<out.size/4; i++) {
		if (*(expect+i) != *(real+i)) {
			V2DLOGD("i:%d,exp:0x%08x,real:0x%08x\n",i,*(expect+i),*(real+i));
			ret = 1;
			break;
		}
	}
	munmap(pLayer0, mapsize0);
	munmap(pDst,    mapsize2);
	destroyAllocator();
	V2DLOGD("v2d simple %s\n", ret ? "v2d simple test case failed!":"v2d simple test case successful!");
	return ret;
}
//fill test
int v2d_fill_test(void)
{
	int ret = 0;
	V2D_HANDLE hHandle;
	V2D_SURFACE_S stDst;
	V2D_AREA_S stDstRect;
	V2D_FILLCOLOR_S stFillColor;

	struct v2d_alloc_dma_buf out;
	void *pDst;
	unsigned int mapsize;
	bool cpu_access_need = true;
	void *tmp = NULL;
	int i;
	unsigned int *real, *expect;

	V2DLOGD("v2d fill test start\n");
	out.size   = 320*240*4;
	mapsize   = ALIGN_UP(out.size, PAGESIZE);
	createAllocator();
	out.fd = DmabufHeapAllocSystem(bufferAllocator, cpu_access_need, mapsize, 0, 0);
	pDst = mmap(NULL, mapsize, PROT_READ | PROT_WRITE, MAP_SHARED, out.fd, 0);
	if (pDst == MAP_FAILED) {
		V2DLOGD(" v2d mmap dst failed\n");
	}
	//set background grey
	memset(pDst, 0x80,  mapsize);
	//config FillColor
	stFillColor.format = V2D_COLOR_FORMAT_RGBA8888;
	stFillColor.colorvalue  = 0x00ffcc66;
	//config output
	memset(&stDst, 0, sizeof(V2D_SURFACE_S));
	stDst.fbc_enable = 0;
	stDst.fd         = out.fd;
	stDst.offset     = 0x00;
	stDst.w          = 320;
	stDst.h          = 240;
	stDst.stride     = 320*4;
	stDst.format     = V2D_COLOR_FORMAT_RGBA8888;
	//output crop x,y must align 16
	stDstRect.x      = 32;
	stDstRect.y      = 32;
	stDstRect.w      = 210;
	stDstRect.h      = 180;

	ret = V2D_BeginJob(&hHandle);
	if (ret) {
		V2DLOGD("V2D_BeginJob err\n");
		return ret;
	}
	ret = V2D_AddFillTask(hHandle, &stDst, &stDstRect,  &stFillColor);
	if (ret) {
		V2DLOGD("V2D_AddBlendTask err\n");
		return ret;
	}

	ret = V2D_EndJob(hHandle);
	if (ret) {
		V2DLOGD("V2D_EndJob err\n");
		return ret;
	}
	if (ret) {
		goto fini;
	}
	//compare
	// tmp = malloc(out.size);
	// if (!tmp) {
	// 	V2DLOGD("malloc fail\n");
	// }
	// readFile("fill_rgba8888.dump", tmp);
	// expect = (unsigned int *)tmp;
	// real   = (unsigned int *)pDst;
	// for (i=0; i<out.size/4; i++) {
	// 	if (*(expect+i) != *(real+i)) {
	// 		V2DLOGD("i:%d,exp:0x%08x,real:0x%08x\n",i,*(expect+i),*(real+i));
	// 		ret = 1;
	// 		break;
	// 	}
	// }
fini:
	munmap(pDst, mapsize);
	destroyAllocator();
	V2DLOGD("v2d fill test %s\n", ret ? "v2d fill test case failed!":"v2d fill test case successful!");
	return ret;
}
//blit test
int v2d_blit_test(void)
{
	int ret = 0;
	V2D_HANDLE hHandle;
	V2D_SURFACE_S stSrc, stDst;
	V2D_AREA_S stSrcRect, stDstRect;
	struct v2d_alloc_dma_buf in, out;
	unsigned int mapsize0, mapsize1;
	void *pSrc, *pDst;
	bool cpu_access_need = true;
	void *tmp = NULL;
	int i;
	unsigned int *real, *expect;

	V2DLOGD("v2d blit test start\n");
	in.size  = 320*240*4;
	out.size = 320*240*4;
	mapsize0 = ALIGN_UP(out.size, PAGESIZE);
	mapsize1 = mapsize0;
	createAllocator();
	in.fd  = DmabufHeapAllocSystem(bufferAllocator, cpu_access_need, mapsize0, 0, 0);
	out.fd = DmabufHeapAllocSystem(bufferAllocator, cpu_access_need, mapsize1, 0, 0);
	pSrc = mmap(NULL, mapsize0, PROT_READ | PROT_WRITE, MAP_SHARED, in.fd, 0);
	pDst = mmap(NULL, mapsize1, PROT_READ | PROT_WRITE, MAP_SHARED, out.fd, 0);
	//read input
	// ret = readFile("adv_320_240_rgba8888.raw", pSrc);
	//set background grey
	memset(pDst, 0x80,  mapsize1);
	//config layer0
	memset(&stDst, 0, sizeof(V2D_SURFACE_S));
	stSrc.fbc_enable = 0;
	stSrc.fd         = in.fd;
	stSrc.offset     = 0x00;
	stSrc.w          = 320;
	stSrc.h          = 240;
	stSrc.stride     = 320*4;
	stSrc.format     = V2D_COLOR_FORMAT_RGBA8888;
	stSrcRect.x      = 53;
	stSrcRect.y      = 37;
	stSrcRect.w      = 200;
	stSrcRect.h      = 180;
	//config output
	memset(&stDst, 0, sizeof(V2D_SURFACE_S));
	stDst.fbc_enable = 0;
	stDst.fd         = out.fd;
	stDst.offset     = 0x00;
	stDst.w          = 320;
	stDst.h          = 240;
	stDst.stride     = 320*4;
	stDst.format     = V2D_COLOR_FORMAT_RGBA8888;
	//output crop x,y must align 16
	stDstRect.x      = 64;
	stDstRect.y      = 32;
	stDstRect.w      = 200;
	stDstRect.h      = 180;

	ret = V2D_BeginJob(&hHandle);
	if (ret) {
		V2DLOGD("V2D_BeginJob err\n");
		return ret;
	}
	ret = V2D_AddBitblitTask(hHandle, &stDst, &stDstRect, &stSrc, &stSrcRect, V2D_CSC_MODE_BUTT);
	if (ret) {
		V2DLOGD("V2D_AddBlendTask err\n");
		return ret;
	}

	ret = V2D_EndJob(hHandle);
	if (ret) {
		V2DLOGD("V2D_EndJob err\n");
		return ret;
	}
	if (ret) {
		goto fini;
	}
	//compare
	// tmp = malloc(out.size);
	// if (!tmp) {
	// 	V2DLOGD("malloc fail\n");
	// }
	// readFile("blit_rgba8888.dump", tmp);
	// expect = (unsigned int *)tmp;
	// real   = (unsigned int *)pDst;
	// for (i=0; i<out.size/4; i++) {
	// 	if (*(expect+i) != *(real+i)) {
	// 		V2DLOGD("i:%d,exp:0x%08x,real:0x%08x\n",i,*(expect+i),*(real+i));
	// 		ret = 1;
	// 		break;
	// 	}
	// }
fini:
	munmap(pSrc, mapsize0);
	munmap(pDst, mapsize1);
	destroyAllocator();
	V2DLOGD("v2d blit test %s\n", ret ? "v2d blit test case failed!":"v2d blit test case successful!");
	return ret;
}
int main(int argc, char** argv)
{
	int ret = 0;

	if (argc < 2) {
		printf("spacemit v2d test cases:\n");
		printf("--rate 204M          default rate 204M \n");
		printf("--rate 307M          default rate 307M \n");
		printf("--rate 491M          default rate 491M \n");
		printf("--blend              blend test case \n");
		printf("--fill               fill test case \n");
		printf("--blit               blit test cass \n");
		return -1;
	}

	if ((argc == 3) && (strcmp(argv[1], "--rate")  == 0) && (strcmp(argv[2], "204M")  == 0)) {
		write_sysfile(sysfile, "204800000");
		ret = v2d_SimpleCase();
	} else if ((argc == 3) && (strcmp(argv[1], "--rate")  == 0) && (strcmp(argv[2], "307M")  == 0)) {
		write_sysfile(sysfile, "307200000");
		ret = v2d_SimpleCase();
	} else if ((argc == 3) && (strcmp(argv[1], "--rate")  == 0) && (strcmp(argv[2], "491M")  == 0)) {
		write_sysfile(sysfile, "491520000");
		ret = v2d_SimpleCase();
	}  else if (strcmp(argv[1], "--blend") == 0) {
		ret = v2d_adv();
	} else if (strcmp(argv[1], "--fill") == 0) {
		ret = ret = v2d_fill_test();
	} else if (strcmp(argv[1], "--blit") == 0) {
		ret = v2d_blit_test();
	} else if (strcmp(argv[1], "--help") == 0) {
		printf("spacemit v2d test cases:\n");
		printf("--rate 204M          default rate 204M \n");
		printf("--rate 307M          default rate 307M \n");
		printf("--rate 491M          default rate 491M \n");
		printf("--blend              blend test case \n");
		printf("--fill               fill test case \n");
		printf("--blit               blit test cass \n");
	} else {
		printf("spacemit v2d test case, intput error!\n");
	}

	return ret;
}
