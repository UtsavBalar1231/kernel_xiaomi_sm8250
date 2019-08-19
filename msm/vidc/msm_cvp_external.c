// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#include <linux/ion_kernel.h>
#include "msm_cvp_external.h"
#include "msm_vidc_common.h"

#define LOWER32(a) ((u32)((u64)a))
#define UPPER32(a) ((u32)((u64)a >> 32))

static void print_cvp_buffer(u32 tag, const char *str,
		struct msm_vidc_inst *inst, struct msm_cvp_buf *cbuf)
{
	struct msm_cvp_external *cvp;

	if (!(tag & msm_vidc_debug) || !inst || !inst->cvp || !cbuf)
		return;

	cvp = inst->cvp;
	dprintk(tag,
		"%s: %x : idx %d fd %d size %d offset %d dbuf %pK kvaddr %pK\n",
		str, cvp->session_id, cbuf->index, cbuf->fd, cbuf->size,
		cbuf->offset, cbuf->dbuf, cbuf->kvaddr);
}

static int fill_cvp_buffer(struct msm_cvp_buffer_type *dst,
		struct msm_cvp_buf *src)
{
	if (!dst || !src) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	dst->buffer_addr = -1;
	dst->reserved1 = LOWER32(src->dbuf);
	dst->reserved2 = UPPER32(src->dbuf);
	dst->size = src->size;

	return 0;
}

static int msm_cvp_get_version_info(struct msm_vidc_inst *inst)
{
	int rc;
	struct msm_cvp_external *cvp;
	struct cvp_kmd_arg *arg;
	struct cvp_kmd_sys_properties *sys_prop;
	struct cvp_kmd_sys_property *prop_data;
	u32 version;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;
	arg = cvp->arg;

	memset(arg, 0, sizeof(struct cvp_kmd_arg));
	arg->type = CVP_KMD_GET_SYS_PROPERTY;
	sys_prop = (struct cvp_kmd_sys_properties *)&arg->data.sys_properties;
	sys_prop->prop_num = CVP_KMD_HFI_VERSION_PROP_NUMBER;
	prop_data = (struct  cvp_kmd_sys_property *)
					&arg->data.sys_properties.prop_data;
	prop_data->prop_type = CVP_KMD_HFI_VERSION_PROP_TYPE;
	rc = msm_cvp_private(cvp->priv, CVP_KMD_GET_SYS_PROPERTY, arg);
	if (rc) {
		dprintk(VIDC_ERR, "%s: failed, rc %d\n", __func__, rc);
		return rc;
	}
	version = prop_data->data;
	dprintk(VIDC_HIGH, "%s: version %#x\n", __func__, version);

	return 0;
}

static int msm_cvp_set_priority(struct msm_vidc_inst *inst)
{
	int rc;
	struct msm_cvp_external *cvp;
	struct cvp_kmd_arg *arg;
	struct cvp_kmd_sys_properties *props;
	struct cvp_kmd_sys_property *prop_array;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;
	arg = cvp->arg;
	props = (struct cvp_kmd_sys_properties *)&arg->data.sys_properties;
	prop_array = (struct cvp_kmd_sys_property *)
			&arg->data.sys_properties.prop_data;

	memset(arg, 0, sizeof(struct cvp_kmd_arg));
	arg->type = CVP_KMD_SET_SYS_PROPERTY;
	props->prop_num = 1;
	prop_array[0].prop_type = CVP_KMD_PROP_SESSION_PRIORITY;
	if (is_realtime_session(inst))
		prop_array[0].data = VIDEO_REALTIME;
	else
		prop_array[0].data = VIDEO_NONREALTIME;
	dprintk(VIDC_HIGH, "%s: %d\n", __func__, prop_array[0].data);
	rc = msm_cvp_private(cvp->priv, CVP_KMD_SET_SYS_PROPERTY, arg);
	if (rc) {
		dprintk(VIDC_ERR, "%s: failed, rc %d\n", __func__, rc);
		return rc;
	}

	return 0;
}

