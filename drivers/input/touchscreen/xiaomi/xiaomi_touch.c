#include "xiaomi_touch.h"

static struct xiaomi_touch_pdata *touch_pdata;
static struct xiaomi_touch *xiaomi_touch_device;

#define RAW_SIZE PAGE_SIZE * 4

static int xiaomi_touch_dev_open(struct inode *inode, struct file *file)
{
	struct xiaomi_touch *dev = NULL;
	int i = MINOR(inode->i_rdev);
	struct xiaomi_touch_pdata *touch_pdata;

	pr_info("%s\n", __func__);
	dev = xiaomi_touch_dev_get(i);
	if (!dev) {
		pr_err("%s cant get dev\n", __func__);
		return -ENOMEM;
	}
	touch_pdata = dev_get_drvdata(dev->dev);

	file->private_data = touch_pdata;
	return 0;
}

static ssize_t xiaomi_touch_dev_read(struct file *file, char __user *buf,
			   size_t count, loff_t *pos)
{
	return 0;
}

static ssize_t xiaomi_touch_dev_write(struct file *file,
		const char __user *buf, size_t count, loff_t *pos)
{
	return 0;
}

static unsigned int xiaomi_touch_dev_poll(struct file *file,
		poll_table *wait)
{
	return 0;
}

static long xiaomi_touch_dev_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	int ret = -EINVAL;
	int buf[MAX_BUF_SIZE] = {0,};
	struct xiaomi_touch_pdata *pdata = file->private_data;
	void __user *argp = (void __user *) arg;
	struct xiaomi_touch_interface *touch_data = NULL;
	struct xiaomi_touch *dev = pdata->device;
	int user_cmd = _IOC_NR(cmd);

	mutex_lock(&dev->mutex);
	ret = copy_from_user(&buf, (int __user *)argp, sizeof(buf));
	if (buf[0] < 0 || buf[0] > 1) {
		pr_err("%s invalid param\n", __func__);
		mutex_unlock(&dev->mutex);
		return -EINVAL;
	}

	touch_data = pdata->touch_data[buf[0]];
	if (!pdata || !touch_data || !dev) {
		pr_err("%s invalid memory\n", __func__);
		mutex_unlock(&dev->mutex);
		return -ENOMEM;
	}

	pr_info("%s cmd:%d, touchId:%d, mode:%d, value:%d\n", __func__, user_cmd, buf[0], buf[1], buf[2]);

	switch (user_cmd) {
	case SET_CUR_VALUE:
		if (touch_data->setModeValue)
			buf[0] = touch_data->setModeValue(buf[1], buf[2]);
		break;
	case GET_CUR_VALUE:
	case GET_DEF_VALUE:
	case GET_MIN_VALUE:
	case GET_MAX_VALUE:
		if (touch_data->getModeValue)
			buf[0] = touch_data->getModeValue(buf[1], user_cmd);
		break;
	case RESET_MODE:
		if (touch_data->resetMode)
			buf[0] = touch_data->resetMode(buf[1]);
		break;
	case GET_MODE_VALUE:
		if (touch_data->getModeValue)
			ret = touch_data->getModeAll(buf[1], buf);
		break;
	case SET_LONG_VALUE:
		if (touch_data->setModeLongValue && buf[2] <= MAX_BUF_SIZE)
			ret = touch_data->setModeLongValue(buf[1], buf[2], &buf[3]);
		break;
	default:
		pr_err("%s don't support mode\n", __func__);
		ret = -EINVAL;
		break;
	}
	if (ret >= 0)
		ret = copy_to_user((int __user *)argp, &buf, sizeof(buf));
	else
		pr_err("%s can't get data from touch driver\n", __func__);

	mutex_unlock(&dev->mutex);

	return ret;
}

