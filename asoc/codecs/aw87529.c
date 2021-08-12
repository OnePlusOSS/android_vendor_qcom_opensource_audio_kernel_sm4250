/*
* w87529_audio.c   aw87529 pa module
*
* Version: v1.0.7
*
* Copyright (c) 2019 AWINIC Technology CO., LTD
*
*  Author: hushanping <hushanping@awinic.com.cn>
*
* This program is free software; you can redistribute  it and/or modify it
* under  the terms of  the GNU General  Public License as published by the
* Free Software Foundation;  either version 2 of the  License, or (at your
* option) any later version.
*/

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/gameport.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include "aw87529.h"

#define D(x...) pr_debug("[PA][AW87529] " x)
#define I(x...) pr_info("[PA][AW87529] " x)
#define E(x...) pr_err("[PA][AW87529] " x)
/*******************************************************************************
* aw87529 marco
******************************************************************************/
#define AW87529_I2C_NAME	"aw87529_pa"
#define AW87529_DRIVER_VERSION	"v1.0.7"
/*******************************************************************************
* aw87529 variable
******************************************************************************/
struct aw87529 *aw87529;

struct aw87529_container *aw87529_kspk_cnt;
struct aw87529_container *aw87529_drcv_cnt;
struct aw87529_container *aw87529_hvload_cnt;
struct aw87529_container *aw87529_expvdd_cnt;
struct aw87529_container *aw87529_hph_cnt;
struct aw87529_container *aw87529_hphkspk_cnt;

static char *aw87529_kspk_name = "aw87529_kspk.bin";
static char *aw87529_drcv_name = "aw87529_drcv.bin";
static char *aw87529_hvload_name = "aw87529_hvload.bin";
static char *aw87529_expvdd_name = "aw87529_expvdd.bin";
static char *aw87529_hph_name = "aw87529_hph.bin";
static char *aw87529_hphkspk_name = "aw87529_hphkspk.bin";

unsigned int kspk_load_cont;
unsigned int drcv_load_cont;
unsigned int hvload_load_cont;
unsigned int expvdd_load_cont;
unsigned int hph_load_cont;
unsigned int hphkspk_load_cont;
/*****************************************************************************
* i2c write and read
****************************************************************************/
static int aw87529_i2c_write(struct aw87529 *aw87529,
			     unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_write_byte_data(aw87529->i2c_client,
						reg_addr, reg_data);
		if (ret < 0) {
			E("%s: i2c write error, cnt = %d ret = %d\n",
			       __func__, cnt, ret);
		} else {
			break;
		}
		cnt++;
		usleep_range(2000, 2500);
	}

	return ret;
}

static int aw87529_i2c_read(struct aw87529 *aw87529,
			    unsigned char reg_addr, unsigned char *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(aw87529->i2c_client, reg_addr);
		if (ret < 0) {
			E("%s: i2c read error, cnt = %d ret = %d\n",
			       __func__, cnt, ret);
		} else {
			*reg_data = ret;
			break;
		}
		cnt++;
		usleep_range(2000, 2500);
	}

	return ret;
}
/****************************************************************************
* aw87529 hardware control
***************************************************************************/
unsigned int aw87529_hw_on(struct aw87529 *aw87529)
{
	D("%s\n", __func__);

	if (aw87529 && gpio_is_valid(aw87529->reset_gpio)) {
		gpio_set_value_cansleep(aw87529->reset_gpio, 0);
		usleep_range(2000, 2500);
		gpio_set_value_cansleep(aw87529->reset_gpio, 1);
		usleep_range(2000, 2500);
		aw87529->hwen_flag = 1;
		aw87529_i2c_write(aw87529, 0x64, 0x2C);
	} else {
		E("%s: failed\n", __func__);
	}

	return 0;
}