static int msm_cvp_fill_planeinfo(struct msm_cvp_color_plane_info *plane_info,
		u32 color_fmt, u32 width, u32 height)
{
	int rc = 0;
	u32 y_stride, y_sclines, uv_stride, uv_sclines;
	u32 y_meta_stride, y_meta_scalines;
	u32 uv_meta_stride, uv_meta_sclines;

	switch (color_fmt) {
	case COLOR_FMT_NV12:
	case COLOR_FMT_P010:
	case COLOR_FMT_NV12_512:
	{
		y_stride = VENUS_Y_STRIDE(color_fmt, width);
		y_sclines = VENUS_Y_SCANLINES(color_fmt, height);
		uv_stride = VENUS_UV_STRIDE(color_fmt, width);
		uv_sclines = VENUS_UV_SCANLINES(color_fmt, height);

		plane_info->stride[HFI_COLOR_PLANE_METADATA] = 0;
		plane_info->stride[HFI_COLOR_PLANE_PICDATA] = y_stride;
		plane_info->stride[HFI_COLOR_PLANE_UV_META] = 0;
		plane_info->stride[HFI_COLOR_PLANE_UV] = uv_stride;
		plane_info->buf_size[HFI_COLOR_PLANE_METADATA] = 0;
		plane_info->buf_size[HFI_COLOR_PLANE_PICDATA] =
			y_stride * y_sclines;
		plane_info->buf_size[HFI_COLOR_PLANE_UV_META] = 0;
		plane_info->buf_size[HFI_COLOR_PLANE_UV] =
			uv_stride * uv_sclines;
		break;
	}
	case COLOR_FMT_NV12_UBWC:
	case COLOR_FMT_NV12_BPP10_UBWC:
	{
		y_meta_stride = VENUS_Y_META_STRIDE(color_fmt, width);
		y_meta_scalines = VENUS_Y_META_SCANLINES(color_fmt, height);
		uv_meta_stride = VENUS_UV_META_STRIDE(color_fmt, width);
		uv_meta_sclines = VENUS_UV_META_SCANLINES(color_fmt, height);

		y_stride = VENUS_Y_STRIDE(color_fmt, width);
		y_sclines = VENUS_Y_SCANLINES(color_fmt, height);
		uv_stride = VENUS_UV_STRIDE(color_fmt, width);
		uv_sclines = VENUS_UV_SCANLINES(color_fmt, height);

		plane_info->stride[HFI_COLOR_PLANE_METADATA] = y_meta_stride;
		plane_info->stride[HFI_COLOR_PLANE_PICDATA] = y_stride;
		plane_info->stride[HFI_COLOR_PLANE_UV_META] = uv_meta_stride;
		plane_info->stride[HFI_COLOR_PLANE_UV] = uv_stride;
		plane_info->buf_size[HFI_COLOR_PLANE_METADATA] =
			MSM_MEDIA_ALIGN(y_meta_stride * y_meta_scalines, 4096);
		plane_info->buf_size[HFI_COLOR_PLANE_PICDATA] =
			MSM_MEDIA_ALIGN(y_stride * y_sclines, 4096);
		plane_info->buf_size[HFI_COLOR_PLANE_UV_META] =
			MSM_MEDIA_ALIGN(uv_meta_stride * uv_meta_sclines, 4096);
		plane_info->buf_size[HFI_COLOR_PLANE_UV] =
			MSM_MEDIA_ALIGN(uv_stride * uv_sclines, 4096);
		break;
	}
	default:
		dprintk(VIDC_ERR, "%s: invalid color_fmt %#x\n",
			__func__, color_fmt);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int msm_cvp_free_buffer(struct msm_vidc_inst *inst,
		struct msm_cvp_buf *buffer)
{
	struct msm_cvp_external *cvp;

	if (!inst || !inst->cvp || !buffer) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;

	if (buffer->kvaddr) {
		dma_buf_vunmap(buffer->dbuf, buffer->kvaddr);
		buffer->kvaddr = NULL;
	}
	if (buffer->dbuf) {
		dma_buf_put(buffer->dbuf);
		buffer->dbuf = NULL;
	}
	return 0;
}

static int msm_cvp_allocate_buffer(struct msm_vidc_inst *inst,
		struct msm_cvp_buf *buffer, bool kernel_map)
{
	int rc = 0;
	struct msm_cvp_external *cvp;
	int ion_flags = 0;
	unsigned long heap_mask = 0;
	struct dma_buf *dbuf;

	if (!inst || !inst->cvp || !buffer) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;

	heap_mask = ION_HEAP(ION_SYSTEM_HEAP_ID);
	if (inst->flags & VIDC_SECURE) {
		ion_flags = ION_FLAG_SECURE | ION_FLAG_CP_NON_PIXEL;
		heap_mask = ION_HEAP(ION_SECURE_HEAP_ID);
	}

	dbuf = ion_alloc(buffer->size, heap_mask, ion_flags);
	if (IS_ERR_OR_NULL(dbuf)) {
		dprintk(VIDC_ERR,
			"%s: failed to allocate, size %d heap_mask %#lx flags %d\n",
			__func__, buffer->size, heap_mask, ion_flags);
		rc = -ENOMEM;
		goto error;
	}
	buffer->dbuf = dbuf;
	buffer->fd = -1;

	if (kernel_map) {
		buffer->kvaddr = dma_buf_vmap(dbuf);
		if (!buffer->kvaddr) {
			dprintk(VIDC_ERR,
				"%s: dma_buf_vmap failed\n", __func__);
			rc = -EINVAL;
			goto error;
		}
	} else {
		buffer->kvaddr = NULL;
	}
	buffer->index = cvp->buffer_idx++;
	buffer->offset = 0;

	return 0;
error:
	msm_cvp_free_buffer(inst, buffer);
	return rc;
}

static int msm_cvp_set_clocks_and_bus(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp;
	struct cvp_kmd_arg *arg;
	struct v4l2_format *fmt;
	struct cvp_kmd_usecase_desc desc;
	struct cvp_kmd_request_power power;
	const u32 fps_max = CVP_FRAME_RATE_MAX;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;
	arg = cvp->arg;
	memset(&desc, 0, sizeof(struct cvp_kmd_usecase_desc));
	memset(&power, 0, sizeof(struct cvp_kmd_request_power));

	fmt = &inst->fmts[INPUT_PORT].v4l2_fmt;
	desc.fullres_width = cvp->width;
	desc.fullres_height = cvp->height;
	desc.downscale_width = cvp->ds_width;
	desc.downscale_height = cvp->ds_height;
	desc.is_downscale = cvp->downscale;
	desc.fps = min(cvp->frame_rate >> 16, fps_max);
	desc.op_rate = cvp->operating_rate >> 16;
	desc.colorfmt = msm_comm_convert_color_fmt(fmt->fmt.pix_mp.pixelformat);
	rc = msm_cvp_est_cycles(&desc, &power);
	if (rc) {
		dprintk(VIDC_ERR, "%s: estimate failed\n", __func__);
		return rc;
	}
	dprintk(VIDC_HIGH,
		"%s: core %d controller %d ddr bw %d\n",
		__func__, power.clock_cycles_a, power.clock_cycles_b,
		power.ddr_bw);

	memset(arg, 0, sizeof(struct cvp_kmd_arg));
	arg->type = CVP_KMD_REQUEST_POWER;
	memcpy(&arg->data.req_power, &power,
		sizeof(struct cvp_kmd_request_power));
	rc = msm_cvp_private(cvp->priv, CVP_KMD_REQUEST_POWER, arg);
	if (rc) {
		dprintk(VIDC_ERR,
			"%s: request_power failed with %d\n", __func__, rc);
		return rc;
	}

	return rc;
}

static int msm_cvp_init_downscale_resolution(struct msm_vidc_inst *inst)
{
	struct msm_cvp_external *cvp;
	const u32 width_max = 1920;
	u32 width, height, ds_width, ds_height, temp;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;

	ds_width = cvp->width;
	ds_height = cvp->height;

	if (!cvp->downscale) {
		dprintk(VIDC_HIGH, "%s: downscaling not enabled\n", __func__);
		goto exit;
	}

	/* Step 1) make width always the larger number */
	if (cvp->height > cvp->width) {
		width = cvp->height;
		height = cvp->width;
	} else {
		width = cvp->width;
		height = cvp->height;
	}
	/*
	 * Step 2) Downscale width by 4 and round
	 * make sure width stays between 480 and 1920
	 */
	ds_width = (width + 2) >> 2;
	if (ds_width < 480)
		ds_width = 480;
	if (ds_width > width_max)
		ds_width = width_max;
	ds_height = (height * ds_width) / width;
	if (ds_height < 128)
		ds_height = 128;

	/* Step 3) do not downscale if width is less than 480 */
	if (width <= 480)
		ds_width = width;
	if (ds_width == width)
		ds_height = height;

	/* Step 4) switch width and height if already switched */
	if (cvp->height > cvp->width) {
		temp = ds_height;
		ds_height = ds_width;
		ds_width = temp;
	}

exit:
	cvp->ds_width = ds_width;
	cvp->ds_height = ds_height;
	return 0;
}

static void msm_cvp_deinit_downscale_buffers(struct msm_vidc_inst *inst)
{
	struct msm_cvp_external *cvp;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return;
	}
	cvp = inst->cvp;
	dprintk(VIDC_HIGH, "%s:\n", __func__);

	if (cvp->src_buffer.dbuf) {
		print_cvp_buffer(VIDC_HIGH, "free: src_buffer",
				inst, &cvp->src_buffer);
		if (msm_cvp_free_buffer(inst, &cvp->src_buffer))
			print_cvp_buffer(VIDC_ERR,
				"free failed: src_buffer",
				inst, &cvp->src_buffer);
	}
	if (cvp->ref_buffer.dbuf) {
		print_cvp_buffer(VIDC_HIGH, "free: ref_buffer",
				inst, &cvp->ref_buffer);
		if (msm_cvp_free_buffer(inst, &cvp->ref_buffer))
			print_cvp_buffer(VIDC_ERR,
				"free failed: ref_buffer",
				inst, &cvp->ref_buffer);
	}
}

static int msm_cvp_init_downscale_buffers(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;

	if (!cvp->downscale) {
		dprintk(VIDC_HIGH, "%s: downscaling not enabled\n", __func__);
		return 0;
	}
	dprintk(VIDC_HIGH, "%s:\n", __func__);

	cvp->src_buffer.size = VENUS_BUFFER_SIZE(COLOR_FMT_NV12_UBWC,
			cvp->ds_width, cvp->ds_height);
	rc = msm_cvp_allocate_buffer(inst, &cvp->src_buffer, false);
	if (rc) {
		print_cvp_buffer(VIDC_ERR,
			"allocate failed: src_buffer",
			inst, &cvp->src_buffer);
		goto error;
	}
	print_cvp_buffer(VIDC_HIGH, "alloc: src_buffer",
			inst, &cvp->src_buffer);

	cvp->ref_buffer.size = cvp->src_buffer.size;
	rc = msm_cvp_allocate_buffer(inst, &cvp->ref_buffer, false);
	if (rc) {
		print_cvp_buffer(VIDC_ERR,
			"allocate failed: ref_buffer",
			inst, &cvp->ref_buffer);
		goto error;
	}
	print_cvp_buffer(VIDC_HIGH, "alloc: ref_buffer",
			inst, &cvp->ref_buffer);

	return rc;

error:
	msm_cvp_deinit_downscale_buffers(inst);
	return rc;
}

static void msm_cvp_deinit_context_buffers(struct msm_vidc_inst *inst)
{
	struct msm_cvp_external *cvp;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return;
	}
	cvp = inst->cvp;
	dprintk(VIDC_HIGH, "%s:\n", __func__);

	if (cvp->context_buffer.dbuf) {
		print_cvp_buffer(VIDC_HIGH, "free: context_buffer",
				inst, &cvp->context_buffer);
		if (msm_cvp_free_buffer(inst, &cvp->context_buffer))
			print_cvp_buffer(VIDC_ERR,
				"free failed: context_buffer",
				inst, &cvp->context_buffer);
	}
	if (cvp->refcontext_buffer.dbuf) {
		print_cvp_buffer(VIDC_HIGH, "free: refcontext_buffer",
				inst, &cvp->refcontext_buffer);
		if (msm_cvp_free_buffer(inst, &cvp->refcontext_buffer))
			print_cvp_buffer(VIDC_ERR,
				"free failed: refcontext_buffer",
				inst, &cvp->refcontext_buffer);
	}
}

