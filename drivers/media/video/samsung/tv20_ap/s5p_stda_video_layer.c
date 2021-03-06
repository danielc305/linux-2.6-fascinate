/* linux/drivers/media/video/samsung/tv20/s5p_stda_video_layer.c
 *
 * Video Layer ftn. file for Samsung TVOut driver
 *
 * Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/delay.h>
#include <linux/ioctl.h>
#include <linux/clk.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include "s5p_tv.h"

#ifdef COFIG_TVOUT_DBG
#define S5P_VLAYER_DEBUG  1
#endif

//#define S5P_VLAYER_DEBUG 1

#ifdef S5P_VLAYER_DEBUG
#define VLAYERPRINTK(fmt, args...) \
	printk("\t[VLAYER] %s: " fmt, __FUNCTION__ , ## args)
#else
#define VLAYERPRINTK(fmt, args...)
#endif


static bool _s5p_vlayer_wait_previous_update(void)
{
	__s5p_vp_get_update_status();
	return false;
}


static void _s5p_vlayer_calc_inner_values(void)
{
	
	s5p_tv_status *st = &s5ptv_status;
	s5p_vl_param *video = &(s5ptv_status.vl_basic_param);
	bool pro = (st->tvout_param.out_mode > 
			TVOUT_OUTPUT_COMPONENT_YPBPR_INERLACED) ? true : false;
	u32 t_y_addr = video->top_y_address;
	u32 t_c_addr = video->top_c_address;
	u32 img_w = video->img_width;
	u32 s_ox = video->src_offset_x;
	u32 s_oy = video->src_offset_y;
	u32 d_ox = video->dest_offset_x;
	u32 d_oy = video->dest_offset_y;
	u32 s_w = video->src_width;
	u32 s_h = video->src_height;
	u32 d_w = video->dest_width;
	u32 d_h = video->dest_height;

	st->vl_top_y_address = t_y_addr;
	st->vl_top_c_address = t_c_addr;

	if (st->src_color == VPROC_SRC_COLOR_NV12IW) {
		st->vl_bottom_y_address = t_y_addr + img_w;
		st->vl_bottom_c_address = t_c_addr + img_w;
	}

	if(st->src_color == VPROC_SRC_COLOR_NV12)


	st->vl_src_offset_x = s_ox;
	st->vl_src_offset_y = s_oy;
	st->vl_src_width = s_w;
	st->vl_src_height = s_h;
	st->vl_dest_offset_x = d_ox;
	st->vl_dest_offset_y = d_oy;
	st->vl_dest_width = d_w;
	st->vl_dest_height = d_h;

	switch (st->src_color) {

	case VPROC_SRC_COLOR_NV12:

	case VPROC_SRC_COLOR_TILE_NV12:

		if (pro) {
		
		} else { // eScanMode == INTERLACE
			st->vl_src_height = s_h / 2;
			st->vl_src_offset_y =s_oy / 2;

			st->vl_dest_height = d_h / 2;
			st->vl_dest_offset_y = d_oy / 2;
		}

		break;

	case VPROC_SRC_COLOR_NV12IW:

	case VPROC_SRC_COLOR_TILE_NV12IW: // Interlace weave

		if (pro) {
			st->vl_src_height = s_h / 2;
			st->vl_src_offset_y = s_oy / 2;
		} else { // eScanMode == INTERLACE
			/*
			* Top field. In fact, in this mode this value  
			* is don't care because FILED_ID_AUTO_TOGGLLING 
			* bit will be enabled
			*/
			st->vl_src_height = s_h / 2;
			st->vl_src_offset_y = s_oy / 2;

			st->vl_dest_height = d_h / 2;
			st->vl_dest_offset_y = d_oy / 2;
		}

		break;

	default:
		VLAYERPRINTK("(ERR) : invalid src_color(%d)\n\r", 
			st->src_color);
		break;
	}
}

