/*	$OpenBSD$	*/
/**************************************************************************

Copyright © 2006 Dave Airlie

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

#include "dvo.h"

#define CH7xxx_REG_VID		0x4a
#define CH7xxx_REG_DID		0x4b

#define CH7011_VID		0x83 /* 7010 as well */
#define CH7009A_VID		0x84
#define CH7009B_VID		0x85
#define CH7301_VID		0x95

#define CH7xxx_VID		0x84
#define CH7xxx_DID		0x17

#define CH7xxx_NUM_REGS		0x4c

#define CH7xxx_CM		0x1c
#define CH7xxx_CM_XCM		(1<<0)
#define CH7xxx_CM_MCP		(1<<2)
#define CH7xxx_INPUT_CLOCK	0x1d
#define CH7xxx_GPIO		0x1e
#define CH7xxx_GPIO_HPIR	(1<<3)
#define CH7xxx_IDF		0x1f

#define CH7xxx_IDF_HSP		(1<<3)
#define CH7xxx_IDF_VSP		(1<<4)

#define CH7xxx_CONNECTION_DETECT 0x20
#define CH7xxx_CDET_DVI		(1<<5)

#define CH7301_DAC_CNTL		0x21
#define CH7301_HOTPLUG		0x23
#define CH7xxx_TCTL		0x31
#define CH7xxx_TVCO		0x32
#define CH7xxx_TPCP		0x33
#define CH7xxx_TPD		0x34
#define CH7xxx_TPVT		0x35
#define CH7xxx_TLPF		0x36
#define CH7xxx_TCT		0x37
#define CH7301_TEST_PATTERN	0x48

#define CH7xxx_PM		0x49
#define CH7xxx_PM_FPD		(1<<0)
#define CH7301_PM_DACPD0	(1<<1)
#define CH7301_PM_DACPD1	(1<<2)
#define CH7301_PM_DACPD2	(1<<3)
#define CH7xxx_PM_DVIL		(1<<6)
#define CH7xxx_PM_DVIP		(1<<7)

#define CH7301_SYNC_POLARITY	0x56
#define CH7301_SYNC_RGB_YUV	(1<<0)
#define CH7301_SYNC_POL_DVI	(1<<5)

/** @file
 * driver for the Chrontel 7xxx DVI chip over DVO.
 */

static struct ch7xxx_id_struct {
	uint8_t vid;
	char *name;
} ch7xxx_ids[] = {
	{ CH7011_VID, "CH7011" },
	{ CH7009A_VID, "CH7009A" },
	{ CH7009B_VID, "CH7009B" },
	{ CH7301_VID, "CH7301" },
};

struct ch7xxx_priv {
	bool quiet;
};

char *ch7xxx_get_id(uint8_t);
bool ch7xxx_readb(struct intel_dvo_device *, int, uint8_t *);
bool ch7xxx_writeb(struct intel_dvo_device *, int, uint8_t);
bool ch7xxx_init(struct intel_dvo_device *, struct i2c_controller *);
enum drm_connector_status ch7xxx_detect(struct intel_dvo_device *);
enum drm_mode_status ch7xxx_mode_valid(struct intel_dvo_device *,
    struct drm_display_mode *);
void ch7xxx_mode_set(struct intel_dvo_device *, struct drm_display_mode *,
    struct drm_display_mode *);
void ch7xxx_dpms(struct intel_dvo_device *, bool);
bool ch7xxx_get_hw_state(struct intel_dvo_device *);
void ch7xxx_dump_regs(struct intel_dvo_device *);
void ch7xxx_destroy(struct intel_dvo_device *);

char *
ch7xxx_get_id(uint8_t vid)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ch7xxx_ids); i++) {
		if (ch7xxx_ids[i].vid == vid)
			return ch7xxx_ids[i].name;
	}

	return NULL;
}