unsigned int aw87529_hw_off(struct aw87529 *aw87529)
{
	D("%s\n", __func__);

	if (aw87529 && gpio_is_valid(aw87529->reset_gpio)) {
		gpio_set_value_cansleep(aw87529->reset_gpio, 0);
		usleep_range(2000, 2500);
		aw87529->hwen_flag = 0;
	} else {
		E("%s: failed\n", __func__);
	}
	return 0;
}
/*******************************************************************************
* aw87529 control interface
******************************************************************************/
unsigned char aw87529_audio_drcv(void)
{
	unsigned int i;
	unsigned int length;

	D("%s\n", __func__);
	if (aw87529 == NULL)
		return 2;

	if (!aw87529->hwen_flag)
		aw87529_hw_on(aw87529);

	length = sizeof(aw87529_drcv_cfg_default) / sizeof(char);
	if (aw87529->drcv_cfg_update_flag == 0) {	/*update array data */
		for (i = 0; i < length; i = i + 2)
			aw87529_i2c_write(aw87529,
					  aw87529_drcv_cfg_default[i],
					  aw87529_drcv_cfg_default[i + 1]);
	}
	if (aw87529->drcv_cfg_update_flag == 1) {	/*update bin data */
		for (i = 0; i < aw87529_drcv_cnt->len; i = i + 2)
			aw87529_i2c_write(aw87529,
					  aw87529_drcv_cnt->data[i],
					  aw87529_drcv_cnt->data[i + 1]);
	}

	return 0;
}

unsigned char aw87529_audio_kspk(void)
{
	unsigned int i;
	unsigned int length;

	D("%s\n", __func__);

	if (aw87529 == NULL)
		return 2;

	if (!aw87529->hwen_flag)
		aw87529_hw_on(aw87529);

	length = sizeof(aw87529_kspk_cfg_default) / sizeof(char);
	if (aw87529->kspk_cfg_update_flag == 0) {	/*send array data */
		for (i = 0; i < length; i = i + 2)
			aw87529_i2c_write(aw87529,
					  aw87529_kspk_cfg_default[i],
					  aw87529_kspk_cfg_default[i + 1]);
	}
	if (aw87529->kspk_cfg_update_flag == 1) {	/*send bin data */
		for (i = 0; i < aw87529_kspk_cnt->len; i = i + 2)
			aw87529_i2c_write(aw87529,
					  aw87529_kspk_cnt->data[i],
					  aw87529_kspk_cnt->data[i + 1]);
	}

	return 0;
}

unsigned char aw87529_audio_hvload(void)
{
	unsigned int i;
	unsigned int length;

	D("%s\n", __func__);

	if (aw87529 == NULL)
		return 2;

	if (!aw87529->hwen_flag)
		aw87529_hw_on(aw87529);

	length = sizeof(aw87529_hvload_cfg_default) / sizeof(char);
	if (aw87529->hvload_cfg_update_flag == 0) {	/*send array data */
		for (i = 0; i < length; i = i + 2)
			aw87529_i2c_write(aw87529,
					  aw87529_hvload_cfg_default[i],
					  aw87529_hvload_cfg_default[i + 1]);
	}
	if (aw87529->hvload_cfg_update_flag == 1) {	/*send bin data */
		for (i = 0; i < aw87529_hvload_cnt->len; i = i + 2)
			aw87529_i2c_write(aw87529,
					  aw87529_hvload_cnt->data[i],
					  aw87529_hvload_cnt->data[i + 1]);
	}

	return 0;
}

unsigned char aw87529_audio_hphkspk(void)
{
	unsigned int i;
	unsigned int length;

	D("%s\n", __func__);

	if (aw87529 == NULL)
		return 2;

	if (!aw87529->hwen_flag)
		aw87529_hw_on(aw87529);

	length = sizeof(aw87529_hphkspk_cfg_default) / sizeof(char);
	if (aw87529->hphkspk_cfg_update_flag == 0) {	/*send array data */
		for (i = 0; i < length; i = i + 2)
			aw87529_i2c_write(aw87529,
					  aw87529_hphkspk_cfg_default[i],
					  aw87529_hphkspk_cfg_default[i + 1]);
	}
	if (aw87529->hphkspk_cfg_update_flag == 1) {	/*send bin data */
		for (i = 0; i < aw87529_hphkspk_cnt->len; i = i + 2)
			aw87529_i2c_write(aw87529,
					  aw87529_hphkspk_cnt->data[i],
					  aw87529_hphkspk_cnt->data[i + 1]);
	}

	return 0;
}