bool _s5p_vlayer_start(void)
{
	int i;

	s5p_tv_vp_err verr;
	s5p_tv_vmx_err merr;
	s5p_video_img_address temp_addr;
	s5p_img_size img_size;

	s5p_vl_param param = s5ptv_status.vl_basic_param;

	u8 contrast = s5ptv_status.vl_contrast;

	u32 ty_addr = s5ptv_status.vl_top_y_address;
	u32 tc_addr = s5ptv_status.vl_top_c_address;
	u32 by_addr = s5ptv_status.vl_bottom_y_address;
	u32 bc_addr = s5ptv_status.vl_bottom_c_address;
	u32 endian = param.src_img_endian;
//	u32 i_w  = param.img_width;   //mkh
//	u32 i_h  = param.img_height;  //mkh
	u32 s_ox = s5ptv_status.vl_src_offset_x;
	u32 s_xf = s5ptv_status.vl_src_x_fact_step;
	u32 s_oy = s5ptv_status.vl_src_offset_y;
	u32 s_w  = s5ptv_status.vl_src_width;
	u32 s_h  = s5ptv_status.vl_src_height;
	u32 d_ox = s5ptv_status.vl_dest_offset_x;
	u32 d_oy = s5ptv_status.vl_dest_offset_y;
	u32 d_w  = s5ptv_status.vl_dest_width;
	u32 d_h  = s5ptv_status.vl_dest_height;
	u32 noise = s5ptv_status.vl_sharpness.th_noise;
	u32 saturation = s5ptv_status.vl_saturation;
	u32 alpha = param.alpha;
	u32 priority = param.priority;
	u32 br_offset= s5ptv_status.vl_bright_offset;
	
	bool ipc = s5ptv_status.vl2d_ipc;
	bool l_skip = s5ptv_status.vl_op_mode.line_skip;
	bool bypass = s5ptv_status.vl_bypass_post_process;
	bool po_def = s5ptv_status.vl_poly_filter_default;
	bool bright = s5ptv_status.us_vl_brightness;
	bool w_blend= param.win_blending;
	bool csc_en = s5ptv_status.vl_csc_control.csc_en;
	bool s_off_en = s5ptv_status.vl_csc_control.sub_y_offset_en;
	bool csc_coef_def = s5ptv_status.vl_csc_coef_default;
	
	s5p_vp_field f_id = s5ptv_status.field_id;
	s5p_vp_mem_mode m_mode = s5ptv_status.vl_op_mode.mem_mode;
	s5p_vp_chroma_expansion cro_ex = s5ptv_status.vl_op_mode.chroma_exp;
	s5p_vp_filed_id_toggle f_id_tog = s5ptv_status.vl_op_mode.toggle_id;
	s5p_vp_pxl_rate p_rate = s5ptv_status.vl_rate;
	s5p_vp_sharpness_control sharp = s5ptv_status.vl_sharpness.sharpness;
	s5p_vp_csc_type	csc_type = s5ptv_status.vl_csc_type;
	
	__s5p_tv_clk_set_vp_clk_onoff(true);
	__s5p_vp_sw_reset();
	__s5p_vp_init_field_id(f_id);
	//__s5p_vp_init_op_mode(l_skip,/*m_mode*/0, cro_ex, f_id_tog);//mkh	
	__s5p_vp_init_op_mode(l_skip,m_mode, cro_ex, f_id_tog);//mkh	
	__s5p_vp_init_pixel_rate_control(p_rate);

	temp_addr.y_address = param.top_y_address;
	temp_addr.c_address = param.top_c_address;
	img_size.img_width = param.src_width;
	img_size.img_height = param.src_height;
	_s5p_vlayer_set_top_address((unsigned long)&temp_addr);
	_s5p_vlayer_set_img_size((unsigned long)&img_size);
	_s5p_vlayer_set_src_size((unsigned long)&img_size);

u32 i_w  = img_size.img_width;   //mkh
u32 i_h  = img_size.img_height;  //mkh

	if (po_def)
		verr = __s5p_vp_init_layer_def_poly_filter_coef(ty_addr,
				tc_addr,by_addr,bc_addr,endian,i_w,i_h,s_ox,
				s_xf,s_oy,s_w,s_h,d_ox,d_oy,d_w,d_h,ipc);
	else
		verr = __s5p_vp_init_layer(ty_addr,tc_addr,by_addr,bc_addr,
				endian,i_w,i_h,s_ox,s_xf,s_oy,s_w,s_h,d_ox,
				d_oy,d_w,d_h,ipc);

	if (verr != VPROC_NO_ERROR)
		return false;

	__s5p_vp_init_bypass_post_process(bypass);
	__s5p_vp_init_sharpness(noise,sharp);
	__s5p_vp_init_saturation(saturation);
	__s5p_vp_init_brightness(bright);
	__s5p_vp_init_contrast(contrast);

	for (i = VProc_LINE_EQ_0;i <= VProc_LINE_EQ_7;i++) {
		if (s5ptv_status.vl_bc_control[i].eq_num == i)
			verr = __s5p_vp_init_brightness_contrast_control(
					s5ptv_status.vl_bc_control[i].eq_num,
					s5ptv_status.vl_bc_control[i].intc,
					s5ptv_status.vl_bc_control[i].slope);

		if (verr != VPROC_NO_ERROR)
			return false;
	}

	__s5p_vp_init_brightness_offset(br_offset);

	__s5p_vp_init_csc_control(s_off_en,csc_en);

	if (csc_en && csc_coef_def) {
		verr = __s5p_vp_init_csc_coef_default(csc_type);

		if (verr != VPROC_NO_ERROR)
			return false;
	}

	verr = __s5p_vp_start();

	if (verr != VPROC_NO_ERROR)
		return false;

	merr = __s5p_vm_init_layer(VM_VIDEO_LAYER,true,w_blend,alpha,priority,
					VM_DIRECT_RGB565,false,false,false,
					0,0,0,0,0,0,0,0,0);

	if (merr != VPROC_NO_ERROR)
		return false;

	__s5p_vm_start();

	s5ptv_status.vp_layer_enable = true;

	return true;
}