static int xiaomi_touch_dev_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct xiaomi_touch_pdata *pdata = file->private_data;
	unsigned long start = vma->vm_start;
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long page;
	unsigned long pos;

	if (!pdata) {
		pr_err("%s invalid memory\n", __func__);
		return -ENOMEM;
	}

	/*
	tx_num = pdata->touch_data->get_touch_tx_num();
	rx_num = pdata->touch_data->get_touch_rx_num();
	*/

	pos = (unsigned long)pdata->phy_base + offset;
	page = pos >> PAGE_SHIFT;

	if(remap_pfn_range(vma, start, page, size, PAGE_SHARED)) {
		return -EAGAIN;
	} else {
		pr_info( "%s remap_pfn_range %u, size:%ld, success\n", __func__, (unsigned int)page, size);
	}
	return 0;
}



static int xiaomi_touch_dev_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations xiaomitouch_dev_fops = {
	.owner = THIS_MODULE,
	.open = xiaomi_touch_dev_open,
	.read = xiaomi_touch_dev_read,
	.write = xiaomi_touch_dev_write,
	.poll = xiaomi_touch_dev_poll,
	.mmap = xiaomi_touch_dev_mmap,
	.unlocked_ioctl = xiaomi_touch_dev_ioctl,
	.compat_ioctl = xiaomi_touch_dev_ioctl,
	.release = xiaomi_touch_dev_release,
	.llseek	= no_llseek,
};

static struct xiaomi_touch xiaomi_touch_dev = {
	.misc_dev = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = "xiaomi-touch",
		.fops = &xiaomitouch_dev_fops,
		.parent = NULL,
	},
	.mutex = __MUTEX_INITIALIZER(xiaomi_touch_dev.mutex),
	.palm_mutex = __MUTEX_INITIALIZER(xiaomi_touch_dev.palm_mutex),
	.prox_mutex = __MUTEX_INITIALIZER(xiaomi_touch_dev.prox_mutex),
	.wait_queue = __WAIT_QUEUE_HEAD_INITIALIZER(xiaomi_touch_dev.wait_queue),
	.fod_press_status_mutex = __MUTEX_INITIALIZER(xiaomi_touch_dev.fod_press_status_mutex),
};

struct xiaomi_touch *xiaomi_touch_dev_get(int minor)
{
	if (xiaomi_touch_dev.misc_dev.minor == minor)
		return &xiaomi_touch_dev;
	else
		return NULL;
}

struct class *get_xiaomi_touch_class(void)
{
	return xiaomi_touch_dev.class;
}
EXPORT_SYMBOL_GPL(get_xiaomi_touch_class);

struct device *get_xiaomi_touch_dev(void)
{
	return xiaomi_touch_dev.dev;
}
EXPORT_SYMBOL_GPL(get_xiaomi_touch_dev);

