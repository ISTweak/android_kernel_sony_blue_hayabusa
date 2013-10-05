/* drivers/video/msm/mipi_renesas_r63306.c
 *
 * Copyright (C) [2011] Sony Ericsson Mobile Communications AB.
 * Copyright (C) 2012-2013 Sony Mobile Communications AB.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2; as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_dsi_panel.h"

#ifdef CONFIG_FB_MSM_RECOVER_PANEL
#define	NVRW_RETRY		10
#define	NVRW_SEPARATOR_POS	2
#define NVRW_NUM_ONE_PARAM	3
#define NVRW_PANEL_OFF_MSLEEP	100
#endif

static int mipi_r63306_disp_on(struct msm_fb_data_type *mfd)
{
	int ret = 0;
	struct mipi_dsi_data *dsi_data;
	struct dsi_controller *pctrl;

	dsi_data = platform_get_drvdata(mfd->panel_pdev);
	if (!dsi_data || !dsi_data->lcd_power) {
		ret = -ENODEV;
		goto disp_on_fail;
	}
	pctrl = dsi_data->panel->pctrl;

	if (!dsi_data->panel_detecting) {
		ret = dsi_data->lcd_power(TRUE);

		if (ret)
			goto disp_on_fail;

		mipi_dsi_op_mode_config(DSI_CMD_MODE);

		if (pctrl->display_init_cmds) {
			mipi_dsi_buf_init(&dsi_data->tx_buf);
			mipi_dsi_cmds_tx(&dsi_data->tx_buf,
				pctrl->display_init_cmds,
				pctrl->display_init_cmds_size);
		}
		if (dsi_data->eco_mode_on && pctrl->display_on_eco_cmds) {
			mipi_dsi_buf_init(&dsi_data->tx_buf);
			mipi_dsi_cmds_tx(&dsi_data->tx_buf,
				pctrl->display_on_eco_cmds,
				pctrl->display_on_eco_cmds_size);
			dev_info(&mfd->panel_pdev->dev, "ECO MODE ON\n");
		} else {
			mipi_dsi_buf_init(&dsi_data->tx_buf);
			mipi_dsi_cmds_tx(&dsi_data->tx_buf,
				pctrl->display_on_cmds,
				pctrl->display_on_cmds_size);
			dev_info(&mfd->panel_pdev->dev, "ECO MODE OFF\n");
		}
	}

disp_on_fail:
	return ret;
}

static int mipi_r63306_disp_off(struct msm_fb_data_type *mfd)
{
	int ret = 0;
	struct mipi_dsi_data *dsi_data;

	dsi_data = platform_get_drvdata(mfd->panel_pdev);

	if (!dsi_data || !dsi_data->lcd_power)
		return -ENODEV;

	if (!dsi_data->panel_detecting) {
		mipi_dsi_op_mode_config(DSI_CMD_MODE);

		mipi_dsi_buf_init(&dsi_data->tx_buf);
		mipi_dsi_cmds_tx(&dsi_data->tx_buf,
			dsi_data->panel->pctrl->display_off_cmds,
			dsi_data->panel->pctrl->display_off_cmds_size);
		ret = dsi_data->lcd_power(FALSE);
	} else {
		dsi_data->panel_detecting = false;
		ret = 0;
	}

	return ret;
}

static int mipi_r63306_lcd_on(struct platform_device *pdev)
{
	int ret;
	struct msm_fb_data_type *mfd;
	struct mipi_dsi_data *dsi_data;

	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	dsi_data = platform_get_drvdata(mfd->panel_pdev);
	if (dsi_data->panel && dsi_data->panel->plncfg)
		mipi_dsi_update_lane_cfg(dsi_data->panel->plncfg);
	ret = mipi_r63306_disp_on(mfd);
	if (ret)
		dev_err(&pdev->dev, "%s: Display on failed\n", __func__);

	return ret;
}

static int mipi_r63306_lcd_off(struct platform_device *pdev)
{
	int ret;
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	ret = mipi_r63306_disp_off(mfd);
	if (ret)
		dev_err(&pdev->dev, "%s: Display off failed\n", __func__);

	return ret;
}

#ifdef CONFIG_FB_MSM_RECOVER_PANEL
static int mipi_r63306_nvm_override_data(struct msm_fb_data_type *mfd,
		const char *buf, int count)
{
	struct mipi_dsi_data		*dsi_data;
	struct dsi_nvm_rewrite_ctl	*pnvrw_ctl;
	char	work[count + 1];
	char	*pos = work;
	ulong	dat;
	int	i, n;
	int	rc;
	int	cnt = 0;

	dsi_data = platform_get_drvdata(mfd->panel_pdev);
	if (!dsi_data->panel->pnvrw_ctl)
		return 0;
	pnvrw_ctl = dsi_data->panel->pnvrw_ctl;
	if (count < (NVRW_NUM_E7_PARAM + NVRW_NUM_DE_PARAM)
			* NVRW_NUM_ONE_PARAM - 1)
		return 0;
	memcpy(work, buf, count);
	work[count] = 0;

	/* override E7 register data */
	for (i = 0; i < pnvrw_ctl->nvm_write_rsp_size; i++) {
		if (pnvrw_ctl->nvm_write_rsp[i].payload[0] == 0xE7)
			break;
	}
	if (i >= pnvrw_ctl->nvm_write_rsp_size)
		return 0;
	for (n = 0; n < NVRW_NUM_E7_PARAM;
			n++, pos += NVRW_NUM_ONE_PARAM, cnt++) {
		*(pos + NVRW_SEPARATOR_POS) = 0;
		rc = strict_strtoul(pos, 16, &dat);
		if (rc < 0)
			return 0;
		pnvrw_ctl->nvm_write_rsp[i].payload[n + 1] = dat & 0xff;
	}

	/* override DE register data */
	for (i = 0; i < pnvrw_ctl->nvm_write_user_size; i++) {
		if (pnvrw_ctl->nvm_write_user[i].payload[0] == 0xDE)
			break;
	}
	if (i >= pnvrw_ctl->nvm_write_user_size)
		return 0;
	for (n = 0; n < NVRW_NUM_DE_PARAM;
			n++, pos += NVRW_NUM_ONE_PARAM, cnt++) {
		*(pos + NVRW_SEPARATOR_POS) = 0;
		rc = strict_strtoul(pos, 16, &dat);
		if (rc < 0)
			return 0;
		pnvrw_ctl->nvm_write_user[i].payload[n + 1] = dat & 0xff;
	}

	return cnt;
}