bool _s5p_vlayer_stop(void)
{
	s5p_tv_vp_err verr;
	s5p_tv_vmx_err merr;

	merr = __s5p_vm_set_layer_show(VM_VIDEO_LAYER, false);

	if (merr != VMIXER_NO_ERROR)
		return false;

	__s5p_vm_start();

	if (_s5p_vlayer_wait_previous_update())
		return false;

	verr = __s5p_vp_stop();

	if (verr != VPROC_NO_ERROR)
		return false;

	__s5p_tv_clk_set_vp_clk_onoff(false);

	s5ptv_status.vp_layer_enable = false;

	return true;
}


bool _s5p_vlayer_set_priority(unsigned long buf_in)
{
	s5p_tv_vmx_err merr;
	u32 pri;

	s5ptv_status.vl_basic_param.priority = (unsigned int)(buf_in);

	pri = s5ptv_status.vl_basic_param.priority;

	merr = __s5p_vm_set_layer_priority(VM_VIDEO_LAYER,pri);

	if (merr != VMIXER_NO_ERROR)
		return false;

	return true;
}

bool _s5p_vlayer_set_blending(unsigned long buf_in)
{
	s5p_tv_vmx_err merr;
	bool blend; 

	s5ptv_status.vl_basic_param.win_blending = (bool)(buf_in);
	blend = s5ptv_status.vl_basic_param.win_blending;

	merr = __s5p_vm_set_win_blend(VM_VIDEO_LAYER,blend);

	if (merr != VMIXER_NO_ERROR)
		return false;

	return true;
}

bool _s5p_vlayer_set_alpha(unsigned long buf_in)
{
	s5p_tv_vmx_err merr;
	u32 alpha;

	s5ptv_status.vl_basic_param.alpha = (unsigned int)(buf_in);
	alpha = s5ptv_status.vl_basic_param.alpha;

	merr = __s5p_vm_set_layer_alpha(VM_VIDEO_LAYER, alpha);

	if (merr != VMIXER_NO_ERROR) {
		return false;
	}

	return true;
}

bool _s5p_vlayer_set_field_id(unsigned long buf_in)
{
	s5p_tv_vp_err verr;

	s5ptv_status.field_id = (s5p_vp_field)(buf_in);

	if (_s5p_vlayer_wait_previous_update()) {
		return false;
	}

	__s5p_vp_set_field_id(s5ptv_status.field_id);

	verr = __s5p_vp_update();

	if (verr != VPROC_NO_ERROR)
		return false;

	return true;
}

