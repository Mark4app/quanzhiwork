
#include  "csi_dev_i.h"

#if  1
//#define __here__            eLIBs_printf("@L%d(%s)\n", __LINE__, __FILE__);
#define __inf(...)    		(eLIBs_printf("MSG:L%d(%s):", __LINE__, __FILE__),                 \
						     eLIBs_printf(__VA_ARGS__)									        )
#else
#define __inf(...)    	
#endif


#if(CSI_DEV_TYPE == CSI_DEV_TYPE_TVP5150)

#define MIRROR 0//0 = normal, 1 = mirror
#define FLIP   0//0 = normal, 1 = flip

//每个device私有特性的配置都在此文件完成
//包括reset pin，power down pin的操作
//mclk的配置
//各个模式下的i2c配置以及对应的csi配置
//思路如下：
//通常sensor的设置有
//1.初始化
//2.从任何模式到模式A
//3.从任何模式到模式B
//...
//因此定义几个参数表：reg_init， reg_A，reg_B，...
//在csi_dev_init函数中一次性配置reg_init
//在需要的时候可以多次配置reg_A或reg_B...

//==================全局变量dev_csi.c要调用========================
//每个模组必须要有这些参数
__bool              bProgressiveSrc = 0;   // Indicating the source is progressive or not
__bool              bTopFieldFirst = 1;    // VPO should check this flag when bProgressiveSrc is FALSE
__u16               eAspectRatio = 1000;     //the source picture aspect ratio

__u32 csi_mclk = 24; //MHz

__csi_mode_t csi_mode;//当前使用模式
__csi_mode_all_t csi_mode_all;//支持的模式集
__csi_conf_t conf;//csi配置参数

__csi_color_effect_t csi_color_effect = CSI_COLOR_EFFECT_NORMAL;   	//当前的csi_color_effect
__csi_awb_t          csi_awb 					= CSI_AWB_AUTO;   						//当前的csi_awb
__csi_ae_t					 csi_ae 					= CSI_AE_0;   								//当前的csi_ae
__csi_bright_t			 csi_bright 			= CSI_BRIGHT_0;   						//当前的csi_bright
__csi_contrast_t		 csi_contrast 		= CSI_CONTRAST_0;   					//当前的csi_contrast
__csi_band_t				 csi_band 				= CSI_BAND_50HZ;   						//当前的csi_band
//==================================================================================

extern ES_FILE   *fiic;//i2c句柄


//==================非全局变量仅在此文件中用到========================
__u32  last_mode_num = 0xff;
__buf_scrab_mode_t  last_buf_mode = CSI_SC_NA;//区分单双csi工作状态，last_buf_mode为双，切换不reset power，单->单不reset，单->双reset一次

//====================================================================
static __u8  da = 0xb8 >> 1; //设备地址, DATA_bit7有关，上电时候电平：0/1 -> 0xB8>>1 / 0xBA>>1
static __u32 reg_step = 2; //一次写操作所包含的寄存器地址+寄存器值的byte数
//====================================================================


