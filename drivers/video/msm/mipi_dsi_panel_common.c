/* drivers/video/msm/mipi_dsi_panel_common.c
 *
 * Copyright (C) [2011] Sony Ericsson Mobile Communications AB.
 * Copyright (C) 2012-2013 Sony Mobile Communications AB.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2; as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_dsi_panel.h"

#ifdef CONFIG_FB_MSM_RECOVER_PANEL
#define DSI_VIDEO_BASE	0xE0000
static int on_state;
static int drv_ic_sysfs_create;
#endif

void mipi_dsi_set_default_panel(struct mipi_dsi_data *dsi_data)
{
	if (dsi_data->default_panels[0] != NULL)
		dsi_data->panel = dsi_data->default_panels[0];
	else
		dsi_data->panel = dsi_data->panels[0];

	MSM_FB_INFO("default panel: %s\n", dsi_data->panel->name);
	dsi_data->panel_data.panel_info =
		*dsi_data->panel->pctrl->get_panel_info();
	dsi_data->panel_data.panel_info.width =
		dsi_data->panel->width;
	dsi_data->panel_data.panel_info.height =
		dsi_data->panel->height;
#ifdef CONFIG_FB_MSM_RECOVER_PANEL
	dsi_data->nvrw_panel_detective = false;
#endif
}

static int rx_len;
static char rx_panel_id[16];
static void panel_id_cb(int len, char* data)
{
	int i;

	if (!data)
		return;
	rx_len = len;
	for (i = 0; i < len; i++)
		rx_panel_id[i] = data[i];
}

static int panel_id_reg_check(struct msm_fb_data_type *mfd,
			      const struct panel_id* panel)
{
	int i;
	struct dcs_cmd_req cmdreq;

	mipi_set_tx_power_mode(0);
	cmdreq.cmds = panel->pctrl->read_id_cmds;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
	cmdreq.rlen = panel->id_num;
	cmdreq.cb = panel_id_cb;
	mipi_dsi_cmdlist_put(&cmdreq);
	mipi_set_tx_power_mode(1);

	for (i = 0; i < panel->id_num; i++) {
		if ((i >= rx_len) ||
			((rx_panel_id[i] != panel->id[i]) &&
				(panel->id[i] != 0xff)))
			return -ENODEV;
	}

	return 0;
}

struct msm_panel_info *mipi_dsi_detect_panel(
	struct msm_fb_data_type *mfd)
{
	int i;
	int ret;
	struct mipi_dsi_data *dsi_data;

	dsi_data = platform_get_drvdata(mfd->panel_pdev);

	mipi_dsi_op_mode_config(DSI_CMD_MODE);
	if (dsi_data->default_panels[0] != NULL) {
		for (i = 0; dsi_data->default_panels[i]; i++) {
			ret = panel_id_reg_check(mfd,
					dsi_data->default_panels[i]);
			if (!ret)
				break;
		}

		if (dsi_data->default_panels[i]) {
			dsi_data->panel = dsi_data->default_panels[i];
#ifdef CONFIG_FB_MSM_RECOVER_PANEL
			dsi_data->nvrw_panel_detective = true;
#endif
			dev_info(&mfd->panel_pdev->dev,
				"found panel vendor: %s\n",
				dsi_data->panel->name);
		} else {
			dev_warn(&mfd->panel_pdev->dev,
				"cannot detect panel vendor!\n");
			return NULL;
		}
	}

	for (i = 0; dsi_data->panels[i]; i++) {
		ret = panel_id_reg_check(mfd, dsi_data->panels[i]);
		if (!ret)
			break;
	}

	if (dsi_data->panels[i]) {
		dsi_data->panel = dsi_data->panels[i];
#ifdef CONFIG_FB_MSM_RECOVER_PANEL
		dsi_data->nvrw_panel_detective = true;
#endif
		dev_info(&mfd->panel_pdev->dev, "found panel: %s\n",
			 dsi_data->panel->name);
	} else {
		dev_warn(&mfd->panel_pdev->dev, "cannot detect panel!\n");
		return NULL;
	}

	dsi_data->panel_data.panel_info =
		*dsi_data->panel->pctrl->get_panel_info();
	dsi_data->panel_data.panel_info.width =
		dsi_data->panel->width;
	dsi_data->panel_data.panel_info.height =
		dsi_data->panel->height;
	dsi_data->panel_data.panel_info.mipi.dsi_pclk_rate =
		mfd->panel_info.mipi.dsi_pclk_rate;
	mipi_dsi_op_mode_config
		(dsi_data->panel_data.panel_info.mipi.mode);

	return &dsi_data->panel_data.panel_info;
}

int __devinit mipi_dsi_need_detect_panel(
	const struct panel_id **panels)
{
	int num = 0;
	int i;

	for (i = 0; panels[i]; i++)
		num++;

	return (num > 1) ? 1 : 0;
}

int mipi_dsi_update_panel(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct fb_info *fbi;
	struct msm_panel_info *pinfo;
	struct mipi_panel_info *mipi;
	uint8 lanes = 0, bpp;
	uint32 h_period, v_period, dsi_pclk_rate;

	mfd = platform_get_drvdata(pdev);
	pinfo = &mfd->panel_info;

	fbi = mfd->fbi;
	fbi->var.pixclock = pinfo->clk_rate;
	fbi->var.left_margin = pinfo->lcdc.h_back_porch;
	fbi->var.right_margin = pinfo->lcdc.h_front_porch;
	fbi->var.upper_margin = pinfo->lcdc.v_back_porch;
	fbi->var.lower_margin = pinfo->lcdc.v_front_porch;
	fbi->var.hsync_len = pinfo->lcdc.h_pulse_width;
	fbi->var.vsync_len = pinfo->lcdc.v_pulse_width;

	h_period = ((pinfo->lcdc.h_pulse_width)
			+ (pinfo->lcdc.h_back_porch)
			+ (pinfo->xres)
			+ (pinfo->lcdc.h_front_porch));

	v_period = ((pinfo->lcdc.v_pulse_width)
			+ (pinfo->lcdc.v_back_porch)
			+ (pinfo->yres)
			+ (pinfo->lcdc.v_front_porch));

	mipi  = &pinfo->mipi;

	if (mipi->data_lane3)
		lanes += 1;
	if (mipi->data_lane2)
		lanes += 1;
	if (mipi->data_lane1)
		lanes += 1;
	if (mipi->data_lane0)
		lanes += 1;

	if ((mipi->dst_format == DSI_CMD_DST_FORMAT_RGB888)
	    || (mipi->dst_format == DSI_VIDEO_DST_FORMAT_RGB888)
	    || (mipi->dst_format == DSI_VIDEO_DST_FORMAT_RGB666_LOOSE))
		bpp = 3;
	else if ((mipi->dst_format == DSI_CMD_DST_FORMAT_RGB565)
		 || (mipi->dst_format == DSI_VIDEO_DST_FORMAT_RGB565))
		bpp = 2;
	else
		bpp = 3;		/* Default format set to RGB888 */

	if (pinfo->type == MIPI_VIDEO_PANEL) {
		if (lanes > 0) {
			pinfo->clk_rate =
			((h_period * v_period * (mipi->frame_rate) * bpp * 8)
			   / lanes);
		} else {
			pr_err("%s: forcing mipi_dsi lanes to 1\n", __func__);
			pinfo->clk_rate =
				(h_period * v_period
					 * (mipi->frame_rate) * bpp * 8);
		}
	}
	pll_divider_config.clk_rate = pinfo->clk_rate;

	mipi_dsi_clk_div_config(bpp, lanes, &dsi_pclk_rate);

	if ((dsi_pclk_rate < 3300000) || (dsi_pclk_rate > 103300000))
		dsi_pclk_rate = 35000000;
	mipi->dsi_pclk_rate = dsi_pclk_rate;

	return 0;
}