bool _s5p_vlayer_set_top_address(unsigned long buf_in)
{
	u32 t_y_addr = 0;
	u32 t_c_addr = 0;
	u32 b_y_addr = 0;
	u32 b_c_addr = 0;
	
	s5p_video_img_address *addr = (s5p_video_img_address*)buf_in;
	s5p_tv_vp_err verr;

	s5ptv_status.vl_basic_param.top_y_address = addr->y_address;
	s5ptv_status.vl_basic_param.top_c_address = addr->c_address;

	_s5p_vlayer_calc_inner_values();

	t_y_addr = s5ptv_status.vl_top_y_address;
	t_c_addr = s5ptv_status.vl_top_c_address;
	b_y_addr = s5ptv_status.vl_bottom_y_address;
	b_c_addr = s5ptv_status.vl_bottom_c_address;

	if (_s5p_vlayer_wait_previous_update()) {
		return false;
	}

	verr = __s5p_vp_set_top_field_address(t_y_addr,t_c_addr);

	if (verr != VPROC_NO_ERROR) {
		return false;
	}

	if (s5ptv_status.src_color == VPROC_SRC_COLOR_NV12IW) {
		verr = __s5p_vp_set_bottom_field_address(b_y_addr, b_c_addr);

		if (verr != VPROC_NO_ERROR) 
			return false;
	}

	verr = __s5p_vp_update();

	if (verr != VPROC_NO_ERROR)
		return false;

	return true;
}

bool _s5p_vlayer_set_bottom_address(unsigned long buf_in)
{
	u32 t_y_addr = 0;
	u32 t_c_addr = 0;
	u32 b_y_addr = 0;
	u32 b_c_addr = 0;

	u32 img_width = s5ptv_status.vl_basic_param.img_width;
	
	s5p_video_img_address *addr = (s5p_video_img_address*)buf_in;
	s5p_tv_vp_err verr;
	s5p_vp_src_color s_color = s5ptv_status.src_color;

	if (s_color == VPROC_SRC_COLOR_NV12IW) {
		s5ptv_status.vl_basic_param.top_y_address =
			addr->y_address - img_width;
		s5ptv_status.vl_basic_param.top_c_address =
			addr->c_address - img_width;
	}

	_s5p_vlayer_calc_inner_values();

	t_y_addr = s5ptv_status.vl_top_y_address;
	t_c_addr = s5ptv_status.vl_top_c_address;
	b_y_addr = s5ptv_status.vl_bottom_y_address;
	b_c_addr = s5ptv_status.vl_bottom_c_address;	

	if (_s5p_vlayer_wait_previous_update())
		return false;

	verr = __s5p_vp_set_bottom_field_address(b_y_addr,b_c_addr);

	if (verr != VPROC_NO_ERROR) {
		return false;
	}

	if (s5ptv_status.src_color == VPROC_SRC_COLOR_NV12IW) {
		verr = __s5p_vp_set_top_field_address(t_y_addr,t_c_addr);

		if (verr != VPROC_NO_ERROR) {
			return false;
		}
	}

	verr = __s5p_vp_update();

	if (verr != VPROC_NO_ERROR) {
		return false;
	}

	return true;
}

bool _s5p_vlayer_set_img_size(unsigned long buf_in)
{
	s5p_img_size *size = (s5p_img_size*)buf_in;
	s5p_tv_vp_err verr;

	s5ptv_status.vl_basic_param.img_width = size->img_width;
	s5ptv_status.vl_basic_param.img_height = size->img_height;

	if (_s5p_vlayer_wait_previous_update()) {
		return false;
	}

	verr = __s5p_vp_set_img_size(size->img_width, size->img_height);

	if (verr != VPROC_NO_ERROR) {
		return false;
	}

	verr = __s5p_vp_update();

	if (verr != VPROC_NO_ERROR) {
		return false;
	}

	VLAYERPRINTK("()\n\r");

	return true;
}