//=======================================================================================
static __u8 reg_init[] = //寄存器表，格式为：[寄存器地址], [寄存器值], (地址或值如为16-bit仍然按byte为单位，高位在前，低位在后，例如对0xABCD写入0x1234: 0xab, 0xcd, 0x12, 0x34,)
{
    //selmux
    0x05, 0x01,
    0x05, 0x00,
    //0x02,0x30,//operation mode control
    //0x00,0x00,//video in source select    0x00:AIP1A 0x02:AIP1B

    //selmux
    0x02, 0x00, //0x30,//operation mode control
    0x00, 0x00, //video in source select    0x00:AIP1A 0x02:AIP1B
    //ntsc
    0x0f, 0x0a, //Activate YCrCb output 0x9 or 0xd ?
    0x03, 0x69, //auto switch
    0x28, 0x00, //0x02,//mask ntsc443 mode
    0x04, 0x3f, //0xc0,//luma peak disable
    //0x02,0x02,//pedestral enable
    0x33, 0x40,
    0x34, 0x11,


    //0x07,0x20,//
    0x09, 0x8b, //brightness
    0x0a, 0x88, //saturation
    0x0b, 0x05, //hue
    0x0c, 0x90, //contrast
    0x0d, 0x07, // Default format: 0x47. For ITU-R BT.656: 0x40
    0x1b, 0x37, //pal-17 ntsc-37
    0x11, 0x00, //
};
static __u8 reg_720_576_25000[] =
{
    //selmux
    0x02, 0x30, //operation mode control
    0x00, 0x00, //video in source select    0x00:AIP1A 0x02:AIP1B
    //pal
    0x0f, 0x0a, ///Activate YCrCb output 0x9 or 0xd ?
    0x03, 0x69, //auto switch
    0x28, 0x04, //pal mode
    0x04, 0xc0, //luma peak disable
    0x02, 0x02, //pedestral enable
    0x07, 0x20, //saturation
    0x0a, 0x90, //contrast
    0x0c, 0x90, //Default format: 0x47. For ITU-R BT.656: 0x40
    0x0d, 0x07, //
    0x1b, 0x17, //
    0x11, 0x00, //
};
static __u8 reg_720_480_30000[] =
{
    //selmux
    0x02, 0x30, //operation mode control
    0x00, 0x00, //video in source select    0x00:AIP1A 0x02:AIP1B
    //ntsc
    0x0f, 0x0a, //Activate YCrCb output 0x9 or 0xd ?
    0x03, 0x69, //auto switch
    0x28, 0x00, //0x02,//mask ntsc443 mode
    0x04, 0xc0, //luma peak disable
    0x02, 0x02, //pedestral enable
    0x07, 0x20, //saturation
    0x0a, 0x90, //contrast
    0x0c, 0x90, // Default format: 0x47. For ITU-R BT.656: 0x40
    0x0d, 0x07, //
    0x1b, 0x17, //
    0x11, 0x00, //
};

//=======================================================================================



__s32  csi_power_on(void)
{
    csi_pwdn(1);//pwdn=1
    csi_rst(1);//rst=1

    csi_pwren(0);//pwr=0
    esKRNL_TimeDly(1);//shut sensor power first in case system already open power before pull pwdn and reset
    csi_pwren(1);//pwr=1
    esKRNL_TimeDly(1);
    csi_pwdn(0);//pwdn=0
    esKRNL_TimeDly(1);
    csi_rst(0);//rst=0
    esKRNL_TimeDly(1);
    csi_rst(1);//rst=1

    return EPDK_OK;
}

__s32  csi_power_off(void)
{
    csi_rst(0);//rst=0
    csi_pwren(0);//pwr=0
    return EPDK_OK;
}


__s32  csi_clk_on(void)
{
    csi_mclk_on_off(1);
    return EPDK_OK;
}

__s32  csi_clk_off(void)
{
    csi_mclk_on_off(0);
    return EPDK_OK;
}


__s32 block_write(ES_FILE *hdl_iic, __u8 slv_addr, __u8 *reg, __u32 length)
{
    __u32 i = 0;
    __s32 ret = EPDK_FAIL;

    for (i = 0; i < length; i += reg_step)
    {
        //__u8 iic_val=0;
        //__u8 tmp=0;
        //__inf("8_8\n");
        if(iic_write_8_8(hdl_iic, slv_addr, reg[i], reg[i + 1]) != EPDK_OK)
        {
            __wrn("csi[%d] iic write fail %x = %x\n", reg[i], reg[i + 1], CSI_PORT);
        }
        if (i == 0)esKRNL_TimeDly(1); //must wait
        //__inf("csi0 iic write %d, %x = %x\n", ret, reg[i],reg[i+1]);
    }

    return ret;
}