/** Reads an 8 bit register */
bool
ch7xxx_readb(struct intel_dvo_device *dvo, int addr, uint8_t *ch)
{
	struct ch7xxx_priv *ch7xxx = dvo->dev_priv;
	struct i2c_controller *adapter = dvo->i2c_bus;
	u8 out_buf[2];
	u8 in_buf[2];
	int ret;
	uint8_t cmd = 0;

	out_buf[0] = addr;
	out_buf[1] = 0;

	iic_acquire_bus(adapter, 0);
	ret = iic_exec(adapter, I2C_OP_WRITE_WITH_STOP, dvo->slave_addr,
	    &cmd, 1, out_buf, 1, 0);
	if (ret)
		goto read_err;
	ret = iic_exec(adapter, I2C_OP_READ_WITH_STOP, dvo->slave_addr,
	    &cmd, 1, in_buf, 1, 0);
	if (ret)
		goto read_err;
	iic_release_bus(adapter, 0);

	*ch = in_buf[0];
	return true;

read_err:
	iic_release_bus(adapter, 0);
	if (!ch7xxx->quiet) {
		DRM_DEBUG_KMS("Unable to read register 0x%02x from %02x.\n",
			  addr, dvo->slave_addr);
	}
	return false;
}

/** Writes an 8 bit register */
bool
ch7xxx_writeb(struct intel_dvo_device *dvo, int addr, uint8_t ch)
{
	struct ch7xxx_priv *ch7xxx = dvo->dev_priv;
	struct i2c_controller *adapter = dvo->i2c_bus;
	uint8_t out_buf[2];
	int ret;
	uint8_t cmd = 0;

	out_buf[0] = addr;
	out_buf[1] = ch;

	iic_acquire_bus(adapter, 0);
	ret = iic_exec(adapter, I2C_OP_WRITE_WITH_STOP, dvo->slave_addr,
	    &cmd, 1, out_buf, 2, 0);
	iic_release_bus(adapter, 0);
	if (ret)
		goto write_err;

	return true;

write_err:
	if (!ch7xxx->quiet) {
		DRM_DEBUG_KMS("Unable to write register 0x%02x to %d.\n",
			  addr, dvo->slave_addr);
	}

	return false;
}

bool
ch7xxx_init(struct intel_dvo_device *dvo,
			struct i2c_controller *adapter)
{
	/* this will detect the CH7xxx chip on the specified i2c bus */
	struct ch7xxx_priv *ch7xxx;
	uint8_t vendor, device;
	char *name;

	ch7xxx = malloc(sizeof(struct ch7xxx_priv), M_DRM, M_WAITOK | M_ZERO);
	if (ch7xxx == NULL)
		return false;

	dvo->i2c_bus = adapter;
	dvo->dev_priv = ch7xxx;
	ch7xxx->quiet = true;

	if (!ch7xxx_readb(dvo, CH7xxx_REG_VID, &vendor))
		goto out;

	name = ch7xxx_get_id(vendor);
	if (!name) {
		DRM_DEBUG_KMS("ch7xxx not detected; got 0x%02x from "
				"slave %d.\n",
			  vendor, dvo->slave_addr);
		goto out;
	}


	if (!ch7xxx_readb(dvo, CH7xxx_REG_DID, &device))
		goto out;

	if (device != CH7xxx_DID) {
		DRM_DEBUG_KMS("ch7xxx not detected; got 0x%02x "
				"slave %d.\n",
			  vendor, dvo->slave_addr);
		goto out;
	}

	ch7xxx->quiet = false;
	DRM_DEBUG_KMS("Detected %s chipset, vendor/device ID 0x%02x/0x%02x\n",
		  name, vendor, device);
	return true;
out:
	free(ch7xxx, M_DRM);
	return false;
}

enum drm_connector_status
ch7xxx_detect(struct intel_dvo_device *dvo)
{
	uint8_t cdet, orig_pm, pm;

	ch7xxx_readb(dvo, CH7xxx_PM, &orig_pm);

	pm = orig_pm;
	pm &= ~CH7xxx_PM_FPD;
	pm |= CH7xxx_PM_DVIL | CH7xxx_PM_DVIP;

	ch7xxx_writeb(dvo, CH7xxx_PM, pm);

	ch7xxx_readb(dvo, CH7xxx_CONNECTION_DETECT, &cdet);

	ch7xxx_writeb(dvo, CH7xxx_PM, orig_pm);

	if (cdet & CH7xxx_CDET_DVI)
		return connector_status_connected;
	return connector_status_disconnected;
}