unsigned char aw87529_audio_hph(void)
{
	unsigned int i;
	unsigned int length;

	D("%s\n", __func__);

	if (aw87529 == NULL)
		return 2;

	if (!aw87529->hwen_flag)
		aw87529_hw_on(aw87529);

	length = sizeof(aw87529_hph_cfg_default) / sizeof(char);
	if (aw87529->hph_cfg_update_flag == 0) {	/*send array data */
		for (i = 0; i < length; i = i + 2)
			aw87529_i2c_write(aw87529,
					  aw87529_hph_cfg_default[i],
					  aw87529_hph_cfg_default[i + 1]);
	}
	if (aw87529->hph_cfg_update_flag == 1) {	/*send bin data */
		for (i = 0; i < aw87529_hph_cnt->len; i = i + 2)
			aw87529_i2c_write(aw87529,
					  aw87529_hph_cnt->data[i],
					  aw87529_hph_cnt->data[i + 1]);
	}

	return 0;
}

unsigned char aw87529_audio_expvdd(void)
{
	unsigned int i;
	unsigned int length;

	D("%s\n", __func__);

	if (aw87529 == NULL)
		return 2;

	if (!aw87529->hwen_flag)
		aw87529_hw_on(aw87529);

	length = sizeof(aw87529_expvdd_cfg_default) / sizeof(char);
	if (aw87529->expvdd_cfg_update_flag == 0) {	/*send array data */
		for (i = 0; i < length; i = i + 2)
			aw87529_i2c_write(aw87529,
					  aw87529_expvdd_cfg_default[i],
					  aw87529_expvdd_cfg_default[i + 1]);
	}
	if (aw87529->expvdd_cfg_update_flag == 1) {	/*send bin data */
		for (i = 0; i < aw87529_expvdd_cnt->len; i = i + 2)
			aw87529_i2c_write(aw87529,
					  aw87529_expvdd_cnt->data[i],
					  aw87529_expvdd_cnt->data[i + 1]);
	}

	return 0;
}

unsigned char aw87529_audio_off(void)
{
	D("%s\n", __func__);
	if (aw87529 == NULL)
		return 2;

	if (aw87529->hwen_flag)
		aw87529_i2c_write(aw87529, 0x01, 0x00);	/*CHIP Disable */
	aw87529_hw_off(aw87529);

	return 0;
}
/*******************************************************************************
* aw87529 firmware cfg update
******************************************************************************/
static void aw87529_hphkspk_cfg_loaded(const struct firmware *cont,
				       void *context)
{
	int i = 0;
	int ram_timer_val = 2000;

	D("%s\n", __func__);
	hphkspk_load_cont++;
	if (!cont) {
		E("%s: failed to read %s\n", __func__,
		       aw87529_hphkspk_name);
		release_firmware(cont);
		if (hphkspk_load_cont <= 2) {
			schedule_delayed_work(&aw87529->ram_work,
					      msecs_to_jiffies(ram_timer_val));
			I("%s: restart hrtimer to load firmware\n",
				__func__);
		}
		return;
	}

	I("%s: loaded %s - size: %zu\n", __func__, aw87529_hphkspk_name,
		cont ? cont->size : 0);

	for (i = 0; i < cont->size; i = i + 2)
		I("%s: addr:0x%04x, data:0x%02x\n",
			__func__, *(cont->data + i), *(cont->data + i + 1));

	/* aw87529 ram update */
	aw87529_hphkspk_cnt = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!aw87529_hphkspk_cnt) {
		release_firmware(cont);
		E("%s: Error allocating memory\n", __func__);
		return;
	}
	aw87529_hphkspk_cnt->len = cont->size;
	memcpy(aw87529_hphkspk_cnt->data, cont->data, cont->size);
	release_firmware(cont);
	aw87529->hphkspk_cfg_update_flag = 1;

	I("%s: fw update complete\n", __func__);
}

static int aw87529_hphkspk_update(struct aw87529 *aw87529)
{
	D("%s\n", __func__);
	return request_firmware_nowait(THIS_MODULE,
				       FW_ACTION_HOTPLUG,
				       aw87529_hphkspk_name,
				       &aw87529->i2c_client->dev,
				       GFP_KERNEL,
				       aw87529, aw87529_hphkspk_cfg_loaded);
}