static int mipi_r63306_nvm_read(struct msm_fb_data_type *mfd, char *buf)
{
	struct mipi_dsi_data		*dsi_data;
	struct dsi_nvm_rewrite_ctl	*pnvrw_ctl;
	int	n;
	int	len = 0;
	char	*pos = buf;

	dsi_data = platform_get_drvdata(mfd->panel_pdev);
	pnvrw_ctl = dsi_data->panel->pnvrw_ctl;
	mipi_dsi_cmds_tx(&dsi_data->tx_buf,
		pnvrw_ctl->nvm_mcap, pnvrw_ctl->nvm_mcap_size);

	/* E7 register data */
	mipi_dsi_cmds_rx(mfd, &dsi_data->tx_buf, &dsi_data->rx_buf,
		&pnvrw_ctl->nvm_read[0], NVRW_NUM_E7_PARAM);
	for (n = 0; n < NVRW_NUM_E7_PARAM; n++)
		len += snprintf(pos + len, PAGE_SIZE - len, "%02x ",
				dsi_data->rx_buf.data[n]);

	/* DE register data */
	mipi_dsi_cmds_rx(mfd, &dsi_data->tx_buf, &dsi_data->rx_buf,
		&pnvrw_ctl->nvm_read[1], NVRW_NUM_DE_PARAM);
	for (n = 0; n < NVRW_NUM_DE_PARAM; n++) {
		if (2 <= n && n <= 3)
			len += snprintf(pos + len, PAGE_SIZE - len, "%02x ",
					dsi_data->rx_buf.data[n]);
		else
			len += snprintf(pos + len, PAGE_SIZE - len, "00 ");
	}
	*(pos + len) = 0;

	mipi_dsi_cmds_tx(&dsi_data->tx_buf,
		pnvrw_ctl->nvm_mcap_lock, pnvrw_ctl->nvm_mcap_lock_size);

	return len;
}