static int msm_cvp_init_context_buffers(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;
	dprintk(VIDC_HIGH, "%s:\n", __func__);

	cvp->context_buffer.size = HFI_DME_FRAME_CONTEXT_BUFFER_SIZE;
	rc = msm_cvp_allocate_buffer(inst, &cvp->context_buffer, false);
	if (rc) {
		print_cvp_buffer(VIDC_ERR,
			"allocate failed: context_buffer",
			inst, &cvp->context_buffer);
		goto error;
	}
	print_cvp_buffer(VIDC_HIGH, "alloc: context_buffer",
			inst, &cvp->context_buffer);

	cvp->refcontext_buffer.size = cvp->context_buffer.size;
	rc = msm_cvp_allocate_buffer(inst, &cvp->refcontext_buffer, false);
	if (rc) {
		print_cvp_buffer(VIDC_ERR,
			"allocate failed: refcontext_buffer",
			inst, &cvp->refcontext_buffer);
		goto error;
	}
	print_cvp_buffer(VIDC_HIGH, "alloc: refcontext_buffer",
			inst, &cvp->refcontext_buffer);

	return rc;

error:
	msm_cvp_deinit_context_buffers(inst);
	return rc;
}

static void msm_cvp_deinit_internal_buffers(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return;
	}

	cvp = inst->cvp;
	dprintk(VIDC_HIGH, "%s:\n", __func__);

	if (cvp->output_buffer.dbuf) {
		print_cvp_buffer(VIDC_HIGH, "free: output_buffer",
				inst, &cvp->output_buffer);
		rc = msm_cvp_free_buffer(inst, &cvp->output_buffer);
		if (rc)
			print_cvp_buffer(VIDC_ERR,
				"unregister failed: output_buffer",
				inst, &cvp->output_buffer);
	}

	if (cvp->persist2_buffer.dbuf) {
		print_cvp_buffer(VIDC_HIGH, "free: persist2_buffer",
			inst, &cvp->persist2_buffer);
		rc = msm_cvp_free_buffer(inst, &cvp->persist2_buffer);
		if (rc)
			print_cvp_buffer(VIDC_ERR,
				"free failed: persist2_buffer",
				inst, &cvp->persist2_buffer);
	}
}