static void aw87529_hph_cfg_loaded(const struct firmware *cont, void *context)
{
	int i = 0;
	int ram_timer_val = 2000;

	D("%s\n", __func__);
	hph_load_cont++;
	if (!cont) {
		E("%s: failed to read %s\n", __func__, aw87529_hph_name);
		release_firmware(cont);
		if (hph_load_cont <= 2) {
			schedule_delayed_work(&aw87529->ram_work,
					      msecs_to_jiffies(ram_timer_val));
			I("%s: restart hrtimer to load firmware\n",
				__func__);
		}
		return;
	}

	I("%s: loaded %s - size: %zu\n", __func__, aw87529_hph_name,
		cont ? cont->size : 0);

	for (i = 0; i < cont->size; i = i + 2)
		I("%s: addr:0x%04x, data:0x%02x\n",
			__func__, *(cont->data + i), *(cont->data + i + 1));

	/* aw87529 ram update */
	aw87529_hph_cnt = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!aw87529_hph_cnt) {
		release_firmware(cont);
		E("%s: Error allocating memory\n", __func__);
		return;
	}
	aw87529_hph_cnt->len = cont->size;
	memcpy(aw87529_hph_cnt->data, cont->data, cont->size);
	release_firmware(cont);
	aw87529->hph_cfg_update_flag = 1;

	I("%s: fw update complete\n", __func__);
}

static int aw87529_hph_update(struct aw87529 *aw87529)
{
	D("%s\n", __func__);
	return request_firmware_nowait(THIS_MODULE,
				       FW_ACTION_HOTPLUG,
				       aw87529_hph_name,
				       &aw87529->i2c_client->dev,
				       GFP_KERNEL,
				       aw87529, aw87529_hph_cfg_loaded);
}

static void aw87529_expvdd_cfg_loaded(const struct firmware *cont,
				      void *context)
{
	int i = 0;
	int ram_timer_val = 2000;

	D("%s\n", __func__);
	expvdd_load_cont++;
	if (!cont) {
		E("%s: failed to read %s\n", __func__,
		       aw87529_expvdd_name);
		release_firmware(cont);
		if (expvdd_load_cont <= 2) {
			schedule_delayed_work(&aw87529->ram_work,
					      msecs_to_jiffies(ram_timer_val));
			I("%s: restart hrtimer to load firmware\n",
				__func__);
		}
		return;
	}

	I("%s: loaded %s - size: %zu\n", __func__, aw87529_expvdd_name,
		cont ? cont->size : 0);

	for (i = 0; i < cont->size; i = i + 2)
		I("%s: addr:0x%04x, data:0x%02x\n",
			__func__, *(cont->data + i), *(cont->data + i + 1));

	/* aw87529 ram update */
	aw87529_expvdd_cnt = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!aw87529_expvdd_cnt) {
		release_firmware(cont);
		E("%s: Error allocating memory\n", __func__);
		return;
	}
	aw87529_expvdd_cnt->len = cont->size;
	memcpy(aw87529_expvdd_cnt->data, cont->data, cont->size);
	release_firmware(cont);
	aw87529->expvdd_cfg_update_flag = 1;

	I("%s: fw update complete\n", __func__);
}

static int aw87529_expvdd_update(struct aw87529 *aw87529)
{
	I("%s enter\n", __func__);
	return request_firmware_nowait(THIS_MODULE,
				       FW_ACTION_HOTPLUG,
				       aw87529_expvdd_name,
				       &aw87529->i2c_client->dev,
				       GFP_KERNEL,
				       aw87529, aw87529_expvdd_cfg_loaded);
}