static int mipi_r63306_nvm_erase(struct msm_fb_data_type *mfd)
{
	struct mipi_dsi_data		*dsi_data;
	struct dsi_nvm_rewrite_ctl	*pnvrw_ctl;
	int	i;

	dsi_data = platform_get_drvdata(mfd->panel_pdev);
	pnvrw_ctl = dsi_data->panel->pnvrw_ctl;

	mipi_dsi_cmds_tx(&dsi_data->tx_buf,
		pnvrw_ctl->nvm_disp_off, pnvrw_ctl->nvm_disp_off_size);
	mipi_dsi_cmds_tx(&dsi_data->tx_buf,
		pnvrw_ctl->nvm_mcap, pnvrw_ctl->nvm_mcap_size);

	for (i = 0; i < NVRW_RETRY; i++) {
		mipi_dsi_cmds_tx(&dsi_data->tx_buf,
			pnvrw_ctl->nvm_open, pnvrw_ctl->nvm_open_size);
		mipi_dsi_cmds_tx(&dsi_data->tx_buf,
			pnvrw_ctl->nvm_write_rsp,
			pnvrw_ctl->nvm_write_rsp_size);
		mipi_dsi_cmds_tx(&dsi_data->tx_buf,
			pnvrw_ctl->nvm_write_user,
			pnvrw_ctl->nvm_write_user_size);

		mipi_dsi_cmds_tx(&dsi_data->tx_buf,
			pnvrw_ctl->nvm_erase, pnvrw_ctl->nvm_erase_size);
		mipi_dsi_cmds_rx(mfd, &dsi_data->tx_buf, &dsi_data->rx_buf,
			pnvrw_ctl->nvm_erase_res, 1);
		if (dsi_data->rx_buf.data[0] == 0xBD) {
			mipi_dsi_cmds_tx(&dsi_data->tx_buf,
				pnvrw_ctl->nvm_disp_off,
				pnvrw_ctl->nvm_disp_off_size);
			pr_info("%s (%d): RETRY %d", __func__, __LINE__, i+1);
			dsi_data->nvrw_retry_cnt++;
			continue;
		}

		mipi_dsi_cmds_tx(&dsi_data->tx_buf,
			pnvrw_ctl->nvm_close, pnvrw_ctl->nvm_close_size);
		mipi_dsi_cmds_rx(mfd, &dsi_data->tx_buf, &dsi_data->rx_buf,
			pnvrw_ctl->nvm_status, 1);
		if (dsi_data->rx_buf.data[0] == 0x00) {
			mipi_dsi_cmds_tx(&dsi_data->tx_buf,
				pnvrw_ctl->nvm_disp_off,
				pnvrw_ctl->nvm_disp_off_size);
			pr_info("%s (%d): RETRY %d", __func__, __LINE__, i+1);
			dsi_data->nvrw_retry_cnt++;
			continue;
		}
		break;
	}
	if (i >= NVRW_RETRY)
		return 0;
	mipi_dsi_cmds_tx(&dsi_data->tx_buf,
		pnvrw_ctl->nvm_mcap_lock, pnvrw_ctl->nvm_mcap_lock_size);

	dsi_data->lcd_power(0);
	dsi_data->vreg_power(0);
	msleep(NVRW_PANEL_OFF_MSLEEP);

	return 1;
}
static int mipi_r63306_nvm_rsp_write(struct msm_fb_data_type *mfd)
{
	struct mipi_dsi_data		*dsi_data;
	struct dsi_nvm_rewrite_ctl	*pnvrw_ctl;
	int	i;

	dsi_data = platform_get_drvdata(mfd->panel_pdev);
	pnvrw_ctl = dsi_data->panel->pnvrw_ctl;

	dsi_data->vreg_power(1);
	dsi_data->lcd_power(1);

	mipi_dsi_cmds_tx(&dsi_data->tx_buf,
		pnvrw_ctl->nvm_mcap, pnvrw_ctl->nvm_mcap_size);
	for (i = 0; i < NVRW_RETRY; i++) {
		mipi_dsi_cmds_tx(&dsi_data->tx_buf,
			pnvrw_ctl->nvm_open, pnvrw_ctl->nvm_open_size);
		mipi_dsi_cmds_tx(&dsi_data->tx_buf,
			pnvrw_ctl->nvm_write_rsp,
			pnvrw_ctl->nvm_write_rsp_size);
		mipi_dsi_cmds_tx(&dsi_data->tx_buf,
			pnvrw_ctl->nvm_flash_rsp,
			pnvrw_ctl->nvm_flash_rsp_size);

		mipi_dsi_cmds_tx(&dsi_data->tx_buf,
			pnvrw_ctl->nvm_close, pnvrw_ctl->nvm_close_size);
		mipi_dsi_cmds_rx(mfd, &dsi_data->tx_buf, &dsi_data->rx_buf,
			pnvrw_ctl->nvm_status, 1);
		if (dsi_data->rx_buf.data[0] == 0x00) {
			pr_info("%s (%d): RETRY %d", __func__, __LINE__, i+1);
			dsi_data->nvrw_retry_cnt++;
			continue;
		}
		break;
	}
	if (i >= NVRW_RETRY)
		return 0;
	mipi_dsi_cmds_tx(&dsi_data->tx_buf,
		pnvrw_ctl->nvm_mcap_lock, pnvrw_ctl->nvm_mcap_lock_size);

	dsi_data->lcd_power(0);
	dsi_data->vreg_power(0);
	msleep(NVRW_PANEL_OFF_MSLEEP);

	return 1;
}