void mipi_dsi_update_lane_cfg(const struct mipi_dsi_lane_cfg *plncfg)
{
	int i, j, ln_offset;

	ln_offset = 0x300;
	for (i = 0; i < 4; i++) {
		/* DSI1_DSIPHY_LN_CFG */
		for (j = 0; j < 3; j++)
			MIPI_OUTP(MIPI_DSI_BASE + ln_offset + j * 4,
				plncfg->ln_cfg[i][j]);
		/* DSI1_DSIPHY_LN_TEST_DATAPATH */
		MIPI_OUTP(MIPI_DSI_BASE + ln_offset + 0x0c,
			plncfg->ln_dpath[i]);
		/* DSI1_DSIPHY_LN_TEST_STR */
		for (j = 0; j < 2; j++)
			MIPI_OUTP(MIPI_DSI_BASE + ln_offset + 0x14 + j * 4,
				plncfg->ln_str[i][j]);

		ln_offset += 0x40;
	}

	/* DSI1_DSIPHY_LNCK_CFG */
	for (i = 0; i < 3; i++)
		MIPI_OUTP(MIPI_DSI_BASE + 0x0400 + i * 4,
			plncfg->lnck_cfg[i]);
	/* DSI1_DSIPHY_LNCK_TEST_DATAPATH */
	MIPI_OUTP(MIPI_DSI_BASE + 0x040c, plncfg->lnck_dpath);
	/* DSI1_DSIPHY_LNCK_TEST_STR */
	for (i = 0; i < 2; i++)
		MIPI_OUTP(MIPI_DSI_BASE + 0x0414 + i * 4,
			plncfg->lnck_str[i]);
}