static void aw87529_hvload_cfg_loaded(const struct firmware *cont,
				      void *context)
{
	int i = 0;
	int ram_timer_val = 2000;

	D("%s\n", __func__);
	hvload_load_cont++;
	if (!cont) {
		E("%s: failed to read %s\n", __func__,
		       aw87529_hvload_name);
		release_firmware(cont);
		if (hvload_load_cont <= 2) {
			schedule_delayed_work(&aw87529->ram_work,
					      msecs_to_jiffies(ram_timer_val));
			I("%s: restart hrtimer to load firmware\n",
				__func__);
		}
		return;
	}

	I("%s: loaded %s - size: %zu\n", __func__, aw87529_hvload_name,
		cont ? cont->size : 0);

	for (i = 0; i < cont->size; i = i + 2)
		I("%s: addr:0x%04x, data:0x%02x\n",
			__func__, *(cont->data + i), *(cont->data + i + 1));

	/* aw87529 ram update */
	aw87529_hvload_cnt = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!aw87529_hvload_cnt) {
		release_firmware(cont);
		E("%s: Error allocating memory\n", __func__);
		return;
	}
	aw87529_hvload_cnt->len = cont->size;
	memcpy(aw87529_hvload_cnt->data, cont->data, cont->size);
	release_firmware(cont);
	aw87529->hvload_cfg_update_flag = 1;

	I("%s: fw update complete\n", __func__);
}

static int aw87529_hvload_update(struct aw87529 *aw87529)
{
	D("%s\n", __func__);
	return request_firmware_nowait(THIS_MODULE,
				       FW_ACTION_HOTPLUG,
				       aw87529_hvload_name,
				       &aw87529->i2c_client->dev,
				       GFP_KERNEL,
				       aw87529, aw87529_hvload_cfg_loaded);
}

static void aw87529_drcv_cfg_loaded(const struct firmware *cont, void *context)
{
	int i = 0;
	int ram_timer_val = 2000;

	D("%s\n", __func__);
	drcv_load_cont++;

	if (!cont) {
		E("%s: failed to read %s\n", __func__, aw87529_drcv_name);
		release_firmware(cont);
		if (drcv_load_cont <= 2) {
			schedule_delayed_work(&aw87529->ram_work,
					      msecs_to_jiffies(ram_timer_val));
			I("%s: restart hrtimer to load firmware\n",
				__func__);
		}
		return;
	}

	I("%s: loaded %s - size: %zu\n", __func__, aw87529_drcv_name,
		cont ? cont->size : 0);

	for (i = 0; i < cont->size; i = i + 2)
		I("%s: addr:0x%04x, data:0x%02x\n",
			__func__, *(cont->data + i), *(cont->data + i + 1));

	/* aw87529 ram update */
	aw87529_drcv_cnt = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!aw87529_drcv_cnt) {
		release_firmware(cont);
		E("%s: Error allocating memory\n", __func__);
		return;
	}
	aw87529_drcv_cnt->len = cont->size;
	memcpy(aw87529_drcv_cnt->data, cont->data, cont->size);
	release_firmware(cont);
	aw87529->drcv_cfg_update_flag = 1;

	I("%s: fw update complete\n", __func__);
}

static int aw87529_drcv_update(struct aw87529 *aw87529)
{
	D("%s\n", __func__);
	return request_firmware_nowait(THIS_MODULE,
				       FW_ACTION_HOTPLUG,
				       aw87529_drcv_name,
				       &aw87529->i2c_client->dev,
				       GFP_KERNEL,
				       aw87529, aw87529_drcv_cfg_loaded);
}

static void aw87529_kspk_cfg_loaded(const struct firmware *cont, void *context)
{
	int i = 0;
	int ram_timer_val = 2000;

	D("%s\n", __func__);
	kspk_load_cont++;
	if (!cont) {
		E("%s: failed to read %s\n", __func__, aw87529_kspk_name);
		release_firmware(cont);
		if (kspk_load_cont <= 2) {
			schedule_delayed_work(&aw87529->ram_work,
					      msecs_to_jiffies(ram_timer_val));
			I("%s: restart hrtimer to load firmware\n",
				__func__);
		}
		return;
	}

	I("%s: loaded %s - size: %zu\n", __func__, aw87529_kspk_name,
		cont ? cont->size : 0);

	for (i = 0; i < cont->size; i = i + 2)
		I("%s: addr:0x%04x, data:0x%02x\n",
			__func__, *(cont->data + i), *(cont->data + i + 1));

	/* aw87529 ram update */
	aw87529_kspk_cnt = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!aw87529_kspk_cnt) {
		release_firmware(cont);
		E("%s: Error allocating memory\n", __func__);
		return;
	}
	aw87529_kspk_cnt->len = cont->size;
	memcpy(aw87529_kspk_cnt->data, cont->data, cont->size);
	release_firmware(cont);
	aw87529->kspk_cfg_update_flag = 1;

	I("%s: fw update complete\n", __func__);
}