bool _s5p_vlayer_set_src_position(unsigned long buf_in)
{
	s5p_img_offset *offset = (s5p_img_offset*)buf_in;
	s5p_tv_vp_err verr;

	s5ptv_status.vl_basic_param.src_offset_x = offset->offset_x;
	s5ptv_status.vl_basic_param.src_offset_y = offset->offset_y;
	_s5p_vlayer_calc_inner_values();

	if (_s5p_vlayer_wait_previous_update()) {
		return false;
	}

	__s5p_vp_set_src_position(s5ptv_status.vl_src_offset_x,
				  s5ptv_status.vl_src_x_fact_step,
				  s5ptv_status.vl_src_offset_y);

	verr = __s5p_vp_update();

	if (verr != VPROC_NO_ERROR) {
		return false;
	}

	return true;
}

bool _s5p_vlayer_set_dest_position(unsigned long buf_in)
{
	u32 d_ox = 0;
	u32 d_oy = 0;
	s5p_img_offset *offset = (s5p_img_offset*)buf_in;
	s5p_tv_vp_err verr;

	s5ptv_status.vl_basic_param.dest_offset_x = offset->offset_x;
	s5ptv_status.vl_basic_param.dest_offset_y = offset->offset_y;
	_s5p_vlayer_calc_inner_values();

	d_ox = s5ptv_status.vl_dest_offset_x;
	d_oy = s5ptv_status.vl_dest_offset_y;

	if (_s5p_vlayer_wait_previous_update())
		return false;

	__s5p_vp_set_dest_position(d_ox, d_oy);

	verr = __s5p_vp_update();

	if (verr != VPROC_NO_ERROR)
		return false;

	return true;
}

bool _s5p_vlayer_set_src_size(unsigned long buf_in)
{
	u32 s_w = 0;
	u32 s_h = 0;
	u32 d_w = 0;
	u32 d_h = 0;
	bool ipc = false;
	
	s5p_img_size *size = (s5p_img_size*)buf_in;
	s5p_tv_vp_err verr;

	s5ptv_status.vl_basic_param.src_width = size->img_width;
	s5ptv_status.vl_basic_param.src_height = size->img_height;
	_s5p_vlayer_calc_inner_values();

	s_w = s5ptv_status.vl_src_width; 
	s_h = s5ptv_status.vl_src_height;
	d_w = s5ptv_status.vl_dest_width;
	d_h = s5ptv_status.vl_dest_height;
	ipc = s5ptv_status.vl2d_ipc;

	if (_s5p_vlayer_wait_previous_update())
		return false;

	__s5p_vp_set_src_dest_size(s_w, s_h, d_w, d_h, ipc);

	verr = __s5p_vp_update();

	if (verr != VPROC_NO_ERROR)
		return false;

	return true;
}

bool _s5p_vlayer_set_dest_size(unsigned long buf_in)
{
	u32 s_w = 0;
	u32 s_h = 0;
	u32 d_w = 0;
	u32 d_h = 0;
	bool ipc = false;
	
	s5p_img_size *size = (s5p_img_size*)buf_in;
	s5p_tv_vp_err verr;

	s5ptv_status.vl_basic_param.dest_width = size->img_width;
	s5ptv_status.vl_basic_param.dest_height = size->img_height;
	_s5p_vlayer_calc_inner_values();

	s_w = s5ptv_status.vl_src_width; 
	s_h = s5ptv_status.vl_src_height;
	d_w = s5ptv_status.vl_dest_width;
	d_h = s5ptv_status.vl_dest_height;
	ipc = s5ptv_status.vl2d_ipc;

	if (_s5p_vlayer_wait_previous_update())
		return false;

	__s5p_vp_set_src_dest_size(s_w, s_h, d_w, d_h, ipc);

	verr = __s5p_vp_update();

	if (verr != VPROC_NO_ERROR)
		return false;

	return true;
}

bool _s5p_vlayer_set_brightness(unsigned long buf_in)
{
	s5p_tv_vp_err verr;

	s5ptv_status.us_vl_brightness = (unsigned short)buf_in;

	if (_s5p_vlayer_wait_previous_update()) {
		return false;
	}

	__s5p_vp_set_brightness(s5ptv_status.us_vl_brightness);


	verr = __s5p_vp_update();

	if (verr != VPROC_NO_ERROR) {
		return false;
	}

	return true;
}

