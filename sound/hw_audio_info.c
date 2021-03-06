/*
 * Copyright (C) huawei company
 *
 * This    program    is free    software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public    License    version    2 as
 * published by    the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/printk.h>

/* Audio property is an unsigned 32-bit integer stored in the variable of audio_property.
   The meaning of audio_property is defined as following MACROs in groups of 4 bits */

/* Delete ambiguous builtin mic type */
/* Bit4 ~ bit7:
   Actually existing mics on the phone, it's NOT relevant to fluence. Using one bit to denote
   the existence of one kind of mic, possible mics are:
     master mic: the basic mic used for call and record, if it doesn't exist means the
                 config or software is wrong.
     secondary mic: auxiliary mic, usually used for fluence or paired with speaker in
                    handsfree mode, it's possible that this mic exist but fluence isn't enabled.
     error mic: used in handset ANC. */
#define AUDIO_PROP_MASTER_MIC_EXIST_NODE    "builtin-master-mic-exist"
#define AUDIO_PROP_SECONDARY_MIC_EXIST_NODE "builtin-2nd-mic-exist"
#define AUDIO_PROP_ERROR_MIC_EXIST_NODE     "builtin-error-mic-exist"
#define AUDIO_PROP_MASTER_MIC_EXIST_MASK    0x00000010
#define AUDIO_PROP_SECONDARY_MIC_EXIST_MASK 0x00000020
#define AUDIO_PROP_ERROR_MIC_EXIST_MASK     0x00000040

/* Bit12 ~ bit15:
   Denote which mic would be used in hand held mode, please add as needed */
#define AUDIO_PROP_HANDHELD_MASTER_MIC_NODE "hand_held_master_mic_strategy"
#define AUDIO_PROP_HANDHELD_DUAL_MIC_NODE "hand_held_dual_mic_strategy"
#define AUDIO_PROP_HANDHELD_AANC_MIC_NODE "hand_held_aanc_mic_strategy"
#define AUDIO_PROP_HANDHELD_MASTER_MIC 0x00001000
#define AUDIO_PROP_HANDHELD_DUAL_MIC 0x00002000
#define AUDIO_PROP_HANDHELD_AANC_MIC 0x00004000

#define PRODUCT_IDENTIFIER_NODE             "product-identifier"
#define PRODUCT_IDENTIFIER_BUFF_SIZE        64
#define AUD_PARAM_VER_NODE             "aud_param_ver"
#define AUD_PARAM_VER_BUFF_SIZE        32

#define SPEAKER_PA_NODE             "speaker-pa"
#define SPEAKER_PA_BUFF_SIZE        32

#define PA_i2C_NODE             "pa-i2c"
#define PA_i2C_BUFF_SIZE        32

#define SPEAKER_BOX_NODE             "speaker-box"
#define SPEAKER_BOX_BUFF_SIZE        32
#define PIN_VOLTAGE_LOW   0
#define PIN_VOLTAGE_HIGH  1
#define PIN_VOLTAGE_FLOAT 2

#define AUDIO_PROP_BTSCO_NREC_ADAPT_MASK 0xf0000000
#define AUDIO_PROP_BTSCO_NREC_ADAPT_OFF  0x10000000
#define AUDIO_PROP_BTSCO_NREC_ADAPT_ON   0x20000000

#define PRODUCT_NERC_ADAPT_CONFIG    "product-btsco-nrec-adapt"

/* Bit16 ~ bit19:
   Denote which mic would be used in loud speaker mode, please add as needed */
#define AUDIO_PROP_LOUDSPEAKER_MASTER_MIC_NODE "loud_speaker_master_mic_strategy"
#define AUDIO_PROP_LOUDSPEAKER_SECOND_MIC_NODE "loud_speaker_second_mic_strategy"
#define AUDIO_PROP_LOUDSPEAKER_ERROR_MIC_NODE "loud_speaker_error_mic_strategy"
#define AUDIO_PROP_LOUDSPEAKER_MASTER_MIC 0x00010000
#define AUDIO_PROP_LOUDSPEAKER_SECOND_MIC 0x00020000
#define AUDIO_PROP_LOUDSPEAKER_ERROR_MIC 0x00040000
/* Delete unused codes related to string_to_value function */

/**
* audio_property - Product specified audio properties
*/
static unsigned int audio_property = 0;

/**
* product_identifier - Product identifier, used for audio parameter auto-adapt
*/
static char product_identifier[PRODUCT_IDENTIFIER_BUFF_SIZE] = "default";