#ifdef AWINIC_CFG_UPDATE_DELAY
static int aw87529_kspk_update(struct aw87529 *aw87529)
{
	D("%s\n", __func__);

	return request_firmware_nowait(THIS_MODULE,
				       FW_ACTION_HOTPLUG,
				       aw87529_kspk_name,
				       &aw87529->i2c_client->dev,
				       GFP_KERNEL,
				       aw87529, aw87529_kspk_cfg_loaded);
}

static void aw87529_cfg_work_routine(struct work_struct *work)
{
	D("%s\n", __func__);
	if (aw87529->kspk_cfg_update_flag == 0)
		aw87529_kspk_update(aw87529);
	if (aw87529->drcv_cfg_update_flag == 0)
		aw87529_drcv_update(aw87529);
	if (aw87529->hvload_cfg_update_flag == 0)
		aw87529_hvload_update(aw87529);
	if (aw87529->expvdd_cfg_update_flag == 0)
		aw87529_expvdd_update(aw87529);
	if (aw87529->hph_cfg_update_flag == 0)
		aw87529_hph_update(aw87529);
	if (aw87529->hphkspk_cfg_update_flag == 0)
		aw87529_hphkspk_update(aw87529);
}
#endif

static int aw87529_cfg_init(struct aw87529 *aw87529)
{
	int ret = -1;
#ifdef AWINIC_CFG_UPDATE_DELAY
	int cfg_timer_val = 5000;

	INIT_DELAYED_WORK(&aw87529->ram_work, aw87529_cfg_work_routine);
	schedule_delayed_work(&aw87529->ram_work,
			      msecs_to_jiffies(cfg_timer_val));
	ret = 0;
#endif
	return ret;
}
/*******************************************************************************
 * aw87529 attribute
 ******************************************************************************/
static ssize_t aw87529_get_reg(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	unsigned int i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW87529_REG_MAX; i++) {
		aw87529_i2c_read(aw87529, i, &reg_val);
		len += snprintf(buf + len, PAGE_SIZE - len,
				"reg:0x%02x=0x%02x\n", i, reg_val);
	}
	for (i = 0x60; i <= 0x69; i++) {
		aw87529_i2c_read(aw87529, i, &reg_val);
		len += snprintf(buf + len, PAGE_SIZE - len,
				"reg:0x%02x=0x%02x\n", i, reg_val);
	}
	return len;
}

static ssize_t aw87529_set_reg(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t len)
{
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2)
		aw87529_i2c_write(aw87529, databuf[0], databuf[1]);
	return len;
}

static ssize_t aw87529_get_hwen(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "hwen: %d\n",
			aw87529->hwen_flag);

	return len;
}

static ssize_t aw87529_set_hwen(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t len)
{
	ssize_t ret;
	unsigned int state;

	ret = kstrtouint(buf, 10, &state);
	if (ret)
		goto out_strtoint;
	if (state == 1)
		aw87529_hw_on(aw87529);
	else
		aw87529_hw_off(aw87529);

	if (ret < 0)
		goto out;

	return len;

 out:
	E("%s: i2c access fail to register\n", __func__);
 out_strtoint:
	E("%s: fail to change str to int\n", __func__);
	return ret;
}

static ssize_t aw87529_get_update(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	return len;
}

static ssize_t aw87529_set_update(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t len)
{
	ssize_t ret;
	unsigned int state;
	int cfg_timer_val = 10;

	ret = kstrtouint(buf, 10, &state);
	if (ret)
		goto out_strtoint;

	if (state == 0) {
	} else {
		aw87529->kspk_cfg_update_flag = 0;
		aw87529->drcv_cfg_update_flag = 0;
		aw87529->hvload_cfg_update_flag = 0;
		aw87529->expvdd_cfg_update_flag = 0;
		aw87529->hph_cfg_update_flag = 0;
		aw87529->hphkspk_cfg_update_flag = 0;
		schedule_delayed_work(&aw87529->ram_work,
				      msecs_to_jiffies(cfg_timer_val));
	}
	return len;

 out_strtoint:
	E("%s: fail to change str to int\n", __func__);
	return ret;
}