static int msm_cvp_set_persist_buffer(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp;
	struct cvp_kmd_arg *arg;
	struct msm_cvp_session_set_persist_buffers_packet persist2_packet = {0};

	if (!inst || !inst->cvp || !inst->cvp->arg) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;
	arg = cvp->arg;

	persist2_packet.size =
		sizeof(struct msm_cvp_session_set_persist_buffers_packet);
	persist2_packet.packet_type = HFI_CMD_SESSION_CVP_SET_PERSIST_BUFFERS;
	persist2_packet.session_id = cvp->session_id;
	persist2_packet.cvp_op = CVP_DME;
	fill_cvp_buffer(&persist2_packet.persist2_buffer,
			&cvp->persist2_buffer);

	memset(arg, 0, sizeof(struct cvp_kmd_arg));
	arg->type = CVP_KMD_HFI_PERSIST_CMD;
	arg->buf_offset = offsetof(
		struct msm_cvp_session_set_persist_buffers_packet,
		persist1_buffer) / sizeof(u32);
	arg->buf_num = (sizeof(
		struct msm_cvp_session_set_persist_buffers_packet) -
		(arg->buf_offset * sizeof(u32))) /
		sizeof(struct msm_cvp_buffer_type);
	memcpy(&(arg->data.pbuf_cmd), &persist2_packet,
		sizeof(struct msm_cvp_session_set_persist_buffers_packet));
	rc = msm_cvp_private(cvp->priv, CVP_KMD_HFI_PERSIST_CMD, arg);
	if (rc) {
		print_cvp_buffer(VIDC_ERR,
			"set failed: persist2_buffer",
			inst, &cvp->persist2_buffer);
	}

	return rc;
}

static int msm_cvp_init_internal_buffers(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;

	cvp->persist2_buffer.size = HFI_DME_INTERNAL_PERSIST_2_BUFFER_SIZE;
	rc = msm_cvp_allocate_buffer(inst, &cvp->persist2_buffer, false);
	if (rc) {
		print_cvp_buffer(VIDC_ERR,
			"allocate failed: persist2_buffer",
			inst, &cvp->persist2_buffer);
		goto error;
	}
	print_cvp_buffer(VIDC_HIGH, "alloc: persist2_buffer",
			inst, &cvp->persist2_buffer);

	/* allocate one output buffer for internal use */
	cvp->output_buffer.size = HFI_DME_OUTPUT_BUFFER_SIZE;
	rc = msm_cvp_allocate_buffer(inst, &cvp->output_buffer, true);
	if (rc) {
		print_cvp_buffer(VIDC_ERR,
			"allocate failed: output_buffer",
			inst, &cvp->output_buffer);
		goto error;
	}
	print_cvp_buffer(VIDC_HIGH, "alloc: output_buffer",
			inst, &cvp->output_buffer);

	return rc;

error:
	msm_cvp_deinit_internal_buffers(inst);
	return rc;
}

