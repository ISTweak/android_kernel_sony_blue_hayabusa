/* drivers/video/msm/mipi_dsi_panel.h
 *
 * Copyright (C) 2010 Sony Ericsson Mobile Communications AB.
 * Copyright (C) 2012 Sony Mobile Communications AB.
 * Author: Yosuke Hatanaka <yosuke.hatanaka@sonyericsson.com>
 * Copyright (C) 2012-2013 Sony Mobile Communications AB.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */


#ifndef MIPI_DSI_PANEL_H
#define MIPI_DSI_PANEL_H

#include <linux/types.h>
#include "mipi_dsi.h"

#define ONE_FRAME_TRANSMIT_WAIT_MS 20

#ifdef CONFIG_FB_MSM_RECOVER_PANEL
enum {
	NVRW_DRV_NONE,
	NVRW_DRV_RENESAS,
	NVRW_DRV_SAMSUNG,
	NVRW_DRV_NOVATEK
};
#define NVRW_NUM_E7_PARAM	4
#define NVRW_NUM_DE_PARAM	12
#endif

struct dsi_controller {
	struct msm_panel_info *(*get_panel_info) (void);
	struct dsi_cmd_desc *display_init_cmds;
	struct dsi_cmd_desc *display_on_eco_cmds;
	struct dsi_cmd_desc *display_on_cmds;
	struct dsi_cmd_desc *display_off_cmds;
	struct dsi_cmd_desc *read_id_cmds;
	struct dsi_cmd_desc *eco_mode_gamma_cmds;
	struct dsi_cmd_desc *normal_gamma_cmds;
	int display_init_cmds_size;
	int display_on_eco_cmds_size;
	int display_on_cmds_size;
	int display_off_cmds_size;
	int eco_mode_gamma_cmds_size;
	int normal_gamma_cmds_size;
};

#ifdef CONFIG_FB_MSM_RECOVER_PANEL
struct dsi_nvm_rewrite_ctl {
	struct dsi_cmd_desc *nvm_disp_off;
	struct dsi_cmd_desc *nvm_mcap;
	struct dsi_cmd_desc *nvm_mcap_lock;
	struct dsi_cmd_desc *nvm_open;
	struct dsi_cmd_desc *nvm_close;
	struct dsi_cmd_desc *nvm_status;
	struct dsi_cmd_desc *nvm_erase;
	struct dsi_cmd_desc *nvm_erase_res;
	struct dsi_cmd_desc *nvm_read;
	struct dsi_cmd_desc *nvm_write_rsp;
	struct dsi_cmd_desc *nvm_flash_rsp;
	struct dsi_cmd_desc *nvm_write_user;
	struct dsi_cmd_desc *nvm_flash_user;

	int nvm_disp_off_size;
	int nvm_mcap_size;
	int nvm_mcap_lock_size;
	int nvm_open_size;
	int nvm_close_size;
	int nvm_erase_size;
	int nvm_read_size;
	int nvm_write_rsp_size;
	int nvm_flash_rsp_size;
	int nvm_write_user_size;
	int nvm_flash_user_size;
};
#endif

struct mipi_dsi_lane_cfg {
	uint32_t	ln_cfg[4][3];
	uint32_t	ln_dpath[4];
	uint32_t	ln_str[4][2];
	uint32_t	lnck_cfg[3];
	uint32_t	lnck_dpath;
	uint32_t	lnck_str[2];
};

struct panel_id {
	const char			*name;
	struct dsi_controller		*pctrl;
#ifdef CONFIG_FB_MSM_RECOVER_PANEL
	struct dsi_nvm_rewrite_ctl	*pnvrw_ctl;
#endif
	const u32			width;	/* in mm */
	const u32			height;	/* in mm */
	const char			*id;
	const int			id_num;
	const struct mipi_dsi_lane_cfg	*plncfg;
};

struct mipi_dsi_data {
	struct dsi_buf tx_buf;
	struct dsi_buf rx_buf;
	struct msm_fb_panel_data panel_data;
	const struct panel_id *panel;
	const struct panel_id **default_panels;
	const struct panel_id **panels;
	int (*lcd_power)(int on);
	int (*lcd_reset)(int on);
	int (*vreg_power)(int on);
	int (*eco_mode_switch)(struct msm_fb_data_type *mfd);
#ifdef CONFIG_DEBUG_FS
	struct dentry *panel_driver_ic_dir;
	char *debug_buf;
#endif
	int panel_detecting;
	int eco_mode_on;

#ifdef CONFIG_FB_MSM_RECOVER_PANEL
	int	nvrw_ic_vendor;
	bool	nvrw_panel_detective;
	int	nvrw_result;
	int	nvrw_retry_cnt;

	int	(*override_nvm_data)(struct msm_fb_data_type *mfd,
					const char *buf, int count);
	int	(*seq_nvm_read)(struct msm_fb_data_type *mfd, char *buf);
	int	(*seq_nvm_erase)(struct msm_fb_data_type *mfd);
	int	(*seq_nvm_rsp_write)(struct msm_fb_data_type *mfd);
	int	(*seq_nvm_user_write)(struct msm_fb_data_type *mfd);
#endif
};

void mipi_dsi_set_default_panel(struct mipi_dsi_data *dsi_data);
struct msm_panel_info *mipi_dsi_detect_panel(
	struct msm_fb_data_type *mfd);
int __devinit mipi_dsi_need_detect_panel(
	const struct panel_id **panels);
int mipi_dsi_update_panel(struct platform_device *pdev);
int mipi_dsi_eco_mode_switch(struct msm_fb_data_type *mfd);
void mipi_dsi_update_lane_cfg(const struct mipi_dsi_lane_cfg *plncfg);

int eco_mode_sysfs_register(struct device *dev);

#ifdef CONFIG_FB_MSM_RECOVER_PANEL
int drv_ic_sysfs_register(struct device *dev);
void drv_ic_sysfs_unregister(struct device *dev);
#endif

#ifdef CONFIG_DEBUG_FS
void mipi_dsi_debugfs_init(struct platform_device *pdev,
	const char *sub_name);
void mipi_dsi_debugfs_exit(struct platform_device *pdev);
#endif /* CONFIG_DEBUG_FS */

#endif /* MIPI_DSI_PANEL_H */