int mipi_dsi_eco_mode_switch(struct msm_fb_data_type *mfd)
{
	int ret = 0;
	struct mipi_dsi_data *dsi_data;
	struct dsi_controller *pctrl;
	struct dcs_cmd_req cmdreq;

	dsi_data = platform_get_drvdata(mfd->panel_pdev);
	if (!dsi_data || !dsi_data->lcd_power) {
		ret = -ENODEV;
		goto eco_mode_switch_fail;
	}
	pctrl = dsi_data->panel->pctrl;

	mipi_set_tx_power_mode(0);

	if (dsi_data->eco_mode_on && pctrl->eco_mode_gamma_cmds) {
		cmdreq.cmds = pctrl->eco_mode_gamma_cmds;
		cmdreq.cmds_cnt = pctrl->eco_mode_gamma_cmds_size;
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);
		dev_info(&mfd->panel_pdev->dev, "ECO MODE ON\n");
	} else if (pctrl->normal_gamma_cmds) {
		cmdreq.cmds = pctrl->normal_gamma_cmds;
		cmdreq.cmds_cnt = pctrl->normal_gamma_cmds_size;
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);
		dev_info(&mfd->panel_pdev->dev, "ECO MODE OFF\n");
	}

	mipi_set_tx_power_mode(1);

	return ret;

eco_mode_switch_fail:
	return ret;
}

static ssize_t show_eco_mode(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct platform_device *pdev;
	struct mipi_dsi_data *dsi_data;
	struct msm_fb_data_type *mfd;

	pdev = container_of(dev, struct platform_device, dev);
	mfd = platform_get_drvdata(pdev);

	dsi_data = platform_get_drvdata(mfd->panel_pdev);

	return snprintf(buf, PAGE_SIZE, "%i\n", dsi_data->eco_mode_on);
}

static ssize_t store_eco_mode(struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	ssize_t ret;
	struct platform_device *pdev;
	struct mipi_dsi_data *dsi_data;
	struct msm_fb_data_type *mfd;
	struct msm_fb_panel_data *pdata = NULL;

	pdev = container_of(dev, struct platform_device, dev);
	mfd = platform_get_drvdata(pdev);

	dsi_data = platform_get_drvdata(mfd->panel_pdev);

	pdata = (struct msm_fb_panel_data *)mfd->pdev->dev.platform_data;

	if (sscanf(buf, "%i", &ret) != 1) {
		printk(KERN_ERR"Invalid flag for eco_mode\n");
		goto exit;
	}

	if (ret)
		dsi_data->eco_mode_on = true;
	else
		dsi_data->eco_mode_on = false;

	if (mfd->panel_power_on)
		dsi_data->eco_mode_switch(mfd);

exit:
	ret = strnlen(buf, count);

	return ret;
}