static char speaker_pa[SPEAKER_PA_BUFF_SIZE] = "none";
static bool tfa9895_flag = false;

static char pa_i2c[PA_i2C_BUFF_SIZE] = "none";

static char speaker_box_id[SPEAKER_BOX_BUFF_SIZE] = "";
static int speaker_box = -1;

static char aud_param_ver[AUD_PARAM_VER_BUFF_SIZE] = "default";

static ssize_t audio_property_show(struct device_driver *driver, char *buf)
{
    if(NULL != buf)
    {
        /* The size of buf should be one page(PAGE_SIZE), which will be surely
           enough to hold the string converted from a 32 bit unsigned integer.
           So omit unnecessary overflow check */
        return snprintf(buf, PAGE_SIZE, "%08X\n", audio_property);
    }

    return 0;
}

DRIVER_ATTR(audio_property, 0444, audio_property_show, NULL);

static ssize_t product_identifier_show(struct device_driver *driver, char *buf)
{
    if(NULL != buf)
    {
        /* Newline is not appended on purpose, for convenience of reader programs */
        snprintf(buf, PAGE_SIZE, "%s", product_identifier);
        return strlen(buf);
    }

    return 0;
}

DRIVER_ATTR(product_identifier, 0444, product_identifier_show, NULL);

static ssize_t speaker_pa_show(struct device_driver *driver, char *buf)
{
    if(NULL != buf)
    {
        /* Newline is not appended on purpose, for convenience of reader programs */
        snprintf(buf, PAGE_SIZE, "%s", speaker_pa);
        return strlen(buf);
    }

    return 0;
}
DRIVER_ATTR(speaker_pa, 0444, speaker_pa_show, NULL);

static ssize_t box_id_show(struct device_driver *driver, char *buf)
{
    if(NULL != buf)
    {
        snprintf(buf, PAGE_SIZE, "%s", speaker_box_id);
        return strlen(buf);
    }

    return 0;
}
DRIVER_ATTR(speaker_box_id, 0444, box_id_show, NULL);

static ssize_t pa_i2c_show(struct device_driver *driver, char *buf)
{
    if(NULL != buf)
    {
        /* Newline is not appended on purpose, for convenience of reader programs */
        snprintf(buf, PAGE_SIZE, "%s", pa_i2c);
        return strlen(buf);
    }

    return 0;
}
DRIVER_ATTR(pa_i2c, 0444, pa_i2c_show, NULL);

/**
* audiopara_version_show - used for cat the value of node aud_param_ver
*/
static ssize_t audiopara_version_show(struct device_driver *driver, char *buf)
{
    if(NULL != buf)
    {
        return snprintf(buf, PAGE_SIZE, "%s", aud_param_ver);
    }
    return 0;
}

DRIVER_ATTR(aud_param_ver, 0444, audiopara_version_show, NULL);

static struct attribute *audio_attrs[] = {
    &driver_attr_audio_property.attr,
    &driver_attr_product_identifier.attr,
    &driver_attr_aud_param_ver.attr,
    &driver_attr_speaker_pa.attr,
    &driver_attr_speaker_box_id.attr,
    &driver_attr_pa_i2c.attr,
    NULL,
};

static struct attribute_group audio_group = {
    .name ="hw_audio_info",
    .attrs = audio_attrs,
};

static const struct attribute_group *groups[] = {
    &audio_group,
    NULL,
};

static struct of_device_id audio_info_match_table[] = {
    { .compatible = "huawei,hw_audio_info",},
    { },
};

bool get_tfa9895_flag(void)
{
   if(tfa9895_flag)
      return true;
   return false;
}
EXPORT_SYMBOL_GPL(get_tfa9895_flag);