int xiaomitouch_register_modedata(int touchId, struct xiaomi_touch_interface *data)
{
	int ret = 0;
	struct xiaomi_touch_interface *touch_data = NULL;

	if (!touch_pdata)
		ret = -ENOMEM;

	touch_data = touch_pdata->touch_data[touchId];
	pr_info("%s\n", __func__);

	mutex_lock(&xiaomi_touch_dev.mutex);

	if (data->setModeValue)
		touch_data->setModeValue = data->setModeValue;
	if (data->getModeValue)
		touch_data->getModeValue = data->getModeValue;
	if (data->resetMode)
		touch_data->resetMode = data->resetMode;
	if (data->getModeAll)
		touch_data->getModeAll = data->getModeAll;
	if (data->palm_sensor_read)
		touch_data->palm_sensor_read = data->palm_sensor_read;
	if (data->palm_sensor_write)
		touch_data->palm_sensor_write = data->palm_sensor_write;
	if (data->prox_sensor_read)
		touch_data->prox_sensor_read = data->prox_sensor_read;
	if (data->prox_sensor_write)
		touch_data->prox_sensor_write = data->prox_sensor_write;
	if (data->panel_vendor_read)
		touch_data->panel_vendor_read = data->panel_vendor_read;
	if (data->panel_color_read)
		touch_data->panel_color_read = data->panel_color_read;
	if (data->panel_display_read)
		touch_data->panel_display_read = data->panel_display_read;
	if (data->touch_vendor_read)
		touch_data->touch_vendor_read = data->touch_vendor_read;
	if (data->setModeLongValue)
		touch_data->setModeLongValue = data->setModeLongValue;
	if (data->get_touch_rx_num)
		touch_data->get_touch_rx_num = data->get_touch_rx_num;
	if (data->get_touch_tx_num)
		touch_data->get_touch_tx_num = data->get_touch_tx_num;
	if (data->get_touch_x_resolution)
		touch_data->get_touch_x_resolution = data->get_touch_x_resolution;
	if (data->get_touch_y_resolution)
		touch_data->get_touch_y_resolution = data->get_touch_y_resolution;
	if (data->enable_touch_raw)
		touch_data->enable_touch_raw = data->enable_touch_raw;
	if (data->enable_touch_delta)
		touch_data->enable_touch_delta = data->enable_touch_delta;
	if (data->enable_clicktouch_raw)
		touch_data->enable_clicktouch_raw = data->enable_clicktouch_raw;

	mutex_unlock(&xiaomi_touch_dev.mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(xiaomitouch_register_modedata);

int update_palm_sensor_value(int value)
{
	struct xiaomi_touch *dev = NULL;

	mutex_lock(&xiaomi_touch_dev.palm_mutex);

	if (!touch_pdata) {
		mutex_unlock(&xiaomi_touch_dev.palm_mutex);
		return -ENODEV;
	}

	dev = touch_pdata->device;

	if (value != touch_pdata->palm_value) {
		pr_info("%s value:%d\n", __func__, value);
		touch_pdata->palm_value = value;
		touch_pdata->palm_changed = true;
		sysfs_notify(&xiaomi_touch_dev.dev->kobj, NULL,
		     "palm_sensor");
	}

	mutex_unlock(&xiaomi_touch_dev.palm_mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(update_palm_sensor_value);

static ssize_t palm_sensor_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct xiaomi_touch_pdata *pdata = dev_get_drvdata(dev);

	pdata->palm_changed = false;

	return snprintf(buf, PAGE_SIZE, "%d\n", pdata->palm_value);
}

static ssize_t palm_sensor_store(struct device *dev,
struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	struct xiaomi_touch_pdata *pdata = dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &input) < 0)
			return -EINVAL;

	if (pdata->touch_data[0]->palm_sensor_write)
		pdata->touch_data[0]->palm_sensor_write(!!input);
	else {
		pr_err("%s has not implement\n", __func__);
	}
	pr_info("%s value:%d\n", __func__, !!input);

	return count;
}

static ssize_t set_update_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct xiaomi_touch_pdata *pdata = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", pdata->set_update);
}

static ssize_t set_update_store(struct device *dev,
struct device_attribute *attr, const char *buf, size_t count)
{
	struct xiaomi_touch_pdata *pdata = dev_get_drvdata(dev);
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);

	if (ret < 0)
		return -EINVAL;

	pdata->set_update = !!input;

	return count;
}

static ssize_t bump_sample_rate_start(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct xiaomi_touch_pdata *pdata = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", pdata->bump_sample_rate);
}

static ssize_t bump_sample_rate_store(struct device *dev,
struct device_attribute *attr, const char *buf, size_t count)
{
	struct xiaomi_touch_pdata *pdata = dev_get_drvdata(dev);
	struct xiaomi_touch_interface *touch_data = pdata->touch_data[0];
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);

	if (ret < 0)
		return -EINVAL; // Avoid possible crashes

	if(input) {
		pdata->bump_sample_rate = true;
		pdata->set_update = true;
		touch_data->setModeValue(0, 1);
		touch_data->setModeValue(1, 1);
		touch_data->setModeValue(3, 34);
		touch_data->setModeValue(2, 99);
		touch_data->setModeValue(7, 0);
	} else {
		pdata->bump_sample_rate = false;
		pdata->set_update = false;
		touch_data->resetMode(0);
	}

	return count;
}