static int msm_cvp_prepare_extradata(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf)
{
	int rc = 0;
	struct msm_cvp_external *cvp;
	struct vb2_buffer *vb;
	struct dma_buf *dbuf;
	char *kvaddr = NULL;
	struct msm_vidc_extradata_header *e_hdr;
	bool input_extradata, found_end;

	if (!inst || !inst->cvp || !mbuf) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;

	vb = &mbuf->vvb.vb2_buf;
	if (vb->num_planes <= 1) {
		dprintk(VIDC_ERR, "%s: extradata plane not enabled\n",
			__func__);
		return -EINVAL;
	}

	dbuf = dma_buf_get(vb->planes[1].m.fd);
	if (!dbuf) {
		dprintk(VIDC_ERR, "%s: dma_buf_get(%d) failed\n",
			__func__, vb->planes[1].m.fd);
		return -EINVAL;
	}
	if (dbuf->size < vb->planes[1].length) {
		dprintk(VIDC_ERR, "%s: invalid size %d vs %d\n", __func__,
			dbuf->size, vb->planes[1].length);
		rc = -EINVAL;
		goto error;
	}
	rc = dma_buf_begin_cpu_access(dbuf, DMA_BIDIRECTIONAL);
	if (rc) {
		dprintk(VIDC_ERR, "%s: begin_cpu_access failed\n", __func__);
		goto error;
	}
	kvaddr = dma_buf_vmap(dbuf);
	if (!kvaddr) {
		dprintk(VIDC_ERR, "%s: dma_buf_vmap(%d) failed\n",
			__func__, vb->planes[1].m.fd);
		rc = -EINVAL;
		goto error;
	}
	e_hdr = (struct msm_vidc_extradata_header *)((char *)kvaddr +
			vb->planes[1].data_offset);

	input_extradata =
		!!((inst->prop.extradata_ctrls & EXTRADATA_ENC_INPUT_ROI) ||
		(inst->prop.extradata_ctrls & EXTRADATA_ENC_INPUT_HDR10PLUS));
	found_end = false;
	while ((char *)e_hdr < (char *)(kvaddr + dbuf->size)) {
		if (!input_extradata) {
			found_end = true;
			break;
		}
		if (e_hdr->type == MSM_VIDC_EXTRADATA_NONE) {
			found_end = true;
			break;
		}
		e_hdr += e_hdr->size;
	}
	if (!found_end) {
		dprintk(VIDC_ERR, "%s: extradata_none not found\n", __func__);
		e_hdr = (struct msm_vidc_extradata_header *)((char *)kvaddr +
				vb->planes[1].data_offset);
	}
	/* check if sufficient space available */
	if (((char *)e_hdr + sizeof(struct msm_vidc_extradata_header) +
			sizeof(struct msm_vidc_enc_cvp_metadata_payload) +
			sizeof(struct msm_vidc_extradata_header)) >
			(kvaddr + dbuf->size)) {
		dprintk(VIDC_ERR,
			"%s: couldn't append extradata, (e_hdr[%pK] - kvaddr[%pK]) %#x, size %d\n",
			__func__, e_hdr, kvaddr, (char *)e_hdr - (char *)kvaddr,
			dbuf->size);
		goto error;
	}
	if (cvp->metadata_available) {
		cvp->metadata_available = false;

		/* copy payload */
		e_hdr->version = 0x00000001;
		e_hdr->port_index = 1;
		e_hdr->type = MSM_VIDC_EXTRADATA_CVP_METADATA;
		e_hdr->data_size =
			sizeof(struct msm_vidc_enc_cvp_metadata_payload);
		e_hdr->size = sizeof(struct msm_vidc_extradata_header) +
			e_hdr->data_size;
		dma_buf_begin_cpu_access(cvp->output_buffer.dbuf,
			DMA_BIDIRECTIONAL);
		memcpy(e_hdr->data, cvp->output_buffer.kvaddr,
			sizeof(struct msm_vidc_enc_cvp_metadata_payload));
		dma_buf_end_cpu_access(cvp->output_buffer.dbuf,
			DMA_BIDIRECTIONAL);
	}
	/* fill extradata none */
	e_hdr = (struct msm_vidc_extradata_header *)
			((char *)e_hdr + e_hdr->size);
	e_hdr->version = 0x00000001;
	e_hdr->port_index = 1;
	e_hdr->type = MSM_VIDC_EXTRADATA_NONE;
	e_hdr->data_size = 0;
	e_hdr->size = sizeof(struct msm_vidc_extradata_header) +
			e_hdr->data_size;

	dma_buf_vunmap(dbuf, kvaddr);
	dma_buf_end_cpu_access(dbuf, DMA_BIDIRECTIONAL);
	dma_buf_put(dbuf);

	return rc;

error:
	if (kvaddr) {
		dma_buf_vunmap(dbuf, kvaddr);
		dma_buf_end_cpu_access(dbuf, DMA_BIDIRECTIONAL);
	}
	if (dbuf)
		dma_buf_put(dbuf);

	return rc;
}

static int msm_cvp_reference_management(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp;
	struct msm_cvp_buf temp;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;

	/* swap context buffers */
	memcpy(&temp, &cvp->refcontext_buffer, sizeof(struct msm_cvp_buf));
	memcpy(&cvp->refcontext_buffer, &cvp->context_buffer,
			sizeof(struct msm_cvp_buf));
	memcpy(&cvp->context_buffer, &temp, sizeof(struct msm_cvp_buf));

	/* swap downscale buffers */
	if (cvp->downscale) {
		memcpy(&temp, &cvp->ref_buffer, sizeof(struct msm_cvp_buf));
		memcpy(&cvp->ref_buffer, &cvp->src_buffer,
				sizeof(struct msm_cvp_buf));
		memcpy(&cvp->src_buffer, &temp, sizeof(struct msm_cvp_buf));
	}

	return rc;
}