int get_pa_box_id(struct platform_device *pdev, struct pinctrl *pinctrl)
{
    struct pinctrl_state *pin_default;
	struct pinctrl_state *pin_sleep;
	int gpio_box_id;
	int ret;
	int box_value_pull_up = -1;
	int box_value_pull_down = -1;

	gpio_box_id = of_get_named_gpio(pdev->dev.of_node, "qcom,box-id", 0);
    if (gpio_box_id < 0)
    {
        pr_err("%s: failed to get gpio_box_id\n", __func__);
        return -1;
    }
    ret = gpio_request(gpio_box_id, "gpio_box_id");
    if (ret)
    {
        pr_err("%s: unable to request gpio_box_id\n", __func__);
        return -1;
    }

    pin_default = pinctrl_lookup_state(pinctrl, "box_default");
	if (IS_ERR(pin_default))
	{
        pr_err("%s: Unable to get pinctrl box_default state handle\n", __func__);
        return -1;
	}

	pin_sleep = pinctrl_lookup_state(pinctrl, "box_sleep");
    if (IS_ERR(pin_sleep))
    {
	    pr_err("%s: Unable to get pinctrl box_sleep state handle\n", __func__);
	    return -1;
    }

    pinctrl_select_state(pinctrl, pin_default);
    udelay(10);
    gpio_direction_input(gpio_box_id);
    box_value_pull_up = gpio_get_value(gpio_box_id);
    pr_err("%s: box_value up = %d, ret = %d\n", __func__, box_value_pull_up, ret);

    pinctrl_select_state(pinctrl, pin_sleep);
    udelay(10);
    gpio_direction_input(gpio_box_id);
    box_value_pull_down = gpio_get_value(gpio_box_id);
    pr_err("%s: box_value down = %d, ret = %d\n", __func__, box_value_pull_down, ret);
    gpio_free(gpio_box_id);

    if(box_value_pull_up == box_value_pull_down) // pin is high or low
    {
        if(PIN_VOLTAGE_HIGH == box_value_pull_down)
        {
            pinctrl_select_state(pinctrl, pin_default); // set pull up when pin is high
            return PIN_VOLTAGE_HIGH;
        }
        else
        {
            pinctrl_select_state(pinctrl, pin_sleep); // set pull down when pin is low
            return PIN_VOLTAGE_LOW;
        }
    }
    else // pin is float
    {
        return PIN_VOLTAGE_FLOAT;
    }
}