int update_prox_sensor_value(int value)
{
	struct xiaomi_touch *dev = NULL;

	mutex_lock(&xiaomi_touch_dev.prox_mutex);

	if (!touch_pdata) {
		mutex_unlock(&xiaomi_touch_dev.prox_mutex);
		return -ENODEV;
	}

	dev = touch_pdata->device;

	if (value != touch_pdata->prox_value) {
		pr_info("%s value:%d\n", __func__, value);
		touch_pdata->prox_value = value;
		touch_pdata->prox_changed = true;
		sysfs_notify(&xiaomi_touch_dev.dev->kobj, NULL,
		     "prox_sensor");
	}

	mutex_unlock(&xiaomi_touch_dev.prox_mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(update_prox_sensor_value);

static ssize_t prox_sensor_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct xiaomi_touch_pdata *pdata = dev_get_drvdata(dev);

	pdata->prox_changed = false;

	return snprintf(buf, PAGE_SIZE, "%d\n", pdata->prox_changed);
}

static ssize_t prox_sensor_store(struct device *dev,
struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	struct xiaomi_touch_pdata *pdata = dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &input) < 0)
			return -EINVAL;

	if (pdata->touch_data[0]->prox_sensor_write)
		pdata->touch_data[0]->prox_sensor_write(!!input);
	else {
		pr_err("%s has not implement\n", __func__);
	}
	pr_info("%s value:%d\n", __func__, !!input);

	return count;
}

static ssize_t panel_vendor_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct xiaomi_touch_pdata *pdata = dev_get_drvdata(dev);

	if (pdata->touch_data[0]->panel_vendor_read)
		return snprintf(buf, PAGE_SIZE, "%c", pdata->touch_data[0]->panel_vendor_read());
	else
		return 0;
}

static ssize_t panel_color_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct xiaomi_touch_pdata *pdata = dev_get_drvdata(dev);

	if (pdata->touch_data[0]->panel_color_read)
		return snprintf(buf, PAGE_SIZE, "%c", pdata->touch_data[0]->panel_color_read());
	else
		return 0;
}

static ssize_t panel_display_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct xiaomi_touch_pdata *pdata = dev_get_drvdata(dev);

	if (pdata->touch_data[0]->panel_display_read)
		return snprintf(buf, PAGE_SIZE, "%c", pdata->touch_data[0]->panel_display_read());
	else
		return 0;
}

static ssize_t touch_vendor_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct xiaomi_touch_pdata *pdata = dev_get_drvdata(dev);

	if (pdata->touch_data[0]->touch_vendor_read)
		return snprintf(buf, PAGE_SIZE, "%c", pdata->touch_data[0]->touch_vendor_read());
	else
		return 0;
}

int copy_touch_rawdata(char *raw_base,  int len)
{
	struct xiaomi_touch *dev = NULL;

	if (!touch_pdata) {
		return -ENODEV;
	}

	dev = touch_pdata->device;

	if (touch_pdata->raw_data) {
		memcpy((unsigned char *)touch_pdata->raw_data,  (unsigned char *)raw_base,  len);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(copy_touch_rawdata);

int update_touch_rawdata(void)
{
	sysfs_notify(&xiaomi_touch_dev.dev->kobj, NULL,  "update_rawdata");

	return 0;
}
EXPORT_SYMBOL_GPL(update_touch_rawdata);

static ssize_t enable_touchraw_store(struct device *dev,
struct device_attribute *attr, const char *buf, size_t count)
{
	struct xiaomi_touch_interface *touch_data = NULL;
	unsigned int input;

	if (!touch_pdata) {
		return -ENOMEM;
	}
	touch_data = touch_pdata->touch_data[0];

	if (sscanf(buf, "%d", &input) < 0)
			return -EINVAL;

	pr_info("%s,%d\n", __func__, input);
	if (touch_data->enable_touch_raw)
		touch_data->enable_touch_raw(!!input);
	touch_data->is_enable_touchraw = !!input;
	return count;
}

static ssize_t enable_touchraw_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct xiaomi_touch_interface *touch_data = NULL;

	if (!touch_pdata) {
		return -ENOMEM;
	}
	touch_data = touch_pdata->touch_data[0];


	return snprintf(buf, PAGE_SIZE, "%d\n", touch_data->is_enable_touchraw);
}

static ssize_t enable_touchdelta_store(struct device *dev,
struct device_attribute *attr, const char *buf, size_t count)
{
	struct xiaomi_touch_interface *touch_data = NULL;
	unsigned int input;

	if (!touch_pdata) {
		return -ENOMEM;
	}
	touch_data = touch_pdata->touch_data[0];

	if (sscanf(buf, "%d", &input) < 0)
			return -EINVAL;

	pr_info("%s,%d\n", __func__, input);
	if (touch_data->enable_touch_delta)
		touch_data->enable_touch_delta(!!input);

	touch_data->is_enable_touchdelta = !!input;
	return count;
}

static ssize_t enable_touchdelta_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct xiaomi_touch_interface *touch_data = NULL;

	if (!touch_pdata) {
		return -ENOMEM;
	}
	touch_data = touch_pdata->touch_data[0];


	return snprintf(buf, PAGE_SIZE, "%d\n", touch_data->is_enable_touchdelta);
}