static ssize_t aw87529_get_mode(struct device *cd,
				struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "0: off mode\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "1: kspk mode\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "2: drcv mode\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "3: hvload mode\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "4: expvdd mode\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "5: hph mode\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "6: hphkspk mode\n");
	return len;
}

static ssize_t aw87529_set_mode(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t len)
{
	ssize_t ret;
	unsigned int state;

	ret = kstrtouint(buf, 10, &state);
	ret = kstrtouint(buf, 10, &state);
	if (ret)
		goto out_strtoint;
	if (state == 0)
		aw87529_audio_off();
	else if (state == 1)
		aw87529_audio_kspk();
	else if (state == 2)
		aw87529_audio_drcv();
	else if (state == 3)
		aw87529_audio_hvload();
	else if (state == 4)
		aw87529_audio_expvdd();
	else if (state == 5)
		aw87529_audio_hph();
	else if (state == 6)
		aw87529_audio_hphkspk();

	else
		aw87529_audio_off();

	if (ret < 0)
		goto out;

	return len;

 out:
	E("%s: i2c access fail to register\n", __func__);
 out_strtoint:
	E("%s: fail to change str to int\n", __func__);
	return ret;
}

static DEVICE_ATTR(reg, 0660, aw87529_get_reg, aw87529_set_reg);
static DEVICE_ATTR(hwen, 0660, aw87529_get_hwen, aw87529_set_hwen);
static DEVICE_ATTR(update, 0660, aw87529_get_update, aw87529_set_update);
static DEVICE_ATTR(mode, 0660, aw87529_get_mode, aw87529_set_mode);

static struct attribute *aw87529_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_hwen.attr,
	&dev_attr_update.attr,
	&dev_attr_mode.attr,
	NULL
};

static struct attribute_group aw87529_attribute_group = {
	.attrs = aw87529_attributes
};
/*****************************************************
 * device tree
 *****************************************************/
static int aw87529_parse_dt(struct device *dev, struct device_node *np)
{
	aw87529->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (aw87529->reset_gpio >= 0) {
		I("%s: reset gpio provided ok\n", __func__);
	} else {
		E("%s: no reset gpio provided failed\n", __func__);
		return -1;
	}

	return 0;
}

int aw87529_hw_reset(struct aw87529 *aw87529)
{
	I("%s\n", __func__);

	if (aw87529 && gpio_is_valid(aw87529->reset_gpio)) {
		gpio_set_value_cansleep(aw87529->reset_gpio, 0);
		usleep_range(1000, 2000);
		gpio_set_value_cansleep(aw87529->reset_gpio, 1);
		usleep_range(2000, 2500);
		aw87529->hwen_flag = 1;
	} else {
		aw87529->hwen_flag = 0;
		E("%s: failed\n", __func__);
	}
	return 0;
}
/*****************************************************
 * check chip id
 *****************************************************/
int aw87529_read_chipid(struct aw87529 *aw87529)
{
	unsigned int cnt = 0;
	int ret = -1;
	unsigned char reg_val = 0;

	while (cnt < AW_READ_CHIPID_RETRIES) {
		aw87529_i2c_write(aw87529, 0x64, 0x2C);
		ret = aw87529_i2c_read(aw87529, AW87529_REG_CHIPID, &reg_val);
		if (reg_val == AW87529_CHIPID) {
			I("%s This Chip is  AW87529 chipid = 0x%x\n",
				__func__, reg_val);
			return 0;
		}
		I("%s: aw87529 chipid = 0x%x error\n", __func__, reg_val);
		cnt++;
		usleep_range(2000, 2500);
	}

	return -EINVAL;
}
/*******************************************************************************
 * aw87529 i2c driver
 ******************************************************************************/