static struct device_attribute eco_mode_attributes[] = {
	__ATTR(eco_mode, 0644, show_eco_mode, store_eco_mode),
};

int eco_mode_sysfs_register(struct device *dev)
{
	int i;

	dev_dbg(dev, "%s\n", __func__);

	for (i = 0; i < ARRAY_SIZE(eco_mode_attributes); i++)
		if (device_create_file(dev, eco_mode_attributes + i))
			goto error;

	return 0;

error:
	for (; i >= 0; i--)
		device_remove_file(dev, eco_mode_attributes + i);

	dev_err(dev, "%s: Unable to create interface\n", __func__);

	return -ENODEV;
}



#ifdef CONFIG_FB_MSM_RECOVER_PANEL
static int prepare_for_reg_access(struct msm_fb_data_type *mfd)
{
	struct device *dev = &mfd->panel_pdev->dev;
	struct mipi_dsi_data *dsi_data;
	int ret = 0;

	dsi_data = platform_get_drvdata(mfd->panel_pdev);

	if (mfd->panel_power_on) {
		dev_dbg(dev, "%s: panel is on, don't do anything\n", __func__);
		on_state = false;
	} else {
		dev_dbg(dev, "%s: panel is NOT on, power on stack\n", __func__);

		ret = panel_next_on(mfd->pdev); /* msm_fb_dev */
		if (ret)
			goto exit;
		on_state = true;
	}

	mutex_lock(&mfd->dma->ov_mutex);
	if (mfd->panel_info.mipi.mode == DSI_VIDEO_MODE) {
		mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
		MDP_OUTP(MDP_BASE + DSI_VIDEO_BASE, 0);
		mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
		msleep(ONE_FRAME_TRANSMIT_WAIT_MS);
		mipi_dsi_controller_cfg(0);
		mipi_dsi_op_mode_config(DSI_CMD_MODE);
	}
exit:
	return ret;
}

static void post_reg_access(struct msm_fb_data_type *mfd)
{
	struct mipi_dsi_data *dsi_data;

	dsi_data = platform_get_drvdata(mfd->panel_pdev);

	if (mfd->panel_info.mipi.mode == DSI_VIDEO_MODE) {
		mipi_dsi_op_mode_config(DSI_VIDEO_MODE);
		mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
		mipi_dsi_sw_reset();
		mipi_dsi_controller_cfg(1);
		MDP_OUTP(MDP_BASE + DSI_VIDEO_BASE, 1);
		mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
	}
	mutex_unlock(&mfd->dma->ov_mutex);

	if (on_state)
		(void)panel_next_off(mfd->pdev);
}

static ssize_t show_nvm_is_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev;
	struct mipi_dsi_data *dsi_data;
	struct msm_fb_data_type *mfd;

	pdev = container_of(dev, struct platform_device, dev);
	mfd = platform_get_drvdata(pdev);
	dsi_data = platform_get_drvdata(mfd->panel_pdev);

	if (dsi_data->nvrw_ic_vendor != NVRW_DRV_RENESAS)
		return snprintf(buf, PAGE_SIZE, "skip");

	if (dsi_data->nvrw_panel_detective)
		return snprintf(buf, PAGE_SIZE, "OK");
	else
		return snprintf(buf, PAGE_SIZE, "NG");

	return 0;
}

static ssize_t show_nvm_result(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev;
	struct mipi_dsi_data *dsi_data;
	struct msm_fb_data_type *mfd;

	pdev = container_of(dev, struct platform_device, dev);
	mfd = platform_get_drvdata(pdev);
	dsi_data = platform_get_drvdata(mfd->panel_pdev);

	return snprintf(buf, PAGE_SIZE, "%d", dsi_data->nvrw_result);
}