static int audio_info_probe(struct platform_device *pdev)
{
    int ret;
    const char *string;
    struct pinctrl *pinctrl;

    if(NULL == pdev)
    {
        pr_err( "huawei_audio: audio_info_probe failed, pdev is NULL\n");
        return 0;
    }

    if(NULL == pdev->dev.of_node)
    {
        pr_err( "huawei_audio: audio_info_probe failed, of_node is NULL\n");
        return 0;
    }

    /* Delete ambiguous builtin mic type */

    if(of_property_read_bool(pdev->dev.of_node, AUDIO_PROP_MASTER_MIC_EXIST_NODE))
    {
        audio_property |= AUDIO_PROP_MASTER_MIC_EXIST_MASK;
    }
    else
    {
        pr_err( "huawei_audio: check mic config, no master mic found\n");
    }

    if(of_property_read_bool(pdev->dev.of_node, AUDIO_PROP_SECONDARY_MIC_EXIST_NODE))
    {
        audio_property |= AUDIO_PROP_SECONDARY_MIC_EXIST_MASK;
    }

    if(of_property_read_bool(pdev->dev.of_node, AUDIO_PROP_ERROR_MIC_EXIST_NODE))
    {
        audio_property |= AUDIO_PROP_ERROR_MIC_EXIST_MASK;
    }

    if(of_property_read_bool(pdev->dev.of_node, AUDIO_PROP_HANDHELD_MASTER_MIC_NODE))
    {
        audio_property |= AUDIO_PROP_HANDHELD_MASTER_MIC;
    }

    if(of_property_read_bool(pdev->dev.of_node, AUDIO_PROP_HANDHELD_DUAL_MIC_NODE))
    {
        audio_property |= AUDIO_PROP_HANDHELD_DUAL_MIC;
    }

    if(of_property_read_bool(pdev->dev.of_node, AUDIO_PROP_HANDHELD_AANC_MIC_NODE))
    {
        audio_property |= AUDIO_PROP_HANDHELD_AANC_MIC;
    }

    if(of_property_read_bool(pdev->dev.of_node, AUDIO_PROP_LOUDSPEAKER_MASTER_MIC_NODE))
    {
        audio_property |= AUDIO_PROP_LOUDSPEAKER_MASTER_MIC;
    }

    if(of_property_read_bool(pdev->dev.of_node, AUDIO_PROP_LOUDSPEAKER_SECOND_MIC_NODE))
    {
        audio_property |= AUDIO_PROP_LOUDSPEAKER_SECOND_MIC;
    }

    if(of_property_read_bool(pdev->dev.of_node, AUDIO_PROP_LOUDSPEAKER_ERROR_MIC_NODE))
    {
        audio_property |= AUDIO_PROP_LOUDSPEAKER_ERROR_MIC;
    }

    string = NULL;
    ret = of_property_read_string(pdev->dev.of_node, PRODUCT_IDENTIFIER_NODE, &string);
    if(ret || (NULL == string))
    {
        pr_err( "huawei_audio: of_property_read_string product-identifier failed %d\n", ret);
    }
    else
    {
        memset(product_identifier, 0, sizeof(product_identifier));
        strncpy(product_identifier, string, sizeof(product_identifier) - 1);
    }

    string = NULL;
    ret = of_property_read_string(pdev->dev.of_node, SPEAKER_PA_NODE, &string);
    if(ret || (NULL == string))
    {
        printk("huawei_audio: of_property_read_string speaker-pa failed %d\n", ret);
    }
    else
    {
        memset(speaker_pa, 0, sizeof(speaker_pa));
        strncpy(speaker_pa, string, sizeof(speaker_pa) - 1);
    }
    if(strcmp(speaker_pa, "tfa9895"))
    {
        printk("tfa9895_flag = false\n");
        tfa9895_flag = false;
    }
    else
    {
        printk("tfa9895_flag = true\n");
        tfa9895_flag = true;
    }

    string = NULL;
    ret = of_property_read_string(pdev->dev.of_node, SPEAKER_BOX_NODE, &string);
    if(ret || (NULL == string))
    {
        printk("huawei_audio: of_property_read_string speaker-box failed %d\n", ret);
    }
    else
    {
        pinctrl = devm_pinctrl_get(&pdev->dev);
        if (IS_ERR(pinctrl))
        {
			pr_err( "Unable to get pinctrl handle\n");
		}
        speaker_box = get_pa_box_id(pdev, pinctrl);
        pr_err( "speaker_box = %d\n", speaker_box);
        memset(speaker_box_id, 0, sizeof(speaker_box_id));
        if(PIN_VOLTAGE_LOW == speaker_box)
        {
            strncpy(speaker_box_id, "GK", sizeof(speaker_pa) - 1);
        }
        else if(PIN_VOLTAGE_HIGH == speaker_box)
        {
            strncpy(speaker_box_id, "AAC", sizeof(speaker_pa) - 1);
        }
        else if(PIN_VOLTAGE_FLOAT == speaker_box)
        {
            strncpy(speaker_box_id, "GD", sizeof(speaker_pa) - 1);
        }
        else
        {
            strncpy(speaker_box_id, "GD", sizeof(speaker_pa) - 1);
        }
    }

    if(false == of_property_read_bool(pdev->dev.of_node, PRODUCT_NERC_ADAPT_CONFIG))
    {
        pr_err( "huawei_audio: of_property_read_bool PRODUCT_NERC_ADAPT_CONFIG failed %d\n", ret);
        audio_property |= (AUDIO_PROP_BTSCO_NREC_ADAPT_OFF & AUDIO_PROP_BTSCO_NREC_ADAPT_MASK);
    }
    else
    {
        audio_property |= ( AUDIO_PROP_BTSCO_NREC_ADAPT_ON & AUDIO_PROP_BTSCO_NREC_ADAPT_MASK);
    }

    string = NULL;
    ret = of_property_read_string(pdev->dev.of_node, PA_i2C_NODE, &string);
    if(ret || (NULL == string))
    {
        pr_err( "huawei_audio: of_property_read_string pa-i2c failed %d\n", ret);
    }
    else
    {
        memset(pa_i2c, 0, sizeof(pa_i2c));
        strncpy(pa_i2c, string, sizeof(pa_i2c) - 1);
    }

    string = NULL;
    ret = of_property_read_string(pdev->dev.of_node, AUD_PARAM_VER_NODE, &string);
    if(ret || (NULL == string))
    {
        pr_err( "huawei_audio: of_property_read_string aud_param_ver failed %d\n", ret);
    }
    else
    {
        memset(aud_param_ver, 0, sizeof(aud_param_ver));
        strncpy(aud_param_ver, string, sizeof(aud_param_ver) - 1);
    }

    return 0;
}

static struct platform_driver audio_info_driver = {
    .driver = {
        .name  = "hw_audio_info",
        .owner  = THIS_MODULE,
        .groups = groups,
        .of_match_table = audio_info_match_table,
    },

    .probe = audio_info_probe,
    .remove = NULL,
};

static int __init audio_info_init(void)
{
    return platform_driver_register(&audio_info_driver);
}

static void __exit audio_info_exit(void)
{
    platform_driver_unregister(&audio_info_driver);
}

module_init(audio_info_init);
module_exit(audio_info_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Huawei audio info");
MODULE_AUTHOR("duhongyan<hongyan.du@huawei.com>");
