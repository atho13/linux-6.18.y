// SPDX-License-Identifier: GPL-2.0
/*
 * aml_wifi_stubs.c - Amlogic BSP symbols for mainline Meson kernels
 * Timing: KryptonLee/uwe5621ds-aml (0 / 100ms / 1)
 * Scan: CE uwe5631-aml — reset host without unregistering card
 * Tips: github.com/devmfc/debian-on-amlogic/discussions/259
 */
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sdio_func.h>
#include <linux/device.h>
#include <linux/version.h>

#define STUB_TAG "aml_wifi_stub: "

#ifndef GPIOD_FLAGS_BIT_NONEXCLUSIVE
#define GPIOD_FLAGS_BIT_NONEXCLUSIVE 0
#endif
#ifndef GPIO_IRQ_HIGH
#define GPIO_IRQ_HIGH 1
#endif
#ifndef GPIO_IRQ_LOW
#define GPIO_IRQ_LOW 0
#endif

/*
 * Prefix of drivers/mmc/host/meson-gx-mmc.c::struct meson_host —
 * must stay in sync with that driver's first three fields.
 */
struct aml_meson_host_hdr {
	struct device *dev;
	const void *data;
	struct mmc_host *mmc;
};

static DEFINE_MUTEX(wifi_en_lock);
static struct gpio_desc *wifi_en_gpio;

static int aml_bind_wifi_en_gpio(void)
{
	struct device_node *np;
	struct gpio_desc *desc;

	if (wifi_en_gpio)
		return 0;

	np = of_find_node_by_name(NULL, "sdio-pwrseq");
	if (!np) {
		pr_err(STUB_TAG "sdio-pwrseq node not found\n");
		return -ENODEV;
	}

	desc = fwnode_gpiod_get_index(of_fwnode_handle(np), "reset", 0,
				      GPIOD_ASIS | GPIOD_FLAGS_BIT_NONEXCLUSIVE,
				      "wifi-en");
	of_node_put(np);
	if (IS_ERR(desc)) {
		pr_err(STUB_TAG "reset-gpios get err %ld\n", PTR_ERR(desc));
		return PTR_ERR(desc);
	}

	wifi_en_gpio = desc;
	pr_info(STUB_TAG "bound wifi_en to sdio-pwrseq reset-gpios\n");
	return 0;
}

void extern_wifi_set_enable(int is_on)
{
	int ret;

	mutex_lock(&wifi_en_lock);
	ret = aml_bind_wifi_en_gpio();
	if (ret) {
		mutex_unlock(&wifi_en_lock);
		return;
	}

	if (is_on) {
		gpiod_set_value_cansleep(wifi_en_gpio, 0);
		pr_info(STUB_TAG "power on: reset deasserted\n");
	} else {
		gpiod_set_value_cansleep(wifi_en_gpio, 1);
		pr_info(STUB_TAG "power off: reset asserted\n");
	}
	mutex_unlock(&wifi_en_lock);
}

void extern_bt_set_enable(int is_on)
{
	extern_wifi_set_enable(is_on);
}

static struct mmc_host *aml_get_mmc0_host(void)
{
	struct device *dev;
	struct aml_meson_host_hdr *hdr;
	static const char * const names[] = {
		"ffe03000.mmc", "mmc@ffe03000", NULL
	};
	int i;

	for (i = 0; names[i]; i++) {
		dev = bus_find_device_by_name(&platform_bus_type, NULL, names[i]);
		if (!dev)
			continue;
		hdr = dev_get_drvdata(dev);
		put_device(dev);
		if (hdr && hdr->mmc) {
			pr_info(STUB_TAG "found mmc host via %s (%s)\n",
				names[i], mmc_hostname(hdr->mmc));
			return hdr->mmc;
		}
	}
	return NULL;
}

void sdio_reinit(void)
{
	struct mmc_host *host;
	int ret;

	host = aml_get_mmc0_host();
	if (!host) {
		pr_err(STUB_TAG "%s: SDIO host not found\n", __func__);
		return;
	}
	if (!host->card) {
		pr_info(STUB_TAG "%s: no card, mmc_detect_change\n", __func__);
		mmc_detect_change(host, msecs_to_jiffies(100));
		msleep(500);
		return;
	}
	pr_info(STUB_TAG "%s: mmc_hw_reset(%s)\n", __func__, mmc_hostname(host));
	/* mmc_hw_reset needs claimed host */
	if (host->card && host->card->sdio_func[0])
		sdio_claim_host(host->card->sdio_func[0]);
	ret = mmc_hw_reset(host->card);
	if (host->card && host->card->sdio_func[0])
		sdio_release_host(host->card->sdio_func[0]);
	pr_info(STUB_TAG "%s: mmc_hw_reset ret=%d\n", __func__, ret);
	msleep(100);
}

void sdio_clk_always_on(int on)
{
	pr_debug(STUB_TAG "%s(%d): no-op on Meson\n", __func__, on);
}

void sdio_set_max_reqsz(unsigned int size)
{
	pr_debug(STUB_TAG "%s(%u): no-op on Meson\n", __func__, size);
}

int wifi_irq_num(void)
{
	return 0;
}

int wifi_irq_trigger_level(void)
{
	return GPIO_IRQ_HIGH;
}