static ssize_t update_rawdata_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", "1");
}

static ssize_t enable_clicktouch_store(struct device *dev,
struct device_attribute *attr, const char *buf, size_t count)
{
	struct xiaomi_touch_interface *touch_data = NULL;
	unsigned int input;

	if (!touch_pdata) {
		return -ENOMEM;
	}
	touch_data = touch_pdata->touch_data[0];

	if (sscanf(buf, "%d", &input) < 0)
			return -EINVAL;

	pr_info("%s,%d\n", __func__, input);
	if (touch_data->enable_clicktouch_raw)
		touch_data->enable_clicktouch_raw(input);

	return count;
}

static ssize_t enable_clicktouch_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", "1");
}

int update_clicktouch_raw(void)
{
	sysfs_notify(&xiaomi_touch_dev.dev->kobj, NULL,  "clicktouch_raw");

	return 0;
}
EXPORT_SYMBOL_GPL(update_clicktouch_raw);

int xiaomi_touch_set_suspend_state(int state)
{
	if (!touch_pdata) {
		return -ENODEV;
	}
	touch_pdata->suspend_state = state;

	sysfs_notify(&xiaomi_touch_dev.dev->kobj, NULL,  "suspend_state");

	return 0;
}
EXPORT_SYMBOL_GPL(xiaomi_touch_set_suspend_state);