//设备模式设置函数，配置设备，同时根据设备配置相应的csi设置
__s32 csi_dev_set_mode(__u32 mode)
{
    //__u32 i = 0;
    __u8 *reg_table;//根据sensor的iic宽度而定__u8还是__u16或其他
    __u32 reg_table_len;

    if (last_mode_num == mode)
    {
        __inf("no need to change csi mode!\n");
    }

    else
    {
        csi_mode = csi_mode_all.csi_mode[mode];

        switch (mode)
        {
        case 0:
        {
            reg_table = reg_720_576_25000;
            reg_table_len = sizeof(reg_720_576_25000);
            conf.input_fmt  = CSI_CCIR656;
            conf.output_fmt = CSI_FRAME_UV_CB_YUV422;
            conf.field_sel  = CSI_ODD;
            conf.seq        = CSI_YUYV;//根据实际修改
            conf.vref       = CSI_LOW;//根据实际修改
            conf.href       = CSI_LOW;//根据实际修改
            conf.clock      = CSI_RISING;//根据实际修改

            CSI_set_base((CSI_PORT == 0x00) ? 0x00 : 0x01);
            CSI_configure(&conf);
            CSI_set_size(0, csi_mode.size.width * 2, 0, csi_mode.size.height / 2, csi_mode.size.width); //注意第三个参数，如果sourc为interlaced，最终拼成frame，那么此处的height应该为field的高度，即frame的高度的一半，跟progressive为frame的高度不一样，切记！
            break;
        }
        case 1:
        {
            reg_table = reg_720_576_25000;
            reg_table_len = sizeof(reg_720_576_25000);
            conf.input_fmt  = CSI_CCIR656;
            conf.output_fmt = CSI_FRAME_UV_CB_YUV420;
            conf.field_sel  = CSI_ODD;
            conf.seq        = CSI_YUYV;//根据实际修改
            conf.vref       = CSI_LOW;//根据实际修改
            conf.href       = CSI_LOW;//根据实际修改
            conf.clock      = CSI_RISING;//根据实际修改

            CSI_set_base((CSI_PORT == 0x00) ? 0x00 : 0x01);
            CSI_configure(&conf);
            CSI_set_size(0, csi_mode.size.width * 2, 0, csi_mode.size.height / 2, csi_mode.size.width); //注意第三个参数，如果sourc为interlaced，最终拼成frame，那么此处的height应该为field的高度，即frame的高度的一半，跟progressive为frame的高度不一样，切记！
            break;
        }
        case 2:
        {
            reg_table = reg_720_480_30000;
            reg_table_len = sizeof(reg_720_480_30000);
            conf.input_fmt  = CSI_CCIR656;
            conf.output_fmt = CSI_FRAME_UV_CB_YUV422;
            conf.field_sel  = CSI_ODD;
            conf.seq        = CSI_YUYV;//根据实际修改
            conf.vref       = CSI_LOW;//根据实际修改
            conf.href       = CSI_LOW;//根据实际修改
            conf.clock      = CSI_RISING;//根据实际修改

            CSI_set_base((CSI_PORT == 0x00) ? 0x00 : 0x01);
            CSI_configure(&conf);
            CSI_set_size(0, csi_mode.size.width * 2, 0, csi_mode.size.height / 2, csi_mode.size.width); //注意第三个参数，如果sourc为interlaced，最终拼成frame，那么此处的height应该为field的高度，即frame的高度的一半，跟progressive为frame的高度不一样，切记！
            break;
        }
        case 3:
        {
            reg_table = reg_720_480_30000;
            reg_table_len = sizeof(reg_720_480_30000);
            conf.input_fmt  = CSI_CCIR656;
            conf.output_fmt = CSI_FRAME_UV_CB_YUV420;
            conf.field_sel  = CSI_ODD;
            conf.seq        = CSI_YUYV;//根据实际修改
            conf.vref       = CSI_LOW;//根据实际修改
            conf.href       = CSI_LOW;//根据实际修改
            conf.clock      = CSI_RISING;//根据实际修改

            CSI_set_base((CSI_PORT == 0x00) ? 0x00 : 0x01);
            CSI_configure(&conf);
            CSI_set_size(0, csi_mode.size.width * 2, 0, csi_mode.size.height / 2, csi_mode.size.width); //注意第三个参数，如果sourc为interlaced，最终拼成frame，那么此处的height应该为field的高度，即frame的高度的一半，跟progressive为frame的高度不一样，切记！
            break;
        }

        }


        //block_write(fiic,da,reg_table,reg_table_len);

        last_buf_mode = csi_mode.csi_buf_scrab_mode;
        last_mode_num = mode;
        __inf("set mode %d finished! buf_scrab_mode=%d (0=CSI0, 2=01LR, 4=01UD, 5/7=TDM_2/4CH)\n", mode, last_buf_mode);
    }
    return EPDK_OK;
}