static ssize_t show_nvm(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct platform_device *pdev;
	struct mipi_dsi_data *dsi_data;
	struct msm_fb_data_type *mfd;
	int	rc;

	pdev = container_of(dev, struct platform_device, dev);
	mfd = platform_get_drvdata(pdev);
	dsi_data = platform_get_drvdata(mfd->panel_pdev);

	if (dsi_data->nvrw_ic_vendor != NVRW_DRV_RENESAS ||
		!dsi_data->nvrw_panel_detective)
		return 0;

	rc = prepare_for_reg_access(mfd);
	if (rc)
		return 0;

	mipi_set_tx_power_mode(1);

	rc = 0;
	if (dsi_data->panel->pnvrw_ctl && dsi_data->seq_nvm_read)
		rc = dsi_data->seq_nvm_read(mfd, buf);
	post_reg_access(mfd);

	return rc;
};

static ssize_t store_nvm(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct platform_device *pdev;
	struct mipi_dsi_data *dsi_data;
	struct msm_fb_data_type *mfd;
	int rc;

	pdev = container_of(dev, struct platform_device, dev);
	mfd = platform_get_drvdata(pdev);
	dsi_data = platform_get_drvdata(mfd->panel_pdev);

	dsi_data->nvrw_result = 0;
	dsi_data->nvrw_retry_cnt = 0;
	if (dsi_data->nvrw_ic_vendor != NVRW_DRV_RENESAS ||
		dsi_data->nvrw_panel_detective)
		return count;

	if (!dsi_data->panel->pnvrw_ctl)
		return count;

	if (dsi_data->override_nvm_data) {
		rc = dsi_data->override_nvm_data(mfd, buf, count);
		if (rc == 0) {
			dev_err(dev, "%s : nvm data format error.<%s>\n",
				__func__, buf);
			return count;
		}
	}
	mfd->nvrw_prohibit_draw = true;
	rc = prepare_for_reg_access(mfd);
	if (rc)
		return count;

	mipi_set_tx_power_mode(1);

	if (dsi_data->seq_nvm_erase) {
		rc = dsi_data->seq_nvm_erase(mfd);
		if (rc == 0) {
			dev_err(dev,
				"%s : nvm data erase fail.\n", __func__);
			goto err_exit;
		}
	}
	if (dsi_data->seq_nvm_rsp_write) {
		rc = dsi_data->seq_nvm_rsp_write(mfd);
		if (rc == 0) {
			dev_err(dev,
				"%s : rsp write fail.\n", __func__);
			goto err_exit;
		}
	}
	if (dsi_data->seq_nvm_user_write) {
		rc = dsi_data->seq_nvm_user_write(mfd);
		if (rc == 0) {
			dev_err(dev,
				"%s : user write fail.\n", __func__);
			goto err_exit;
		}
	}

	dsi_data->nvrw_result = dsi_data->nvrw_retry_cnt + 1;
err_exit:
	post_reg_access(mfd);
	mfd->nvrw_prohibit_draw = false;

	return count;
};

static struct device_attribute	drv_ic_sysfs_attrs[] = {
	__ATTR(nvm_is_read, S_IRUGO, show_nvm_is_read, NULL),
	__ATTR(nvm_result, S_IRUGO, show_nvm_result, NULL),
	__ATTR(nvm, S_IRUGO | S_IWUSR, show_nvm, store_nvm),
};

int drv_ic_sysfs_register(struct device *dev)
{
	int i;

	dev_dbg(dev, "%s\n", __func__);
	for (i = 0; i < ARRAY_SIZE(drv_ic_sysfs_attrs); i++) {
		if (device_create_file(dev, drv_ic_sysfs_attrs + i))
			goto error;
	}
	drv_ic_sysfs_create = 1;

	return 0;

error:
	for (; i >= 0; i--)
		device_remove_file(dev, drv_ic_sysfs_attrs + i);
	drv_ic_sysfs_create = 0;
	dev_err(dev, "%s: Unable to create interface\n", __func__);

	return -ENODEV;
};