static ssize_t xiaomi_touch_suspend_state(struct device *dev,
struct device_attribute *attr, char *buf)
{
	if (!touch_pdata) {
		return -ENODEV;
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", touch_pdata->suspend_state);
}

int update_fod_press_status(int value)
{
	struct xiaomi_touch *dev = NULL;

	mutex_lock(&xiaomi_touch_dev.fod_press_status_mutex);

	if (!touch_pdata) {
		mutex_unlock(&xiaomi_touch_dev.fod_press_status_mutex);
		return -ENODEV;
	}

	dev = touch_pdata->device;

	if (value != touch_pdata->fod_press_status_value) {
		pr_info("%s: value:%d\n", __func__, value);
		touch_pdata->fod_press_status_value = value;
		sysfs_notify(&xiaomi_touch_dev.dev->kobj, NULL,
			     "fod_press_status");
	}

	mutex_unlock(&xiaomi_touch_dev.fod_press_status_mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(update_fod_press_status);

static ssize_t fod_press_status_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct xiaomi_touch_pdata *pdata = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", pdata->fod_press_status_value);
}

static DEVICE_ATTR(enable_touch_raw, (S_IRUGO | S_IWUSR | S_IWGRP),
		   enable_touchraw_show, enable_touchraw_store);

static DEVICE_ATTR(enable_touch_delta, (S_IRUGO | S_IWUSR | S_IWGRP),
		   enable_touchdelta_show, enable_touchdelta_store);

static DEVICE_ATTR(palm_sensor, (S_IRUGO | S_IWUSR | S_IWGRP),
		   palm_sensor_show, palm_sensor_store);

static DEVICE_ATTR(prox_sensor, (S_IRUGO | S_IWUSR | S_IWGRP),
		   prox_sensor_show, prox_sensor_store);

static DEVICE_ATTR(clicktouch_raw, (S_IRUGO | S_IWUSR | S_IWGRP),
		   enable_clicktouch_show, enable_clicktouch_store);

static DEVICE_ATTR(panel_vendor, (S_IRUGO), panel_vendor_show, NULL);

static DEVICE_ATTR(panel_color, (S_IRUGO), panel_color_show, NULL);

static DEVICE_ATTR(panel_display, (S_IRUGO), panel_display_show, NULL);

static DEVICE_ATTR(touch_vendor, (S_IRUGO), touch_vendor_show, NULL);

static DEVICE_ATTR(set_update, (S_IRUGO | S_IWUSR | S_IWGRP),
		   set_update_show, set_update_store);

static DEVICE_ATTR(bump_sample_rate, (S_IRUGO | S_IWUSR | S_IWGRP),
		   bump_sample_rate_start, bump_sample_rate_store);

static DEVICE_ATTR(suspend_state, 0644, xiaomi_touch_suspend_state, NULL);

static DEVICE_ATTR(update_rawdata, 0644, update_rawdata_show, NULL);

static DEVICE_ATTR(fod_press_status, (0664), fod_press_status_show, NULL);

static struct attribute *touch_attr_group[] = {
	&dev_attr_enable_touch_raw.attr,
	&dev_attr_enable_touch_delta.attr,
	&dev_attr_clicktouch_raw.attr,
	&dev_attr_palm_sensor.attr,
	&dev_attr_prox_sensor.attr,
	&dev_attr_panel_vendor.attr,
	&dev_attr_panel_color.attr,
	&dev_attr_set_update.attr,
	&dev_attr_bump_sample_rate.attr,
	&dev_attr_panel_display.attr,
	&dev_attr_touch_vendor.attr,
	&dev_attr_update_rawdata.attr,
	&dev_attr_suspend_state.attr,
	&dev_attr_fod_press_status.attr,
	NULL,
};

static const struct of_device_id xiaomi_touch_of_match[] = {
	{ .compatible = "xiaomi-touch", },
	{ },
};

static int xiaomi_touch_parse_dt(struct device *dev, struct xiaomi_touch_pdata *data)
{
	int ret;
	struct device_node *np;

	np = dev->of_node;
	if (!np)
		return -ENODEV;

	ret = of_property_read_string(np, "touch,name", &data->name);
	if (ret)
		return ret;

	pr_info("%s touch,name:%s\n", __func__, data->name);

	return 0;
}

static int xiaomi_touch_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct xiaomi_touch_pdata *pdata;

	pdata = devm_kzalloc(dev, sizeof(struct xiaomi_touch_pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->raw_data = (unsigned int *)kzalloc(RAW_SIZE, GFP_KERNEL);
	if (!pdata->raw_data) {
		ret = -ENOMEM;
		pr_err("%s alloc mem for raw data\n", __func__);
		goto parse_dt_err;
	}
	pdata->phy_base = virt_to_phys(pdata->raw_data);
	pr_info("%s: kernel base:%lld, phy base:%lld\n", __func__,	(unsigned long)pdata->raw_data, (unsigned long)pdata->phy_base);

	pr_info("%s enter\n", __func__);

	ret = xiaomi_touch_parse_dt(dev, pdata);
	if (ret < 0) {
		pr_err("%s parse dt error:%d\n", __func__, ret);
		goto parse_dt_err;
	}

	ret = misc_register(&xiaomi_touch_dev.misc_dev);
	if (ret) {
		pr_err("%s create misc device err:%d\n", __func__, ret);
		goto parse_dt_err;
	}
	xiaomi_touch_device = &xiaomi_touch_dev;
	if (!xiaomi_touch_dev.class)
		xiaomi_touch_dev.class = class_create(THIS_MODULE, "touch");

	if (!xiaomi_touch_dev.class) {
		pr_err("%s create device class err\n", __func__);
		goto class_create_err;
	}

	xiaomi_touch_dev.dev = device_create(xiaomi_touch_dev.class, NULL, 'T', NULL, "touch_dev");
	if (!xiaomi_touch_dev.dev) {
		pr_err("%s create device dev err\n", __func__);
		goto device_create_err;
	}

	pdata->touch_data[0] = (struct xiaomi_touch_interface *)kzalloc(sizeof(struct xiaomi_touch_interface), GFP_KERNEL);
	if (pdata->touch_data[0] == NULL) {
		ret = -ENOMEM;
		pr_err("%s alloc mem for touch_data\n", __func__);
		goto data_mem_err;
	}
	pdata->touch_data[1] = (struct xiaomi_touch_interface *)kzalloc(sizeof(struct xiaomi_touch_interface), GFP_KERNEL);
	if (pdata->touch_data[1] == NULL) {
		ret = -ENOMEM;
		pr_err("%s alloc mem for touch_data\n", __func__);
		goto sys_group_err;
	}

	pdata->device = &xiaomi_touch_dev;
	dev_set_drvdata(xiaomi_touch_dev.dev, pdata);

	touch_pdata = pdata;

	xiaomi_touch_dev.attrs.attrs = touch_attr_group;
	ret = sysfs_create_group(&xiaomi_touch_dev.dev->kobj, &xiaomi_touch_dev.attrs);
	if (ret) {
		pr_err("%s ERROR: Cannot create sysfs structure!:%d\n", __func__, ret);
		ret = -ENODEV;
		goto sys_group_err;
	}

	pr_info("%s over\n", __func__);

	return ret;

sys_group_err:
	if (pdata->touch_data[0]) {
		kfree(pdata->touch_data[0]);
		pdata->touch_data[0] = NULL;
	}
	if (pdata->touch_data[1]) {
		kfree(pdata->touch_data[1]);
		pdata->touch_data[1] = NULL;
	}
data_mem_err:
	device_destroy(xiaomi_touch_dev.class, 'T');
device_create_err:
	class_destroy(xiaomi_touch_dev.class);
	xiaomi_touch_dev.class = NULL;
class_create_err:
	misc_deregister(&xiaomi_touch_dev.misc_dev);
parse_dt_err:
	if (pdata->raw_data) {
		kfree(pdata->raw_data);
		pdata->raw_data = NULL;
	}
	pr_err("%s fail!\n", __func__);
	return ret;
}

static int xiaomi_touch_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&xiaomi_touch_dev.dev->kobj, &xiaomi_touch_dev.attrs);
	device_destroy(xiaomi_touch_dev.class, 'T');
	class_destroy(xiaomi_touch_dev.class);
	xiaomi_touch_dev.class = NULL;
	misc_deregister(&xiaomi_touch_dev.misc_dev);
	if (touch_pdata->raw_data) {
		kfree(touch_pdata->raw_data);
		touch_pdata->raw_data = NULL;
	}
	if (touch_pdata->touch_data[0]) {
		kfree(touch_pdata->touch_data[0]);
		touch_pdata->touch_data[0] = NULL;
	}
	if (touch_pdata->touch_data[1]) {
		kfree(touch_pdata->touch_data[1]);
		touch_pdata->touch_data[1] = NULL;
	}

	return 0;
}

static struct platform_driver xiaomi_touch_device_driver = {
	.probe		= xiaomi_touch_probe,
	.remove		= xiaomi_touch_remove,
	.driver		= {
		.name	= "xiaomi-touch",
		.of_match_table = of_match_ptr(xiaomi_touch_of_match),
	}
};

static int __init xiaomi_touch_init(void)
{
	return platform_driver_register(&xiaomi_touch_device_driver);

}

static void __exit xiaomi_touch_exit(void)
{
	platform_driver_unregister(&xiaomi_touch_device_driver);
}

MODULE_LICENSE("GPL");

module_init(xiaomi_touch_init);
module_exit(xiaomi_touch_exit);
