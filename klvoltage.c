/*
 * Dummy voltage and temperature hwmon driver.
 *
 * Copyright (C) 2016 Steven Toth <stoth@kernellabs.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/slab.h>

/* Data structure for the platform data of "my device" */
struct my_device_platform_data {
	int reset_gpio;
	int power_on_gpio;
};

struct measurement_s {
	/* Voltage - a value in millivolts. */
	/* Temperature - in 1/1000 of a degree C */
	/* Current - in 1/1000 of an amp */
	int input;	/* Current/Active value */
	int min;
	int max;
	int critical;
};

#define KLV_SYSVDD 0
#define KLV_CHIP_TEMP 1
#define KLV_CHIP_CURRENT 2

/* Device driver data structure. */
struct my_driver_data {
	struct measurement_s metrics[3];
	struct device *hwmon_dev;
};

static const char *const input_names[] = {
	[KLV_SYSVDD] = "SYSVDD",
	[KLV_CHIP_TEMP] = "PMIC",
	[KLV_CHIP_CURRENT] = "PMIC",
};

static ssize_t show_label(struct device *dev, struct device_attribute *attr, char *buf)
{
	int channel = to_sensor_dev_attr(attr)->index;
	return sprintf(buf, "%s\n", input_names[channel]);
}

/* Query the current value */
static ssize_t get_input(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct my_driver_data *driver_data = dev_get_drvdata(dev);
	int channel = to_sensor_dev_attr(attr)->index;
	struct measurement_s *m = &driver_data->metrics[channel];
	return sprintf(buf, "%d\n", m->input);
}

static ssize_t get_min(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct my_driver_data *driver_data = dev_get_drvdata(dev);
	int channel = to_sensor_dev_attr(attr)->index;
	struct measurement_s *m = &driver_data->metrics[channel];
	return sprintf(buf, "%d\n", m->min);
}

static ssize_t get_max(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct my_driver_data *driver_data = dev_get_drvdata(dev);
	int channel = to_sensor_dev_attr(attr)->index;
	struct measurement_s *m = &driver_data->metrics[channel];
	return sprintf(buf, "%d\n", m->max);
}

static ssize_t set_input(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct my_driver_data *driver_data = dev_get_drvdata(dev);
	int channel = to_sensor_dev_attr(attr)->index;
	struct measurement_s *m = &driver_data->metrics[channel];

	int err;
	unsigned long val;
	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	m->input = val;

	return count;
}