void drv_ic_sysfs_unregister(struct device *dev)
{
	int i;

	if (drv_ic_sysfs_create) {
		for (i = 0; i < ARRAY_SIZE(drv_ic_sysfs_attrs); i++)
				device_remove_file(dev, drv_ic_sysfs_attrs + i);
		drv_ic_sysfs_create = 0;
	}
}
#endif

#ifdef CONFIG_DEBUG_FS

#define MIPI_PANEL_DEBUG_BUF	2048

#define MSNPRINTF(buf, rsize, ...)			\
do {							\
	ssize_t act = 0;					\
							\
	if (rsize > 0)					\
		act = snprintf(buf, rsize, __VA_ARGS__);	\
	buf += act;					\
	rsize -= act;					\
} while (0)

static void print_cmds2buf(struct dsi_cmd_desc *cmds, int cnt,
			 char **buf, int *rem_size)
{
	int i, j;

	if (!cmds) {
		MSNPRINTF(*buf, *rem_size, "cmds NULL\n");
		goto exit;
	}

	for (i = 0; i < cnt; i++) {
		switch (cmds[i].dtype) {
		case DTYPE_DCS_WRITE:
		case DTYPE_DCS_WRITE1:
			MSNPRINTF(*buf, *rem_size, "DCS_WRITE: ");
			break;
		case DTYPE_DCS_LWRITE:
			MSNPRINTF(*buf, *rem_size, "DCS_LONG_WRITE: ");
			break;
		case DTYPE_GEN_WRITE:
		case DTYPE_GEN_WRITE1:
		case DTYPE_GEN_WRITE2:
			MSNPRINTF(*buf, *rem_size, "GEN_WRITE: ");
			break;
		case DTYPE_GEN_LWRITE:
			MSNPRINTF(*buf, *rem_size, "GEN_LONG_WRITE: ");
			break;
		case DTYPE_DCS_READ:
			MSNPRINTF(*buf, *rem_size, "DCS_READ: ");
			break;
		case DTYPE_GEN_READ:
		case DTYPE_GEN_READ1:
		case DTYPE_GEN_READ2:
			MSNPRINTF(*buf, *rem_size, "GEN_READ: ");
			break;
		case DTYPE_MAX_PKTSIZE:
			MSNPRINTF(*buf, *rem_size, "SET_MAX_PACKET_SIZE: ");
			break;
		case DTYPE_NULL_PKT:
			MSNPRINTF(*buf, *rem_size, "NULL_PACKET: ");
			break;
		case DTYPE_BLANK_PKT:
			MSNPRINTF(*buf, *rem_size, "BLANK_PACKET: ");
			break;
		case DTYPE_PERIPHERAL_ON:
			MSNPRINTF(*buf, *rem_size, "PERIPHERAL_ON: ");
			break;
		case DTYPE_PERIPHERAL_OFF:
			MSNPRINTF(*buf, *rem_size, "PERIPHERAL_OFF: ");
			break;
		default:
			MSNPRINTF(*buf, *rem_size, "UnknownData: ");
			break;
		}
		for (j = 0; j < cmds[i].dlen; j++)
			MSNPRINTF(*buf, *rem_size, "0x%.2x ",
				  cmds[i].payload[j]);
		MSNPRINTF(*buf, *rem_size, "\n");
	}
	MSNPRINTF(*buf, *rem_size, "---------\n");
exit:
	return;
}

static int mipi_dsi_cmd_seq_open(struct inode *inode,
	struct file *file)
{
	struct mipi_dsi_data *dsi_data;

	dsi_data = inode->i_private;
	if (dsi_data->debug_buf != NULL)
		return -EBUSY;
	dsi_data->debug_buf = kzalloc(MIPI_PANEL_DEBUG_BUF, GFP_KERNEL);
	file->private_data = dsi_data;
	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	return 0;
}