static int aw87529_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct device_node *np = client->dev.of_node;
	int ret = -1;

	I("%s: probe\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		E("%s: i2c check failed\n", __func__);
		ret = -ENODEV;
		goto exit_check_functionality_failed;
	}

	aw87529 =
	    devm_kzalloc(&client->dev, sizeof(struct aw87529), GFP_KERNEL);
	if (aw87529 == NULL) {
		ret = -ENOMEM;
		goto exit_devm_kzalloc_failed;
	}

	aw87529->i2c_client = client;
	i2c_set_clientdata(client, aw87529);

	/* aw87529 rst */
	if (np) {
		ret = aw87529_parse_dt(&client->dev, np);
		if (ret) {
			E("%s: failed to parse device tree node\n", __func__);
			goto exit_gpio_get_failed;
		}
	} else {
		aw87529->reset_gpio = -1;
	}

	if (gpio_is_valid(aw87529->reset_gpio)) {
		ret = devm_gpio_request_one(&client->dev, aw87529->reset_gpio,
					    GPIOF_OUT_INIT_LOW, "aw87529_rst");
		if (ret) {
			E("%s: rst request failed\n", __func__);
			goto exit_gpio_request_failed;
		}
	}

	/* hardware reset */
	aw87529_hw_reset(aw87529);

	/* aw87529 chip id */
	ret = aw87529_read_chipid(aw87529);
	if (ret < 0) {
		E("%s: aw87529_read_chipid failed ret=%d\n",
			__func__, ret);
		goto exit_i2c_check_id_failed;
	}

	ret = sysfs_create_group(&client->dev.kobj, &aw87529_attribute_group);
	if (ret < 0) {
		I("%s error creating sysfs attr files\n", __func__);
	}

	/* aw87529 cfg update */
	kspk_load_cont = 0;
	drcv_load_cont = 0;
	hvload_load_cont = 0;
	expvdd_load_cont = 0;
	hph_load_cont = 0;
	hphkspk_load_cont = 0;

	aw87529->kspk_cfg_update_flag = 0;
	aw87529->drcv_cfg_update_flag = 0;
	aw87529->hvload_cfg_update_flag = 0;
	aw87529->expvdd_cfg_update_flag = 0;
	aw87529->hph_cfg_update_flag = 0;
	aw87529->hphkspk_cfg_update_flag = 0;
	aw87529_cfg_init(aw87529);

	/* aw87529 hardware off */
	aw87529_hw_off(aw87529);

	return 0;

 exit_i2c_check_id_failed:
	devm_gpio_free(&client->dev, aw87529->reset_gpio);
 exit_gpio_request_failed:
 exit_gpio_get_failed:
	devm_kfree(&client->dev, aw87529);
	aw87529 = NULL;
 exit_devm_kzalloc_failed:
 exit_check_functionality_failed:
	return ret;
}

static int aw87529_i2c_remove(struct i2c_client *client)
{
	struct aw87529 *aw87529 = i2c_get_clientdata(client);

	I("%s\n", __func__);
	if (gpio_is_valid(aw87529->reset_gpio))
		devm_gpio_free(&client->dev, aw87529->reset_gpio);

	return 0;
}

static const struct i2c_device_id aw87529_i2c_id[] = {
	{AW87529_I2C_NAME, 0},
	{}
};

static const struct of_device_id extpa_of_match[] = {
	{.compatible = "awinic,aw87529_pa"},
	{},
};

static struct i2c_driver aw87529_i2c_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = AW87529_I2C_NAME,
		   .of_match_table = extpa_of_match,
		   },
	.probe = aw87529_i2c_probe,
	.remove = aw87529_i2c_remove,
	.id_table = aw87529_i2c_id,
};

static int __init aw87529_pa_init(void)
{
	int ret;

	I("%s: driver version: %s\n", __func__, AW87529_DRIVER_VERSION);

	ret = i2c_add_driver(&aw87529_i2c_driver);
	if (ret) {
		I("%s Unable to register driver (%d)\n", __func__, ret);
		return ret;
	}
	return 0;
}

static void __exit aw87529_pa_exit(void)
{
	I("%s\n", __func__);
	i2c_del_driver(&aw87529_i2c_driver);
}

EXPORT_SYMBOL(aw87529_audio_kspk);
EXPORT_SYMBOL(aw87529_audio_off);

module_init(aw87529_pa_init);
module_exit(aw87529_pa_exit);

MODULE_AUTHOR("<hushanping@awinic.com.cn>");
MODULE_DESCRIPTION("AWINIC AW87529 PA driver");
MODULE_LICENSE("GPL v2");