/* Voltage */
#define KLV_VOLTAGE(id, name) \
	static SENSOR_DEVICE_ATTR(in##id##_input, S_IRUGO | S_IWUSR, get_input, set_input, name)

#define KLV_NAMED_VOLTAGE(id, name) \
	KLV_VOLTAGE(id, name); \
	static SENSOR_DEVICE_ATTR(in##id##_label, S_IRUGO, show_label, NULL, name); \
	static SENSOR_DEVICE_ATTR(in##id##_min, S_IRUGO, get_min, NULL, name); \
	static SENSOR_DEVICE_ATTR(in##id##_max, S_IRUGO, get_max, NULL, name)

KLV_NAMED_VOLTAGE(0, KLV_SYSVDD);
/* End: Voltage */

/* Temperature */
static SENSOR_DEVICE_ATTR(temp1_input, S_IRUGO | S_IWUSR, get_input, set_input, KLV_CHIP_TEMP);
static SENSOR_DEVICE_ATTR(temp1_min, S_IRUGO, get_min, NULL, KLV_CHIP_TEMP);
static SENSOR_DEVICE_ATTR(temp1_max, S_IRUGO, get_max, NULL, KLV_CHIP_TEMP);
static SENSOR_DEVICE_ATTR(temp1_label, S_IRUGO, show_label, NULL, KLV_CHIP_TEMP);
/* End: Temperature */

/* Current */
static SENSOR_DEVICE_ATTR(curr1_input, S_IRUGO | S_IWUSR, get_input, set_input, KLV_CHIP_CURRENT);
static SENSOR_DEVICE_ATTR(curr1_min, S_IRUGO, get_min, NULL, KLV_CHIP_CURRENT);
static SENSOR_DEVICE_ATTR(curr1_max, S_IRUGO, get_max, NULL, KLV_CHIP_CURRENT);
static SENSOR_DEVICE_ATTR(curr1_label, S_IRUGO, show_label, NULL, KLV_CHIP_CURRENT);
/* End: Current */

static struct attribute *klv_attrs[] = {
	&sensor_dev_attr_in0_input.dev_attr.attr,
	&sensor_dev_attr_in0_label.dev_attr.attr,
	&sensor_dev_attr_in0_min.dev_attr.attr,
	&sensor_dev_attr_in0_max.dev_attr.attr,
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp1_label.dev_attr.attr,
	&sensor_dev_attr_temp1_min.dev_attr.attr,
	&sensor_dev_attr_temp1_max.dev_attr.attr,
	&sensor_dev_attr_curr1_input.dev_attr.attr,
	&sensor_dev_attr_curr1_label.dev_attr.attr,
	&sensor_dev_attr_curr1_min.dev_attr.attr,
	&sensor_dev_attr_curr1_max.dev_attr.attr,
	NULL
};
ATTRIBUTE_GROUPS(klv);

/* "my device" platform data */
static struct my_device_platform_data my_device_pdata = {
	.reset_gpio = 100,
	.power_on_gpio = 101,
};

static int my_driver_remove(struct platform_device *pdev)
{
	struct my_driver_data *driver_data;
	struct my_device_platform_data *my_device_pdata = dev_get_platdata(&pdev->dev);;

	my_device_pdata = dev_get_platdata(&pdev->dev);
	driver_data = platform_get_drvdata(pdev);

	/* do something with the driver */
	//devm_hwmon_device_unregister(&pdev->dev);
	//devm_hwmon_device_unregister(driver_data->hwmon_dev);

	return 0;
}

static void my_device_release(struct device *dev)
{
}

static struct platform_device my_device = {
	.name = "klvoltage-device",
	.id = PLATFORM_DEVID_NONE,
	.dev.platform_data = &my_device_pdata,
	.dev.release = my_device_release
};

void __init my_device_init_pdata(void)
{
	/* Register "my-platform-device" with the OS. */
	platform_device_register(&my_device);
}

static void initializeMeasurement(struct measurement_s *m, int input, int min, int max, int critical)
{
	m->input = input;
	m->min = min;
	m->max = max;
	m->critical = critical;
}

/* Probe driver function called by the OS when the device with the name
 * "my-platform-device" is found in the system. */
static int my_driver_probe(struct platform_device *pdev)
{
	struct my_device_platform_data *my_device_pdata;
	struct my_driver_data *driver_data;

	my_device_pdata = dev_get_platdata(&pdev->dev);

	/* Create the driver data here */
	driver_data = kzalloc(sizeof(struct my_driver_data), GFP_KERNEL);
	if (!driver_data)
		return -ENOMEM;

	/* Set this driver data in platform device structure */
	platform_set_drvdata(pdev, driver_data);

	/* Establish some reasonable default values */
	initializeMeasurement(&driver_data->metrics[KLV_SYSVDD], 5000, 1200, 24000, 24000);
	initializeMeasurement(&driver_data->metrics[KLV_CHIP_TEMP], 2000, 900, 2500, 2400);
	initializeMeasurement(&driver_data->metrics[KLV_CHIP_CURRENT], 500, 10, 5000, 6000);

	driver_data->hwmon_dev = devm_hwmon_device_register_with_groups(&pdev->dev, "klvoltage", driver_data, klv_groups);

	printk("klvoltage driver initialized.\n");
	return 0;
}

static const struct dev_pm_ops my_device_pm_ops = {
#if 0
	.suspend = my_device_pm_suspend,
	.resume = my_device_pm_resume,
#endif
};

static struct platform_driver my_driver = {
	.probe = my_driver_probe,
	.remove = my_driver_remove,
	.driver = {
		   .name = "klvoltage-device",
		   .owner = THIS_MODULE,
		   .pm = &my_device_pm_ops,
		   },
};

/* Entry point for loading this module */
static int __init my_driver_init_module(void)
{
	int ret;

	/* Add the device to the platform.
	   This has to be done by the platform specific code.
	 */
	my_device_init_pdata();

	/* We will use this when we know for sure that this device is not
	   hot-pluggable and is sure to be present in the system.
	 */
	ret = platform_driver_probe(&my_driver, my_driver_probe);

	/* We will use this when we are not sure if this device is present in the
	   system. The driver probe will be called by the OS if the device is 
	   present in the system.
	 */
	//return platform_driver_register(&ineda_femac_driver);
	return ret;
}

/* entry point for unloading the module */
static void __exit my_driver_cleanup_module(void)
{
	printk("klvoltage driver removing.\n");
	platform_device_unregister(&my_device);
	platform_driver_unregister(&my_driver);
}

module_init(my_driver_init_module);
module_exit(my_driver_cleanup_module);

MODULE_AUTHOR("Steven Toth <stoth@kernellabs.com>");
MODULE_DESCRIPTION("Dummy Voltage Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:klvoltage");