bool _s5p_vlayer_set_contrast(unsigned long buf_in)
{
	s5p_tv_vp_err verr;

	s5ptv_status.vl_contrast = (unsigned char)buf_in;

	if (_s5p_vlayer_wait_previous_update()) {
		return false;
	}

	__s5p_vp_set_contrast(s5ptv_status.vl_contrast);

	verr = __s5p_vp_update();

	if (verr != VPROC_NO_ERROR) {
		return false;
	}

	return true;
}

void _s5p_vlayer_get_priority(unsigned long buf_out)
{
	unsigned int *id = (unsigned int*)buf_out;

	*id = s5ptv_status.vl_basic_param.priority;
}

bool _s5p_vlayer_set_brightness_contrast_control(unsigned long buf_in)
{
	u32 intc;
	u32 slope;

	s5p_vp_line_eq eq_num;	
	s5p_tv_vp_err verr;
	s5p_vl_bright_contrast_ctrl *ctrl=(s5p_vl_bright_contrast_ctrl*)buf_in;

	if (ctrl->eq_num > VProc_LINE_EQ_7 || 
		ctrl->eq_num < VProc_LINE_EQ_0) {
		VLAYERPRINTK("(ERR) : invalid eq_num(%d)\n\r", ctrl->eq_num);
		return false;
	}

	memcpy((void *)&(s5ptv_status.vl_bc_control[ctrl->eq_num]), 
		(const void *)ctrl,sizeof(s5p_vl_csc_ctrl));

	eq_num 	= s5ptv_status.vl_bc_control[ctrl->eq_num].eq_num;
	intc 	= s5ptv_status.vl_bc_control[ctrl->eq_num].intc;
	slope 	= s5ptv_status.vl_bc_control[ctrl->eq_num].slope;
	
	if (_s5p_vlayer_wait_previous_update()) {
		return false;
	}

	verr = __s5p_vp_set_brightness_contrast_control(eq_num, intc, slope);

	if (verr != VPROC_NO_ERROR) {
		return false;
	}

	verr = __s5p_vp_update();

	if (verr != VPROC_NO_ERROR) {
		return false;
	}

	return true;
}

bool _s5p_vlayer_set_poly_filter_coef(unsigned long buf_in)
{
	s5p_video_poly_filter_coef *coef = (s5p_video_poly_filter_coef*)buf_in;
	s5p_tv_vp_err verr;

	if (coef->poly_coeff < VPROC_POLY8_Y0_LL ||
		(coef->poly_coeff > VPROC_POLY8_Y3_HH && 
		coef->poly_coeff < VPROC_POLY4_Y0_LL) ||
		coef->poly_coeff > VPROC_POLY4_C1_HH) {
		VLAYERPRINTK("(ERR) : invalid poly_coeff(%d)\n\r", 
			coef->poly_coeff);
		return false;
	}

	if (_s5p_vlayer_wait_previous_update()) {
		return false;
	}

	verr = __s5p_vp_init_poly_filter_coef(coef->poly_coeff,
					coef->ch0, coef->ch1,
					coef->ch2, coef->ch3);

	if (verr != VPROC_NO_ERROR)
		return false;

	verr = __s5p_vp_update();

	if (verr != VPROC_NO_ERROR)
		return false;

	s5ptv_status.vl_poly_filter_default = false;

	return true;
}

bool _s5p_vlayer_set_csc_coef(unsigned long buf_in)
{
	s5p_video_csc_coef *coef = (s5p_video_csc_coef*)buf_in;
	s5p_tv_vp_err verr;

	if (coef->csc_coeff < VPROC_CSC_Y2Y_COEF || 
		coef->csc_coeff > VPROC_CSC_CR2CR_COEF) {
		VLAYERPRINTK("(ERR) : invalid csc_coeff(%d)\n\r", 
			coef->csc_coeff);
		return false;
	}

	if (_s5p_vlayer_wait_previous_update())
		return false;

	verr = __s5p_vp_init_csc_coef(coef->csc_coeff, coef->coeff);

	if (verr != VPROC_NO_ERROR) {
		return false;
	}

	verr = __s5p_vp_update();

	if (verr != VPROC_NO_ERROR) {
		return false;
	}

	s5ptv_status.vl_csc_coef_default = false;

	return true;
}