static int msm_vidc_cvp_session_start(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp = NULL;
	struct cvp_kmd_session_control *ctrl = NULL;
	struct cvp_kmd_arg *arg = NULL;

	if (!inst || !inst->cvp || !inst->cvp->arg) {
		dprintk(VIDC_ERR, "%s: invalid param\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;
	arg = cvp->arg;

	memset(arg, 0, sizeof(struct cvp_kmd_arg));
	arg->type = CVP_KMD_SESSION_CONTROL;
	ctrl = (struct cvp_kmd_session_control *)&arg->data.session_ctrl;
	ctrl->ctrl_type = SESSION_START;

	rc = msm_cvp_private(cvp->priv, CVP_KMD_SESSION_CONTROL, arg);
	if (rc) {
		dprintk(VIDC_ERR,
			"%s: CVP_KMD_SESSION_CONTROL failed, rc %d\n",
			__func__, rc);
		return rc;
	}

	return rc;
}

static int msm_vidc_cvp_session_stop(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp = NULL;
	struct cvp_kmd_session_control *ctrl = NULL;
	struct cvp_kmd_arg *arg = NULL;

	if (!inst || !inst->cvp || !inst->cvp->arg) {
		dprintk(VIDC_ERR, "%s: invalid param\n", __func__);
		return -EINVAL;
	}

	cvp = inst->cvp;
	arg = cvp->arg;

	memset(arg, 0, sizeof(struct cvp_kmd_arg));

	arg->type = CVP_KMD_SESSION_CONTROL;

	ctrl = (struct cvp_kmd_session_control *)&arg->data.session_ctrl;
	ctrl->ctrl_type = SESSION_STOP;

	rc = msm_cvp_private(cvp->priv, CVP_KMD_SESSION_CONTROL, arg);
	if (rc) {
		dprintk(VIDC_ERR,
			"%s: CVP_KMD_SESSION_CONTROL failed, rc %d\n",
			__func__, rc);
		return rc;
	}

	return rc;
}

static int msm_cvp_frame_process(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf)
{
	int rc = 0;
	struct msm_cvp_external *cvp;
	struct vb2_buffer *vb;
	struct cvp_kmd_arg *arg;
	struct msm_cvp_dme_frame_packet *frame;
	const u32 fps_max = CVP_FRAME_RATE_MAX;
	u32 fps, operating_rate, skip_framecount;
	bool skipframe = false;

	if (!inst || !inst->cvp || !inst->cvp->arg || !mbuf) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;
	arg = cvp->arg;

	vb = &mbuf->vvb.vb2_buf;
	cvp->fullres_buffer.index = vb->index;
	cvp->fullres_buffer.fd = vb->planes[0].m.fd;
	cvp->fullres_buffer.size = vb->planes[0].length;
	cvp->fullres_buffer.offset = vb->planes[0].data_offset;
	cvp->fullres_buffer.dbuf = mbuf->smem[0].dma_buf;

	/* handle framerate or operarating rate changes dynamically */
	if (cvp->frame_rate != inst->clk_data.frame_rate ||
		cvp->operating_rate != inst->clk_data.operating_rate) {
		/* update cvp parameters */
		cvp->frame_rate = inst->clk_data.frame_rate;
		cvp->operating_rate = inst->clk_data.operating_rate;
		rc = msm_cvp_set_clocks_and_bus(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: unsupported dynamic changes %#x %#x\n",
				__func__, cvp->frame_rate, cvp->operating_rate);
			goto error;
		}
	}

	/*
	 * Special handling for operating rate INT_MAX,
	 * client's intention is not to skip cvp preprocess
	 * based on operating rate, skip logic can still be
	 * executed based on framerate though.
	 */
	if (cvp->operating_rate == INT_MAX)
		operating_rate = fps_max << 16;
	else
		operating_rate = cvp->operating_rate;

	mbuf->vvb.flags &= ~V4L2_BUF_FLAG_CVPMETADATA_SKIP;
	/* frame skip logic */
	fps = max(cvp->frame_rate, operating_rate) >> 16;
	if (fps > fps_max) {
		/*
		 * fps <= 120: 0, 2, 4, 6 .. are not skipped
		 * fps <= 180: 0, 3, 6, 9 .. are not skipped
		 * fps <= 240: 0, 4, 8, 12 .. are not skipped
		 * fps <= 960: 0, 16, 32, 48 .. are not skipped
		 */
		fps = roundup(fps, fps_max);
		skip_framecount = fps / fps_max;
		skipframe = cvp->framecount % skip_framecount;
	}
	if (skipframe) {
		print_cvp_buffer(VIDC_LOW, "input frame with skipflag",
			inst, &cvp->fullres_buffer);
		cvp->framecount++;
		cvp->metadata_available = false;
		mbuf->vvb.flags |= V4L2_BUF_FLAG_CVPMETADATA_SKIP;
		return 0;
	}

	memset(arg, 0, sizeof(struct cvp_kmd_arg));
	arg->type = CVP_KMD_SEND_CMD_PKT;
	arg->buf_offset = offsetof(struct msm_cvp_dme_frame_packet,
		fullres_srcbuffer) / sizeof(u32);
	arg->buf_num = (sizeof(struct msm_cvp_dme_frame_packet) -
		(arg->buf_offset * sizeof(u32))) /
		sizeof(struct msm_cvp_buffer_type);
	frame = (struct msm_cvp_dme_frame_packet *)&arg->data.hfi_pkt.pkt_data;
	frame->size = sizeof(struct msm_cvp_dme_frame_packet);
	frame->packet_type = HFI_CMD_SESSION_CVP_DME_FRAME;
	frame->session_id = cvp->session_id;
	if (!cvp->framecount)
		frame->skip_mv_calc = 1;
	else
		frame->skip_mv_calc = 0;
	frame->min_fpx_threshold = 2;
	frame->enable_descriptor_lpf = 1;
	frame->enable_ncc_subpel = 1;
	frame->descmatch_threshold = 52;
	frame->ncc_robustness_threshold = 0;

	fill_cvp_buffer(&frame->fullres_srcbuffer,
				&cvp->fullres_buffer);
	fill_cvp_buffer(&frame->videospatialtemporal_statsbuffer,
				&cvp->output_buffer);
	fill_cvp_buffer(&frame->src_buffer, &cvp->fullres_buffer);
	if (cvp->downscale) {
		fill_cvp_buffer(&frame->src_buffer, &cvp->src_buffer);
		fill_cvp_buffer(&frame->ref_buffer, &cvp->ref_buffer);
	}
	fill_cvp_buffer(&frame->srcframe_contextbuffer,
				&cvp->context_buffer);
	fill_cvp_buffer(&frame->refframe_contextbuffer,
				&cvp->refcontext_buffer);

	print_cvp_buffer(VIDC_LOW, "input frame", inst, &cvp->fullres_buffer);
	rc = msm_cvp_private(cvp->priv, CVP_KMD_SEND_CMD_PKT, arg);
	if (rc) {
		print_cvp_buffer(VIDC_ERR, "send failed: input frame",
			inst, &cvp->fullres_buffer);
		goto error;
	}
	/* wait for frame done */
	arg->type = CVP_KMD_RECEIVE_MSG_PKT;
	rc = msm_cvp_private(cvp->priv, CVP_KMD_RECEIVE_MSG_PKT, arg);
	if (rc) {
		print_cvp_buffer(VIDC_ERR, "wait failed: input frame",
			inst, &cvp->fullres_buffer);
		goto error;
	}
	cvp->framecount++;
	cvp->metadata_available = true;

error:
	return rc;
}

int msm_vidc_cvp_preprocess(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf)
{
	int rc = 0;
	struct msm_cvp_external *cvp;

	if (!inst || !inst->cvp || !mbuf) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	if (inst->state != MSM_VIDC_START_DONE) {
		dprintk(VIDC_ERR, "%s: invalid inst state %d\n",
			__func__, inst->state);
		return -EINVAL;
	}
	cvp = inst->cvp;

	rc = msm_cvp_frame_process(inst, mbuf);
	if (rc) {
		dprintk(VIDC_ERR, "%s: cvp process failed\n", __func__);
		return rc;
	}

	rc = msm_cvp_prepare_extradata(inst, mbuf);
	if (rc) {
		dprintk(VIDC_ERR, "%s: prepare extradata failed\n", __func__);
		return rc;
	}

	rc = msm_cvp_reference_management(inst);
	if (rc) {
		dprintk(VIDC_ERR, "%s: ref management failed\n", __func__);
		return rc;
	}

	return rc;
}

static int msm_vidc_cvp_session_delete(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp = NULL;
	struct cvp_kmd_session_control *ctrl = NULL;
	struct cvp_kmd_arg *arg = NULL;

	if (!inst || !inst->cvp || !inst->cvp->arg) {
		dprintk(VIDC_ERR, "%s: invalid param\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;
	arg = cvp->arg;

	memset(arg, 0, sizeof(struct cvp_kmd_arg));
	arg->type = CVP_KMD_SESSION_CONTROL;
	ctrl = (struct cvp_kmd_session_control *)&arg->data.session_ctrl;
	ctrl->ctrl_type = SESSION_DELETE;

	rc = msm_cvp_private(cvp->priv, CVP_KMD_SESSION_CONTROL, arg);
	if (rc) {
		dprintk(VIDC_ERR,
			"%s: CVP_KMD_SESSION_CONTROL failed, rc %d\n",
			__func__, rc);
		return rc;
	}

	return rc;
}

static int msm_cvp_mem_deinit(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;

	dprintk(VIDC_HIGH, "%s: cvp session %#x\n", __func__, cvp->session_id);
	msm_cvp_deinit_internal_buffers(inst);
	msm_cvp_deinit_context_buffers(inst);
	msm_cvp_deinit_downscale_buffers(inst);

	cvp->priv = NULL;
	kfree(cvp->arg);
	cvp->arg = NULL;
	kfree(inst->cvp);
	inst->cvp = NULL;

	return rc;
}

static int msm_vidc_cvp_deinit(struct msm_vidc_inst *inst)
{
	int rc = 0;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	rc = msm_vidc_cvp_session_stop(inst);
	if (rc) {
		dprintk(VIDC_ERR, "%s: cvp stop failed with error %d\n",
			__func__, rc);
	}

	msm_vidc_cvp_session_delete(inst);

	return rc;
}

static int msm_vidc_cvp_close(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;

	dprintk(VIDC_HIGH, "%s: cvp session %#x\n", __func__, cvp->session_id);
	rc = msm_cvp_close(cvp->priv);
	if (rc) {
		dprintk(VIDC_ERR,
			"%s: cvp close failed with error %d\n", __func__, rc);
	}

	return rc;
}

int msm_vidc_cvp_unprepare_preprocess(struct msm_vidc_inst *inst)
{
	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	if (!inst->cvp) {
		dprintk(VIDC_HIGH, "%s: cvp not enabled or closed\n", __func__);
		return 0;
	}

	msm_vidc_cvp_deinit(inst);
	msm_vidc_cvp_close(inst);
	msm_cvp_mem_deinit(inst);

	return 0;
}

static int msm_vidc_cvp_session_create(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp = NULL;
	struct cvp_kmd_session_control *ctrl = NULL;
	struct cvp_kmd_arg *arg = NULL;

	if (!inst || !inst->cvp || !inst->cvp->arg) {
		dprintk(VIDC_ERR, "%s: invalid param\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;
	arg = cvp->arg;

	memset(arg, 0, sizeof(struct cvp_kmd_arg));
	arg->type = CVP_KMD_SESSION_CONTROL;
	ctrl = (struct cvp_kmd_session_control *)&arg->data.session_ctrl;
	ctrl->ctrl_type = SESSION_CREATE;

	rc = msm_cvp_private(cvp->priv, CVP_KMD_SESSION_CONTROL, arg);
	if (rc) {
		dprintk(VIDC_ERR,
			"%s: CVP_KMD_SESSION_CONTROL failed, rc %d\n",
			__func__, rc);
		return rc;
	}

	return rc;
}

static int msm_vidc_cvp_getsessioninfo(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp;
	struct cvp_kmd_arg *arg;

	if (!inst || !inst->cvp || !inst->cvp->arg) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;
	arg = cvp->arg;

	memset(arg, 0, sizeof(struct cvp_kmd_arg));
	arg->type = CVP_KMD_GET_SESSION_INFO;
	rc = msm_cvp_private(cvp->priv, CVP_KMD_GET_SESSION_INFO, arg);
	if (rc) {
		dprintk(VIDC_ERR, "%s: get_session_info failed\n", __func__);
		goto error;
	}
	cvp->session_id = arg->data.session.session_id;
	dprintk(VIDC_HIGH, "%s: cvp session id %#x\n",
		__func__, cvp->session_id);

	rc = msm_cvp_get_version_info(inst);
	if (rc) {
		dprintk(VIDC_ERR, "%s: get_version_info failed\n", __func__);
		goto error;
	}
	return rc;

error:
	msm_vidc_cvp_deinit(inst);
	return rc;
}

static int msm_vidc_cvp_dme_basic_config(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp;
	struct cvp_kmd_arg *arg;
	struct msm_cvp_dme_basic_config_packet *dmecfg;
	struct v4l2_format *fmt;
	u32 color_fmt;

	if (!inst || !inst->cvp || !inst->cvp->arg) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;
	arg = cvp->arg;

	memset(arg, 0, sizeof(struct cvp_kmd_arg));
	arg->type = CVP_KMD_SEND_CMD_PKT;
	dmecfg = (struct msm_cvp_dme_basic_config_packet *)
			&arg->data.hfi_pkt.pkt_data;
	dmecfg->size = sizeof(struct msm_cvp_dme_basic_config_packet);
	dmecfg->packet_type = HFI_CMD_SESSION_CVP_DME_BASIC_CONFIG;
	dmecfg->session_id = cvp->session_id;
	/* source buffer format should be NV12_UBWC always */
	dmecfg->srcbuffer_format = HFI_COLOR_FORMAT_NV12_UBWC;
	dmecfg->src_width = cvp->ds_width;
	dmecfg->src_height = cvp->ds_height;
	rc = msm_cvp_fill_planeinfo(&dmecfg->srcbuffer_planeinfo,
		COLOR_FMT_NV12_UBWC, dmecfg->src_width, dmecfg->src_height);
	if (rc)
		goto error;

	fmt = &inst->fmts[INPUT_PORT].v4l2_fmt;
	color_fmt = msm_comm_convert_color_fmt(fmt->fmt.pix_mp.pixelformat);
	dmecfg->fullresbuffer_format = msm_comm_get_hfi_uncompressed(
			fmt->fmt.pix_mp.pixelformat);
	dmecfg->fullres_width = cvp->width;
	dmecfg->fullres_height = cvp->height;
	rc = msm_cvp_fill_planeinfo(&dmecfg->fullresbuffer_planeinfo,
		color_fmt, dmecfg->fullres_width, dmecfg->fullres_height);
	if (rc)
		goto error;
	dmecfg->ds_enable = cvp->downscale;
	dmecfg->enable_lrme_robustness = 1;
	dmecfg->enable_inlier_tracking = 1;
	rc = msm_cvp_private(cvp->priv, CVP_KMD_SEND_CMD_PKT, arg);
	if (rc) {
		dprintk(VIDC_ERR, "%s: cvp configuration failed\n", __func__);
		goto error;
	}

error:
	return rc;
}

static int msm_cvp_mem_init(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp;
	struct v4l2_format *fmt;

	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	inst->cvp = kzalloc(sizeof(struct msm_cvp_external), GFP_KERNEL);
	if (!inst->cvp) {
		dprintk(VIDC_ERR, "%s: failed to allocate\n", __func__);
		return -ENOMEM;
	}
	cvp = inst->cvp;

	cvp->arg = kzalloc(sizeof(struct cvp_kmd_arg), GFP_KERNEL);
	if (!cvp->arg) {
		kfree(inst->cvp);
		inst->cvp = NULL;
		return -ENOMEM;
	}

	cvp->framecount = 0;
	cvp->metadata_available = false;
	fmt = &inst->fmts[INPUT_PORT].v4l2_fmt;
	cvp->width = fmt->fmt.pix_mp.width;
	cvp->height = fmt->fmt.pix_mp.height;
	cvp->frame_rate = inst->clk_data.frame_rate;
	cvp->operating_rate = inst->clk_data.operating_rate;

	/* enable downscale always */
	cvp->downscale = true;
	rc = msm_cvp_init_downscale_resolution(inst);
	if (rc)
		goto error;

	dprintk(VIDC_HIGH,
		"%s: pixelformat %#x, wxh %dx%d downscale %d ds_wxh %dx%d fps %d op_rate %d\n",
		__func__, fmt->fmt.pix_mp.pixelformat,
		cvp->width, cvp->height, cvp->downscale,
		cvp->ds_width, cvp->ds_height,
		cvp->frame_rate >> 16, cvp->operating_rate >> 16);

	rc = msm_cvp_init_downscale_buffers(inst);
	if (rc)
		goto error;
	rc = msm_cvp_init_internal_buffers(inst);
	if (rc)
		goto error;
	rc = msm_cvp_init_context_buffers(inst);
	if (rc)
		goto error;

	return rc;

error:
	msm_cvp_mem_deinit(inst);
	return rc;
}

static int msm_vidc_cvp_open(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_cvp_external *cvp;

	if (!inst || !inst->cvp) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cvp = inst->cvp;

	dprintk(VIDC_HIGH, "%s: opening cvp\n", __func__);
	cvp->priv = msm_cvp_open(0, MSM_VIDC_CVP);
	if (!cvp->priv) {
		dprintk(VIDC_ERR, "%s: failed to open cvp session\n", __func__);
		rc = -EINVAL;
	}

	return rc;
}

static int msm_vidc_cvp_init(struct msm_vidc_inst *inst)
{
	int rc;

	rc = msm_cvp_set_priority(inst);
	if (rc)
		goto error;

	rc = msm_vidc_cvp_session_create(inst);
	if (rc)
		return rc;

	rc = msm_vidc_cvp_getsessioninfo(inst);
	if (rc)
		return rc;

	rc = msm_cvp_set_clocks_and_bus(inst);
	if (rc)
		goto error;

	rc = msm_vidc_cvp_dme_basic_config(inst);
	if (rc)
		goto error;

	rc = msm_cvp_set_persist_buffer(inst);
	if (rc)
		goto error;

	rc = msm_vidc_cvp_session_start(inst);
	if (rc)
		goto error;

	return 0;

error:
	msm_vidc_cvp_deinit(inst);
	return rc;
}

int msm_vidc_cvp_prepare_preprocess(struct msm_vidc_inst *inst)
{
	int rc;

	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	rc = msm_cvp_mem_init(inst);
	if (rc)
		return rc;

	rc = msm_vidc_cvp_open(inst);
	if (rc)
		return rc;

	rc = msm_vidc_cvp_init(inst);
	if (rc)
		return rc;

	return 0;
}