static int mipi_dsi_cmd_seq_release(struct inode *inode,
	struct file *file)
{
	struct mipi_dsi_data *dsi_data;

	dsi_data = file->private_data;
	kfree(dsi_data->debug_buf);
	dsi_data->debug_buf = NULL;
	return 0;
}

static ssize_t mipi_dsi_cmd_seq_read(struct file *file,
	char __user *buff, size_t count, loff_t *ppos)
{
	char *bp;
	int len = 0;
	int tot = 0;
	int dlen;
	struct mipi_dsi_data *dsi_data;

	if (*ppos)
		return 0;

	dsi_data = file->private_data;

	bp = dsi_data->debug_buf;
	if (bp == NULL)
		return 0;

	dlen = MIPI_PANEL_DEBUG_BUF;

	if (dsi_data->panel) {
		/* show panel info */
		MSNPRINTF(bp, dlen, "Register data for panel %s\n",
			  dsi_data->panel->name);
		MSNPRINTF(bp, dlen, "xres = %d, yres = %d\n",
			  dsi_data->panel_data.panel_info.xres,
			  dsi_data->panel_data.panel_info.yres);
		/* show commands */
		MSNPRINTF(bp, dlen, "init cmds:\n");
		print_cmds2buf(dsi_data->panel->pctrl->display_init_cmds,
			     dsi_data->panel->pctrl->display_init_cmds_size,
			       &bp, &dlen);
		MSNPRINTF(bp, dlen, "display_on cmds:\n");
		print_cmds2buf(dsi_data->panel->pctrl->display_on_cmds,
			     dsi_data->panel->pctrl->display_on_cmds_size,
			       &bp, &dlen);
		MSNPRINTF(bp, dlen, "display_off cmds:\n");
		print_cmds2buf(dsi_data->panel->pctrl->display_off_cmds,
			     dsi_data->panel->pctrl->display_off_cmds_size,
			       &bp, &dlen);
	} else {
		len = snprintf(bp, dlen, "No panel name\n");
		bp += len;
		dlen -= len;
	}

	tot = (uint32)bp - (uint32)dsi_data->debug_buf;
	*bp = 0;
	tot++;

	if (tot < 0)
		return 0;
	if (copy_to_user(buff, dsi_data->debug_buf, tot))
		return -EFAULT;

	*ppos += tot;

	return tot;
}

static const struct file_operations mipi_dsi_cmd_seq_fops = {
	.open = mipi_dsi_cmd_seq_open,
	.release = mipi_dsi_cmd_seq_release,
	.read = mipi_dsi_cmd_seq_read,
};

void mipi_dsi_debugfs_init(struct platform_device *pdev,
	const char *sub_name)
{
	struct dentry *root;
	struct dentry *file;
	struct mipi_dsi_data *dsi_data;

	dsi_data = platform_get_drvdata(pdev);
	root = msm_fb_get_debugfs_root();
	if (root != NULL) {
		dsi_data->panel_driver_ic_dir =
			debugfs_create_dir(sub_name, root);

		if (IS_ERR(dsi_data->panel_driver_ic_dir) ||
			(dsi_data->panel_driver_ic_dir == NULL)) {
			dev_err(&pdev->dev,
				"debugfs_create_dir fail, error %ld\n",
				PTR_ERR(dsi_data->panel_driver_ic_dir));
		} else {
			file = debugfs_create_file("cmd_seq", 0444,
				dsi_data->panel_driver_ic_dir, dsi_data,
				&mipi_dsi_cmd_seq_fops);
			if (file == NULL)
				dev_err(&pdev->dev,
					"debugfs_create_file: index fail\n");
		}
	}
}

void mipi_dsi_debugfs_exit(struct platform_device *pdev)
{
	struct mipi_dsi_data *dsi_data;

	dsi_data = platform_get_drvdata(pdev);
	debugfs_remove_recursive(dsi_data->panel_driver_ic_dir);
}

#endif /* CONFIG_DEBUG_FS */