static int mipi_r63306_nvm_user_write(struct msm_fb_data_type *mfd)
{
	struct mipi_dsi_data		*dsi_data;
	struct dsi_nvm_rewrite_ctl	*pnvrw_ctl;

	dsi_data = platform_get_drvdata(mfd->panel_pdev);
	pnvrw_ctl = dsi_data->panel->pnvrw_ctl;

	dsi_data->vreg_power(1);
	dsi_data->lcd_power(1);

	mipi_dsi_cmds_tx(&dsi_data->tx_buf,
		pnvrw_ctl->nvm_mcap, pnvrw_ctl->nvm_mcap_size);

	mipi_dsi_cmds_tx(&dsi_data->tx_buf,
		pnvrw_ctl->nvm_write_user,
		pnvrw_ctl->nvm_write_user_size);
	mipi_dsi_cmds_tx(&dsi_data->tx_buf,
		pnvrw_ctl->nvm_flash_user,
		pnvrw_ctl->nvm_flash_user_size);

	dsi_data->lcd_power(0);
	dsi_data->vreg_power(0);
	msleep(NVRW_PANEL_OFF_MSLEEP);

	dsi_data->vreg_power(1);
	dsi_data->lcd_power(1);
	mipi_r63306_lcd_on(mfd->pdev);

	return 1;
}
#endif

static int __devexit mipi_r63306_lcd_remove(struct platform_device *pdev)
{
	struct mipi_dsi_data *dsi_data;

	dsi_data = platform_get_drvdata(pdev);
	if (!dsi_data)
		return -ENODEV;

#ifdef CONFIG_DEBUG_FS
	mipi_dsi_debugfs_exit(pdev);
#endif
#ifdef CONFIG_FB_MSM_RECOVER_PANEL
	drv_ic_sysfs_unregister(&pdev->dev);
#endif
	platform_set_drvdata(pdev, NULL);
	mipi_dsi_buf_release(&dsi_data->tx_buf);
	mipi_dsi_buf_release(&dsi_data->rx_buf);
	kfree(dsi_data);

	return 0;
}