__s32  csi_dev_init(void)
{
    //	__u32 i,clock = 0;
    __u8 *reg_table;
    __u32 reg_table_len;

    iic_set_clock(fiic, 200 * 1000); //200*1000=200kHz

    __inf("csi_dev_init......\n");

    //填写本设备支持的模式
    csi_mode_all.number = 4;//所支持模式数量
    //这些设置都是指输出的数据参数，为了传递给显示或者编码时的帧参数
    //---------------------------------------------------------------------------------------------------------------
    //PAL
    csi_mode_all.csi_mode[0].color_format = PIXEL_YUV422;//格式
    csi_mode_all.csi_mode[0].component_seq = YUV_SEQ_UVUV;//排列顺序
    csi_mode_all.csi_mode[0].store_mode = YUV_MOD_UV_NON_MB_COMBINED;//存储格式
    csi_mode_all.csi_mode[0].size.width = 720;//宽
    csi_mode_all.csi_mode[0].size.height = 480;//高
    csi_mode_all.csi_mode[0].frame_rate = 25000;//帧率，单位Hz
    csi_mode_all.csi_mode[0].frame_period = 40000;//帧长（=1/帧率，单位us）
    csi_mode_all.csi_mode[0].color_space = BT601;//色彩空间
    csi_mode_all.csi_mode[0].csi_buf_scrab_mode = CSI0_FULL;
    //PAL
    csi_mode_all.csi_mode[1].color_format = PIXEL_YUV420;//格式
    csi_mode_all.csi_mode[1].component_seq = YUV_SEQ_UVUV;//排列顺序
    csi_mode_all.csi_mode[1].store_mode = YUV_MOD_UV_NON_MB_COMBINED;//存储格式
    csi_mode_all.csi_mode[1].size.width = 720;//宽
    csi_mode_all.csi_mode[1].size.height = 480;//高
    csi_mode_all.csi_mode[1].frame_rate = 25000;//帧率，单位Hz
    csi_mode_all.csi_mode[1].frame_period = 40000;//帧长（=1/帧率，单位us）
    csi_mode_all.csi_mode[1].color_space = BT601;//色彩空间
    csi_mode_all.csi_mode[1].csi_buf_scrab_mode = CSI0_FULL;
    //NTSC
    csi_mode_all.csi_mode[2].color_format = PIXEL_YUV422;//格式
    csi_mode_all.csi_mode[2].component_seq = YUV_SEQ_UVUV;//排列顺序
    csi_mode_all.csi_mode[2].store_mode = YUV_MOD_UV_NON_MB_COMBINED;//存储格式
    csi_mode_all.csi_mode[2].size.width = 720;//宽
    csi_mode_all.csi_mode[2].size.height = 480;//高
    csi_mode_all.csi_mode[2].frame_rate = 30000;//帧率，单位Hz
    csi_mode_all.csi_mode[2].frame_period = 33333;//帧长（=1/帧率，单位us）
    csi_mode_all.csi_mode[2].color_space = BT601;//色彩空间
    csi_mode_all.csi_mode[2].csi_buf_scrab_mode = CSI0_FULL;
    //NTSC
    csi_mode_all.csi_mode[3].color_format = PIXEL_YUV420;//格式
    csi_mode_all.csi_mode[3].component_seq = YUV_SEQ_UVUV;//排列顺序
    csi_mode_all.csi_mode[3].store_mode = YUV_MOD_UV_NON_MB_COMBINED;//存储格式
    csi_mode_all.csi_mode[3].size.width = 720;//宽
    csi_mode_all.csi_mode[3].size.height = 480;//高
    csi_mode_all.csi_mode[3].frame_rate = 30000;//帧率，单位Hz
    csi_mode_all.csi_mode[3].frame_period = 33333;//帧长（=1/帧率，单位us）
    csi_mode_all.csi_mode[3].color_space = BT601;//色彩空间
    csi_mode_all.csi_mode[3].csi_buf_scrab_mode = CSI0_FULL;
    //-----------------------------------------------------------------------------------------------------------
    //配置设备的初始化设置
    block_write(fiic, da, reg_init, sizeof(reg_init)); //视sensor的配置看是否需要，且注意size是按__u8还是__u16来计算

    csi_dev_set_mode(1);//不要忘记设置一个默认模式

    return EPDK_OK;
}

__s32  csi_dev_exit(void)
{
    csi_clk_off();
    csi_power_off();

    return EPDK_OK;
}


//设备color effect设置函数
__s32 csi_dev_set_color_effect(__csi_color_effect_t color_effect)
{

    return EPDK_OK;
}
//设备awb设置函数
__s32 csi_dev_set_awb(__csi_awb_t awb)
{

    return EPDK_OK;
}
//设备ae设置函数
__s32 csi_dev_set_ae(__csi_ae_t ae)
{

    return EPDK_OK;
}
//设备bright设置函数
__s32 csi_dev_set_bright(__csi_bright_t bright)
{

    return EPDK_OK;
}

//设备contrast设置函数
__s32 csi_dev_set_contrast(__csi_contrast_t contrast)
{

    return EPDK_OK;
}

//设备band设置函数
__s32 csi_dev_set_band(__csi_band_t band)
{

    return EPDK_OK;
}


#endif

