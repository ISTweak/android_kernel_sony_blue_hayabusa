#ifdef CONFIG_LEDS_AS3676
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/leds-as3676.h>

struct as3676_platform_data as3676_platform_data = {
	.step_up_vtuning = 17,	/* 0 .. 31 uA on DCDC_FB */
	.audio_speed_down = 1,	/* 0..3 resp. 0, 200, 400, 800ms */
	.audio_speed_up = 4,	/* 0..7 resp. 0, 50, 100, 150,
					200,250,400, 800ms */
	.audio_agc_ctrl = 1,	/* 0 .. 7: 0==no AGC, 7 very aggressive*/
	.audio_gain = 7,	/* 0..7: -12, -6,  0, 6
					12, 18, 24, 30 dB */
	.audio_source = 2,	/* 0..3: 0=curr33, 1=DCDC_FB
					2=GPIO1,  3=GPIO2 */
	.step_up_lowcur = true,
	.reset_on_i2c_shutdown = true,
	.caps_mounted_on_dcdc_feedback = 1,
	.cp_control = 0x40,
	.leds[0] = {
		.name = "lcd-backlight_1",
		.on_charge_pump = 0,
		.max_current_uA = 19950,
		.startup_current_uA = 19950,
		.use_dls = true,
	},
	.leds[1] = {
		.name = "lcd-backlight_2",
		.on_charge_pump = 0,
		.max_current_uA = 19950,
		.startup_current_uA = 19950,
		.use_dls = true,
	},
	.leds[2] = {
		.name = "led3-not-connected",
		.on_charge_pump = 0,
		.max_current_uA = 0,
	},
	.leds[3] = {
		.name = "led4-not-connected",
		.on_charge_pump = 0,
		.max_current_uA = 0,
	},
	.leds[4] = {
		.name = "led5-not-connected",
		.on_charge_pump = 0,
		.max_current_uA = 0,
	},
	.leds[5] = {
		.name = "led6-not-connected",
		.on_charge_pump = 0,
		.max_current_uA = 0,
	},
	.leds[6] = {
		.name = "pwr-red",
		.on_charge_pump = 1,
		.max_current_uA = 3000,
	},
	.leds[7] = {
		.name = "pwr-green",
		.on_charge_pump = 1,
		.max_current_uA = 2400,
	},
	.leds[8] = {
		.name = "pwr-blue",
		.on_charge_pump = 1,
		.max_current_uA = 2400,
	},
	.leds[9] = {
		.name = "torch_1",
		.on_charge_pump = 1,
		.max_current_uA = 24900,
	},
	.leds[10] = {
		.name = "torch_2",
		.on_charge_pump = 1,
		.max_current_uA = 24900,
	},
	.leds[11] = {
		.name = "led12-not-connected",
		.on_charge_pump = 1,
		.max_current_uA = 0,
	},
	.leds[12] = {
		.name = "led13-not-connected",
		.on_charge_pump = 1,
		.max_current_uA = 0,
	},
};

#define VBUS_BIT 0x04
#define WALL_BIT (1 << (14))
static int __init startup_rgb(char *str)
{
	int vbus;
	if (get_option(&str, &vbus)) {
		if (vbus & (VBUS_BIT|WALL_BIT))
			as3676_platform_data.leds[6].startup_current_uA = 3000;
		return 0;
	}
	return -EINVAL;
}

early_param("startup", startup_rgb);

#endif