bool _s5p_vlayer_init_param(unsigned long buf_in)
{
	s5p_tv_status *st = &s5ptv_status;
	bool pro = ((st->tvout_param.out_mode > 
			TVOUT_OUTPUT_COMPONENT_YPBPR_INERLACED)) ? true:false;

	st->src_color = (pro) ? VPROC_SRC_COLOR_NV12:VPROC_SRC_COLOR_NV12IW;

	if (st->tvout_param.disp_mode > TVOUT_576P_50_4_3)
		st->vl_csc_type = VPROC_CSC_SD_HD;
	else if (st->tvout_param.disp_mode < TVOUT_720P_60)
		st->vl_csc_type = VPROC_CSC_HD_SD;

	st->vl_csc_control.csc_en = false;
	st->vl2d_ipc = ((st->src_color == VPROC_SRC_COLOR_NV12IW) &&
			(st->tvout_param.out_mode) > 
			TVOUT_OUTPUT_COMPONENT_YPBPR_INERLACED) 
			? true : false;
	st->vl_op_mode.line_skip = (pro && 
			((st->src_color == VPROC_SRC_COLOR_NV12)||
			(st->src_color == VPROC_SRC_COLOR_TILE_NV12))) 
			? false : true;


#ifdef CONFIG_CPU_S5PC100
	st->vl_op_mode.mem_mode = ((st->src_color==VPROC_SRC_COLOR_NV12)||
				st->src_color==VPROC_SRC_COLOR_NV12IW) ?	
				VPROC_LINEAR_MODE : VPROC_2D_TILE_MODE;
#endif

#ifdef CONFIG_CPU_S5PC110
	st->vl_op_mode.mem_mode = ((st->src_color==VPROC_SRC_COLOR_NV12)||
				st->src_color==VPROC_SRC_COLOR_NV12IW) ?	
				VPROC_2D_TILE_MODE:VPROC_LINEAR_MODE;
#endif
#if 1	
	st->vl_op_mode.chroma_exp = /* (pro) ?*/ VPROC_USING_C_TOP 
					/*: VPROC_USING_C_TOP_BOTTOM*/;
	st->vl_op_mode.toggle_id = /*(pro) ?*/ S5P_TV_VP_FILED_ID_TOGGLE_USER /*: 
					S5P_TV_VP_FILED_ID_TOGGLE_VSYNC*/;
#endif
	
	_s5p_vlayer_calc_inner_values();

	if (st->vl_mode) {
		VLAYERPRINTK("(ERR) : Default values are already updated\n\r");
		return true;
	}

	// Initialize Video Layer Parameters to Default Values
	st->vl_src_x_fact_step = 0;
	st->field_id = VPROC_TOP_FIELD;
	st->vl_rate = VPROC_PIXEL_PER_RATE_1_1;
	st->vl_poly_filter_default = true;
	st->vl_bypass_post_process = false;
	st->vl_saturation = 0x80;
	st->vl_sharpness.th_noise = 0;
	st->vl_sharpness.sharpness = VPROC_SHARPNESS_NO;
	st->us_vl_brightness = 0x00;
	st->vl_contrast = 0x80;
	st->vl_bright_offset = 0x00;
	st->vl_csc_control.sub_y_offset_en = false;
	st->vl_csc_coef_default = true;
	st->vl_bc_control[0].eq_num = VProc_LINE_EQ_7 + 1;
	st->vl_bc_control[1].eq_num = VProc_LINE_EQ_7 + 1;
	st->vl_bc_control[2].eq_num = VProc_LINE_EQ_7 + 1;
	st->vl_bc_control[3].eq_num = VProc_LINE_EQ_7 + 1;
	st->vl_bc_control[4].eq_num = VProc_LINE_EQ_7 + 1;
	st->vl_bc_control[5].eq_num = VProc_LINE_EQ_7 + 1;
	st->vl_bc_control[6].eq_num = VProc_LINE_EQ_7 + 1;
	st->vl_bc_control[7].eq_num = VProc_LINE_EQ_7 + 1;
	st->vl_mode = true;

	//mkh : somebody has started playing video
	st->videoplaying = true;


	return true;
}