static int __devinit mipi_r63306_lcd_probe(struct platform_device *pdev)
{
	int ret;
	struct lcd_panel_platform_data *platform_data;
	struct mipi_dsi_data *dsi_data;
	struct platform_device *fb_pdev;

	platform_data = pdev->dev.platform_data;
	if (platform_data == NULL)
		return -EINVAL;

	dsi_data = kzalloc(sizeof(struct mipi_dsi_data), GFP_KERNEL);
	if (dsi_data == NULL)
		return -ENOMEM;

	dsi_data->panel_data.on = mipi_r63306_lcd_on;
	dsi_data->panel_data.off = mipi_r63306_lcd_off;
	dsi_data->default_panels = platform_data->default_panels;
	dsi_data->panels = platform_data->panels;
	dsi_data->lcd_power = platform_data->lcd_power;
	dsi_data->lcd_reset = platform_data->lcd_reset;
	dsi_data->vreg_power = platform_data->vreg_power;
	dsi_data->eco_mode_switch = mipi_dsi_eco_mode_switch;
	if (mipi_dsi_need_detect_panel(dsi_data->panels)) {
		dsi_data->panel_data.panel_detect = mipi_dsi_detect_panel;
		dsi_data->panel_data.update_panel = mipi_dsi_update_panel;
		dsi_data->panel_detecting = true;
	} else {
		dev_info(&pdev->dev, "no need to detect panel\n");
	}
#ifdef CONFIG_FB_MSM_RECOVER_PANEL
	dsi_data->override_nvm_data = mipi_r63306_nvm_override_data;
	dsi_data->seq_nvm_read = mipi_r63306_nvm_read;
	dsi_data->seq_nvm_erase = mipi_r63306_nvm_erase;
	dsi_data->seq_nvm_rsp_write = mipi_r63306_nvm_rsp_write;
	dsi_data->seq_nvm_user_write = mipi_r63306_nvm_user_write;
#endif
	ret = mipi_dsi_buf_alloc(&dsi_data->tx_buf, DSI_BUF_SIZE);
	if (ret <= 0) {
		dev_err(&pdev->dev, "mipi_dsi_buf_alloc(tx) failed!\n");
		goto err_dsibuf_free;
	}

	ret = mipi_dsi_buf_alloc(&dsi_data->rx_buf, DSI_BUF_SIZE);
	if (ret <= 0) {
		dev_err(&pdev->dev, "mipi_dsi_buf_alloc(rx) failed!\n");
		goto err_txbuf_free;
	}

	platform_set_drvdata(pdev, dsi_data);

	mipi_dsi_set_default_panel(dsi_data);
	ret = platform_device_add_data(pdev, &dsi_data->panel_data,
		sizeof(dsi_data->panel_data));
	if (ret) {
		dev_err(&pdev->dev,
			"platform_device_add_data failed!\n");
		goto err_rxbuf_free;
	}
	fb_pdev = msm_fb_add_device(pdev);
#ifdef CONFIG_FB_MSM_PANEL_ECO_MODE
	eco_mode_sysfs_register(&fb_pdev->dev);
#endif
#ifdef CONFIG_FB_MSM_RECOVER_PANEL
	dsi_data->nvrw_ic_vendor = NVRW_DRV_RENESAS;
	drv_ic_sysfs_register(&fb_pdev->dev);
#endif
#ifdef CONFIG_DEBUG_FS
	mipi_dsi_debugfs_init(pdev, "mipi_r63306");
#endif

	return 0;
err_rxbuf_free:
	mipi_dsi_buf_release(&dsi_data->rx_buf);
err_txbuf_free:
	mipi_dsi_buf_release(&dsi_data->tx_buf);
err_dsibuf_free:
	kfree(dsi_data);
	return ret;
}

static struct platform_driver this_driver = {
	.probe  = mipi_r63306_lcd_probe,
	.remove = mipi_r63306_lcd_remove,
	.driver = {
		.name   = "mipi_renesas_r63306",
	},
};

static int __init mipi_r63306_lcd_init(void)
{
	return platform_driver_register(&this_driver);
}

static void __exit mipi_r63306_lcd_exit(void)
{
	platform_driver_unregister(&this_driver);
}

module_init(mipi_r63306_lcd_init);
module_exit(mipi_r63306_lcd_exit);