enum drm_mode_status
ch7xxx_mode_valid(struct intel_dvo_device *dvo,
					      struct drm_display_mode *mode)
{
	if (mode->clock > 165000)
		return MODE_CLOCK_HIGH;

	return MODE_OK;
}

void
ch7xxx_mode_set(struct intel_dvo_device *dvo,
			    struct drm_display_mode *mode,
			    struct drm_display_mode *adjusted_mode)
{
	uint8_t tvco, tpcp, tpd, tlpf, idf;

	if (mode->clock <= 65000) {
		tvco = 0x23;
		tpcp = 0x08;
		tpd = 0x16;
		tlpf = 0x60;
	} else {
		tvco = 0x2d;
		tpcp = 0x06;
		tpd = 0x26;
		tlpf = 0xa0;
	}

	ch7xxx_writeb(dvo, CH7xxx_TCTL, 0x00);
	ch7xxx_writeb(dvo, CH7xxx_TVCO, tvco);
	ch7xxx_writeb(dvo, CH7xxx_TPCP, tpcp);
	ch7xxx_writeb(dvo, CH7xxx_TPD, tpd);
	ch7xxx_writeb(dvo, CH7xxx_TPVT, 0x30);
	ch7xxx_writeb(dvo, CH7xxx_TLPF, tlpf);
	ch7xxx_writeb(dvo, CH7xxx_TCT, 0x00);

	ch7xxx_readb(dvo, CH7xxx_IDF, &idf);

	idf &= ~(CH7xxx_IDF_HSP | CH7xxx_IDF_VSP);
	if (mode->flags & DRM_MODE_FLAG_PHSYNC)
		idf |= CH7xxx_IDF_HSP;

	if (mode->flags & DRM_MODE_FLAG_PVSYNC)
		idf |= CH7xxx_IDF_HSP;

	ch7xxx_writeb(dvo, CH7xxx_IDF, idf);
}

/* set the CH7xxx power state */
void
ch7xxx_dpms(struct intel_dvo_device *dvo, bool enable)
{
	if (enable)
		ch7xxx_writeb(dvo, CH7xxx_PM, CH7xxx_PM_DVIL | CH7xxx_PM_DVIP);
	else
		ch7xxx_writeb(dvo, CH7xxx_PM, CH7xxx_PM_FPD);
}

bool
ch7xxx_get_hw_state(struct intel_dvo_device *dvo)
{
	u8 val;

	ch7xxx_readb(dvo, CH7xxx_PM, &val);

	if (val & (CH7xxx_PM_DVIL | CH7xxx_PM_DVIP))
		return true;
	else
		return false;
}

void
ch7xxx_dump_regs(struct intel_dvo_device *dvo)
{
	int i;

	for (i = 0; i < CH7xxx_NUM_REGS; i++) {
		uint8_t val;
		if ((i % 8) == 0)
			DRM_LOG_KMS("\n %02X: ", i);
		ch7xxx_readb(dvo, i, &val);
		DRM_LOG_KMS("%02X ", val);
	}
}

void
ch7xxx_destroy(struct intel_dvo_device *dvo)
{
	struct ch7xxx_priv *ch7xxx = dvo->dev_priv;

	if (ch7xxx) {
		free(ch7xxx, M_DRM);
		dvo->dev_priv = NULL;
	}
}

struct intel_dvo_dev_ops ch7xxx_ops = {
	.init = ch7xxx_init,
	.detect = ch7xxx_detect,
	.mode_valid = ch7xxx_mode_valid,
	.mode_set = ch7xxx_mode_set,
	.dpms = ch7xxx_dpms,
	.get_hw_state = ch7xxx_get_hw_state,
	.dump_regs = ch7xxx_dump_regs,
	.destroy = ch7xxx_destroy,
};
